#include "syscall/handlers/Handlers.hpp"

#include "debug/Log.hpp"
#include "filesystem/virtual/VirtualFileSystem.hpp"
#include "runtime/handles/DirectoryHandle.hpp"
#include "runtime/handles/EventHandle.hpp"
#include "runtime/handles/FileHandle.hpp"
#include "runtime/handles/SocketHandle.hpp"
#include "runtime/memory/AddressSpace.hpp"
#include "runtime/process/PS4Process.hpp"
#include "runtime/thread/ThreadObject.hpp"
#include "syscall/freebsd/FreeBsd.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <unistd.h>

using namespace fusionps4::syscall::freebsd;

namespace fusionps4::syscall::handlers {

namespace {

// A per-process context for the FS layer. Owned by PS4Process in Phase 4;
// for Phase 3 it is a static, single-process instance, which is sufficient
// because FusionPS4 currently hosts exactly one guest process.
filesystem::virtual_fs::VirtualFileSystem& vfsFor(
        runtime::process::PS4Process& process) {
    return process.virtualFileSystem();
}

std::int64_t translateHostErrnoToFreeBsd(int e) {
    switch (e) {
        case 0:       return kOk;
        case EPERM:   return kEperm;
        case ENOENT:  return kEnoent;
        case ESRCH:   return kEsrch;
        case EINTR:   return kEintr;
        case EIO:     return kEio;
        case ENXIO:   return kEnxio;
        case EBADF:   return kEbadf;
        case EAGAIN:  return kEagain;
        case ENOMEM:  return kEnomem;
        case EACCES:  return kEacces;
        case EFAULT:  return kEfault;
        case EBUSY:   return kEbusy;
        case EEXIST:  return kEexists;
        case EXDEV:   return kExdev;
        case ENODEV:  return kEnodev;
        case ENOTDIR: return kEnotdir;
        case EISDIR:  return kEisdir;
        case EINVAL:  return kEinval;
        case ENFILE:  return kEnfile;
        case EMFILE:  return kEmfile;
        case ENOSPC:  return kEnospc;
        case ESPIPE:  return kEspipe;
        case EROFS:   return kErofs;
        case EPIPE:   return kEpipe;
        case ERANGE:  return kErange;
        case ENOSYS:  return kEnosys;
        case ENAMETOOLONG: return kEnametoolong;
        case EOPNOTSUPP:   return kEopnotsupp;
        case ENOTEMPTY:    return kEnotempty;
        default:      return kEinval;
    }
}

// Read a NUL-terminated C string from guest memory. Bounded by `maxLen`.
bool readGuestString(runtime::process::PS4Process& process,
                     std::uint64_t                 guestAddr,
                     std::string&                  out,
                     std::size_t                   maxLen = 4096) {
    out.clear();
    if (guestAddr == 0) return false;
    const auto* p = reinterpret_cast<const char*>(std::uintptr_t(guestAddr));
    for (std::size_t i = 0; i < maxLen; ++i) {
        const char c = p[i];
        if (c == '\0') return true;
        out.push_back(c);
    }
    return false;
}

// Convert a FreeBSD O_* flag set to a Linux one.
int convertOpenFlags(std::int32_t f) {
    int out = 0;
    switch (f & kO_AccMode) {
        case kO_RdOnly: out |= O_RDONLY; break;
        case kO_WrOnly: out |= O_WRONLY; break;
        case kO_RdWr:   out |= O_RDWR;   break;
    }
    if (f & kO_NonBlock)  out |= O_NONBLOCK;
    if (f & kO_Append)    out |= O_APPEND;
    if (f & kO_Creat)     out |= O_CREAT;
    if (f & kO_Trunc)     out |= O_TRUNC;
    if (f & kO_Excl)      out |= O_EXCL;
    if (f & kO_CloExec)   out |= O_CLOEXEC;
    if (f & kO_Directory) out |= O_DIRECTORY;
    return out;
}

// Fill a FreeBSD struct stat from Linux stat data.
void fillStatFromLinux(const struct ::stat& src, Stat& dst) {
    std::memset(&dst, 0, sizeof(dst));
    dst.st_dev     = src.st_dev;
    dst.st_ino     = src.st_ino;
    dst.st_nlink   = src.st_nlink;
    dst.st_mode    = static_cast<std::uint16_t>(src.st_mode);
    dst.st_uid     = src.st_uid;
    dst.st_gid     = src.st_gid;
    dst.st_rdev    = src.st_rdev;
    dst.st_atim.tv_sec  = src.st_atim.tv_sec;
    dst.st_atim.tv_nsec = src.st_atim.tv_nsec;
    dst.st_mtim.tv_sec  = src.st_mtim.tv_sec;
    dst.st_mtim.tv_nsec = src.st_mtim.tv_nsec;
    dst.st_ctim.tv_sec  = src.st_ctim.tv_sec;
    dst.st_ctim.tv_nsec = src.st_ctim.tv_nsec;
    dst.st_size    = src.st_size;
    dst.st_blocks  = src.st_blocks;
    dst.st_blksize = static_cast<std::uint32_t>(src.st_blksize);
}

// ---- FS HANDLERS ---------------------------------------------------------

void h_open(SyscallContext& ctx) {
    auto& proc = *ctx.process;
    std::string guestPath;
    if (!readGuestString(proc, ctx.arg(0), guestPath)) {
        ctx.fail(kEfault);
        return;
    }
    const auto flags = static_cast<std::int32_t>(ctx.arg(1));

    const auto isDir = (flags & kO_Directory) != 0;
    const auto op = isDir
        ? filesystem::policy::FsOp::List
        : (((flags & (kO_Creat)) != 0)
              ? filesystem::policy::FsOp::Create
              : (((flags & kO_AccMode) == kO_RdOnly)
                    ? filesystem::policy::FsOp::Read
                    : filesystem::policy::FsOp::Write));

    std::int64_t perr = 0;
    const auto tr = vfsFor(proc).resolve(guestPath, op, perr);
    if (!tr.ok) {
        ctx.fail(perr ? perr : kEnoent);
        return;
    }

    const int linFlags = convertOpenFlags(flags);
    const int fd = ::open(tr.hostPath.c_str(), linFlags,
                          static_cast<mode_t>(ctx.arg(2) & 07777));
    if (fd < 0) {
        ctx.fail(translateHostErrnoToFreeBsd(errno));
        return;
    }

    std::shared_ptr<runtime::handles::HandleObject> obj;
    if (isDir) {
        obj = std::make_shared<runtime::handles::DirectoryHandle>(
            fd, tr.guestPath, tr.hostPath);
    } else {
        obj = std::make_shared<runtime::handles::FileHandle>(
            fd, tr.guestPath, tr.hostPath, flags);
    }
    const auto h = proc.handleTable().registerObject(std::move(obj));
    ctx.ok(h);
}

void h_close(SyscallContext& ctx) {
    auto& proc = *ctx.process;
    const auto h = static_cast<runtime::handles::Handle>(ctx.arg(0));

    auto obj = proc.handleTable().get(h);
    if (!obj) {
        ctx.fail(kEbadf);
        return;
    }
    // HandleObject's destructor closes the fd. Dropping the shared_ptr by
    // removing it from the table is enough.
    if (!proc.handleTable().close(h)) {
        ctx.fail(kEbadf);
        return;
    }
    ctx.ok(0);
}

void h_read(SyscallContext& ctx) {
    auto& proc = *ctx.process;
    const auto h = static_cast<runtime::handles::Handle>(ctx.arg(0));

    auto base = proc.handleTable().get(h);
    auto fh = std::dynamic_pointer_cast<runtime::handles::FileHandle>(base);
    if (!fh) {
        ctx.fail(kEbadf);
        return;
    }
    if (!fh->isReadable()) {
        ctx.fail(kEbadf);
        return;
    }

    void* buf = reinterpret_cast<void*>(std::uintptr_t(ctx.arg(1)));
    const auto count = static_cast<std::size_t>(ctx.arg(2));

    const ssize_t n = ::read(fh->hostFd(), buf, count);
    if (n < 0) {
        ctx.fail(translateHostErrnoToFreeBsd(errno));
        return;
    }
    ctx.ok(n);
}

void h_write(SyscallContext& ctx) {
    auto& proc = *ctx.process;
    const auto h = static_cast<runtime::handles::Handle>(ctx.arg(0));

    auto base = proc.handleTable().get(h);
    auto fh = std::dynamic_pointer_cast<runtime::handles::FileHandle>(base);
    if (!fh) {
        ctx.fail(kEbadf);
        return;
    }
    if (!fh->isWritable()) {
        ctx.fail(kEbadf);
        return;
    }

    const void* buf = reinterpret_cast<const void*>(std::uintptr_t(ctx.arg(1)));
    const auto count = static_cast<std::size_t>(ctx.arg(2));

    const ssize_t n = ::write(fh->hostFd(), buf, count);
    if (n < 0) {
        ctx.fail(translateHostErrnoToFreeBsd(errno));
        return;
    }
    ctx.ok(n);
}

void h_lseek(SyscallContext& ctx) {
    auto& proc = *ctx.process;
    const auto h = static_cast<runtime::handles::Handle>(ctx.arg(0));
    auto fh = std::dynamic_pointer_cast<runtime::handles::FileHandle>(
        proc.handleTable().get(h));
    if (!fh) { ctx.fail(kEbadf); return; }

    const auto off = static_cast<off_t>(ctx.arg(1));
    const int whence = static_cast<int>(ctx.arg(2));
    const off_t r = ::lseek(fh->hostFd(), off, whence);
    if (r < 0) { ctx.fail(translateHostErrnoToFreeBsd(errno)); return; }
    ctx.ok(r);
}

void h_fstat(SyscallContext& ctx) {
    auto& proc = *ctx.process;
    const auto h = static_cast<runtime::handles::Handle>(ctx.arg(0));
    auto base = proc.handleTable().get(h);
    if (!base) { ctx.fail(kEbadf); return; }

    int fd = -1;
    if (auto fh = std::dynamic_pointer_cast<runtime::handles::FileHandle>(base))
        fd = fh->hostFd();
    else if (auto dh =
                 std::dynamic_pointer_cast<runtime::handles::DirectoryHandle>(base))
        fd = dh->hostFd();
    else if (auto sh =
                 std::dynamic_pointer_cast<runtime::handles::SocketHandle>(base))
        fd = sh->hostFd();
    else {
        ctx.fail(kEinval);
        return;
    }

    struct ::stat st{};
    if (::fstat(fd, &st) != 0) {
        ctx.fail(translateHostErrnoToFreeBsd(errno));
        return;
    }
    Stat out{};
    fillStatFromLinux(st, out);

    auto* dst = reinterpret_cast<Stat*>(std::uintptr_t(ctx.arg(1)));
    if (!dst) { ctx.fail(kEfault); return; }
    std::memcpy(dst, &out, sizeof(out));
    ctx.ok(0);
}

void h_stat(SyscallContext& ctx) {
    // FreeBSD's `stat(2)` is not a syscall on PS4; the guest uses
    // `__sysctl`/`fstatat`-family calls. We nonetheless provide the number
    // for other FreeBSD binaries by mapping guest paths through the VFS.
    auto& proc = *ctx.process;
    std::string guestPath;
    if (!readGuestString(proc, ctx.arg(0), guestPath)) {
        ctx.fail(kEfault);
        return;
    }
    std::int64_t perr = 0;
    const auto tr = vfsFor(proc).resolve(
        guestPath, filesystem::policy::FsOp::Read, perr);
    if (!tr.ok) { ctx.fail(perr ? perr : kEnoent); return; }

    struct ::stat st{};
    if (::stat(tr.hostPath.c_str(), &st) != 0) {
        ctx.fail(translateHostErrnoToFreeBsd(errno));
        return;
    }
    Stat out{};
    fillStatFromLinux(st, out);
    auto* dst = reinterpret_cast<Stat*>(std::uintptr_t(ctx.arg(1)));
    std::memcpy(dst, &out, sizeof(out));
    ctx.ok(0);
}

void h_mkdir(SyscallContext& ctx) {
    auto& proc = *ctx.process;
    std::string guestPath;
    if (!readGuestString(proc, ctx.arg(0), guestPath)) {
        ctx.fail(kEfault); return;
    }
    std::int64_t perr = 0;
    const auto tr = vfsFor(proc).resolve(
        guestPath, filesystem::policy::FsOp::Create, perr);
    if (!tr.ok) { ctx.fail(perr ? perr : kEnoent); return; }

    if (::mkdir(tr.hostPath.c_str(), static_cast<mode_t>(ctx.arg(1) & 07777)) != 0) {
        ctx.fail(translateHostErrnoToFreeBsd(errno));
        return;
    }
    ctx.ok(0);
}

void h_rmdir(SyscallContext& ctx) {
    auto& proc = *ctx.process;
    std::string guestPath;
    if (!readGuestString(proc, ctx.arg(0), guestPath)) {
        ctx.fail(kEfault); return;
    }
    std::int64_t perr = 0;
    const auto tr = vfsFor(proc).resolve(
        guestPath, filesystem::policy::FsOp::Delete, perr);
    if (!tr.ok) { ctx.fail(perr ? perr : kEnoent); return; }

    if (::rmdir(tr.hostPath.c_str()) != 0) {
        ctx.fail(translateHostErrnoToFreeBsd(errno));
        return;
    }
    ctx.ok(0);
}

void h_unlink(SyscallContext& ctx) {
    auto& proc = *ctx.process;
    std::string guestPath;
    if (!readGuestString(proc, ctx.arg(0), guestPath)) {
        ctx.fail(kEfault); return;
    }
    std::int64_t perr = 0;
    const auto tr = vfsFor(proc).resolve(
        guestPath, filesystem::policy::FsOp::Delete, perr);
    if (!tr.ok) { ctx.fail(perr ? perr : kEnoent); return; }

    if (::unlink(tr.hostPath.c_str()) != 0) {
        ctx.fail(translateHostErrnoToFreeBsd(errno));
        return;
    }
    ctx.ok(0);
}

void h_rename(SyscallContext& ctx) {
    auto& proc = *ctx.process;
    std::string fromGuest, toGuest;
    if (!readGuestString(proc, ctx.arg(0), fromGuest) ||
        !readGuestString(proc, ctx.arg(1), toGuest)) {
        ctx.fail(kEfault); return;
    }

    std::int64_t perr = 0;
    const auto trFrom = vfsFor(proc).resolve(
        fromGuest, filesystem::policy::FsOp::Rename, perr);
    if (!trFrom.ok) { ctx.fail(perr ? perr : kEnoent); return; }
    const auto trTo = vfsFor(proc).resolve(
        toGuest, filesystem::policy::FsOp::Rename, perr);
    if (!trTo.ok) { ctx.fail(perr ? perr : kEnoent); return; }

    if (::rename(trFrom.hostPath.c_str(), trTo.hostPath.c_str()) != 0) {
        ctx.fail(translateHostErrnoToFreeBsd(errno));
        return;
    }
    ctx.ok(0);
}

void h_fsync(SyscallContext& ctx) {
    auto& proc = *ctx.process;
    const auto h = static_cast<runtime::handles::Handle>(ctx.arg(0));
    auto fh = std::dynamic_pointer_cast<runtime::handles::FileHandle>(
        proc.handleTable().get(h));
    if (!fh) { ctx.fail(kEbadf); return; }
    if (::fsync(fh->hostFd()) != 0) {
        ctx.fail(translateHostErrnoToFreeBsd(errno));
        return;
    }
    ctx.ok(0);
}

void h_ftruncate(SyscallContext& ctx) {
    auto& proc = *ctx.process;
    const auto h = static_cast<runtime::handles::Handle>(ctx.arg(0));
    auto fh = std::dynamic_pointer_cast<runtime::handles::FileHandle>(
        proc.handleTable().get(h));
    if (!fh) { ctx.fail(kEbadf); return; }
    if (::ftruncate(fh->hostFd(), static_cast<off_t>(ctx.arg(1))) != 0) {
        ctx.fail(translateHostErrnoToFreeBsd(errno));
        return;
    }
    ctx.ok(0);
}

// ---- MEMORY HANDLERS -----------------------------------------------------

void h_mmap(SyscallContext& ctx) {
    auto& proc = *ctx.process;
    auto& as = proc.addressSpace();

    const auto hint = static_cast<runtime::memory::GuestAddress>(ctx.arg(0));
    const auto len  = static_cast<std::size_t>(ctx.arg(1));
    const auto prot = static_cast<std::int32_t>(ctx.arg(2));
    const auto flags= static_cast<std::int32_t>(ctx.arg(3));

    // We only accept anonymous mappings in Phase 3. File-backed mmap is
    // implemented once the file handle model is complete (Phase 4),
    // because file-backed mappings require pinning the FileHandle for the
    // lifetime of the region.
    if (!(flags & kMapAnon)) {
        FP4_ERROR(LogCategory::Syscall)
            << "mmap: file-backed mappings not yet implemented "
            << "(flags=" << flags << ", fd=" << ctx.arg(4) << ")";
        ctx.fail(kEnosys);
        return;
    }

    runtime::memory::RegionProt rp = runtime::memory::RegionProt::None;
    if (prot & kProtRead)  rp = rp | runtime::memory::RegionProt::Read;
    if (prot & kProtWrite) rp = rp | runtime::memory::RegionProt::Write;
    if (prot & kProtExec)  rp = rp | runtime::memory::RegionProt::Execute;

    const auto mapped = as.map(hint, len, rp, "mmap");
    if (mapped == 0) {
        ctx.fail(kEnomem);
        return;
    }
    ctx.ok(static_cast<std::int64_t>(mapped));
}

void h_munmap(SyscallContext& ctx) {
    auto& proc = *ctx.process;
    const auto addr = static_cast<runtime::memory::GuestAddress>(ctx.arg(0));
    const auto len  = static_cast<std::size_t>(ctx.arg(1));
    if (!proc.addressSpace().unmap(addr, len)) {
        ctx.fail(kEinval);
        return;
    }
    ctx.ok(0);
}

void h_mprotect(SyscallContext& ctx) {
    auto& proc = *ctx.process;
    const auto addr = static_cast<runtime::memory::GuestAddress>(ctx.arg(0));
    const auto len  = static_cast<std::size_t>(ctx.arg(1));
    const auto prot = static_cast<std::int32_t>(ctx.arg(2));

    runtime::memory::RegionProt rp = runtime::memory::RegionProt::None;
    if (prot & kProtRead)  rp = rp | runtime::memory::RegionProt::Read;
    if (prot & kProtWrite) rp = rp | runtime::memory::RegionProt::Write;
    if (prot & kProtExec)  rp = rp | runtime::memory::RegionProt::Execute;

    if (!proc.addressSpace().protect(addr, len, rp)) {
        ctx.fail(kEinval);
        return;
    }
    ctx.ok(0);
}

// ---- THREAD HANDLERS -----------------------------------------------------

void h_thr_self(SyscallContext& ctx) {
    if (!ctx.thread) { ctx.fail(kEinval); return; }
    ctx.ok(static_cast<std::int64_t>(ctx.thread->id()));
}

void h_thr_exit(SyscallContext& ctx) {
    if (ctx.thread) ctx.thread->requestExit();
    ctx.ok(0);
}

void h_thr_suspend(SyscallContext& ctx) {
    auto& proc = *ctx.process;
    const auto tid = static_cast<runtime::thread::ThreadId>(ctx.arg(0));
    auto t = proc.threadManager().get(tid);
    if (!t) { ctx.fail(kEsrch); return; }
    t->suspend();
    ctx.ok(0);
}

void h_thr_wake(SyscallContext& ctx) {
    auto& proc = *ctx.process;
    const auto tid = static_cast<runtime::thread::ThreadId>(ctx.arg(0));
    auto t = proc.threadManager().get(tid);
    if (!t) { ctx.fail(kEsrch); return; }
    t->resume();
    ctx.ok(0);
}

// ---- MISC HANDLERS -------------------------------------------------------

void h_getpid(SyscallContext& ctx) {
    ctx.ok(ctx.process ? ctx.process->id() : 0);
}

void h_getpagesize(SyscallContext& ctx) {
    ctx.ok(static_cast<std::int64_t>(::sysconf(_SC_PAGESIZE)));
}

void h_gettimeofday(SyscallContext& ctx) {
    auto* tv = reinterpret_cast<TimeVal*>(std::uintptr_t(ctx.arg(0)));
    auto* tz = reinterpret_cast<void*>(std::uintptr_t(ctx.arg(1)));
    (void)tz;

    struct ::timeval host{};
    if (::gettimeofday(&host, nullptr) != 0) {
        ctx.fail(translateHostErrnoToFreeBsd(errno));
        return;
    }
    if (tv) {
        tv->tv_sec  = host.tv_sec;
        tv->tv_usec = host.tv_usec;
    }
    ctx.ok(0);
}

void h_socket(SyscallContext& ctx) {
    auto& proc = *ctx.process;
    const int domain   = static_cast<int>(ctx.arg(0));
    const int type     = static_cast<int>(ctx.arg(1));
    const int protocol = static_cast<int>(ctx.arg(2));

    const int fd = ::socket(domain, type, protocol);
    if (fd < 0) {
        ctx.fail(translateHostErrnoToFreeBsd(errno));
        return;
    }
    auto obj = std::make_shared<runtime::handles::SocketHandle>(
        fd, domain, type, protocol);
    ctx.ok(proc.handleTable().registerObject(std::move(obj)));
}

void h_exit(SyscallContext& ctx) {
    if (ctx.thread) ctx.thread->requestExit();
    FP4_INFO(LogCategory::Syscall)
        << "guest requested exit(" << ctx.arg(0) << ")";
    ctx.ok(0);
}

} // namespace

// ---- registration --------------------------------------------------------

void registerFilesystemHandlers(SyscallDispatcher& d,
                                runtime::process::PS4Process& process) {
    (void)process;
    d.registerHandler(kSysOpen,   "open",   h_open);
    d.registerHandler(kSysClose,  "close",  h_close);
    d.registerHandler(kSysRead,   "read",   h_read);
    d.registerHandler(kSysWrite,  "write",  h_write);
    d.registerHandler(kSysLseek,  "lseek",  h_lseek);
    d.registerHandler(kSysFstat,  "fstat",  h_fstat);
    // FreeBSD syscall 188 = `stat` legacy; PS4 uses the same number.
    d.registerHandler(188,        "stat",   h_stat);
    d.registerHandler(kSysMkdir,  "mkdir",  h_mkdir);
    d.registerHandler(kSysRmdir,  "rmdir",  h_rmdir);
    d.registerHandler(10,         "unlink", h_unlink);   // FreeBSD unlink
    d.registerHandler(kSysRename, "rename", h_rename);
    d.registerHandler(kSysFsync,  "fsync",  h_fsync);
    d.registerHandler(kSysFtruncate, "ftruncate", h_ftruncate);
}

void registerMemoryHandlers(SyscallDispatcher& d,
                            runtime::process::PS4Process& process) {
    (void)process;
    d.registerHandler(kSysMmap,     "mmap",     h_mmap);
    d.registerHandler(kSysMunmap,   "munmap",   h_munmap);
    d.registerHandler(kSysMprotect, "mprotect", h_mprotect);
}

void registerThreadHandlers(SyscallDispatcher& d,
                            runtime::process::PS4Process& process) {
    (void)process;
    d.registerHandler(kSysThrSelf,    "thr_self",    h_thr_self);
    d.registerHandler(kSysThrExit,    "thr_exit",    h_thr_exit);
    d.registerHandler(kSysThrSuspend, "thr_suspend", h_thr_suspend);
    d.registerHandler(kSysThrWake,    "thr_wake",    h_thr_wake);
}

void registerMiscHandlers(SyscallDispatcher& d,
                          runtime::process::PS4Process& process) {
    (void)process;
    d.registerHandler(kSysGetPid,       "getpid",       h_getpid);
    d.registerHandler(kSysGetPageSize,  "getpagesize",  h_getpagesize);
    d.registerHandler(kSysGetTimeOfDay, "gettimeofday", h_gettimeofday);
    d.registerHandler(kSysSocket,       "socket",       h_socket);
    d.registerHandler(kSysExit,         "exit",         h_exit);
}

void registerAll(SyscallDispatcher& d,
                 runtime::process::PS4Process& process) {
    registerFilesystemHandlers(d, process);
    registerMemoryHandlers(d, process);
    registerThreadHandlers(d, process);
    registerMiscHandlers(d, process);
    FP4_INFO(LogCategory::Syscall)
        << "SyscallDispatcher: " << d.handlerCount()
        << " handlers registered";
}

} // namespace fusionps4::syscall::handlers
