#pragma once

#include "graphics/abstraction/GraphicsTypes.hpp"
#include "sce/gnm/GnmTypes.hpp"

#include <mutex>

namespace fusionps4::sce::gnm {

// The GNM pipeline state that persists between calls. GNM tracks state on
// the *device* (indirectly, via the command buffer): each draw uses the
// state that was current when the draw was recorded. We mirror that here.
class GnmState {
public:
    GnmState() = default;

    // All setters are guarded; getters return by value to avoid exposing
    // mutable references across threads.
    void setPipeline(GnmPipelineId id);
    GnmPipelineId currentPipeline() const;

    void setRenderTarget(GnmRenderTargetId id);
    GnmRenderTargetId currentRenderTarget() const;

    void setVertexBuffer(GnmBufferId id, std::uint64_t offset);
    GnmBufferId currentVertexBuffer() const;
    std::uint64_t currentVertexBufferOffset() const;

    void setIndexBuffer(GnmBufferId id, std::uint64_t offset,
                        graphics::IndexType type);
    GnmBufferId currentIndexBuffer() const;
    std::uint64_t currentIndexBufferOffset() const;
    graphics::IndexType currentIndexType() const;

    void setViewport(const graphics::Viewport& vp);
    graphics::Viewport currentViewport() const;

    void setScissor(const graphics::Scissor& s);
    graphics::Scissor currentScissor() const;

    void setClearColor(const graphics::ClearColor& c);
    graphics::ClearColor currentClearColor() const;

    // Reset to defaults. Called at sceGnmInit and after a submit.
    void reset();

private:
    mutable std::mutex m_mutex;

    GnmPipelineId     m_pipeline = kGnmInvalidId;
    GnmRenderTargetId m_rt       = kGnmInvalidId;
    GnmBufferId       m_vbuf     = kGnmInvalidId;
    std::uint64_t     m_vbufOff  = 0;
    GnmBufferId       m_ibuf     = kGnmInvalidId;
    std::uint64_t     m_ibufOff  = 0;
    graphics::IndexType m_indexType = graphics::IndexType::Uint16;
    graphics::Viewport  m_viewport{};
    graphics::Scissor   m_scissor{};
    graphics::ClearColor m_clear{};
};

} // namespace fusionps4::sce::gnm
