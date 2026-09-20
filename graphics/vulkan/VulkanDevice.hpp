#pragma once

#include "graphics/abstraction/GraphicsDevice.hpp"

#include <array>
#include <mutex>
#include <unordered_map>
#include <vector>

// Forward declarations. We do not include <vulkan/vulkan.h> here to keep
// the GAL surface independent of the Vulkan loader headers; the .cpp file
// includes them.
struct VkInstance_T;
struct VkPhysicalDevice_T;
struct VkDevice_T;
struct VkQueue_T;
struct VkSurfaceKHR_T;
struct VkSwapchainKHR_T;
struct VkCommandPool_T;
struct VkCommandBuffer_T;
struct VkFence_T;
struct VkSemaphore_T;
struct VkBuffer_T;
struct VkDeviceMemory_T;
struct VkImage_T;
struct VkImageView_T;
struct VkSampler_T;
struct VkShaderModule_T;
struct VkPipeline_T;
struct VkPipelineLayout_T;

namespace fusionps4::graphics::vulkan {

class VulkanDevice final : public GraphicsDevice {
public:
    VulkanDevice();
    ~VulkanDevice() override;

    Backend backend() const override { return Backend::Vulkan; }
    const char* backendName() const override { return "Vulkan"; }

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
    // Backend-specific internal structures.
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    mutable std::mutex m_mutex;
};

} // namespace fusionps4::graphics::vulkan
