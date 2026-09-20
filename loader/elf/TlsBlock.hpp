#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace fusionps4::loader::elf {

// TLS layout follows x86_64 variant II (as used by glibc/FreeBSD):
//
//     [ module 0 .tdata ][ module 1 .tdata ] ... [ TCB ]
//                                                      ^ FS points here
//
// Only one module's worth of TLS is managed per TlsBlock, because in this
// runtime each loaded ELF owns its own PT_TLS segment. Cross-module TLS
// (a single TCB covering multiple modules) will be introduced when the
// module registry gains a global TLS planner; the API below is designed so
// that the per-module block can later be embedded into a larger plan.
class TlsBlock {
public:
    TlsBlock() = default;

    void reset();

    // Configure the template from PT_TLS. `align` is p_align; the block is
    // allocated respecting the module's alignment requirements.
    void configure(const std::uint8_t* templateBytes,
                   std::uint64_t      filesz,
                   std::uint64_t      memsz,
                   std::uint64_t      align);

    bool configured() const { return m_configured; }

    std::uint64_t memsz() const { return m_memsz; }
    std::uint64_t align() const { return m_align; }
    std::uint64_t blockSize() const { return m_blockSize; }

    const std::vector<std::uint8_t>& bytes() const { return m_bytes; }

    // Allocate a fresh per-thread TLS image on the host, initialize it with
    // the template and the zero-fill tail, then install it as the calling
    // thread's TLS base via arch_prctl(ARCH_SET_FS, tcb). On success,
    // returns the address of the TCB (which is what FS should point to).
    // Returns 0 on failure (and logs). Ownership of the host allocation
    // belongs to the runtime; the returned pointer must be freed with
    // freeThreadTls().
    std::uint64_t allocateAndInstallForCurrentThread();

    // Tear down a block allocated by allocateAndInstallForCurrentThread.
    static void freeThreadTls(std::uint64_t tcbAddress);

private:
    bool                       m_configured = false;
    std::uint64_t              m_memsz      = 0;
    std::uint64_t              m_align      = 1;
    std::uint64_t              m_blockSize  = 0;   // bytes + TCB, aligned
    std::vector<std::uint8_t>  m_bytes;             // template, size = memsz
};

} // namespace fusionps4::loader::elf
