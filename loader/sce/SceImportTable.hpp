#pragma once

#include "loader/elf/ElfTypes.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace fusionps4::loader::sce {

// PS4 modules declare their imports through a table reached from
// DT_SCE_IMPORT_LIB. The table layout (from fail0verflow's ps4-kernel and
// OpenOrbis work) is:

struct SceImportsEntry {
    std::uint16_t type       = 0;    // 0x01 = function, 0x02 = variable, 0x04 = tls
    std::uint16_t reserved0  = 0;
    std::uint32_t reserved1  = 0;
    std::uint64_t nid        = 0;
    std::uint64_t address    = 0;    // GOT slot in the module's address space
    std::uint64_t reserved2  = 0;
};
static_assert(sizeof(SceImportsEntry) == 32, "SceImportsEntry layout");

struct SceImportsTable {
    std::uint16_t numFuncImports  = 0;
    std::uint16_t numVarImports   = 0;
    std::uint16_t numTlsImports   = 0;
    std::uint16_t reserved0       = 0;
    std::uint64_t reserved1       = 0;
    // Followed by numFuncImports + numVarImports + numTlsImports entries.
};

// One import-library reference. A module imports many libraries, each with
// its own name and its own table of NID-addressed symbols.
struct ImportLibrary {
    std::string                     name;         // "libScePad"
    std::vector<SceImportsEntry>    funcImports;
    std::vector<SceImportsEntry>    varImports;
    std::vector<SceImportsEntry>    tlsImports;
};

// The set of import libraries declared by a module.
struct ImportSet {
    std::vector<ImportLibrary> libraries;

    std::size_t totalFuncs() const {
        std::size_t n = 0;
        for (const auto& l : libraries) n += l.funcImports.size();
        return n;
    }
};

// Parses the SCE import chain from the dynamic segment. The chain uses
// DT_SCE_IMPORT_LIB (0x61000009) entries whose d_val points at a
// NUL-terminated string in the module's strtab, and DT_SCE_IMPORT_LIB_ATTR
// (0x6100000B) entries whose d_val is an index into the import table.
//
// Because different PS4 SDK versions emit slightly different chains, the
// parser is tolerant: it accepts any combination that puts a name and a
// table pointer adjacently, logs what it saw, and refuses to guess.
class SceImportParser {
public:
    // `image` provides the raw bytes. `moduleBase` is the module's actual
    // guest base; `delta` = actualBase - preferredBase, used to translate
    // addresses that appear in the dynamic table.
    static bool parse(const elf::ElfImage&          image,
                      std::uint64_t                 moduleBase,
                      std::int64_t                  delta,
                      std::vector<elf::Elf64_Dyn>   dynamics,
                      ImportSet&                    out);
};

} // namespace fusionps4::loader::sce
