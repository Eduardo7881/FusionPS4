#pragma once

#include "graphics/abstraction/GraphicsTypes.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace fusionps4::graphics {
class GraphicsDevice;
}

namespace fusionps4::graphics::overlay {

// A minimal 2D overlay used to draw dialogs on the HostWindow. It owns:
//   * a font atlas (rendered once via FreeType, uploaded as an RGBA texture),
//   * a dynamic vertex buffer,
//   * an overlay pipeline (built from a precompiled SPIR-V VS/FS pair).
//
// The renderer is deliberately not exposed to the guest: it is an internal
// tool of the runtime.
//
// When SPIR-V shaders or FreeType are unavailable at build time, the
// renderer reports UNIMPLEMENTED and renders nothing. Dialogs still
// complete logically so the guest does not deadlock; they are simply not
// visible.
class OverlayRenderer {
public:
    OverlayRenderer();
    ~OverlayRenderer();

    OverlayRenderer(const OverlayRenderer&) = delete;
    OverlayRenderer& operator=(const OverlayRenderer&) = delete;

    bool initialize(GraphicsDevice* device);
    void shutdown();

    void beginFrame(std::uint32_t width, std::uint32_t height);

    // Rectangles (in pixels, origin top-left).
    void drawRect(float x, float y, float w, float h, std::uint32_t rgba);

    // Text using the internal font atlas. Line breaks are not automatic;
    // the caller splits. `scale` is a multiplier on the base font size.
    void drawText(float x, float y, const std::string& text,
                  std::uint32_t rgba, float scale = 1.0f);

    // Text metrics so callers can centre or wrap.
    float textWidth(const std::string& text, float scale) const;
    float lineHeight(float scale) const;

    // Push all queued quads to the GPU.
    void endFrame();

private:
    struct Vertex {
        float x, y;
        float u, v;
        float r, g, b, a;
    };

    void ensureBufferCapacity(std::size_t vertices);
    void ensureFontLoaded();

    GraphicsDevice*           m_device      = nullptr;
    BufferHandle              m_vertexBuffer = 0;
    PipelineHandle            m_pipeline     = 0;
    ShaderHandle              m_vs           = 0;
    ShaderHandle              m_fs           = 0;
    ImageHandle               m_fontImage    = 0;
    ImageViewHandle           m_fontView     = 0;
    SamplerHandle             m_fontSampler  = 0;

    std::uint32_t             m_width  = 0;
    std::uint32_t             m_height = 0;
    std::size_t               m_capacityVertices = 0;

    std::vector<Vertex>       m_vertices;

    // Font atlas: 16x16 grid of 32x32 cells for the printable ASCII
    // range plus a handful of control glyphs.
    struct Glyph {
        float u0, v0, u1, v1;
        float advance;
    };
    std::vector<Glyph>        m_glyphs;   // indexed by codepoint - 32
    float                     m_baseSize   = 32.0f;

    bool                      m_available   = false;
    std::string               m_unavailableReason;
};

} // namespace fusionps4::graphics::overlay
