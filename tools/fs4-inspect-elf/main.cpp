// fs4-inspect-elf — offline ELF inspection for PS4 binaries. Loads an ELF,
// prints type, machine, entry, segments, dynamic tags that we recognise,
// and (if present) the SCE module info note. Does not link against the
// runtime: it embeds a copy of ElfReader and the PS4 constants to remain a
// standalone diagnostic tool.

#include "loader/elf/ElfReader.hpp"
#include "loader/elf/ElfTypes.hpp"

#include <cstdio>
#include <string>
#include <vector>

using namespace fusionps4::loader::elf;

static void printUsage(const char* argv0) {
    std::fprintf(stderr, "usage: %s <elf-file>\n", argv0);
}

static const char* dynTagName(std::int64_t t) {
    switch (t) {
        case kDtNull:        return "DT_NULL";
        case kDtNeeded:      return "DT_NEEDED";
        case kDtPltRelSz:    return "DT_PLTRELSZ";
        case kDtPltGot:      return "DT_PLTGOT";
        case kDtHash:        return "DT_HASH";
        case kDtStrtab:      return "DT_STRTAB";
        case kDtSymtab:      return "DT_SYMTAB";
        case kDtRela:        return "DT_RELA";
        case kDtRelaSz:      return "DT_RELASZ";
        case kDtRelaEnt:     return "DT_RELAENT";
        case kDtStrtabSz:    return "DT_STRTABSZ";
        case kDtSymtabSz:    return "DT_SYMTABSZ";
        case kDtJmpRel:      return "DT_JMPREL";
        case kDtInitArray:   return "DT_INIT_ARRAY";
        case kDtFiniArray:   return "DT_FINI_ARRAY";
        case kDtSceNeededModule:  return "DT_SCE_NEEDED_MODULE";
        case kDtSceModuleInfo:    return "DT_SCE_MODULE_INFO";
        case kDtSceImportLib:     return "DT_SCE_IMPORT_LIB";
        case kDtSceImportLibAttr: return "DT_SCE_IMPORT_LIB_ATTR";
        case kDtSceSymtab:        return "DT_SCE_SYMTAB";
        case kDtSceStrtab:        return "DT_SCE_STRTAB";
        case kDtSceRela:          return "DT_SCE_RELA";
        case kDtSceRelaSz:        return "DT_SCE_RELASZ";
        default:                  return "(unknown)";
    }
}

int main(int argc, char** argv) {
    if (argc != 2) { printUsage(argv[0]); return 2; }

    std::vector<std::uint8_t> bytes;
    if (!ElfReader::readFile(argv[1], bytes)) {
        std::fprintf(stderr, "read failed\n");
        return 1;
    }
    ElfImage img;
    if (!ElfReader::parse(bytes, img)) {
        std::fprintf(stderr, "parse failed\n");
        return 1;
    }

    std::printf("file: %s\n", argv[1]);
    std::printf("  e_type    = %s (0x%04x)\n",
                ElfReader::typeName(img.ehdr.e_type), img.ehdr.e_type);
    std::printf("  e_machine = %s\n",
                ElfReader::machineName(img.ehdr.e_machine));
    std::printf("  entry     = 0x%llx\n",
                (unsigned long long)img.ehdr.e_entry);
    std::printf("  phnum     = %u\n", img.ehdr.e_phnum);

    for (std::size_t i = 0; i < img.phdrs.size(); ++i) {
        const auto& s = img.phdrs[i];
        std::printf("  PH[%zu] %s flags=0x%x off=0x%llx vaddr=0x%llx "
                    "filesz=0x%llx memsz=0x%llx align=0x%llx\n",
                    i, ElfReader::phdrTypeName(s.type), s.flags,
                    (unsigned long long)s.offset,
                    (unsigned long long)s.vaddr,
                    (unsigned long long)s.filesz,
                    (unsigned long long)s.memsz,
                    (unsigned long long)s.align);

        if (s.type == kPtDynamic) {
            const auto* dyn = img.as<Elf64_Dyn>(s.offset);
            const auto count = s.filesz / sizeof(Elf64_Dyn);
            for (std::size_t d = 0; d < count && dyn; ++d) {
                if (dyn[d].d_tag == kDtNull) break;
                std::printf("       DYN %-22s = 0x%llx\n",
                            dynTagName(dyn[d].d_tag),
                            (unsigned long long)dyn[d].d_val);
            }
        }
    }

    return 0;
}
