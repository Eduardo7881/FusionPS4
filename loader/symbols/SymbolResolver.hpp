#pragma once

#include <cstdint>
#include <string>

namespace fusionps4::loader::modules { class Module; class ModuleRegistry; }

namespace fusionps4::loader::symbols {

// Performs name-based resolution of guest symbols. The resolver consults,
// in order:
//
//   1. The module's own exported symbols (for a self-referencing GLOB_DAT).
//   2. Every module registered in the ModuleRegistry.
//   3. Registered host "SCE stubs" (populated by Phase 4). For Phase 2 the
//      stub set is empty; unresolved SCE symbols are reported as
//      UNIMPLEMENTED, not silently zero-filled.
//
// The resolver never inspects Linux symbols: the guest never links against
// libc or libdl of the host.
class SymbolResolver {
public:
    explicit SymbolResolver(modules::ModuleRegistry& registry);

    // Returns the actual guest address of the symbol, or 0 if unresolved.
    // When `outSource` is provided, it is filled with "module:<name>" or
    // "stub:<name>" for debugging.
    std::uint64_t resolve(const modules::Module& requester,
                          const std::string&     symbolName,
                          std::string*           outSource = nullptr) const;

    // Register a host-provided stub (used by SCE compatibility libs in
    // Phase 4). `addr` is a raw function pointer that the guest will call
    // through its PLT/GOT.
    void registerStub(const std::string& symbolName, std::uint64_t addr);

private:
    modules::ModuleRegistry& m_registry;
    // Filled in Phase 4.
    // std::unordered_map<std::string, std::uint64_t> m_stubs;
};

} // namespace fusionps4::loader::symbols
