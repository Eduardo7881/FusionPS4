#include "debug/Symbolizer.hpp"

#include "loader/modules/ModuleRegistry.hpp"
#include "loader/modules/Module.hpp"
#include "sce/SceStubTable.hpp"

#include <cstdio>

namespace fusionps4::debug {

void Symbolizer::bindModules(const loader::modules::ModuleRegistry* reg) {
    m_registry = reg;
}

std::string Symbolizer::describe(std::uintptr_t pc) const {
    if (!m_registry) return {};
    for (const auto& mod : m_registry->all()) {
        if (!mod) continue;
        std::uint64_t lo = UINT64_MAX, hi = 0;
        for (const auto& seg : mod->segments()) {
            lo = std::min(lo, seg.vaddr);
            hi = std::max(hi, seg.vaddr + seg.memsz);
        }
        if (pc >= lo && pc < hi) {
            char buf[128];
            std::snprintf(buf, sizeof(buf), "%s+0x%llx",
                          mod->name().c_str(),
                          static_cast<unsigned long long>(pc - lo));
            return buf;
        }
    }
    return {};
}

std::string Symbolizer::describeStub(std::uintptr_t pc) const {
    const auto& table = sce::SceStubTable::instance();
    for (const auto& [key, addr] : table.snapshot()) {
        if (reinterpret_cast<std::uintptr_t>(addr) == pc) return key;
    }
    return {};
}

} // namespace fusionps4::debug
