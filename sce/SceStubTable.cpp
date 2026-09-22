#include "sce/SceStubTable.hpp"

#include "debug/Log.hpp"

using fusionps4::debug::LogCategory;

namespace fusionps4::sce {

SceStubTable& SceStubTable::instance() {
    static SceStubTable s;
    return s;
}

std::string SceStubTable::key(const std::string& lib, const std::string& fn) {
    return lib + "::" + fn;
}

void SceStubTable::registerStub(const std::string& library,
                                const std::string& function,
                                void*              addr) {
    if (!addr) {
        FP4_ERROR(LogCategory::Sce)
            << "refusing null stub for " << key(library, function);
        return;
    }
    std::lock_guard lock(m_mutex);
    m_stubs[key(library, function)] = addr;
    if (m_byId.size() < 0x0FFFFFFF) {
        m_byId[m_nextId++] = addr;
    }
}

void* SceStubTable::resolve(const std::string& library,
                            const std::string& function) const {
    std::lock_guard lock(m_mutex);
    auto it = m_stubs.find(key(library, function));
    return it == m_stubs.end() ? nullptr : it->second;
}

void* SceStubTable::resolveSymbol(const std::string& qualified) const {
    std::lock_guard lock(m_mutex);
    auto it = m_stubs.find(qualified);
    return it == m_stubs.end() ? nullptr : it->second;
}

std::size_t SceStubTable::size() const {
    std::lock_guard lock(m_mutex);
    return m_stubs.size();
}

std::vector<std::pair<std::string, void*>> SceStubTable::snapshot() const {
    std::lock_guard lock(m_mutex);
    std::vector<std::pair<std::string, void*>> out;
    out.reserve(m_stubs.size());
    for (const auto& [k, v] : m_stubs) out.emplace_back(k, v);
    return out;
}

std::uint32_t SceStubTable::getOrAssignId(const std::string& qualified) {
    std::lock_guard lock(m_mutex);
    auto it = m_stubs.find(qualified);
    if (it == m_stubs.end()) return 0;
    for (const auto& [id, p] : m_byId) if (p == it->second) return id;
    return 0;
}

void* SceStubTable::resolveById(std::uint32_t id) const {
    std::lock_guard lock(m_mutex);
    auto it = m_byId.find(id);
    return it == m_byId.end() ? nullptr : it->second;
}

} // namespace fusionps4::sce
