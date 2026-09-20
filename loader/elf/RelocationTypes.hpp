#pragma once

#include <cstdint>

namespace fusionps4::loader::elf {

// x86_64 relocations used by PS4 executables and shared libraries.
constexpr std::uint32_t kRX86_64None      = 0;
constexpr std::uint32_t kRX86_64_64       = 1;
constexpr std::uint32_t kRX86_64Pc32      = 2;
constexpr std::uint32_t kRX86_64Got32     = 3;
constexpr std::uint32_t kRX86_64Plt32     = 4;
constexpr std::uint32_t kRX86_64Copy      = 5;
constexpr std::uint32_t kRX86_64GlobDat   = 6;
constexpr std::uint32_t kRX86_64JumpSlot  = 7;
constexpr std::uint32_t kRX86_64Relative  = 8;
constexpr std::uint32_t kRX86_64GotPcRel  = 9;
constexpr std::uint32_t kRX86_64_32       = 10;
constexpr std::uint32_t kRX86_64_32S      = 11;
constexpr std::uint32_t kRX86_64_16       = 12;
constexpr std::uint32_t kRX86_64Pc16      = 13;
constexpr std::uint32_t kRX86_64_8        = 14;
constexpr std::uint32_t kRX86_64Pc8       = 15;
constexpr std::uint32_t kRX86_64Pc64      = 24;

constexpr const char* relocationName(std::uint32_t type) {
    switch (type) {
        case kRX86_64None:     return "R_X86_64_NONE";
        case kRX86_64_64:      return "R_X86_64_64";
        case kRX86_64Pc32:     return "R_X86_64_PC32";
        case kRX86_64Got32:    return "R_X86_64_GOT32";
        case kRX86_64Plt32:    return "R_X86_64_PLT32";
        case kRX86_64Copy:     return "R_X86_64_COPY";
        case kRX86_64GlobDat:  return "R_X86_64_GLOB_DAT";
        case kRX86_64JumpSlot: return "R_X86_64_JUMP_SLOT";
        case kRX86_64Relative: return "R_X86_64_RELATIVE";
        case kRX86_64GotPcRel: return "R_X86_64_GOTPCREL";
        case kRX86_64_32:      return "R_X86_64_32";
        case kRX86_64_32S:     return "R_X86_64_32S";
        case kRX86_64_16:      return "R_X86_64_16";
        case kRX86_64Pc16:     return "R_X86_64_PC16";
        case kRX86_64_8:       return "R_X86_64_8";
        case kRX86_64Pc8:      return "R_X86_64_PC8";
        case kRX86_64Pc64:     return "R_X86_64_PC64";
        default:               return "R_X86_64_UNKNOWN";
    }
}

} // namespace fusionps4::loader::elf
