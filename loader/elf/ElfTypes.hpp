#pragma once

#include <cstdint>
#include <cstddef>

// On-disk ELF structures for PS4 executables (ELF64, little-endian, x86_64,
// OSABI = FreeBSD). PS4 extends the standard format with SCE-specific
// e_types, PT_* and DT_* tags, which we declare here so the rest of the
// loader does not hardcode magic numbers.

namespace fusionps4::loader::elf {

// ---- e_ident -------------------------------------------------------------
constexpr unsigned char kElfMagic[4]     = {0x7F, 'E', 'L', 'F'};
constexpr unsigned char kElfClass64      = 2;
constexpr unsigned char kElfDataLsb      = 1;
constexpr unsigned char kElfVersionCur   = 1;
constexpr unsigned char kElfOsAbiFreeBsd = 9;

// ---- e_machine -----------------------------------------------------------
constexpr std::uint16_t kEmX86_64 = 62;

// ---- e_type --------------------------------------------------------------
constexpr std::uint16_t kEtExec        = 2;
constexpr std::uint16_t kEtDyn         = 3;
constexpr std::uint16_t kEtSceExec     = 0xFE00;
constexpr std::uint16_t kEtSceRelExec  = 0xFE04;
constexpr std::uint16_t kEtSceStubLib  = 0xFE0C;
constexpr std::uint16_t kEtSceDynExec  = 0xFE10;
constexpr std::uint16_t kEtSceDynamic  = 0xFE18;

// ---- Program header types ------------------------------------------------
constexpr std::uint32_t kPtNull         = 0;
constexpr std::uint32_t kPtLoad         = 1;
constexpr std::uint32_t kPtDynamic      = 2;
constexpr std::uint32_t kPtInterp       = 3;
constexpr std::uint32_t kPtNote         = 4;
constexpr std::uint32_t kPtTls          = 7;

constexpr std::uint32_t kPtSceRel       = 0x61000000;
constexpr std::uint32_t kPtSceProcParam = 0x61000001;
constexpr std::uint32_t kPtSceDynlibData= 0x61000010;
constexpr std::uint32_t kPtSceComment   = 0x6FFFFF00;
constexpr std::uint32_t kPtSceLibVer    = 0x6FFFFF01;

// ---- p_flags -------------------------------------------------------------
constexpr std::uint32_t kPfX = 1u << 0;
constexpr std::uint32_t kPfW = 1u << 1;
constexpr std::uint32_t kPfR = 1u << 2;

// ---- Dynamic tags (standard) --------------------------------------------
constexpr std::int64_t kDtNull         = 0;
constexpr std::int64_t kDtNeeded       = 1;
constexpr std::int64_t kDtPltRelSz     = 2;
constexpr std::int64_t kDtPltGot       = 3;
constexpr std::int64_t kDtHash         = 4;
constexpr std::int64_t kDtStrtab       = 5;
constexpr std::int64_t kDtSymtab       = 6;
constexpr std::int64_t kDtRela         = 7;
constexpr std::int64_t kDtRelaSz       = 8;
constexpr std::int64_t kDtRelaEnt      = 9;
constexpr std::int64_t kDtStrtabSz     = 10;
constexpr std::int64_t kDtSymtabSz     = 11;
constexpr std::int64_t kDtInit         = 12;
constexpr std::int64_t kDtFini         = 13;
constexpr std::int64_t kDtRel          = 17;
constexpr std::int64_t kDtRelSz        = 18;
constexpr std::int64_t kDtRelEnt       = 19;
constexpr std::int64_t kDtPltRel       = 20;
constexpr std::int64_t kDtJmpRel       = 23;
constexpr std::int64_t kDtInitArray    = 25;
constexpr std::int64_t kDtFiniArray    = 26;
constexpr std::int64_t kDtInitArraySz  = 27;
constexpr std::int64_t kDtFiniArraySz  = 28;
constexpr std::int64_t kDtFlags        = 30;
constexpr std::int64_t kDtRelACount    = 0x6FFFFFF9;

// ---- Dynamic tags (SCE) --------------------------------------------------
constexpr std::int64_t kDtSceNeededModule  = 0x61000000;
constexpr std::int64_t kDtSceModuleInfo    = 0x61000001;
constexpr std::int64_t kDtSceImportLib     = 0x61000009;
constexpr std::int64_t kDtSceImportLibAttr = 0x6100000B;
constexpr std::int64_t kDtSceSymtab        = 0x6100000D;
constexpr std::int64_t kDtSceStrtab        = 0x6100000F;
constexpr std::int64_t kDtSceSymtabSz      = 0x61000011;
constexpr std::int64_t kDtSceStrtabSz      = 0x61000013;
constexpr std::int64_t kDtSceHash          = 0x61000015;
constexpr std::int64_t kDtSceHashSz        = 0x61000017;
constexpr std::int64_t kDtSceRela          = 0x61000019;
constexpr std::int64_t kDtSceRelaSz        = 0x6100001B;
constexpr std::int64_t kDtSceRelaEnt       = 0x6100001D;
constexpr std::int64_t kDtScePltGot        = 0x61000021;

// ---- Section indices -----------------------------------------------------
constexpr std::uint16_t kShnUndef = 0;

// ---- Structures ----------------------------------------------------------
#pragma pack(push, 1)

struct Elf64_Ehdr {
    unsigned char e_ident[16];
    std::uint16_t e_type;
    std::uint16_t e_machine;
    std::uint32_t e_version;
    std::uint64_t e_entry;
    std::uint64_t e_phoff;
    std::uint64_t e_shoff;
    std::uint32_t e_flags;
    std::uint16_t e_ehsize;
    std::uint16_t e_phentsize;
    std::uint16_t e_phnum;
    std::uint16_t e_shentsize;
    std::uint16_t e_shnum;
    std::uint16_t e_shstrndx;
};

struct Elf64_Phdr {
    std::uint32_t p_type;
    std::uint32_t p_flags;
    std::uint64_t p_offset;
    std::uint64_t p_vaddr;
    std::uint64_t p_paddr;
    std::uint64_t p_filesz;
    std::uint64_t p_memsz;
    std::uint64_t p_align;
};

struct Elf64_Dyn {
    std::int64_t  d_tag;
    std::uint64_t d_val;
};

struct Elf64_Sym {
    std::uint32_t st_name;
    std::uint8_t  st_info;
    std::uint8_t  st_other;
    std::uint16_t st_shndx;
    std::uint64_t st_value;
    std::uint64_t st_size;
};

struct Elf64_Rela {
    std::uint64_t r_offset;
    std::uint64_t r_info;
    std::int64_t  r_addend;
};

struct Elf64_Rel {
    std::uint64_t r_offset;
    std::uint64_t r_info;
};

#pragma pack(pop)

// Convenience accessors.
constexpr std::uint32_t elfSymBind(std::uint8_t info) { return info >> 4; }
constexpr std::uint32_t elfSymType(std::uint8_t info) { return info & 0xF; }
constexpr std::uint32_t elfRelSym(std::uint64_t info) {
    return static_cast<std::uint32_t>(info >> 32);
}
constexpr std::uint32_t elfRelType(std::uint64_t info) {
    return static_cast<std::uint32_t>(info & 0xFFFFFFFFu);
}

} // namespace fusionps4::loader::elf
