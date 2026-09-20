#pragma once

#include "loader/elf/ElfReader.hpp"

#include <cstdint>

namespace fusionps4::loader::modules { class Module; }
namespace fusionps4::loader::symbols { class SymbolResolver; }

namespace fusionps4::loader::elf {

struct RelocationStats {
    std::uint64_t total    = 0;
    std::uint64_t relative = 0;
    std::uint64_t globDat  = 0;
    std::uint64_t jumpSlot = 0;
    std::uint64_t abs64    = 0;
    std::uint64_t skipped  = 0;
    std::uint64_t failed   = 0;
};

// Applies every relocation that the module records through its PT_DYNAMIC
// (or PT_SCE_RELA) segments. The processor is responsible for preserving
// the *semantics* of each relocation as the guest expects them; it never
// silently zero-fills a relocation it does not understand.
class RelocationProcessor {
public:
    RelocationProcessor(modules::Module&            module,
                        symbols::SymbolResolver&    resolver);

    // Walks DT_RELA/DT_RELASZ/DT_RELAENT and DT_JMPREL/DT_PLTRELSZ.
    // Populates stats; returns true only if no relocation reported failure.
    bool applyAll(RelocationStats& stats);

private:
    bool applyRelaRange(const Elf64_Rela* relas,
                        std::size_t       count,
                        RelocationStats&  stats);

    bool applyOne(const Elf64_Rela& r, RelocationStats& stats);

    modules::Module&         m_module;
    symbols::SymbolResolver& m_resolver;
};

} // namespace fusionps4::loader::elf
