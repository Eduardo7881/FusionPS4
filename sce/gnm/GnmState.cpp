#include "sce/gnm/GnmState.hpp"

namespace fusionps4::sce::gnm {

void GnmState::setPipeline(GnmPipelineId id) {
    std::lock_guard lock(m_mutex);
    m_pipeline = id;
}
GnmPipelineId GnmState::currentPipeline() const {
    std::lock_guard lock(m_mutex);
    return m_pipeline;
}

void GnmState::setRenderTarget(GnmRenderTargetId id) {
    std::lock_guard lock(m_mutex);
    m_rt = id;
}
GnmRenderTargetId GnmState::currentRenderTarget() const {
    std::lock_guard lock(m_mutex);
    return m_rt;
}

void GnmState::setVertexBuffer(GnmBufferId id, std::uint64_t offset) {
    std::lock_guard lock(m_mutex);
    m_vbuf = id;
    m_vbufOff = offset;
}
GnmBufferId GnmState::currentVertexBuffer() const {
    std::lock_guard lock(m_mutex);
    return m_vbuf;
}
std::uint64_t GnmState::currentVertexBufferOffset() const {
    std::lock_guard lock(m_mutex);
    return m_vbufOff;
}

void GnmState::setIndexBuffer(GnmBufferId id, std::uint64_t offset,
                              graphics::IndexType type) {
    std::lock_guard lock(m_mutex);
    m_ibuf = id;
    m_ibufOff = offset;
    m_indexType = type;
}
GnmBufferId GnmState::currentIndexBuffer() const {
    std::lock_guard lock(m_mutex);
    return m_ibuf;
}
std::uint64_t GnmState::currentIndexBufferOffset() const {
    std::lock_guard lock(m_mutex);
    return m_ibufOff;
}
graphics::IndexType GnmState::currentIndexType() const {
    std::lock_guard lock(m_mutex);
    return m_indexType;
}

void GnmState::setViewport(const graphics::Viewport& vp) {
    std::lock_guard lock(m_mutex);
    m_viewport = vp;
}
graphics::Viewport GnmState::currentViewport() const {
    std::lock_guard lock(m_mutex);
    return m_viewport;
}

void GnmState::setScissor(const graphics::Scissor& s) {
    std::lock_guard lock(m_mutex);
    m_scissor = s;
}
graphics::Scissor GnmState::currentScissor() const {
    std::lock_guard lock(m_mutex);
    return m_scissor;
}

void GnmState::setClearColor(const graphics::ClearColor& c) {
    std::lock_guard lock(m_mutex);
    m_clear = c;
}
graphics::ClearColor GnmState::currentClearColor() const {
    std::lock_guard lock(m_mutex);
    return m_clear;
}

void GnmState::reset() {
    std::lock_guard lock(m_mutex);
    m_pipeline   = kGnmInvalidId;
    m_rt         = kGnmInvalidId;
    m_vbuf       = kGnmInvalidId;
    m_vbufOff    = 0;
    m_ibuf       = kGnmInvalidId;
    m_ibufOff    = 0;
    m_indexType  = graphics::IndexType::Uint16;
    m_viewport   = {};
    m_scissor    = {};
    m_clear      = {0, 0, 0, 1};
}

} // namespace fusionps4::sce::gnm
