#include "loader/elf/TlsBlock.hpp"

#include "debug/Log.hpp"

#include <cstring>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#if defined(__linux__)
#  include <asm/prctl.h>
#endif

using fusionps4::debug::LogCategory;

namespace fusionps4::loader::elf {

namespace {

constexpr std::size_t kPageSize = 0x1000;

// Minimal glibc-compatible Thread Control Block for x86_64.
//   offset 0: self pointer  (equivalent to glibc tcb->tcb)
//   offset 8: DTV pointer
//   offset 16: padding reserved for future use
// We keep the TCB at the tail of the TLS block per variant II.
struct Tcb {
    void*         self;
    void*         dtv;
    std::uint64_t reserved[6];
};

constexpr std::size_t kTcbSize = sizeof(Tcb);

std::uint64_t alignUp(std::uint64_t v, std::uint64_t a) {
    return (v + (a - 1)) & ~(a - 1);
}

} // namespace

void TlsBlock::reset() {
    m_configured = false;
    m_memsz      = 0;
    m_align      = 1;
    m_blockSize  = 0;
    m_bytes.clear();
}

void TlsBlock::configure(const std::uint8_t* templateBytes,
                         std::uint64_t      filesz,
                         std::uint64_t      memsz,
                         std::uint64_t      align) {
    reset();
    if (memsz == 0) {
        m_configured = true;
        return;
    }
    if (align == 0) align = 1;

    m_memsz = memsz;
    m_align = align;
    m_bytes.assign(memsz, 0);
    if (filesz > 0) {
        if (!templateBytes) {
            FP4_ERROR(LogCategory::Loader)
                << "PT_TLS filesz=" << filesz << " but no template pointer";
            return;
        }
        std::memcpy(m_bytes.data(), templateBytes,
                    static_cast<std::size_t>(filesz));
    }

    // Total host allocation: TLS bytes + TCB, aligned to page size.
    const std::uint64_t tailAligned = alignUp(memsz, m_align);
    const std::uint64_t withTcb     = tailAligned + kTcbSize;
    m_blockSize = alignUp(withTcb, kPageSize);
    m_configured = true;

    FP4_DEBUG(LogCategory::Loader)
        << "TLS template configured: memsz=" << m_memsz
        << " align=" << m_align
        << " blockSize=" << m_blockSize;
}

std::uint64_t TlsBlock::allocateAndInstallForCurrentThread() {
    if (!m_configured || m_blockSize == 0) {
        FP4_ERROR(LogCategory::Loader)
            << "TLS allocate requested on unconfigured block";
        return 0;
    }

    void* host = ::mmap(nullptr, m_blockSize, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (host == MAP_FAILED) {
        FP4_ERROR(LogCategory::Loader) << "mmap for TLS block failed";
        return 0;
    }

    auto* base = static_cast<std::uint8_t*>(host);

    // Copy the template to the tail of the TLS region so that it ends
    // immediately before the TCB; this is variant II.
    const std::uint64_t tailAligned = alignUp(m_memsz, m_align);
    auto* tlsBase = base + (tailAligned - m_memsz);
    std::memcpy(tlsBase, m_bytes.data(), static_cast<std::size_t>(m_memsz));

    auto* tcb = reinterpret_cast<Tcb*>(base + tailAligned);
    tcb->self = tcb;
    tcb->dtv  = nullptr;  // populated by the runtime when multiple modules
                          // share a TCB; not required for a single module.

#if defined(__linux__)
    const long rc = ::syscall(SYS_arch_prctl, ARCH_SET_FS, tcb);
    if (rc != 0) {
        FP4_ERROR(LogCategory::Loader)
            << "arch_prctl(ARCH_SET_FS) failed; TLS not installed";
        ::munmap(host, m_blockSize);
        return 0;
    }
#else
    FP4_ERROR(LogCategory::Loader)
        << "TLS installation only implemented for Linux x86_64 host";
    ::munmap(host, m_blockSize);
    return 0;
#endif

    FP4_TRACE(LogCategory::Loader)
        << "TLS installed for current thread: tcb=" << tcb;
    return reinterpret_cast<std::uint64_t>(tcb);
}

void TlsBlock::freeThreadTls(std::uint64_t tcbAddress) {
    if (!tcbAddress) return;
    // The TCB sits at the end of the allocation; we need to recover the base
    // and size. The runtime stores those alongside the address in the
    // ThreadObject (Phase 3+). For now, conservative unmapping is deferred.
    (void)tcbAddress;
}

} // namespace fusionps4::loader::elf
