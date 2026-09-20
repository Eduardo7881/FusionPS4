#include "sce/gnm/GnmCommandRecorder.hpp"

namespace fusionps4::sce::gnm {

void GnmCommandRecorder::record(const graphics::RecordedDraw& cmd) {
    std::lock_guard lock(m_mutex);
    m_commands.push_back(cmd);
}

std::vector<graphics::RecordedDraw> GnmCommandRecorder::drain() {
    std::lock_guard lock(m_mutex);
    auto out = std::move(m_commands);
    m_commands.clear();
    return out;
}

std::size_t GnmCommandRecorder::pending() const {
    std::lock_guard lock(m_mutex);
    return m_commands.size();
}

void GnmCommandRecorder::requestFrame() {
    std::lock_guard lock(m_mutex);
    m_frameRequested = true;
}

bool GnmCommandRecorder::takeFrameRequest() {
    std::lock_guard lock(m_mutex);
    const bool r = m_frameRequested;
    m_frameRequested = false;
    return r;
}

} // namespace fusionps4::sce::gnm
