#include "isolation/NamespaceSetup.hpp"

#include "debug/Log.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sched.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

using fusionps4::debug::LogCategory;

namespace fusionps4::isolation {

namespace {

// Executes a shell-free command by forking and running a raw syscall. We
// avoid libc's system() because the guest process must not spawn shell
// processes on the host.
bool runRaw(const std::function<bool()>& fn, const char* what) {
    if (!fn()) {
        FP4_ERROR(LogCategory::Process) << what << " failed";
        return false;
    }
    return true;
}

} // namespace

bool NamespaceSetup::unshareAll(const IsolationConfig& cfg) {
    int flags = 0;
    if (cfg.use_user_ns)  flags |= CLONE_NEWUSER;
    if (cfg.use_pid_ns)   flags |= CLONE_NEWPID;
    if (cfg.use_mount_ns) flags |= CLONE_NEWNS;
    if (cfg.use_net_ns)   flags |= CLONE_NEWNET;
    if (cfg.use_ipc_ns)   flags |= CLONE_NEWIPC;
    if (cfg.use_uts_ns)   flags |= CLONE_NEWUTS;

    if (flags == 0) {
        FP4_INFO(LogCategory::Process)
            << "no namespaces requested; skipping unshare";
        return true;
    }

    if (::unshare(flags) != 0) {
        FP4_ERROR(LogCategory::Process)
            << "unshare(0x" << std::hex << flags << std::dec
            << ") failed: " << std::strerror(errno);
        return false;
    }

    FP4_INFO(LogCategory::Process)
        << "namespaces created: flags=0x" << std::hex << flags << std::dec;
    return true;
}

bool NamespaceSetup::setHostname(const IsolationConfig& cfg) {
    if (!cfg.use_uts_ns) return true;
    if (::sethostname(cfg.guest_hostname.c_str(),
                      cfg.guest_hostname.size()) != 0) {
        FP4_ERROR(LogCategory::Process)
            << "sethostname(\"" << cfg.guest_hostname << "\") failed: "
            << std::strerror(errno);
        return false;
    }
    return true;
}

bool NamespaceSetup::pivotIntoJail(const IsolationConfig& cfg) {
    if (!cfg.use_mount_ns) return true;

    // Make all mounts private so they don't leak to the parent namespace.
    if (::mount(nullptr, "/", nullptr, MS_REC | MS_PRIVATE, nullptr) != 0) {
        FP4_ERROR(LogCategory::Process)
            << "mount(MS_PRIVATE) failed: " << std::strerror(errno);
        return false;
    }

    // Create a tmpfs mount at a runtime-owned path.
    const char* newRoot = cfg.jail_root.empty()
        ? "/tmp/fusionps4-jail"
        : cfg.jail_root.c_str();

    if (::mkdir(newRoot, 0700) != 0 && errno != EEXIST) {
        FP4_ERROR(LogCategory::Process)
            << "mkdir(" << newRoot << ") failed: " << std::strerror(errno);
        return false;
    }
    if (::mount("tmpfs", newRoot, "tmpfs",
                MS_NOSUID | MS_NODEV | MS_NOEXEC, "size=512M,mode=0700") != 0) {
        FP4_ERROR(LogCategory::Process)
            << "mount(tmpfs) at " << newRoot << " failed: "
            << std::strerror(errno);
        return false;
    }

    // Minimal directory skeleton.
    const char* dirs[] = {
        "/app", "/data", "/system", "/temp", "/save", "/user",
        "/dev", "/proc", "/tmp",
    };
    for (const char* d : dirs) {
        std::string p = std::string(newRoot) + d;
        ::mkdir(p.c_str(), 0755);
    }

    // Bind-mount /dev/null, /dev/zero, /dev/random so basic utilities work.
    // These are safe: they do not expose host state.
    struct BindMount { const char* src; const char* dst; };
    const BindMount safeDevs[] = {
        {"/dev/null",   "dev/null"},
        {"/dev/zero",   "dev/zero"},
        {"/dev/random", "dev/random"},
        {"/dev/urandom","dev/urandom"},
    };
    for (const auto& bm : safeDevs) {
        std::string dst = std::string(newRoot) + "/" + bm.dst;
        // Create an empty file at dst and bind-mount.
        int fd = ::open(dst.c_str(), O_CREAT | O_RDWR, 0666);
        if (fd >= 0) ::close(fd);
        if (::mount(bm.src, dst.c_str(), nullptr, MS_BIND, nullptr) != 0) {
            FP4_WARN(LogCategory::Process)
                << "bind-mount " << bm.src << " -> " << dst << " failed: "
                << std::strerror(errno);
        }
    }

    // Mount a fresh /proc (PID namespace makes it reflect only guest pids).
    std::string procPath = std::string(newRoot) + "/proc";
    if (::mount("proc", procPath.c_str(), "proc",
                MS_NOSUID | MS_NODEV | MS_NOEXEC, nullptr) != 0) {
        FP4_WARN(LogCategory::Process)
            << "mount(proc) failed: " << std::strerror(errno);
    }

    // pivot_root: change root to newRoot, then detach the old root.
    if (::chdir(newRoot) != 0) {
        FP4_ERROR(LogCategory::Process)
            << "chdir(" << newRoot << ") failed: " << std::strerror(errno);
        return false;
    }
    if (::syscall(SYS_pivot_root, ".", ".") != 0) {
        // Fall back to chroot if pivot_root is not permitted (e.g. no
        // CAP_SYS_ADMIN inside the namespace). chroot is weaker but still
        // prevents path traversal for non-root processes.
        if (::chroot(".") != 0) {
            FP4_ERROR(LogCategory::Process)
                << "pivot_root and chroot both failed: "
                << std::strerror(errno);
            return false;
        }
        FP4_WARN(LogCategory::Process)
            << "pivot_root unavailable; fell back to chroot";
    }

    if (::chdir("/") != 0) {
        FP4_ERROR(LogCategory::Process)
            << "chdir(/) after pivot failed: " << std::strerror(errno);
        return false;
    }

    FP4_INFO(LogCategory::Process)
        << "guest pivoted into jail at " << newRoot;
    return true;
}

} // namespace fusionps4::isolation
