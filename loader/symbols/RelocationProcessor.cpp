#include "loader/elf/RelocationProcessor.hpp"

#include "debug/Log.hpp"
#include "loader/elf/RelocationTypes.hpp"
#include "loader/modules/Module.hpp"
#include "loader/symbols/SymbolResolver.hpp"

#include <cstring>

using fusionps4::debug::LogCategory;

namespace fusionps4::loader::elf {

namespace {

// All guest VAs are identity-mapped to host VAs, so we can write to a guest
// address by casting it. The loader validates that the address falls inside
// a loaded segment before writing (via the Module's segment list).
bool isWritableAddress(const modules::Module& m, std::uint64_t addr) {
    for (const auto& s : m.segments()) {
        if (addr >= s.vaddr && addr < s.vaddr + s.memsz) {
            return (s.prot & kPfW) != 0;
        }
    }
    return false;
}

void writeU64(std::uint64_t addr, std::uint64_t value) {
    *reinterpret_cast<std::uint64_t*>(std::uintptr_t(addr)) = value;
}

std::uint64_t readU64(std::uint64_t addr) {
    return *reinterpret_cast<const std::uint64_t*>(std::uintptr_t(addr));
}

} // namespace

RelocationProcessor::RelocationProcessor(modules::Module& module,
                                         symbols::SymbolResolver& resolver)
    : m_module(module), m_resolver(resolver) {}

bool RelocationProcessor::applyOne(const Elf64_Rela& r, RelocationStats& stats) {
    const std::uint32_t type = elfRelType(r.r_info);
    const std::uint32_t sym  = elfRelSym(r.r_info);

    // r_offset in a PS4 RELA refers to a *preferred* address. Translate.
    const std::uint64_t target = m_module.relocate(r.r_offset);

    ++stats.total;

    if (!isWritableAddress(m_module, target)) {
        FP4_ERROR(LogCategory::Loader)
            << "relocation target " << reinterpret_cast<void*>(target)
            << " (" << relocationName(type) << ") not writable in module \""
            << m_module.name() << "\"";
        ++stats.failed;
        return false;
    }

    switch (type) {
        case kRX86_64None:
            return true;

        case kRX86_64Relative: {
            // Actual value = delta + addend. delta already accounts for the
            // base shift the loader applied.
            writeU64(target, static_cast<std::uint64_t>(m_module.delta() + r.r_addend));
            ++stats.relative;
            return true;
        }

        case kRX86_64_64: {
            // S + A, where S is the resolved symbol value.
            const auto* symtab = m_module.dynsym();
            const char* strtab = m_module.dynstr();
            if (!symtab || !strtab || sym >= m_module.dynsymCount()) {
                FP4_ERROR(LogCategory::Loader)
                    << "R_X86_64_64 with invalid symbol index " << sym;
                ++stats.failed;
                return false;
            }
            const char* name = strtab + symtab[sym].st_name;
            std::string src;
            const auto symAddr = m_resolver.resolve(m_module, name, &src);
            if (!symAddr) {
                ++stats.failed;
                return false;
            }
            writeU64(target, symAddr + static_cast<std::uint64_t>(r.r_addend));
            ++stats.abs64;
            return true;
        }

        case kRX86_64GlobDat:
        case kRX86_64JumpSlot: {
            const auto* symtab = m_module.dynsym();
            const char* strtab = m_module.dynstr();
            if (!symtab || !strtab || sym >= m_module.dynsymCount()) {
                FP4_ERROR(LogCategory::Loader)
                    << "GLOB_DAT/JUMP_SLOT with invalid symbol index " << sym;
                ++stats.failed;
                return false;
            }
            const char* name = strtab + symtab[sym].st_name;
            std::string src;
            const auto symAddr = m_resolver.resolve(m_module, name, &src);
            if (!symAddr) {
                ++stats.failed;
                return false;
            }
            writeU64(target, symAddr);
            if (type == kRX86_64GlobDat) ++stats.globDat;
            else                          ++stats.jumpSlot;
            return true;
        }

        default:
            FP4_ERROR(LogCategory::Loader)
                << "UNIMPLEMENTED relocation type " << type
                << " (" << relocationName(type) << ") at "
                << reinterpret_cast<void*>(target)
                << " in module \"" << m_module.name() << "\"";
            ++stats.skipped;
            ++stats.failed;
            return false;
    }
}

bool RelocationProcessor::applyRelaRange(const Elf64_Rela* relas,
                                         std::size_t       count,
                                         RelocationStats&  stats) {
    if (!relas || count == 0) return true;
    bool allOk = true;
    for (std::size_t i = 0; i < count; ++i) {
        if (!applyOne(relas[i], stats)) allOk = false;
    }
    return allOk;
}

bool RelocationProcessor::applyAll(RelocationStats& stats) {
    stats = {};

    const auto& dyns = m_module.dynamics();

    // Collect dynamic-table-derived relocation descriptors.
    std::uint64_t relaAddr  = 0;
    std::uint64_t relaSize  = 0;
    std::uint64_t relaEnt   = sizeof(Elf64_Rela);
    std::uint64_t jmprel    = 0;
    std::uint64_t pltrelsz  = 0;

    bool haveRela = false;
    bool haveJmprel = false;

    for (const auto& d : dyns) {
        switch (d.d_tag) {
            case kDtRela:
            case kDtSceRela:
                relaAddr = d.d_val; haveRela = true; break;
            case kDtRelaSz:
            case kDtSceRelaSz:
                relaSize = d.d_val; break;
            case kDtRelaEnt:
            case kDtSceRelaEnt:
                relaEnt = d.d_val; break;
            case kDtJmpRel:
                jmprel = d.d_val; haveJmprel = true; break;
            case kDtPltRelSz:
                pltrelsz = d.d_val; break;
            default:
                break;
        }
    }

    bool allOk = true;

    if (haveRela && relaSize) {
        if (relaEnt != sizeof(Elf64_Rela)) {
            FP4_ERROR(LogCategory::Loader)
                << "unsupported RELA entsize " << relaEnt
                << " (expected " << sizeof(Elf64_Rela) << ")";
            return false;
        }
        const auto* relas = reinterpret_cast<const Elf64_Rela*>(
            std::uintptr_t(m_module.relocate(relaAddr)));
        const auto count = static_cast<std::size_t>(relaSize / relaEnt);
        FP4_DEBUG(LogCategory::Loader)
            << "applying " << count << " DT_RELA relocations for \""
            << m_module.name() << "\"";
        if (!applyRelaRange(relas, count, stats)) allOk = false;
    }

    if (haveJmprel && pltrelsz) {
        const auto* jrel = reinterpret_cast<const Elf64_Rela*>(
            std::uintptr_t(m_module.relocate(jmprel)));
        const auto count = static_cast<std::size_t>(pltrelsz / sizeof(Elf64_Rela));
        FP4_DEBUG(LogCategory::Loader)
            << "applying " << count << " DT_JMPREL relocations for \""
            << m_module.name() << "\"";
        if (!applyRelaRange(jrel, count, stats)) allOk = false;
    }

    if (!haveRela && !haveJmprel) {
        FP4_WARN(LogCategory::Loader)
            << "module \"" << m_module.name()
            << "\" has no PT_DYNAMIC relocation descriptors";
    }

    FP4_INFO(LogCategory::Loader)
        << "relocations for \"" << m_module.name() << "\": total="
        << stats.total << " rel=" << stats.relative
        << " glob=" << stats.globDat << " jmp=" << stats.jumpSlot
        << " abs64=" << stats.abs64 << " skipped=" << stats.skipped
        << " failed=" << stats.failed;

    return allOk;
}

} // namespace fusionps4::loader::elf
