#pragma once

#include <cstdint>

namespace fusionps4::graphics {

enum class Backend {
    Vulkan = 0,
    OpenGL,
};

// Pixel formats the GAL exposes to the guest. Guest-visible values (SCE)
// are translated to these in sceVideoOut / sceGnm; backends then translate
// these to their native formats.
enum class Format {
    Unknown = 0,
    R8G8B8A8_Unorm,
    B8G8R8A8_Unorm,
    R8G8B8A8_Srgb,
    B8G8R8A8_Srgb,
    R16G16B16A16_Sfloat,
    R32G32B32A32_Sfloat,
    D32_Sfloat,
    D24_Unorm_S8_Uint,
};

enum class ResourceUsage : std::uint32_t {
    None     = 0,
    Sampled  = 1u << 0,
    Storage  = 1u << 1,
    Color    = 1u << 2,
    Depth    = 1u << 3,
    Vertex   = 1u << 4,
    Index    = 1u << 5,
    Uniform  = 1u << 6,
    Transfer = 1u << 7,
};

inline ResourceUsage operator|(ResourceUsage a, ResourceUsage b) {
    return static_cast<ResourceUsage>(static_cast<std::uint32_t>(a) |
                                      static_cast<std::uint32_t>(b));
}
inline bool hasUsage(ResourceUsage v, ResourceUsage f) {
    return (static_cast<std::uint32_t>(v) &
            static_cast<std::uint32_t>(f)) != 0u;
}

enum class ShaderStage {
    Vertex,
    Fragment,
    Compute,
};

// Opaque handles. Zero is reserved as "invalid".
using BufferHandle    = std::uint64_t;
using ImageHandle     = std::uint64_t;
using ImageViewHandle = std::uint64_t;
using SamplerHandle   = std::uint64_t;
using PipelineHandle  = std::uint64_t;
using ShaderHandle    = std::uint64_t;
using CmdBufferHandle = std::uint64_t;
using FenceHandle     = std::uint64_t;
using SemaphoreHandle = std::uint64_t;

struct BufferDesc {
    std::uint64_t size = 0;
    ResourceUsage usage = ResourceUsage::None;
    const char*   debugName = nullptr;
};

struct ImageDesc {
    std::uint32_t width  = 0;
    std::uint32_t height = 0;
    std::uint32_t depth  = 1;
    std::uint32_t mips   = 1;
    std::uint32_t layers = 1;
    Format        format = Format::Unknown;
    ResourceUsage usage  = ResourceUsage::None;
    const char*   debugName = nullptr;
};

struct SamplerDesc {
    bool  nearest  = false;
    bool  repeat   = false;
    float maxAniso = 1.0f;
};

struct SwapchainDesc {
    std::uint32_t width  = 0;
    std::uint32_t height = 0;
    bool          vsync  = true;
};

struct Viewport {
    float x = 0, y = 0, w = 0, h = 0;
    float minDepth = 0.0f, maxDepth = 1.0f;
};

struct Scissor {
    std::int32_t x = 0, y = 0;
    std::uint32_t w = 0, h = 0;
};

struct ClearColor {
    float r = 0, g = 0, b = 0, a = 1;
};

// A single backend-agnostic view of the swapchain image we are about to
// render into. Valid between beginFrame() and presentFrame().
struct FrameContext {
    ImageViewHandle imageView = 0;
    std::uint32_t   width     = 0;
    std::uint32_t   height    = 0;
    std::uint32_t   imageIndex = 0;
    Format          format    = Format::Unknown;
};

} // namespace fusionps4::graphics
