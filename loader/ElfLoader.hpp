#pragma once

#include "loader/modules/Module.hpp"

#include <memory>
#include <string>

namespace fusionps4::runtime::memory { class AddressSpace; }

namespace fusionps4::loader {

namespace modules { class ModuleRegistry; }
namespace symbols { class SymbolResolver; }

// Owns the lifetime of the ELF file buffer for every module it loads. One
// ElfLoader instance may load any number of modules; when it is destroyed,
// the backing buffers are freed only after the modules are no longer
// referenced.
class ElfLoader {
public:
    ElfLoader(runtime::memory::AddressSpace& as,
              modules::ModuleRegistry&       registry,
              symbols::SymbolResolver&       resolver);
    ~ElfLoader();

    ElfLoader(const ElfLoader&) = delete;
    ElfLoader& operator=(const ElfLoader&) = delete;

    // Loads the executable at `path` into the given address space, resolves
    // its relocations, configures TLS, and returns the resulting module.
    // Returns nullptr on failure with detailed logging.
    modules::ModulePtr load(const std::string& path);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace fusionps4::loader
