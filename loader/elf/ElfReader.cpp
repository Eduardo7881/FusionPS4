#include "loader/elf/ElfReader.hpp"

#include "debug/Log.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>

using fusionps4::debug::LogCategory;

namespace fusionps4::loader::elf {

bool ElfReader::readFile(const std::string& path, std::vector<std::uint8_t>& out) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) {
        FP4_ERROR(LogCategory::Loader)
            << "cannot open \"" << path << "\" for reading";
        return false;
    }
    const auto sz = static_cast<std::size_t>(f.tellg());
    if (sz < sizeof(Elf64_Ehdr)) {
        FP4_ERROR(LogCategory::Loader)
            << "\"" << path << "\" too small for ELF header (" << sz << " bytes)";
        return false;
    }
    f.seekg(0);
    out.resize(sz);
    if (!f.read(reinterpret_cast<char*>(out.data()),
                static_cast<std::streamsize>(sz))) {
        FP4_ERROR(LogCategory::Loader)
            << "short read on \"" << path << "\"";
        return false;
    }
    FP4_INFO(LogCategory::Loader)
        << "loaded ELF file \"" << path << "\" (" << sz << " bytes)";
    return true;
}

bool ElfReader::parse(const std::vector<std::uint8_t>& data, ElfImage& out) {
    out = {};

    if (data.size() < sizeof(Elf64_Ehdr)) {
        FP4_ERROR(LogCategory::Loader) << "ELF image smaller than header";
        return false;
    }
    std::memcpy(&out.ehdr, data.data(), sizeof(Elf64_Ehdr));
    out.data = data.data();
    out.size = data.size();

    const auto& e = out.ehdr;

    if (std::memcmp(e.e_ident, kElfMagic, 4) != 0) {
        FP4_ERROR(LogCategory::Loader) << "bad ELF magic";
        return false;
    }
    if (e.e_ident[4] != kElfClass64) {
        FP4_ERROR(LogCategory::Loader)
            << "unsupported ELF class " << static_cast<int>(e.e_ident[4]);
        return false;
    }
    if (e.e_ident[5] != kElfDataLsb) {
        FP4_ERROR(LogCategory::Loader) << "ELF is not little-endian";
        return false;
    }
    if (e.e_ident[6] != kElfVersionCur) {
        FP4_ERROR(LogCategory::Loader) << "ELF ident version mismatch";
        return false;
    }
    if (e.e_machine != kEmX86_64) {
        FP4_ERROR(LogCategory::Loader)
            << "expected x86_64 ELF, got e_machine=" << e.e_machine;
        return false;
    }

    // PS4 images are flagged as FreeBSD, or carry an SCE e_type with OSABI=0.
    const unsigned char osabi = e.e_ident[7];
    const bool isSceType =
        (e.e_type == kEtSceExec || e.e_type == kEtSceRelExec ||
         e.e_type == kEtSceDynExec || e.e_type == kEtSceDynamic ||
         e.e_type == kEtSceStubLib);
    if (osabi != kElfOsAbiFreeBsd && !isSceType) {
        FP4_ERROR(LogCategory::Loader)
            << "unsupported ELF OSABI " << static_cast<int>(osabi)
            << " (e_type=" << typeName(e.e_type) << ")";
        return false;
    }

    if (e.e_phentsize != sizeof(Elf64_Phdr) || e.e_phnum == 0) {
        FP4_ERROR(LogCategory::Loader)
            << "bad program header table (entsize=" << e.e_phentsize
            << ", num=" << e.e_phnum << ")";
        return false;
    }
    if (e.e_phoff + std::uint64_t(e.e_phnum) * sizeof(Elf64_Phdr) > data.size()) {
        FP4_ERROR(LogCategory::Loader) << "program header table out of file";
        return false;
    }

    out.phdrs.reserve(e.e_phnum);
    for (std::uint16_t i = 0; i < e.e_phnum; ++i) {
        const auto* src = data.data() + e.e_phoff + i * sizeof(Elf64_Phdr);
        Elf64_Phdr ph{};
        std::memcpy(&ph, src, sizeof(ph));
        SegmentInfo s;
        s.type   = ph.p_type;
        s.flags  = ph.p_flags;
        s.offset = ph.p_offset;
        s.vaddr  = ph.p_vaddr;
        s.filesz = ph.p_filesz;
        s.memsz  = ph.p_memsz;
        s.align  = ph.p_align;
        out.phdrs.push_back(s);
    }

    FP4_DEBUG(LogCategory::Loader)
        << "ELF parsed: e_type=" << typeName(e.e_type)
        << " entry=" << reinterpret_cast<void*>(std::uintptr_t(e.e_entry))
        << " phnum=" << e.e_phnum;
    return true;
}

const char* ElfReader::typeName(std::uint16_t t) {
    switch (t) {
        case kEtExec:        return "ET_EXEC";
        case kEtDyn:         return "ET_DYN";
        case kEtSceExec:     return "ET_SCE_EXEC";
        case kEtSceRelExec:  return "ET_SCE_RELEXEC";
        case kEtSceStubLib:  return "ET_SCE_STUBLIB";
        case kEtSceDynExec:  return "ET_SCE_DYNEXEC";
        case kEtSceDynamic:  return "ET_SCE_DYNAMIC";
        default:             return "ET_UNKNOWN";
    }
}

const char* ElfReader::machineName(std::uint16_t m) {
    return m == kEmX86_64 ? "x86_64" : "unknown";
}

const char* ElfReader::phdrTypeName(std::uint32_t p) {
    switch (p) {
        case kPtNull:         return "PT_NULL";
        case kPtLoad:         return "PT_LOAD";
        case kPtDynamic:      return "PT_DYNAMIC";
        case kPtInterp:       return "PT_INTERP";
        case kPtNote:         return "PT_NOTE";
        case kPtTls:          return "PT_TLS";
        case kPtSceRel:       return "PT_SCE_RELA";
        case kPtSceProcParam: return "PT_SCE_PROCPARAM";
        case kPtSceDynlibData:return "PT_SCE_DYNLIBDATA";
        case kPtSceComment:   return "PT_SCE_COMMENT";
        case kPtSceLibVer:    return "PT_SCE_LIBVERSION";
        default:              return "PT_?";
    }
}

} // namespace fusionps4::loader::elf
