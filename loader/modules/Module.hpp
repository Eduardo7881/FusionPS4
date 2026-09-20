#pragma once

#include "loader/elf/ElfReader.hpp"
#include "loader/elf/TlsBlock.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace fusionps4::loader::modules {

struct LoadedSegment {
    std::uint64_t vaddr  = 0;   // actual guest address
    std::uint64_t filesz = 0;
    std::uint64_t memsz  = 0;
    std::uint32_t prot   = 0;   // p_flags as in the file
};

class Module {
public:
    Module();
    ~Module();

    Module(const Module&) = delete;
    Module& operator=(const Module&) = delete;

    // Non-owning view over image bytes; the caller must keep the backing
    // buffer alive for the lifetime of the Module (ElfLoader does).
    bool setup(std::string            name,
               elf::ElfImage          image,
               std::uint64_t          preferredBase,
               std::uint64_t          actualBase);

    const std::string& name() const { return m_name; }

    std::uint64_t preferredBase() const { return m_preferredBase; }
    std::uint64_t actualBase()    const { return m_actualBase; }
    std::uint64_t delta()         const { return m_delta; }
    std::uint64_t entryPoint()    const { return m_entry; }

    const std::vector<LoadedSegment>&  segments() const { return m_segments; }
    const std::vector<elf::Elf64_Dyn>& dynamics() const { return m_dynamics; }

    const elf::ElfImage& image() const { return m_image; }

    elf::TlsBlock&       tls()       { return m_tls; }
    const elf::TlsBlock& tls() const { return m_tls; }

    // Translate an address as it appears in the file (a "preferred" vaddr)
    // to the actual guest address where we mapped it.
    std::uint64_t relocate(std::uint64_t preferredVaddr) const {
        return preferredVaddr + m_delta;
    }

    // Symbol table access, if the module exposes one via DT_SYMTAB/DT_STRTAB
    // (or their SCE equivalents). Both pointers point into the mapped image.
    const elf::Elf64_Sym* dynsym() const { return m_dynsym; }
    const char*           dynstr() const { return m_dynstr; }
    std::size_t           dynsymCount() const { return m_dynsymCount; }

    // Returns the actual guest address of an exported symbol, or 0.
    std::uint64_t lookupExport(const std::string& name) const;

private:
    std::string                 m_name;
    elf::ElfImage               m_image;
    std::vector<LoadedSegment>  m_segments;
    std::vector<elf::Elf64_Dyn> m_dynamics;
    std::uint64_t               m_preferredBase = 0;
    std::uint64_t               m_actualBase    = 0;
    std::uint64_t               m_delta         = 0;
    std::uint64_t               m_entry         = 0;
    elf::TlsBlock               m_tls;

    const elf::Elf64_Sym*       m_dynsym      = nullptr;
    const char*                 m_dynstr      = nullptr;
    std::size_t                 m_dynsymCount = 0;

public:
    // Called by ElfLoader after segments are mapped but before relocations,
    // so that Module can populate m_dynsym/m_dynstr from the dynamic table.
    void populateSymbolTableFromDynamic();

    // Called by ElfLoader to attach PT_TLS data.
    void configureTls(const std::uint8_t* templateBytes,
                      std::uint64_t      filesz,
                      std::uint64_t      memsz,
                      std::uint64_t      align);

    // Access for the loader to append a segment descriptor once mapped.
    void appendSegment(LoadedSegment s) { m_segments.push_back(s); }

    // Set after relocation processing completes.
    void setEntryPoint(std::uint64_t e) { m_entry = e; }
};

using ModulePtr = std::shared_ptr<Module>;

} // namespace fusionps4::loader::modules
