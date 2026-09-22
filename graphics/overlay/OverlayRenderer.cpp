#include "graphics/overlay/OverlayRenderer.hpp"

#include "assets/CodecRegistry.hpp"
#include "debug/Log.hpp"
#include "graphics/abstraction/GraphicsDevice.hpp"

#if FUSIONPS4_HAVE_FREETYPE
#  include <ft2build.h>
#  include FT_FREETYPE_H
#endif

#include <algorithm>
#include <cstring>

using fusionps4::debug::LogCategory;
using namespace fusionps4::graphics;

// The overlay shaders are compiled at build time by CMake (glslangValidator
// or glslc) and embedded as byte arrays named fusionps4_overlay_vert_spv[]
// and fusionps4_overlay_frag_spv[] with corresponding _len constants. If
// the build did not find a shader compiler, the arrays are omitted and the
// renderer's shader creation is skipped with an UNIMPLEMENTED reason.

#if defined(FUSIONPS4_HAVE_OVERLAY_SPIRV) && FUSIONPS4_HAVE_OVERLAY_SPIRV
extern const unsigned char fusionps4_overlay_vert_spv[];
extern const unsigned int  fusionps4_overlay_vert_spv_len;
extern const unsigned char fusionps4_overlay_frag_spv[];
extern const unsigned int  fusionps4_overlay_frag_spv_len;
#endif

namespace fusionps4::graphics::overlay {

OverlayRenderer::OverlayRenderer() = default;

OverlayRenderer::~OverlayRenderer() {
    shutdown();
}

void OverlayRenderer::ensureFontLoaded() {
#if FUSIONPS4_HAVE_FREETYPE
    if (!m_glyphs.empty()) return;

    FT_Library ft = nullptr;
    if (FT_Init_FreeType(&ft) != 0) {
        m_unavailableReason = "FT_Init_FreeType failed";
        return;
    }

    // Load an embedded fallback font is out of scope; instead we try a
    // small set of well-known system fonts. If none is found, the overlay
    // stays unavailable, and this is reported at init.
    static const char* kCandidates[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
    };
    FT_Face face = nullptr;
    for (const auto* path : kCandidates) {
        if (FT_New_Face(ft, path, 0, &face) == 0) break;
    }
    if (!face) {
        FT_Done_FreeType(ft);
        m_unavailableReason = "no system font found for overlay atlas";
        return;
    }

    constexpr std::uint32_t kGlyphSize = 32;
    constexpr std::uint32_t kAtlasCols = 16;
    constexpr std::uint32_t kAtlasRows = 16;
    constexpr std::uint32_t kAtlasW = kGlyphSize * kAtlasCols;
    constexpr std::uint32_t kAtlasH = kGlyphSize * kAtlasRows;

    std::vector<std::uint8_t> atlas(kAtlasW * kAtlasH, 0);
    m_glyphs.resize(95);   // ASCII 32..126

    FT_Set_Pixel_Sizes(face, 0, kGlyphSize - 4);

    for (int cp = 32; cp <= 126; ++cp) {
        if (FT_Load_Char(face, cp, FT_LOAD_RENDER) != 0) continue;
        const auto& bmp = face->glyph->bitmap;
        const auto gx = (cp - 32) % kAtlasCols;
        const auto gy = (cp - 32) / kAtlasCols;
        const auto x0 = gx * kGlyphSize;
        const auto y0 = gy * kGlyphSize;

        for (std::uint32_t y = 0; y < bmp.rows && y < kGlyphSize - 4; ++y) {
            for (std::uint32_t x = 0; x < bmp.width && x < kGlyphSize; ++x) {
                const auto v = bmp.buffer[y * bmp.pitch + x];
                atlas[(y0 + 2 + y) * kAtlasW + (x0 + x)] = v;
            }
        }

        Glyph g;
        g.u0 = static_cast<float>(x0) / kAtlasW;
        g.v0 = static_cast<float>(y0) / kAtlasH;
        g.u1 = static_cast<float>(x0 + kGlyphSize) / kAtlasW;
        g.v1 = static_cast<float>(y0 + kGlyphSize) / kAtlasH;
        g.advance = static_cast<float>(face->glyph->advance.x >> 6);
        m_glyphs[cp - 32] = g;
    }

    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    // Expand the single-channel atlas into RGBA with the channel stored in
    // alpha and RGB set to white, so the shader can tint per vertex.
    std::vector<std::uint8_t> rgba(kAtlasW * kAtlasH * 4, 0);
    for (std::size_t i = 0; i < atlas.size(); ++i) {
        rgba[i * 4 + 0] = 255;
        rgba[i * 4 + 1] = 255;
        rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = atlas[i];
    }

    ImageDesc desc;
    desc.width  = kAtlasW;
    desc.height = kAtlasH;
    desc.depth  = 1;
    desc.mips   = 1;
    desc.layers = 1;
    desc.format = Format::R8G8B8A8_Unorm;
    desc.usage  = ResourceUsage::Sampled | ResourceUsage::Transfer;
    m_fontImage = m_device->createImage(desc);
    if (m_fontImage) {
        m_fontView = m_device->createImageView(m_fontImage);
        SamplerDesc sd;
        sd.nearest = true;
        m_fontSampler = m_device->createSampler(sd);
        m_device->uploadImage(m_fontImage, std::as_bytes(std::span(rgba)));
    }

    m_baseSize = static_cast<float>(kGlyphSize);
    FP4_INFO(LogCategory::Graphics)
        << "Overlay font atlas: " << kAtlasW << "x" << kAtlasH
        << " (" << m_glyphs.size() << " glyphs)";
#endif
}

bool OverlayRenderer::initialize(GraphicsDevice* device) {
    m_device = device;
    if (!m_device) {
        m_unavailableReason = "no GraphicsDevice";
        return true;
    }

#if !defined(FUSIONPS4_HAVE_OVERLAY_SPIRV) || !FUSIONPS4_HAVE_OVERLAY_SPIRV
    m_unavailableReason =
        "overlay shaders not compiled (glslangValidator not found at build)";
    FP4_UNIMPLEMENTED(LogCategory::Graphics, "OverlayRenderer::initialize");
    FP4_ERROR(LogCategory::Graphics)
        << "  reason=" << m_unavailableReason
        << "; dialogs will complete logically but will not render.";
    return true;   // not fatal: dialogs still function
#else
    if (!assets::CodecRegistry::instance().hasFreetype) {
        m_unavailableReason = "freetype not linked into this build";
        FP4_UNIMPLEMENTED(LogCategory::Graphics, "OverlayRenderer::initialize");
        FP4_ERROR(LogCategory::Graphics)
            << "  reason=" << m_unavailableReason;
        return true;
    }

    // Build the two shader modules.
    const std::span<const std::byte> vsBytes(
        reinterpret_cast<const std::byte*>(fusionps4_overlay_vert_spv),
        fusionps4_overlay_vert_spv_len);
    const std::span<const std::byte> fsBytes(
        reinterpret_cast<const std::byte*>(fusionps4_overlay_frag_spv),
        fusionps4_overlay_frag_spv_len);
    m_vs = m_device->createShader(ShaderStage::Vertex,   vsBytes, "main");
    m_fs = m_device->createShader(ShaderStage::Fragment, fsBytes, "main");
    if (!m_vs || !m_fs) {
        m_unavailableReason = "shader module creation failed";
        return true;
    }

    m_pipeline = m_device->createOverlayPipeline(m_vs, m_fs);
    if (!m_pipeline) {
        m_unavailableReason = "overlay pipeline creation failed";
        return true;
    }

    ensureFontLoaded();
    if (m_glyphs.empty()) {
        m_unavailableReason = "font atlas unavailable";
        return true;
    }

    m_available = true;
    FP4_INFO(LogCategory::Graphics) << "OverlayRenderer initialized";
    return true;
#endif
}

void OverlayRenderer::shutdown() {
    if (m_device) {
        if (m_pipeline)     m_device->destroyPipeline(m_pipeline);
        if (m_vs)           m_device->destroyShader(m_vs);
        if (m_fs)           m_device->destroyShader(m_fs);
        if (m_fontView)     m_device->destroyImageView(m_fontView);
        if (m_fontImage)    m_device->destroyImage(m_fontImage);
        if (m_fontSampler)  m_device->destroySampler(m_fontSampler);
        if (m_vertexBuffer) m_device->destroyBuffer(m_vertexBuffer);
    }
    m_pipeline = m_vs = m_fs = 0;
    m_fontView = m_fontImage = m_fontSampler = 0;
    m_vertexBuffer = 0;
    m_vertices.clear();
    m_glyphs.clear();
    m_available = false;
    m_device = nullptr;
}

void OverlayRenderer::beginFrame(std::uint32_t width, std::uint32_t height) {
    m_width  = width;
    m_height = height;
    m_vertices.clear();
}

void OverlayRenderer::ensureBufferCapacity(std::size_t vertices) {
    if (m_capacityVertices >= vertices) return;
    if (m_vertexBuffer) m_device->destroyBuffer(m_vertexBuffer);
    BufferDesc bd;
    bd.size  = vertices * sizeof(Vertex);
    bd.usage = ResourceUsage::Vertex | ResourceUsage::Transfer;
    m_vertexBuffer = m_device->createBuffer(bd);
    m_capacityVertices = vertices;
}

void OverlayRenderer::drawRect(float x, float y, float w, float h,
                               std::uint32_t rgba) {
    if (!m_available) return;
    const float r = ((rgba >>  0) & 0xFF) / 255.0f;
    const float g = ((rgba >>  8) & 0xFF) / 255.0f;
    const float b = ((rgba >> 16) & 0xFF) / 255.0f;
    const float a = ((rgba >> 24) & 0xFF) / 255.0f;

    // UV points to a solid white pixel in the atlas (top-left cell).
    const float u0 = 0.5f / m_baseSize;
    const float v0 = 0.5f / m_baseSize;

    Vertex v0v{ x,     y,     u0, v0, r, g, b, a };
    Vertex v1v{ x + w, y,     u0, v0, r, g, b, a };
    Vertex v2v{ x + w, y + h, u0, v0, r, g, b, a };
    Vertex v3v{ x,     y + h, u0, v0, r, g, b, a };
    m_vertices.push_back(v0v); m_vertices.push_back(v1v); m_vertices.push_back(v2v);
    m_vertices.push_back(v0v); m_vertices.push_back(v2v); m_vertices.push_back(v3v);
}

void OverlayRenderer::drawText(float x, float y, const std::string& text,
                               std::uint32_t rgba, float scale) {
    if (!m_available || m_glyphs.empty()) return;
    const float r = ((rgba >>  0) & 0xFF) / 255.0f;
    const float g = ((rgba >>  8) & 0xFF) / 255.0f;
    const float b = ((rgba >> 16) & 0xFF) / 255.0f;
    const float a = ((rgba >> 24) & 0xFF) / 255.0f;

    float cursorX = x;
    const float cell = m_baseSize * scale;
    for (unsigned char c : text) {
        if (c == '\n') { continue; }   // caller splits lines
        if (c < 32 || c > 126) c = '?';
        const auto& gl = m_glyphs[c - 32];
        const float x0 = cursorX;
        const float y0 = y;
        const float x1 = cursorX + cell;
        const float y1 = y + cell;

        Vertex v0{ x0, y0, gl.u0, gl.v0, r, g, b, a };
        Vertex v1{ x1, y0, gl.u1, gl.v0, r, g, b, a };
        Vertex v2{ x1, y1, gl.u1, gl.v1, r, g, b, a };
        Vertex v3{ x0, y1, gl.u0, gl.v1, r, g, b, a };
        m_vertices.push_back(v0); m_vertices.push_back(v1); m_vertices.push_back(v2);
        m_vertices.push_back(v0); m_vertices.push_back(v2); m_vertices.push_back(v3);

        cursorX += gl.advance * scale;
    }
}

float OverlayRenderer::textWidth(const std::string& text, float scale) const {
    if (m_glyphs.empty()) return 0.0f;
    float w = 0.0f;
    for (unsigned char c : text) {
        if (c < 32 || c > 126) c = '?';
        w += m_glyphs[c - 32].advance * scale;
    }
    return w;
}

float OverlayRenderer::lineHeight(float scale) const {
    return m_baseSize * scale * 1.2f;
}

void OverlayRenderer::endFrame() {
    if (!m_available || m_vertices.empty()) return;

    ensureBufferCapacity(m_vertices.size());
    if (!m_vertexBuffer) return;

    m_device->uploadBuffer(
        m_vertexBuffer, 0,
        std::as_bytes(std::span(m_vertices.data(), m_vertices.size())));

    VertexBufferBinding binding;
    binding.buffer  = m_vertexBuffer;
    binding.offset  = 0;
    binding.stride  = sizeof(Vertex);
    binding.binding = 0;

    Viewport vp;
    vp.x = 0; vp.y = 0;
    vp.w = static_cast<float>(m_width);
    vp.h = static_cast<float>(m_height);
    vp.minDepth = 0; vp.maxDepth = 1;
    m_device->cmdSetViewport(vp);

    Scissor sc;
    sc.x = 0; sc.y = 0;
    sc.w = m_width; sc.h = m_height;
    m_device->cmdSetScissor(sc);

    m_device->cmdBindPipeline(m_pipeline);
    m_device->cmdBindVertexBuffer(binding);
    m_device->cmdBindTexture(m_fontView, m_fontSampler, 0);
    m_device->cmdPushConstant(&m_width, 0, sizeof(std::uint32_t));
    m_device->cmdPushConstant(&m_height, sizeof(std::uint32_t), sizeof(std::uint32_t));
    m_device->cmdDraw(static_cast<std::uint32_t>(m_vertices.size()), 1, 0, 0);
    m_vertices.clear();
}

} // namespace fusionps4::graphics::overlay
