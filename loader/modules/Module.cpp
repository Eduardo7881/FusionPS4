#include "loader/modules/Module.hpp"

#include "debug/Log.hpp"

#include <cstring>

using fusionps4::debug::LogCategory;
using namespace fusionps4::loader::elf;

namespace fusionps4::loader::modules {

Module::Module() = default;
Module::~Module() = default;

bool Module::setup(std::string   name,
                   ElfImage      image,
                   std::uint64_t preferredBase,
                   std::uint64_t actualBase) {
    m_name          = std::move(name);
    m_image         = image;
    m_preferredBase = preferredBase;
    m_actualBase    = actualBase;
    m_delta         = actualBase - preferredBase;

    FP4_INFO(LogCategory::Loader)
        << "Module \"" << m_name << "\" base="
        << reinterpret_cast<void*>(std::uintptr_t(m_actualBase))
        << " preferred="
        << reinterpret_cast<void*>(std::uintptr_t(m_preferredBase))
        << " delta=" << reinterpret_cast<void*>(std::uintptr_t(m_delta));
    return true;
}

void Module::populateSymbolTableFromDynamic() {
    m_dynsym = nullptr;
    m_dynstr = nullptr;
    m_dynsymCount = 0;

    const Elf64_Dyn* symtabDyn = nullptr;
    const Elf64_Dyn* strtabDyn = nullptr;
    const Elf64_Dyn* symtabSzDyn = nullptr;
    const Elf64_Dyn* strtabSzDyn = nullptr;

    for (const auto& d : m_dynamics) {
        switch (d.d_tag) {
            case kDtSymtab:     symtabDyn   = &d; break;
            case kDtStrtab:     strtabDyn   = &d; break;
            case kDtSymtabSz:   symtabSzDyn = &d; break;
            case kDtStrtabSz:   strtabSzDyn = &d; break;
            case kDtSceSymtab:  symtabDyn   = &d; break;
            case kDtSceStrtab:  strtabDyn   = &d; break;
            case kDtSceSymtabSz:symtabSzDyn = &d; break;
            case kDtSceStrtabSz:strtabSzDyn = &d; break;
            default: break;
        }
    }

    if (!symtabDyn || !strtabDyn) {
        FP4_DEBUG(LogCategory::Loader)
            << "Module \"" << m_name << "\" has no dynamic symbol table";
        return;
    }

    const std::uint64_t symAddr = relocate(symtabDyn->d_val);
    const std::uint64_t strAddr = relocate(strtabDyn->d_val);

    // In our model, guest VAs are identity-mapped to host VAs, so a raw cast
    // is valid once we validated that the range was mapped.
    m_dynsym = reinterpret_cast<const Elf64_Sym*>(std::uintptr_t(symAddr));
    m_dynstr = reinterpret_cast<const char*>(std::uintptr_t(strAddr));

    if (symtabSzDyn) {
        m_dynsymCount = static_cast<std::size_t>(symtabSzDyn->d_val) /
                        sizeof(Elf64_Sym);
    } else {
        // Fallback: stop when the symbol table runs into the string table.
        // PS4 images usually provide the size; this fallback exists for
        // robustness on non-PS4 test binaries.
        m_dynsymCount = 0;
        while (true) {
            const Elf64_Sym* s = m_dynsym + m_dynsymCount;
            const auto symEnd = std::uintptr_t(s + 1);
            const auto strBegin = std::uintptr_t(m_dynstr);
            if (symEnd > strBegin) break;
            if (s->st_name == 0 && s->st_info == 0 &&
                s->st_shndx == 0 && s->st_value == 0) {
                // A leading null entry means "stop" only if the string table
                // starts immediately after; the null entry is at index 0, so
                // we cannot detect termination this way. Bail out.
                break;
            }
            ++m_dynsymCount;
            if (m_dynsymCount > 1u << 20) break;  // sanity
        }
    }

    (void)strtabSzDyn;
    FP4_DEBUG(LogCategory::Loader)
        << "Module \"" << m_name << "\" dynsym entries=" << m_dynsymCount;
}

void Module::configureTls(const std::uint8_t* templateBytes,
                          std::uint64_t      filesz,
                          std::uint64_t      memsz,
                          std::uint64_t      align) {
    m_tls.configure(templateBytes, filesz, memsz, align);
}

std::uint64_t Module::lookupExport(const std::string& name) const {
    if (!m_dynsym || !m_dynstr || m_dynsymCount == 0) return 0;

    for (std::size_t i = 0; i < m_dynsymCount; ++i) {
        const Elf64_Sym& s = m_dynsym[i];
        if (s.st_shndx == kShnUndef) continue;    // import, not export
        if (s.st_name == 0) continue;
        const char* symName = m_dynstr + s.st_name;
        if (name == symName) {
            return s.st_value + m_delta;
        }
    }
    return 0;
}

} // namespace fusionps4::loader::modules
