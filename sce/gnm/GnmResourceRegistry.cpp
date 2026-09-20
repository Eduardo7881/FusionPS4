#include "sce/gnm/GnmResourceRegistry.hpp"

#include "debug/Log.hpp"
#include "graphics/abstraction/GraphicsDevice.hpp"

using fusionps4::debug::LogCategory;
namespace gfx = fusionps4::graphics;

namespace fusionps4::sce::gnm {

GnmResourceRegistry::GnmResourceRegistry() = default;
GnmResourceRegistry::~GnmResourceRegistry() { clear(); }

void GnmResourceRegistry::bindDevice(gfx::GraphicsDevice* dev) {
    std::lock_guard lock(m_mutex);
    m_device = dev;
}

GnmBufferId GnmResourceRegistry::registerBuffer(std::uint64_t size,
                                                gfx::ResourceUsage usage,
                                                std::uint64_t guestAddr,
                                                const char* debugName) {
    std::lock_guard lock(m_mutex);
    if (!m_device) {
        FP4_ERROR(LogCategory::Sce)
            << "GnmResourceRegistry::registerBuffer before bindDevice";
        return kGnmInvalidId;
    }
    gfx::BufferDesc d;
    d.size      = size;
    d.usage     = usage;
    d.debugName = debugName;
    const auto gal = m_device->createBuffer(d);
    if (!gal) {
        FP4_ERROR(LogCategory::Sce)
            << "GAL createBuffer failed for \"" << (debugName ? debugName : "")
            << "\" size=" << size;
        return kGnmInvalidId;
    }
    const auto id = m_nextId++;
    m_buffers[id] = { gal, size, guestAddr };
    FP4_DEBUG(LogCategory::Sce)
        << "GNM buffer id=" << id << " -> GAL " << gal
        << " size=" << size << " guest="
        << reinterpret_cast<void*>(std::uintptr_t(guestAddr));
    return id;
}

const GnmResourceRegistry::BufferEntry*
GnmResourceRegistry::buffer(GnmBufferId id) const {
    std::lock_guard lock(m_mutex);
    auto it = m_buffers.find(id);
    return it == m_buffers.end() ? nullptr : &it->second;
}

bool GnmResourceRegistry::destroyBuffer(GnmBufferId id) {
    std::lock_guard lock(m_mutex);
    auto it = m_buffers.find(id);
    if (it == m_buffers.end()) return false;
    if (m_device) m_device->destroyBuffer(it->second.gal);
    m_buffers.erase(it);
    return true;
}

GnmTextureId GnmResourceRegistry::registerTexture(std::uint32_t width,
                                                  std::uint32_t height,
                                                  gfx::Format format,
                                                  gfx::ResourceUsage usage,
                                                  const char* debugName) {
    std::lock_guard lock(m_mutex);
    if (!m_device) return kGnmInvalidId;
    gfx::ImageDesc d;
    d.width     = width;
    d.height    = height;
    d.depth     = 1;
    d.mips      = 1;
    d.layers    = 1;
    d.format    = format;
    d.usage     = usage;
    d.debugName = debugName;

    const auto img = m_device->createImage(d);
    if (!img) return kGnmInvalidId;
    const auto v = m_device->createImageView(img);
    if (!v) {
        m_device->destroyImage(img);
        return kGnmInvalidId;
    }
    const auto id = m_nextId++;
    m_textures[id] = { img, v, width, height, format };
    return id;
}

const GnmResourceRegistry::TextureEntry*
GnmResourceRegistry::texture(GnmTextureId id) const {
    std::lock_guard lock(m_mutex);
    auto it = m_textures.find(id);
    return it == m_textures.end() ? nullptr : &it->second;
}

bool GnmResourceRegistry::destroyTexture(GnmTextureId id) {
    std::lock_guard lock(m_mutex);
    auto it = m_textures.find(id);
    if (it == m_textures.end()) return false;
    if (m_device) {
        if (it->second.view)  m_device->destroyImageView(it->second.view);
        if (it->second.image) m_device->destroyImage(it->second.image);
    }
    m_textures.erase(it);
    return true;
}

GnmRenderTargetId GnmResourceRegistry::registerRenderTarget(
    std::uint32_t width, std::uint32_t height, gfx::Format format,
    const char* debugName) {
    std::lock_guard lock(m_mutex);
    if (!m_device) return kGnmInvalidId;
    gfx::ImageDesc d;
    d.width     = width;
    d.height    = height;
    d.depth     = 1;
    d.mips      = 1;
    d.layers    = 1;
    d.format    = format;
    d.usage     = gfx::ResourceUsage::Color | gfx::ResourceUsage::Transfer;
    d.debugName = debugName;

    const auto img = m_device->createImage(d);
    if (!img) return kGnmInvalidId;
    const auto v = m_device->createImageView(img);
    if (!v) { m_device->destroyImage(img); return kGnmInvalidId; }

    const auto id = m_nextId++;
    m_renderTargets[id] = { img, v, width, height, format };
    return id;
}

const GnmResourceRegistry::RenderTargetEntry*
GnmResourceRegistry::renderTarget(GnmRenderTargetId id) const {
    std::lock_guard lock(m_mutex);
    auto it = m_renderTargets.find(id);
    return it == m_renderTargets.end() ? nullptr : &it->second;
}

bool GnmResourceRegistry::destroyRenderTarget(GnmRenderTargetId id) {
    std::lock_guard lock(m_mutex);
    auto it = m_renderTargets.find(id);
    if (it == m_renderTargets.end()) return false;
    if (m_device) {
        if (it->second.view)  m_device->destroyImageView(it->second.view);
        if (it->second.image) m_device->destroyImage(it->second.image);
    }
    m_renderTargets.erase(it);
    return true;
}

GnmVsShaderId GnmResourceRegistry::registerVsShader(gfx::ShaderHandle gal) {
    std::lock_guard lock(m_mutex);
    const auto id = m_nextId++;
    m_vs[id] = gal;
    return id;
}

GnmPsShaderId GnmResourceRegistry::registerPsShader(gfx::ShaderHandle gal) {
    std::lock_guard lock(m_mutex);
    const auto id = m_nextId++;
    m_ps[id] = gal;
    return id;
}

GnmPipelineId GnmResourceRegistry::registerPipeline(gfx::PipelineHandle gal) {
    std::lock_guard lock(m_mutex);
    const auto id = m_nextId++;
    m_pipelines[id] = gal;
    return id;
}

gfx::ShaderHandle GnmResourceRegistry::vsShader(GnmVsShaderId id) const {
    std::lock_guard lock(m_mutex);
    auto it = m_vs.find(id);
    return it == m_vs.end() ? 0 : it->second;
}

gfx::ShaderHandle GnmResourceRegistry::psShader(GnmPsShaderId id) const {
    std::lock_guard lock(m_mutex);
    auto it = m_ps.find(id);
    return it == m_ps.end() ? 0 : it->second;
}

gfx::PipelineHandle GnmResourceRegistry::pipeline(GnmPipelineId id) const {
    std::lock_guard lock(m_mutex);
    auto it = m_pipelines.find(id);
    return it == m_pipelines.end() ? 0 : it->second;
}

void GnmResourceRegistry::clear() {
    std::lock_guard lock(m_mutex);
    if (m_device) {
        for (auto& [_, e] : m_buffers)       m_device->destroyBuffer(e.gal);
        for (auto& [_, e] : m_textures) {
            if (e.view)  m_device->destroyImageView(e.view);
            if (e.image) m_device->destroyImage(e.image);
        }
        for (auto& [_, e] : m_renderTargets) {
            if (e.view)  m_device->destroyImageView(e.view);
            if (e.image) m_device->destroyImage(e.image);
        }
        for (auto& [_, h] : m_vs)        m_device->destroyShader(h);
        for (auto& [_, h] : m_ps)        m_device->destroyShader(h);
        for (auto& [_, h] : m_pipelines) m_device->destroyPipeline(h);
    }
    m_buffers.clear();
    m_textures.clear();
    m_renderTargets.clear();
    m_vs.clear();
    m_ps.clear();
    m_pipelines.clear();
}

std::size_t GnmResourceRegistry::totalResources() const {
    std::lock_guard lock(m_mutex);
    return m_buffers.size() + m_textures.size() + m_renderTargets.size() +
           m_vs.size() + m_ps.size() + m_pipelines.size();
}

} // namespace fusionps4::sce::gnm
