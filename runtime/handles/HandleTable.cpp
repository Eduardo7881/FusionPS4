#include "runtime/handles/HandleTable.hpp"

#include "debug/Log.hpp"

using fusionps4::debug::LogCategory;

namespace fusionps4::runtime::handles {

Handle HandleTable::registerObject(std::shared_ptr<HandleObject> obj) {
    if (!obj) return kInvalidHandle;
    std::lock_guard lock(m_mutex);
    const Handle h = m_next++;
    m_objects.emplace(h, std::move(obj));
    FP4_TRACE(LogCategory::Handle) << "register handle=" << h;
    return h;
}

std::shared_ptr<HandleObject> HandleTable::get(Handle h) const {
    std::lock_guard lock(m_mutex);
    auto it = m_objects.find(h);
    if (it == m_objects.end()) return nullptr;
    return it->second;
}

bool HandleTable::close(Handle h) {
    std::lock_guard lock(m_mutex);
    auto it = m_objects.find(h);
    if (it == m_objects.end()) return false;
    m_objects.erase(it);
    FP4_TRACE(LogCategory::Handle) << "close handle=" << h;
    return true;
}

void HandleTable::closeAll() {
    std::lock_guard lock(m_mutex);
    m_objects.clear();
    m_next = 3;
}

std::vector<std::pair<Handle, std::shared_ptr<HandleObject>>>
HandleTable::snapshot() const {
    std::lock_guard lock(m_mutex);
    std::vector<std::pair<Handle, std::shared_ptr<HandleObject>>> out;
    out.reserve(m_objects.size());
    for (const auto& [h, obj] : m_objects) out.emplace_back(h, obj);
    return out;
}

std::size_t HandleTable::size() const {
    std::lock_guard lock(m_mutex);
    return m_objects.size();
}

} // namespace fusionps4::runtime::handles
