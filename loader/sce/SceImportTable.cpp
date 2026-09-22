#include "loader/sce/SceImportTable.hpp"

#include "debug/Log.hpp"

#include <cstring>
#include <unordered_map>

using fusionps4::debug::LogCategory;
using namespace fusionps4::loader::elf;

namespace fusionps4::loader::sce {

namespace {

// Read a NUL-terminated string from the ELF's dynamic string table. The
// caller passes the string table base (already relocated) and a
// *preferred* offset (as stored in DT_*).
std::string readStr(const std::uint8_t* strtab,
                    std::uint64_t        strtabSize,
                    std::uint64_t        offset) {
    if (!strtab || offset >= strtabSize) return {};
    const char* p = reinterpret_cast<const char*>(strtab + offset);
    // Bounded scan for NUL.
    std::size_t i = 0;
    while (i < strtabSize - offset && p[i] != '\0') ++i;
    return std::string(p, i);
}

// Locate the dynamic string table from the collected dynamics. Returns
// {ptr, size} in *host* addresses (already relocated).
struct StrTab {
    const std::uint8_t* ptr  = nullptr;
    std::uint64_t       size = 0;
};

StrTab locateStrtab(std::vector<Elf64_Dyn>& dynamics, std::int64_t delta) {
    StrTab out;
    std::uint64_t strtab = 0;
    std::uint64_t strsz  = 0;
    for (const auto& d : dynamics) {
        if (d.d_tag == kDtStrtab || d.d_tag == kDtSceStrtab) strtab = d.d_val + delta;
        else if (d.d_tag == kDtStrtabSz || d.d_tag == kDtSceStrtabSz) strsz = d.d_val;
    }
    if (strtab) {
        out.ptr  = reinterpret_cast<const std::uint8_t*>(strtab);
        out.size = strsz ? strsz : 64 * 1024;   // conservative bound
    }
    return out;
}

} // namespace

bool SceImportParser::parse(const ElfImage&          /*image*/,
                            std::uint64_t             /*moduleBase*/,
                            std::int64_t              delta,
                            std::vector<Elf64_Dyn>    dynamics,
                            ImportSet&                out) {
    out = {};

    const auto strtab = locateStrtab(dynamics, delta);
    if (!strtab.ptr) {
        FP4_WARN(LogCategory::Loader)
            << "SCE import parser: no dynamic string table; "
            << "module declares no SCE imports";
        return true;   // not an error: some modules have no imports
    }

    // The import chain is a run of (DT_SCE_IMPORT_LIB, DT_SCE_IMPORT_LIB_ATTR)
    // pairs, terminated by anything that is not one of those two tags.
    // Names appear in the same order as attributes.
    struct PendingLib { std::string name; };
    std::vector<PendingLib>                          pendingNames;
    std::vector<std::uint64_t>                       pendingTableAddrs;
    std::vector<std::pair<std::size_t, std::size_t>> tableSpans;   // (start, count)

    for (std::size_t i = 0; i < dynamics.size(); ++i) {
        const auto& d = dynamics[i];
        if (d.d_tag == kDtSceImportLib) {
            PendingLib pl;
            pl.name = readStr(strtab.ptr, strtab.size, d.d_val);
            pendingNames.push_back(std::move(pl));
        } else if (d.d_tag == kDtSceImportLibAttr) {
            // d_val is the *preferred* address of the SceImportsTable.
            pendingTableAddrs.push_back(d.d_val + delta);
        }
    }

    if (pendingNames.size() != pendingTableAddrs.size()) {
        FP4_WARN(LogCategory::Loader)
            << "SCE import parser: " << pendingNames.size()
            << " library names but " << pendingTableAddrs.size()
            << " table attributes; refusing to guess the pairing";
        return false;
    }

    for (std::size_t i = 0; i < pendingNames.size(); ++i) {
        ImportLibrary lib;
        lib.name = pendingNames[i].name;

        const auto* tbl = reinterpret_cast<const SceImportsTable*>(
            static_cast<std::uintptr_t>(pendingTableAddrs[i]));
        if (!tbl) {
            FP4_WARN(LogCategory::Loader)
                << "SCE import parser: null table for library \""
                << lib.name << "\"";
            continue;
        }

        const auto total =
            tbl->numFuncImports + tbl->numVarImports + tbl->numTlsImports;
        const auto* entries = reinterpret_cast<const SceImportsEntry*>(tbl + 1);

        // Addresses in each entry are *preferred*; relocate them.
        auto reloc = [delta](SceImportsEntry e) {
            e.address += delta;
            return e;
        };

        std::size_t idx = 0;
        for (std::uint16_t k = 0; k < tbl->numFuncImports; ++k) {
            lib.funcImports.push_back(reloc(entries[idx++]));
        }
        for (std::uint16_t k = 0; k < tbl->numVarImports; ++k) {
            lib.varImports.push_back(reloc(entries[idx++]));
        }
        for (std::uint16_t k = 0; k < tbl->numTlsImports; ++k) {
            lib.tlsImports.push_back(reloc(entries[idx++]));
        }

        FP4_DEBUG(LogCategory::Loader)
            << "  import lib \"" << lib.name << "\": "
            << lib.funcImports.size() << " funcs, "
            << lib.varImports.size() << " vars, "
            << lib.tlsImports.size() << " tls";
        out.libraries.push_back(std::move(lib));
        (void)total;
    }

    FP4_INFO(LogCategory::Loader)
        << "SCE import parser: " << out.libraries.size()
        << " libraries, " << out.totalFuncs() << " function imports";
    return true;
}

} // namespace fusionps4::loader::sce
