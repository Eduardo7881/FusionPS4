#include "isolation/SeccompFilter.hpp"

#include "debug/Log.hpp"

#include <cerrno>
#include <cstring>
#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

using fusionps4::debug::LogCategory;

namespace fusionps4::isolation {

namespace {

// The BPF program is built as a vector of sock_filter and installed via the
// seccomp(2) syscall. We avoid libseccomp so the runtime has no runtime
// dependency beyond the kernel interface.

struct BpfBuilder {
    std::vector<sock_filter> ops;
    std::vector<sock_filter> jumps;   // stack of jump instructions to patch

    void emit(std::uint16_t code, std::uint32_t k) {
        ops.push_back(sock_filter{code, 0, 0, k});
    }

    void loadSyscallNumber() {
        // BPF_LD | BPF_W | BPF_ABS with offset = offsetof(seccomp_data, nr)
        emit(BPF_LD | BPF_W | BPF_ABS, 0);
    }

    // If syscall number == k, continue to next instruction; else skip next.
    void jumpIfEqual(std::uint32_t k, std::uint8_t jt, std::uint8_t jf) {
        ops.push_back(sock_filter{BPF_JMP | BPF_JEQ | BPF_K, jt, jf, k});
    }

    void ret(std::uint32_t action) {
        emit(BPF_RET | BPF_K, action);
    }
};

} // namespace

bool SeccompFilter::install(const IsolationConfig& cfg) {
    return installImpl(cfg, /*audit=*/false);
}

bool SeccompFilter::installWithAudit(const IsolationConfig& cfg) {
    return installImpl(cfg, /*audit=*/true);
}

bool SeccompFilter::installImpl(const IsolationConfig& cfg, bool audit) {
    BpfBuilder b;
    b.loadSyscallNumber();

    // Each allowlisted syscall is emitted as:
    //   JEQ <nr> , 0 , 1    ; if equal, fall through; else skip next
    //   RET ALLOW           ; allow
    // The default action after the last check is TRAP.
    //
    // Because we have more than 255 allowlist entries in theory (we don't,
    // but the encoding is one jump per entry), we do not use a jump table.
    // Linear scan is fine because the number of entries is small.
    //
    // IMPORTANT: The instruction that follows the last allowlist jump must
    // be the TRAP return. Each jump instruction's "else" branch (jf=1)
    // skips the RET ALLOW that follows it, landing on the next JEQ.
    for (std::int32_t nr : cfg.syscall_allowlist) {
        b.jumpIfEqual(static_cast<std::uint32_t>(nr), /*jt=*/0, /*jf=*/1);
        b.ret(SECCOMP_RET_ALLOW);
    }

    // Default action: trap to SIGSYS. The handler runs in the guest process
    // and forwards the syscall to the runtime.
    b.ret(SECCOMP_RET_TRAP);

    sock_fprog prog{};
    prog.len    = static_cast<unsigned short>(b.ops.size());
    prog.filter = b.ops.data();

    unsigned long flags = SECCOMP_FILTER_FLAG_TSYNC;
    if (audit) flags |= SECCOMP_FILTER_FLAG_LOG;

    if (::syscall(SYS_seccomp, SECCOMP_SET_MODE_FILTER, flags, &prog) != 0) {
        FP4_ERROR(LogCategory::Process)
            << "seccomp(SET_MODE_FILTER, flags=0x" << std::hex << flags
            << std::dec << ") failed: " << std::strerror(errno);
        return false;
    }

    FP4_INFO(LogCategory::Process)
        << "seccomp filter installed: " << b.ops.size() << " BPF instructions, "
        << cfg.syscall_allowlist.size() << " allowed syscalls, all others trap";
    return true;
}

} // namespace fusionps4::isolation
