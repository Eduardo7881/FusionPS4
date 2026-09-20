#pragma once

#include "graphics/abstraction/GraphicsTypes.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fusionps4::graphics {

// Backend-agnostic GPU device. Implementations must be safe to call from
// the runtime's main thread only; submission of guest work happens on the
// main thread by construction (see Runtime::tick).
class GraphicsDevice {
public:
    virtual ~GraphicsDevice() = default;

    virtual Backend backend() const = 0;
    virtual const char* backendName() const = 0;

    // ---- initialization -------------------------------------------------
    // `surfaceFactory` returns a backend-specific native surface descriptor
    // (VkSurfaceKHR / GL context setup). It is invoked during initialize()
    // with the backend given a chance to consume it.
    virtual bool initialize(struct HostSurfaceFactory& surfaceFactory) = 0;
    virtual void shutdown() = 0;

    // ---- swapchain ------------------------------------------------------
    virtual bool createSwapchain(const SwapchainDesc& desc) = 0;
    virtual void destroySwapchain() = 0;
    virtual void recreateSwapchain(std::uint32_t width,
                                   std::uint32_t height) = 0;

    // Begin a new frame: acquire the next image, reset the frame's fence,
    // and prepare the command buffer. Returns false if the swapchain is
    // out of date and must be recreated.
    virtual bool beginFrame(FrameContext& outFrame) = 0;

    // Present the image rendered during the current frame.
    virtual bool presentFrame() = 0;

    // ---- resources ------------------------------------------------------
    virtual BufferHandle    createBuffer(const BufferDesc& desc) = 0;
    virtual void            destroyBuffer(BufferHandle h) = 0;
    virtual ImageHandle     createImage(const ImageDesc& desc) = 0;
    virtual void            destroyImage(ImageHandle h) = 0;
    virtual ImageViewHandle createImageView(ImageHandle image) = 0;
    virtual void            destroyImageView(ImageViewHandle h) = 0;
    virtual SamplerHandle   createSampler(const SamplerDesc& desc) = 0;
    virtual void            destroySampler(SamplerHandle h) = 0;

    // Uploads are synchronous for correctness; they flush the GPU queue
    // before returning. The guest-visible sceGnm path uses this for
    // resource initialization and is fine with the cost.
    virtual bool            uploadBuffer(BufferHandle h,
                                         std::uint64_t  offset,
                                         std::span<const std::byte> data) = 0;
    virtual bool            uploadImage(ImageHandle image,
                                        std::span<const std::byte> data) = 0;

    // ---- pipelines ------------------------------------------------------
    virtual ShaderHandle   createShader(ShaderStage        stage,
                                        std::span<const std::byte> spirv,
                                        std::string_view   entry) = 0;
    virtual void           destroyShader(ShaderHandle h) = 0;

    virtual PipelineHandle createGraphicsPipeline(ShaderHandle vs,
                                                  ShaderHandle fs) = 0;
    virtual void           destroyPipeline(PipelineHandle h) = 0;

    // ---- rendering ------------------------------------------------------
    // Guest-visible "flip" translates to: begin rendering into the current
    // swapchain image with a clear color, then present on the next frame
    // boundary. Real geometry submission (sceGnm) will call the deeper
    // methods below once they are populated in Phase 6.
    virtual void  clearCurrentFrame(const ClearColor& c) = 0;

    // ---- synchronization ------------------------------------------------
    virtual FenceHandle     createFence(bool signaled) = 0;
    virtual void            destroyFence(FenceHandle h) = 0;
    virtual bool            waitFence(FenceHandle h, std::uint64_t timeoutNs) = 0;
    virtual void            resetFence(FenceHandle h) = 0;

    virtual SemaphoreHandle createSemaphore() = 0;
    virtual void            destroySemaphore(SemaphoreHandle h) = 0;
};

// Small interface used to hand the SDL window's surface-creation ability
// back to the backend without exposing SDL in the GAL headers.
struct HostSurfaceFactory {
    virtual ~HostSurfaceFactory() = default;

    // Vulkan path: `instance` is a VkInstance as void*, outSurface is a
    // VkSurfaceKHR as void*.
    virtual bool createVulkanSurface(void* instance, void*& outSurface) = 0;

    // Required Vulkan instance extensions (e.g. VK_KHR_surface,
    // VK_KHR_xlib_surface). Filled before instance creation.
    virtual std::vector<const char*> requiredVulkanExtensions() const = 0;

    // OpenGL path: initializes the window's GL context attributes; the
    // backend then creates its own context with SDL_GL_CreateContext
    // through its own copy of the SDL window pointer. For the purposes of
    // the GAL, only the Vulkan surface helper is mandatory.
    virtual void* nativeWindowHandle() = 0;
};

} // namespace fusionps4::graphics
