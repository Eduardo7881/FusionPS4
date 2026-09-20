#pragma once

#include "graphics/abstraction/GraphicsDevice.hpp"

#include <memory>
#include <mutex>

struct SDL_Window;

namespace fusionps4::graphics::opengl {

class OpenGLDevice final : public GraphicsDevice {
public:
    OpenGLDevice();
    ~OpenGLDevice() override;

    Backend backend() const override { return Backend::OpenGL; }
    const char* backendName() const override { return "OpenGL"; }

    bool initialize(HostSurfaceFactory& surfaceFactory) override;
    void shutdown() override;

    bool createSwapchain(const SwapchainDesc& desc) override;
    void destroySwapchain() override;
    void recreateSwapchain(std::uint32_t width, std::uint32_t height) override;

    bool beginFrame(FrameContext& outFrame) override;
    bool presentFrame() override;

    BufferHandle    createBuffer(const BufferDesc& desc) override;
    void            destroyBuffer(BufferHandle h) override;
    ImageHandle     createImage(const ImageDesc& desc) override;
    void            destroyImage(ImageHandle h) override;
    ImageViewHandle createImageView(ImageHandle image) override;
    void            destroyImageView(ImageViewHandle h) override;
    SamplerHandle   createSampler(const SamplerDesc& desc) override;
    void            destroySampler(SamplerHandle h) override;

    bool uploadBuffer(BufferHandle h,
                      std::uint64_t  offset,
                      std::span<const std::byte> data) override;
    bool uploadImage(ImageHandle image,
                     std::span<const std::byte> data) override;

    ShaderHandle   createShader(ShaderStage stage,
                                std::span<const std::byte> spirv,
                                std::string_view entry) override;
    void           destroyShader(ShaderHandle h) override;

    PipelineHandle createGraphicsPipeline(ShaderHandle vs,
                                          ShaderHandle fs) override;
    void           destroyPipeline(PipelineHandle h) override;

    void clearCurrentFrame(const ClearColor& c) override;

    FenceHandle     createFence(bool signaled) override;
    void            destroyFence(FenceHandle h) override;
    bool            waitFence(FenceHandle h, std::uint64_t timeoutNs) override;
    void            resetFence(FenceHandle h) override;

    SemaphoreHandle createSemaphore() override;
    void            destroySemaphore(SemaphoreHandle h) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    mutable std::mutex    m_mutex;
};

} // namespace fusionps4::graphics::opengl
