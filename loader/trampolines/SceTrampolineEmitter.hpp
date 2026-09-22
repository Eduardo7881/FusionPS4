#pragma once

#include "runtime/memory/AddressSpace.hpp"

#include <cstdint>

namespace fusionps4::loader::trampolines {

// Writes ABI-compatible trampolines into a dedicated executable page of
// the guest's address space. Each trampoline is 11 bytes:
//
//     mov r10, rcx       ; 49 89 CA
//     mov eax, imm32     ; B8 <id>
//     syscall            ; 0F 05
//     ret                ; C3
//     padding to 16 bytes for alignment
//
// The `syscall` instruction triggers SIGSYS in the isolated guest; the
// trap handler (syscall/trap/TrapGate) reads RAX, sees the SCE_CALL_BASE
// marker, and forwards the call to the runtime with the numeric id.
//
// One page (4096 bytes) holds 256 trampolines at 16-byte stride.
class SceTrampolineEmitter {
public:
    static constexpr std::size_t kSlotStride = 16;
    static constexpr std::size_t kSlotsPerPage = 4096 / kSlotStride;   // 256

    // Reserve a page in the guest's AddressSpace. Returns the base address,
    // or 0 on failure.
    static std::uint64_t reservePage(runtime::memory::AddressSpace& as);

    // Write a trampoline at slot index. `pageBase` is the address returned
    // by reservePage(). Returns the trampoline address, or 0 on failure.
    static std::uint64_t emit(std::uint8_t*   pageBase,
                              std::size_t     slot,
                              std::uint32_t   sceId);

    // Emit a variable-import accessor: a small piece of code that returns
    // the current value of a runtime-owned variable. For now this returns
    // the address of a fixed-size slot in the page, which the runtime can
    // freely update.
    static std::uint64_t emitVariable(std::uint8_t* pageBase,
                                      std::size_t   slot,
                                      std::uint32_t sceVarId);
};

} // namespace fusionps4::loader::trampolines
