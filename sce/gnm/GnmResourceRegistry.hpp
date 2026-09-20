#pragma once

#include "graphics/abstraction/GraphicsTypes.hpp"
#include "sce/gnm/GnmTypes.hpp"

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

namespace fusionps4::sce::gnm {

// Process-local table that maps guest GNM IDs to GAL handles. Every GNM ID
// is allocated by the registry; the guest never sees the underlying GAL
// handle. When the process shuts down the registry destroys every GAL
// resource it created.
class GnmResourceRegistry {
public:
    GnmResourceRegistry();
    ~GnmResourceRegistry();

    GnmResourceRegistry(const GnmResourceRegistry&) = delete;
    GnmResourceRegistry& operator=(const GnmResourceRegistry&) = delete;

    void bindDevice(graphics::GraphicsDevice* dev);

    // ---- buffers --------------------------------------------------------
    struct BufferEntry {
        graphics::BufferHandle gal    = 0;
        std::uint64_t          size   = 0;
        std::uint64_t          guestAddr = 0;
    };
    GnmBufferId registerBuffer(std::uint64_t size,
                               graphics::ResourceUsage usage,
                               std::uint64_t guestAddr,
                               const char* debugName);
    const BufferEntry* buffer(GnmBufferId id) const;
    bool destroyBuffer(GnmBufferId id);

    // ---- textures -------------------------------------------------------
    struct TextureEntry {
        graphics::ImageHandle     image     = 0;
        graphics::ImageViewHandle view      = 0;
        std::uint32_t             width     = 0;
        std::uint32_t             height    = 0;
        graphics::Format          format    = graphics::Format::Unknown;
    };
    GnmTextureId registerTexture(std::uint32_t width,
                                 std::uint32_t height,
                                 graphics::Format format,
                                 graphics::ResourceUsage usage,
                                 const char* debugName);
    const TextureEntry* texture(GnmTextureId id) const;
    bool destroyTexture(GnmTextureId id);

    // ---- render targets -------------------------------------------------
    // A render target is an image + view usable as a color attachment. It
    // is deliberately distinct from a texture because its usage flags and
    // lifetime differ.
    struct RenderTargetEntry {
        graphics::ImageHandle     image  = 0;
        graphics::ImageViewHandle view   = 0;
        std::uint32_t             width  = 0;
        std::uint32_t             height = 0;
        graphics::Format          format = graphics::Format::Unknown;
    };
    GnmRenderTargetId registerRenderTarget(std::uint32_t width,
                                           std::uint32_t height,
                                           graphics::Format format,
                                           const char* debugName);
    const RenderTargetEntry* renderTarget(GnmRenderTargetId id) const;
    bool destroyRenderTarget(GnmRenderTargetId id);

    // ---- shaders / pipelines -------------------------------------------
    GnmVsShaderId registerVsShader(graphics::ShaderHandle gal);
    GnmPsShaderId registerPsShader(graphics::ShaderHandle gal);
    GnmPipelineId registerPipeline(graphics::PipelineHandle gal);

    graphics::ShaderHandle   vsShader(GnmVsShaderId id) const;
    graphics::ShaderHandle   psShader(GnmPsShaderId id) const;
    graphics::PipelineHandle pipeline(GnmPipelineId id) const;

    // Destroy everything. Called during process shutdown.
    void clear();

    std::size_t totalResources() const;

private:
    mutable std::mutex m_mutex;
    graphics::GraphicsDevice* m_device = nullptr;

    std::uint32_t m_nextId = 1;
    std::unordered_map<GnmBufferId,       BufferEntry>       m_buffers;
    std::unordered_map<GnmTextureId,      TextureEntry>      m_textures;
    std::unordered_map<GnmRenderTargetId, RenderTargetEntry> m_renderTargets;
    std::unordered_map<GnmVsShaderId,     graphics::ShaderHandle>   m_vs;
    std::unordered_map<GnmPsShaderId,     graphics::ShaderHandle>   m_ps;
    std::unordered_map<GnmPipelineId,     graphics::PipelineHandle> m_pipelines;
};

} // namespace fusionps4::sce::gnm
