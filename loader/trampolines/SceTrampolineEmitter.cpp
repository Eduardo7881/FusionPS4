#include "loader/trampolines/SceTrampolineEmitter.hpp"

#include "debug/Log.hpp"
#include "runtime/memory/AddressSpace.hpp"
#include "syscall/trap/SceCallIds.hpp"

#include <cstring>

using fusionps4::debug::LogCategory;
using fusionps4::runtime::memory::AddressSpace;
using fusionps4::runtime::memory::GuestAddress;
using fusionps4::runtime::memory::RegionProt;

namespace fusionps4::loader::trampolines {

std::uint64_t SceTrampolineEmitter::reservePage(AddressSpace& as) {
    const auto base = as.map(
        /*hint=*/0, 4096,
        RegionProt::Read | RegionProt::Execute,
        "sce-trampolines");
    if (!base) {
        FP4_ERROR(LogCategory::Loader)
            << "failed to reserve trampoline page in guest address space";
        return 0;
    }
    // Zero the page so accidental execution of a free slot faults cleanly.
    std::memset(reinterpret_cast<void*>(std::uintptr_t(base)), 0xCC, 4096);
    FP4_INFO(LogCategory::Loader)
        << "trampoline page at " << reinterpret_cast<void*>(std::uintptr_t(base))
        << " (" << kSlotsPerPage << " slots)";
    return base;
}

std::uint64_t SceTrampolineEmitter::emit(std::uint8_t* pageBase,
                                         std::size_t   slot,
                                         std::uint32_t sceId) {
    if (!pageBase || slot >= kSlotsPerPage) return 0;

    std::uint8_t* p = pageBase + slot * kSlotStride;

    // mov r10, rcx
    p[0] = 0x49; p[1] = 0x89; p[2] = 0xCA;
    // mov eax, SCE_CALL_BASE | sceId
    p[3] = 0xB8;
    const std::uint32_t immediate =
        static_cast<std::uint32_t>(syscall::trap::kSceCallBase) |
        (sceId & syscall::trap::kSceCallMask);
    std::memcpy(p + 4, &immediate, 4);
    // syscall
    p[8] = 0x0F; p[9] = 0x05;
    // ret
    p[10] = 0xC3;
    // Remaining bytes (11..15) already 0xCC from reservation.

    return reinterpret_cast<std::uint64_t>(p);
}

std::uint64_t SceTrampolineEmitter::emitVariable(std::uint8_t* pageBase,
                                                 std::size_t   slot,
                                                 std::uint32_t sceVarId) {
    if (!pageBase || slot >= kSlotsPerPage) return 0;

    std::uint8_t* p = pageBase + slot * kSlotStride;
    // mov eax, imm32; ret — returns the runtime-populated slot value.
    p[0] = 0xB8;
    std::memcpy(p + 1, &sceVarId, 4);
    p[5] = 0xC3;
    for (std::size_t i = 6; i < kSlotStride; ++i) p[i] = 0xCC;
    return reinterpret_cast<std::uint64_t>(p);
}

} // namespace fusionps4::loader::trampolines
