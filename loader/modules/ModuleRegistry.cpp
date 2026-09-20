#include "loader/modules/ModuleRegistry.hpp"

#include "debug/Log.hpp"

using fusionps4::debug::LogCategory;

namespace fusionps4::loader::modules {

void ModuleRegistry::add(const std::string& name, ModulePtr module) {
    if (!module) return;
    std::lock_guard lock(m_mutex);
    m_modules[name] = std::move(module);
    FP4_DEBUG(LogCategory::Loader)
        << "registry: added module \"" << name << "\"";
}

ModulePtr ModuleRegistry::find(const std::string& name) const {
    std::lock_guard lock(m_mutex);
    auto it = m_modules.find(name);
    return it == m_modules.end() ? nullptr : it->second;
}

std::uint64_t ModuleRegistry::resolveExport(const std::string& symbolName,
                                            std::string* outModuleName) const {
    std::lock_guard lock(m_mutex);
    for (const auto& [name, mod] : m_modules) {
        if (!mod) continue;
        const auto addr = mod->lookupExport(symbolName);
        if (addr) {
            if (outModuleName) *outModuleName = name;
            return addr;
        }
    }
    return 0;
}

std::vector<ModulePtr> ModuleRegistry::all() const {
    std::lock_guard lock(m_mutex);
    std::vector<ModulePtr> out;
    out.reserve(m_modules.size());
    for (const auto& [_, m] : m_modules) out.push_back(m);
    return out;
}

std::size_t ModuleRegistry::size() const {
    std::lock_guard lock(m_mutex);
    return m_modules.size();
}

void ModuleRegistry::clear() {
    std::lock_guard lock(m_mutex);
    m_modules.clear();
}

} // namespace fusionps4::loader::modules
