#pragma once

#include <cstdint>
#include <string>

namespace fusionps4::loader::modules { class ModuleRegistry; }

namespace fusionps4::debug {

// Maps a guest virtual address (== host VA by construction) to
// "module(+offset)" using the process's ModuleRegistry. When the address
// is not inside any loaded module, returns "" and the caller falls back to
// printing the raw pointer.
class Symbolizer {
public:
    Symbolizer() = default;

    void bindModules(const loader::modules::ModuleRegistry* reg);

    // "module.exe+0x1234" or "" if unresolved.
    std::string describe(std::uintptr_t pc) const;

    // "<library>::<function>" if the address matches a registered SCE stub
    // and the symbolizer was bound to the stub table. Empty otherwise.
    std::string describeStub(std::uintptr_t pc) const;

private:
    const loader::modules::ModuleRegistry* m_registry = nullptr;
};

} // namespace fusionps4::debug
