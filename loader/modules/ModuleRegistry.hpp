#pragma once

#include "loader/modules/Module.hpp"

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace fusionps4::loader::modules {

class ModuleRegistry {
public:
    ModuleRegistry() = default;
    ~ModuleRegistry() = default;

    ModuleRegistry(const ModuleRegistry&) = delete;
    ModuleRegistry& operator=(const ModuleRegistry&) = delete;

    // Registers a module by its soname (or file name for the main
    // executable). Replaces any previous module with the same name.
    void add(const std::string& name, ModulePtr module);

    ModulePtr find(const std::string& name) const;

    // Search every registered module for an exported symbol. Returns 0 if
    // nothing matched.
    std::uint64_t resolveExport(const std::string& symbolName,
                                std::string*       outModuleName = nullptr) const;

    std::vector<ModulePtr> all() const;
    std::size_t            size() const;

    void clear();

private:
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, ModulePtr> m_modules;
};

} // namespace fusionps4::loader::modules
