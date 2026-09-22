#pragma once

#include <cstdint>

namespace fusionps4::syscall::trap {

// The trap mechanism distinguishes real FreeBSD syscalls from SCE function
// calls by reserving a numeric range for the latter. The guest's trampolines
// use `mov eax, SCE_CALL_BASE | id; syscall` to invoke a runtime-implemented
// SCE function; the SIGSYS handler inspects RAX and forwards accordingly.
//
// SCE_CALL_BASE is chosen to be far above any FreeBSD or Linux syscall
// number (both are below 1000 on x86_64), so a well-formed request is
// unambiguous.

inline constexpr std::uint32_t kSceCallBase = 0x50000000u;
inline constexpr std::uint32_t kSceCallMask = 0x0FFFFFFFu;

inline constexpr bool isSceCall(std::uint64_t number) {
    return (static_cast<std::uint32_t>(number) & 0xF0000000u) == kSceCallBase;
}

inline constexpr std::uint32_t sceCallId(std::uint64_t number) {
    return static_cast<std::uint32_t>(number) & kSceCallMask;
}

inline constexpr std::uint64_t makeSceCall(std::uint32_t id) {
    return kSceCallBase | (id & kSceCallMask);
}

} // namespace fusionps4::syscall::trap
