#include "loader/symbols/SymbolResolver.hpp"

#include "debug/Log.hpp"
#include "loader/modules/Module.hpp"
#include "loader/modules/ModuleRegistry.hpp"

using fusionps4::debug::LogCategory;

namespace fusionps4::loader::symbols {

SymbolResolver::SymbolResolver(modules::ModuleRegistry& registry)
    : m_registry(registry) {}

std::uint64_t SymbolResolver::resolve(const modules::Module& requester,
                                      const std::string&     symbolName,
                                      std::string*           outSource) const {
    // (1) self
    if (auto self = requester.lookupExport(symbolName); self != 0) {
        if (outSource) *outSource = "self";
        return self;
    }

    // (2) any registered module
    std::string src;
    const auto fromRegistry = m_registry.resolveExport(symbolName, &src);
    if (fromRegistry) {
        if (outSource) *outSource = "module:" + src;
        return fromRegistry;
    }

    // (3) SCE stubs are registered in Phase 4.
    FP4_WARN(LogCategory::Loader)
        << "UNIMPLEMENTED symbol lookup for \"" << symbolName
        << "\" requested by module \"" << requester.name() << "\"";
    if (outSource) *outSource = "unresolved";
    return 0;
}

void SymbolResolver::registerStub(const std::string& symbolName,
                                  std::uint64_t      addr) {
    // Phase 4 will implement this via a dedicated stub table.
    FP4_UNIMPLEMENTED(LogCategory::Loader, "SymbolResolver::registerStub");
    (void)symbolName;
    (void)addr;
}

} // namespace fusionps4::loader::symbols
