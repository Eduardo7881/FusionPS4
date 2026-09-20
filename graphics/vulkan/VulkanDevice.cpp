#include "graphics/vulkan/VulkanDevice.hpp"

#include "debug/Log.hpp"

#include <vulkan/vulkan.h>

#include <algorithm>
#include <cstring>

using fusionps4::debug::LogCategory;

namespace fusionps4::graphics::vulkan {

namespace {

constexpr std::uint32_t kMaxFramesInFlight = 2;
constexpr const char* kValidationLayer = "VK_LAYER_KHRONOS_validation";

VkFormat toVkFormat(Format f) {
    switch (f) {
        case Format::R8G8B8A8_Unorm:        return VK_FORMAT_R8G8B8A8_UNORM;
        case Format::B8G8R8A8_Unorm:        return VK_FORMAT_B8G8R8A8_UNORM;
        case Format::R8G8B8A8_Srgb:         return VK_FORMAT_R8G8B8A8_SRGB;
        case Format::B8G8R8A8_Srgb:         return VK_FORMAT_B8G8R8A8_SRGB;
        case Format::R16G16B16A16_Sfloat:   return VK_FORMAT_R16G16B16A16_SFLOAT;
        case Format::R32G32B32A32_Sfloat:   return VK_FORMAT_R32G32B32A32_SFLOAT;
        case Format::D32_Sfloat:            return VK_FORMAT_D32_SFLOAT;
        case Format::D24_Unorm_S8_Uint:     return VK_FORMAT_D24_UNORM_S8_UINT;
        default:                            return VK_FORMAT_UNDEFINED;
    }
}

Format fromVkFormat(VkFormat f) {
    switch (f) {
        case VK_FORMAT_R8G8B8A8_UNORM:      return Format::R8G8B8A8_Unorm;
        case VK_FORMAT_B8G8R8A8_UNORM:      return Format::B8G8R8A8_Unorm;
        case VK_FORMAT_R8G8B8A8_SRGB:       return Format::R8G8B8A8_Srgb;
        case VK_FORMAT_B8G8R8A8_SRGB:       return Format::B8G8R8A8_Srgb;
        case VK_FORMAT_R16G16B16A16_SFLOAT: return Format::R16G16B16A16_Sfloat;
        case VK_FORMAT_R32G32B32A32_SFLOAT: return Format::R32G32B32A32_Sfloat;
        case VK_FORMAT_D32_SFLOAT:          return Format::D32_Sfloat;
        case VK_FORMAT_D24_UNORM_S8_UINT:   return Format::D24_Unorm_S8_Uint;
        default:                            return Format::Unknown;
    }
}

VkImageUsageFlags toVkImageUsage(ResourceUsage u) {
    VkImageUsageFlags out = 0;
    if (hasUsage(u, ResourceUsage::Sampled))  out |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (hasUsage(u, ResourceUsage::Storage))  out |= VK_IMAGE_USAGE_STORAGE_BIT;
    if (hasUsage(u, ResourceUsage::Color))    out |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (hasUsage(u, ResourceUsage::Depth))    out |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (hasUsage(u, ResourceUsage::Transfer)) out |= VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                                     VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (out == 0) out = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    return out;
}

VkBufferUsageFlags toVkBufferUsage(ResourceUsage u) {
    VkBufferUsageFlags out = 0;
    if (hasUsage(u, ResourceUsage::Vertex))   out |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (hasUsage(u, ResourceUsage::Index))    out |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (hasUsage(u, ResourceUsage::Uniform))  out |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (hasUsage(u, ResourceUsage::Storage))  out |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (hasUsage(u, ResourceUsage::Transfer)) out |= VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (out == 0) out = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    return out;
}

} // namespace

// -------------------------------------------------------------------------
// Internal structures
// -------------------------------------------------------------------------

struct VulkanDevice::Impl {
    VkInstance        instance      = VK_NULL_HANDLE;
    VkPhysicalDevice  physicalDevice= VK_NULL_HANDLE;
    VkDevice          device        = VK_NULL_HANDLE;
    VkQueue           graphicsQueue = VK_NULL_HANDLE;
    VkQueue           presentQueue  = VK_NULL_HANDLE;
    std::uint32_t     graphicsFamily = 0;
    std::uint32_t     presentFamily  = 0;

    VkSurfaceKHR      surface       = VK_NULL_HANDLE;

    // Swapchain.
    VkSwapchainKHR              swapchain   = VK_NULL_HANDLE;
    std::vector<VkImage>        swapImages;
    std::vector<VkImageView>    swapViews;
    std::vector<ImageViewHandle> swapViewHandles;
    VkFormat                    swapFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D                  swapExtent{};
    std::uint32_t               swapIndex = 0;

    // Per-frame resources.
    std::array<VkCommandPool,   kMaxFramesInFlight> cmdPools{};
    std::array<VkCommandBuffer, kMaxFramesInFlight> cmdBuffers{};
    std::array<VkFence,         kMaxFramesInFlight> inFlight{};
    std::array<VkSemaphore,     kMaxFramesInFlight> imageAvailable{};
    std::array<VkSemaphore,     kMaxFramesInFlight> renderFinished{};
    std::uint32_t frameIndex = 0;
    bool          inFrame    = false;
    bool          framebufferResized = false;

    // Resource tables.
    std::uint64_t nextHandle = 1;

    struct BufferEntry {
        VkBuffer       buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        std::uint64_t  size   = 0;
    };
    struct ImageEntry {
        VkImage        image  = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkFormat       format = VK_FORMAT_UNDEFINED;
        std::uint32_t  width  = 0;
        std::uint32_t  height = 0;
    };
    struct ViewEntry {
        VkImageView imageView = VK_NULL_HANDLE;
    };
    struct SamplerEntry {
        VkSampler sampler = VK_NULL_HANDLE;
    };
    struct ShaderEntry {
        VkShaderModule module = VK_NULL_HANDLE;
        ShaderStage    stage  = ShaderStage::Vertex;
    };
    struct PipelineEntry {
        VkPipeline pipeline = VK_NULL_HANDLE;
    };
    struct FenceEntry {
        VkFence fence = VK_NULL_HANDLE;
    };
    struct SemaphoreEntry {
        VkSemaphore sem = VK_NULL_HANDLE;
    };

    std::unordered_map<BufferHandle,    BufferEntry>    buffers;
    std::unordered_map<ImageHandle,     ImageEntry>     images;
    std::unordered_map<ImageViewHandle, ViewEntry>      views;
    std::unordered_map<SamplerHandle,   SamplerEntry>   samplers;
    std::unordered_map<ShaderHandle,    ShaderEntry>    shaders;
    std::unordered_map<PipelineHandle,  PipelineEntry>  pipelines;
    std::unordered_map<FenceHandle,     FenceEntry>     fences;
    std::unordered_map<SemaphoreHandle, SemaphoreEntry> semaphores;

    // One persistent command pool used for one-shot transfers.
    VkCommandPool transferPool = VK_NULL_HANDLE;

    bool           hasValidation = false;
    std::uint32_t  apiVersion    = VK_API_VERSION_1_1;
};

// -------------------------------------------------------------------------
// Construction
// -------------------------------------------------------------------------

VulkanDevice::VulkanDevice() : m_impl(std::make_unique<Impl>()) {}

VulkanDevice::~VulkanDevice() {
    shutdown();
}

bool VulkanDevice::initialize(HostSurfaceFactory& surfaceFactory) {
    std::lock_guard lock(m_mutex);
    auto& I = *m_impl;

    // ---- instance --------------------------------------------------------
    VkApplicationInfo app{};
    app.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName   = "FusionPS4";
    app.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    app.pEngineName        = "FusionPS4";
    app.engineVersion      = VK_MAKE_VERSION(0, 1, 0);
    app.apiVersion         = I.apiVersion;

    auto requiredExts = surfaceFactory.requiredVulkanExtensions();

    VkInstanceCreateInfo ici{};
    ici.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ici.pApplicationInfo        = &app;
    ici.enabledExtensionCount   = static_cast<std::uint32_t>(requiredExts.size());
    ici.ppEnabledExtensionNames = requiredExts.data();

    // Enable validation layer if present.
    std::uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> layers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, layers.data());
    for (const auto& l : layers) {
        if (std::strcmp(l.layerName, kValidationLayer) == 0) {
            I.hasValidation = true;
            break;
        }
    }
    if (I.hasValidation) {
        ici.enabledLayerCount   = 1;
        ici.ppEnabledLayerNames = &kValidationLayer;
    }

    if (vkCreateInstance(&ici, nullptr, &I.instance) != VK_SUCCESS) {
        FP4_ERROR(LogCategory::Vulkan) << "vkCreateInstance failed";
        return false;
    }

    // ---- surface --------------------------------------------------------
    void* rawSurface = nullptr;
    if (!surfaceFactory.createVulkanSurface(I.instance, rawSurface)) {
        FP4_ERROR(LogCategory::Vulkan) << "createVulkanSurface failed";
        vkDestroyInstance(I.instance, nullptr);
        I.instance = VK_NULL_HANDLE;
        return false;
    }
    I.surface = reinterpret_cast<VkSurfaceKHR>(rawSurface);

    // ---- physical device ------------------------------------------------
    std::uint32_t pdCount = 0;
    vkEnumeratePhysicalDevices(I.instance, &pdCount, nullptr);
    if (pdCount == 0) {
        FP4_ERROR(LogCategory::Vulkan) << "no Vulkan physical device";
        return false;
    }
    std::vector<VkPhysicalDevice> pdevs(pdCount);
    vkEnumeratePhysicalDevices(I.instance, &pdCount, pdevs.data());

    VkPhysicalDevice chosen = VK_NULL_HANDLE;
    for (auto pd : pdevs) {
        std::uint32_t qCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &qCount, nullptr);
        std::vector<VkQueueFamilyProperties> qs(qCount);
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &qCount, qs.data());

        std::uint32_t gfam = UINT32_MAX, pfam = UINT32_MAX;
        for (std::uint32_t i = 0; i < qCount; ++i) {
            if ((qs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && gfam == UINT32_MAX)
                gfam = i;
            VkBool32 supportsPresent = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(pd, i, I.surface, &supportsPresent);
            if (supportsPresent && pfam == UINT32_MAX)
                pfam = i;
        }
        if (gfam != UINT32_MAX && pfam != UINT32_MAX) {
            chosen = pd;
            I.graphicsFamily = gfam;
            I.presentFamily  = pfam;
            break;
        }
    }
    if (chosen == VK_NULL_HANDLE) {
        FP4_ERROR(LogCategory::Vulkan)
            << "no physical device with graphics + present queues";
        return false;
    }
    I.physicalDevice = chosen;

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(chosen, &props);
    FP4_INFO(LogCategory::Vulkan)
        << "device: " << props.deviceName
        << " (api " << VK_VERSION_MAJOR(props.apiVersion) << "."
        << VK_VERSION_MINOR(props.apiVersion) << ")";

    // ---- logical device -------------------------------------------------
    std::vector<VkDeviceQueueCreateInfo> qcis;
    std::vector<std::uint32_t> uniqueFamilies = {I.graphicsFamily, I.presentFamily};
    std::sort(uniqueFamilies.begin(), uniqueFamilies.end());
    uniqueFamilies.erase(std::unique(uniqueFamilies.begin(), uniqueFamilies.end()),
                         uniqueFamilies.end());
    const float prio = 1.0f;
    for (auto fam : uniqueFamilies) {
        VkDeviceQueueCreateInfo qci{};
        qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qci.queueFamilyIndex = fam;
        qci.queueCount = 1;
        qci.pQueuePriorities = &prio;
        qcis.push_back(qci);
    }

    const char* deviceExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    VkPhysicalDeviceFeatures features{};

    VkDeviceCreateInfo dci{};
    dci.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount    = static_cast<std::uint32_t>(qcis.size());
    dci.pQueueCreateInfos       = qcis.data();
    dci.enabledExtensionCount   = 1;
    dci.ppEnabledExtensionNames = deviceExtensions;
    dci.pEnabledFeatures        = &features;

    if (vkCreateDevice(chosen, &dci, nullptr, &I.device) != VK_SUCCESS) {
        FP4_ERROR(LogCategory::Vulkan) << "vkCreateDevice failed";
        return false;
    }
    vkGetDeviceQueue(I.device, I.graphicsFamily, 0, &I.graphicsQueue);
    vkGetDeviceQueue(I.device, I.presentFamily,  0, &I.presentQueue);

    // ---- command pools --------------------------------------------------
    for (std::uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        VkCommandPoolCreateInfo pci{};
        pci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pci.queueFamilyIndex = I.graphicsFamily;
        if (vkCreateCommandPool(I.device, &pci, nullptr, &I.cmdPools[i]) != VK_SUCCESS) {
            FP4_ERROR(LogCategory::Vulkan) << "vkCreateCommandPool failed";
            return false;
        }

        VkCommandBufferAllocateInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        ai.commandPool        = I.cmdPools[i];
        ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = 1;
        vkAllocateCommandBuffers(I.device, &ai, &I.cmdBuffers[i]);

        VkFenceCreateInfo fci{};
        fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        vkCreateFence(I.device, &fci, nullptr, &I.inFlight[i]);

        VkSemaphoreCreateInfo sci{};
        sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        vkCreateSemaphore(I.device, &sci, nullptr, &I.imageAvailable[i]);
        vkCreateSemaphore(I.device, &sci, nullptr, &I.renderFinished[i]);
    }

    VkCommandPoolCreateInfo tpci{};
    tpci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    tpci.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    tpci.queueFamilyIndex = I.graphicsFamily;
    if (vkCreateCommandPool(I.device, &tpci, nullptr, &I.transferPool) != VK_SUCCESS) {
        FP4_ERROR(LogCategory::Vulkan) << "transfer command pool failed";
        return false;
    }

    FP4_INFO(LogCategory::Vulkan) << "Vulkan backend initialized";
    return true;
}

void VulkanDevice::shutdown() {
    std::lock_guard lock(m_mutex);
    auto& I = *m_impl;
    if (I.device == VK_NULL_HANDLE) return;

    vkDeviceWaitIdle(I.device);

    destroySwapchain();

    for (auto& [h, e] : I.buffers) {
        vkDestroyBuffer(I.device, e.buffer, nullptr);
        vkFreeMemory(I.device, e.memory, nullptr);
    }
    I.buffers.clear();

    for (auto& [h, e] : I.images) {
        vkDestroyImage(I.device, e.image, nullptr);
        vkFreeMemory(I.device, e.memory, nullptr);
    }
    I.images.clear();

    for (auto& [h, e] : I.views)    vkDestroyImageView(I.device, e.imageView, nullptr);
    for (auto& [h, e] : I.samplers) vkDestroySampler(I.device, e.sampler, nullptr);
    for (auto& [h, e] : I.shaders)  vkDestroyShaderModule(I.device, e.module, nullptr);
    for (auto& [h, e] : I.pipelines)vkDestroyPipeline(I.device, e.pipeline, nullptr);
    for (auto& [h, e] : I.fences)   vkDestroyFence(I.device, e.fence, nullptr);
    for (auto& [h, e] : I.semaphores)vkDestroySemaphore(I.device, e.sem, nullptr);

    I.views.clear(); I.samplers.clear(); I.shaders.clear();
    I.pipelines.clear(); I.fences.clear(); I.semaphores.clear();

    for (std::uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        vkDestroyFence(I.device, I.inFlight[i], nullptr);
        vkDestroySemaphore(I.device, I.imageAvailable[i], nullptr);
        vkDestroySemaphore(I.device, I.renderFinished[i], nullptr);
        vkDestroyCommandPool(I.device, I.cmdPools[i], nullptr);
    }
    vkDestroyCommandPool(I.device, I.transferPool, nullptr);

    vkDestroyDevice(I.device, nullptr);
    I.device = VK_NULL_HANDLE;

    if (I.surface) {
        vkDestroySurfaceKHR(I.instance, I.surface, nullptr);
        I.surface = VK_NULL_HANDLE;
    }
    if (I.instance) {
        vkDestroyInstance(I.instance, nullptr);
        I.instance = VK_NULL_HANDLE;
    }

    FP4_INFO(LogCategory::Vulkan) << "Vulkan backend shut down";
}

// -------------------------------------------------------------------------
// Swapchain
// -------------------------------------------------------------------------

bool VulkanDevice::createSwapchain(const SwapchainDesc& desc) {
    std::lock_guard lock(m_mutex);
    auto& I = *m_impl;

    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(I.physicalDevice, I.surface, &caps);

    VkExtent2D extent{};
    if (caps.currentExtent.width != UINT32_MAX) {
        extent = caps.currentExtent;
    } else {
        extent.width  = std::clamp(desc.width,
                                   caps.minImageExtent.width,
                                   caps.maxImageExtent.width);
        extent.height = std::clamp(desc.height,
                                   caps.minImageExtent.height,
                                   caps.maxImageExtent.height);
    }

    std::uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
        imageCount = caps.maxImageCount;

    // Prefer B8G8R8A8_UNORM or R8G8B8A8_UNORM.
    std::uint32_t fmtCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(I.physicalDevice, I.surface, &fmtCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(fmtCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(I.physicalDevice, I.surface, &fmtCount, formats.data());

    VkSurfaceFormatKHR chosen = formats[0];
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosen = f; break;
        }
    }

    VkPresentModeKHR mode = desc.vsync ? VK_PRESENT_MODE_FIFO_KHR
                                       : VK_PRESENT_MODE_MAILBOX_KHR;

    VkSwapchainCreateInfoKHR sci{};
    sci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sci.surface          = I.surface;
    sci.minImageCount    = imageCount;
    sci.imageFormat      = chosen.format;
    sci.imageColorSpace  = chosen.colorSpace;
    sci.imageExtent      = extent;
    sci.imageArrayLayers = 1;
    sci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                           VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    std::uint32_t families[2] = { I.graphicsFamily, I.presentFamily };
    if (I.graphicsFamily != I.presentFamily) {
        sci.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        sci.queueFamilyIndexCount = 2;
        sci.pQueueFamilyIndices   = families;
    } else {
        sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    sci.preTransform   = caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode    = mode;
    sci.clipped        = VK_TRUE;

    if (vkCreateSwapchainKHR(I.device, &sci, nullptr, &I.swapchain) != VK_SUCCESS) {
        FP4_ERROR(LogCategory::Vulkan) << "vkCreateSwapchainKHR failed";
        return false;
    }

    std::uint32_t scCount = 0;
    vkGetSwapchainImagesKHR(I.device, I.swapchain, &scCount, nullptr);
    I.swapImages.resize(scCount);
    vkGetSwapchainImagesKHR(I.device, I.swapchain, &scCount, I.swapImages.data());

    I.swapViews.resize(scCount);
    I.swapViewHandles.resize(scCount);
    for (std::uint32_t i = 0; i < scCount; ++i) {
        VkImageViewCreateInfo vci{};
        vci.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vci.image                           = I.swapImages[i];
        vci.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        vci.format                          = chosen.format;
        vci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        vci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        vci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        vci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        vci.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        vci.subresourceRange.levelCount     = 1;
        vci.subresourceRange.layerCount     = 1;
        vkCreateImageView(I.device, &vci, nullptr, &I.swapViews[i]);

        const auto vh = I.nextHandle++;
        I.views[vh] = { I.swapViews[i] };
        I.swapViewHandles[i] = vh;
    }

    I.swapFormat = chosen.format;
    I.swapExtent = extent;

    FP4_INFO(LogCategory::Vulkan)
        << "swapchain: " << extent.width << "x" << extent.height
        << " images=" << scCount
        << " format=" << static_cast<int>(chosen.format)
        << " vsync=" << desc.vsync;
    return true;
}

void VulkanDevice::destroySwapchain() {
    auto& I = *m_impl;
    if (I.swapchain == VK_NULL_HANDLE) return;

    vkDeviceWaitIdle(I.device);

    for (auto v : I.swapViews) {
        if (v) vkDestroyImageView(I.device, v, nullptr);
    }
    I.swapViews.clear();
    I.swapViewHandles.clear();
    I.swapImages.clear();
    vkDestroySwapchainKHR(I.device, I.swapchain, nullptr);
    I.swapchain = VK_NULL_HANDLE;
}

void VulkanDevice::recreateSwapchain(std::uint32_t width, std::uint32_t height) {
    std::lock_guard lock(m_mutex);
    SwapchainDesc d;
    d.width  = width;
    d.height = height;
    d.vsync  = true;
    destroySwapchain();
    createSwapchain(d);
}

// -------------------------------------------------------------------------
// Frame lifecycle
// -------------------------------------------------------------------------

bool VulkanDevice::beginFrame(FrameContext& outFrame) {
    std::lock_guard lock(m_mutex);
    auto& I = *m_impl;
    if (I.swapchain == VK_NULL_HANDLE) return false;

    vkWaitForFences(I.device, 1, &I.inFlight[I.frameIndex], VK_TRUE,
                    UINT64_MAX);

    VkResult acq = vkAcquireNextImageKHR(
        I.device, I.swapchain, UINT64_MAX,
        I.imageAvailable[I.frameIndex], VK_NULL_HANDLE, &I.swapIndex);

    if (acq == VK_ERROR_OUT_OF_DATE_KHR) {
        return false;
    }
    if (acq != VK_SUCCESS && acq != VK_SUBOPTIMAL_KHR) {
        FP4_ERROR(LogCategory::Vulkan)
            << "vkAcquireNextImageKHR failed: " << acq;
        return false;
    }

    vkResetFences(I.device, 1, &I.inFlight[I.frameIndex]);
    vkResetCommandBuffer(I.cmdBuffers[I.frameIndex], 0);

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(I.cmdBuffers[I.frameIndex], &bi);

    // Transition image to TRANSFER_DST_OPTIMAL for clear.
    VkImageMemoryBarrier toTransfer{};
    toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.image = I.swapImages[I.swapIndex];
    toTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toTransfer.subresourceRange.levelCount = 1;
    toTransfer.subresourceRange.layerCount = 1;
    toTransfer.srcAccessMask = 0;
    toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(I.cmdBuffers[I.frameIndex],
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &toTransfer);

    outFrame.imageView  = I.swapViewHandles[I.swapIndex];
    outFrame.imageIndex = I.swapIndex;
    outFrame.width      = I.swapExtent.width;
    outFrame.height     = I.swapExtent.height;
    outFrame.format     = fromVkFormat(I.swapFormat);

    I.inFrame = true;
    return true;
}

bool VulkanDevice::presentFrame() {
    std::lock_guard lock(m_mutex);
    auto& I = *m_impl;
    if (!I.inFrame) return false;

    // Transition back to PRESENT_SRC.
    VkImageMemoryBarrier toPresent{};
    toPresent.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toPresent.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    toPresent.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toPresent.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toPresent.image = I.swapImages[I.swapIndex];
    toPresent.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toPresent.subresourceRange.levelCount = 1;
    toPresent.subresourceRange.layerCount = 1;
    toPresent.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toPresent.dstAccessMask = 0;

    vkCmdPipelineBarrier(I.cmdBuffers[I.frameIndex],
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &toPresent);

    vkEndCommandBuffer(I.cmdBuffers[I.frameIndex]);

    VkPipelineStageFlags waitStages = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo si{};
    si.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.waitSemaphoreCount   = 1;
    si.pWaitSemaphores      = &I.imageAvailable[I.frameIndex];
    si.pWaitDstStageMask    = &waitStages;
    si.commandBufferCount   = 1;
    si.pCommandBuffers      = &I.cmdBuffers[I.frameIndex];
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores    = &I.renderFinished[I.frameIndex];

    if (vkQueueSubmit(I.graphicsQueue, 1, &si, I.inFlight[I.frameIndex]) != VK_SUCCESS) {
        FP4_ERROR(LogCategory::Vulkan) << "vkQueueSubmit failed";
        return false;
    }

    VkPresentInfoKHR pi{};
    pi.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores    = &I.renderFinished[I.frameIndex];
    pi.swapchainCount     = 1;
    pi.pSwapchains        = &I.swapchain;
    pi.pImageIndices      = &I.swapIndex;

    VkResult r = vkQueuePresentKHR(I.presentQueue, &pi);
    if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR ||
        I.framebufferResized) {
        I.framebufferResized = false;
        // Caller is expected to recreate; do it on the next beginFrame.
        I.inFrame = false;
        return false;
    }
    if (r != VK_SUCCESS) {
        FP4_ERROR(LogCategory::Vulkan) << "vkQueuePresentKHR failed";
        I.inFrame = false;
        return false;
    }

    I.frameIndex = (I.frameIndex + 1) % kMaxFramesInFlight;
    I.inFrame = false;
    return true;
}

// -------------------------------------------------------------------------
// Resources
// -------------------------------------------------------------------------

BufferHandle VulkanDevice::createBuffer(const BufferDesc& desc) {
    std::lock_guard lock(m_mutex);
    auto& I = *m_impl;

    VkBufferCreateInfo bci{};
    bci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size        = desc.size;
    bci.usage       = toVkBufferUsage(desc.usage);
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buf = VK_NULL_HANDLE;
    if (vkCreateBuffer(I.device, &bci, nullptr, &buf) != VK_SUCCESS) {
        FP4_ERROR(LogCategory::Vulkan) << "vkCreateBuffer failed";
        return 0;
    }

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(I.device, buf, &req);

    VkMemoryAllocateInfo mai{};
    mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize  = req.size;

    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(I.physicalDevice, &memProps);

    std::uint32_t typeIndex = UINT32_MAX;
    const VkMemoryPropertyFlags wantHost =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    for (std::uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((req.memoryTypeBits & (1u << i)) &&
            (memProps.memoryTypes[i].propertyFlags & wantHost) == wantHost) {
            typeIndex = i;
            break;
        }
    }
    if (typeIndex == UINT32_MAX) {
        vkDestroyBuffer(I.device, buf, nullptr);
        FP4_ERROR(LogCategory::Vulkan) << "no host-visible memory type";
        return 0;
    }
    mai.memoryTypeIndex = typeIndex;

    VkDeviceMemory mem = VK_NULL_HANDLE;
    if (vkAllocateMemory(I.device, &mai, nullptr, &mem) != VK_SUCCESS) {
        vkDestroyBuffer(I.device, buf, nullptr);
        FP4_ERROR(LogCategory::Vulkan) << "vkAllocateMemory failed";
        return 0;
    }
    vkBindBufferMemory(I.device, buf, mem, 0);

    const auto h = I.nextHandle++;
    I.buffers[h] = { buf, mem, desc.size };
    return h;
}

void VulkanDevice::destroyBuffer(BufferHandle h) {
    std::lock_guard lock(m_mutex);
    auto& I = *m_impl;
    auto it = I.buffers.find(h);
    if (it == I.buffers.end()) return;
    vkDestroyBuffer(I.device, it->second.buffer, nullptr);
    vkFreeMemory(I.device, it->second.memory, nullptr);
    I.buffers.erase(it);
}

ImageHandle VulkanDevice::createImage(const ImageDesc& desc) {
    std::lock_guard lock(m_mutex);
    auto& I = *m_impl;

    VkImageCreateInfo ici{};
    ici.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType     = desc.depth > 1 ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
    ici.format        = toVkFormat(desc.format);
    ici.extent.width  = desc.width;
    ici.extent.height = desc.height;
    ici.extent.depth  = desc.depth;
    ici.mipLevels     = desc.mips;
    ici.arrayLayers   = desc.layers;
    ici.samples       = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling        = VK_IMAGE_TILING_OPTIMAL;
    ici.usage         = toVkImageUsage(desc.usage);
    ici.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage img = VK_NULL_HANDLE;
    if (vkCreateImage(I.device, &ici, nullptr, &img) != VK_SUCCESS) {
        FP4_ERROR(LogCategory::Vulkan) << "vkCreateImage failed";
        return 0;
    }

    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(I.device, img, &req);

    VkMemoryAllocateInfo mai{};
    mai.sType          = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = req.size;

    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(I.physicalDevice, &memProps);

    std::uint32_t typeIndex = UINT32_MAX;
    for (std::uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((req.memoryTypeBits & (1u << i)) &&
            (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            typeIndex = i; break;
        }
    }
    if (typeIndex == UINT32_MAX) typeIndex = 0;
    mai.memoryTypeIndex = typeIndex;

    VkDeviceMemory mem = VK_NULL_HANDLE;
    if (vkAllocateMemory(I.device, &mai, nullptr, &mem) != VK_SUCCESS) {
        vkDestroyImage(I.device, img, nullptr);
        FP4_ERROR(LogCategory::Vulkan) << "vkAllocateMemory(image) failed";
        return 0;
    }
    vkBindImageMemory(I.device, img, mem, 0);

    const auto h = I.nextHandle++;
    I.images[h] = { img, mem, toVkFormat(desc.format), desc.width, desc.height };
    return h;
}

void VulkanDevice::destroyImage(ImageHandle h) {
    std::lock_guard lock(m_mutex);
    auto& I = *m_impl;
    auto it = I.images.find(h);
    if (it == I.images.end()) return;
    vkDestroyImage(I.device, it->second.image, nullptr);
    vkFreeMemory(I.device, it->second.memory, nullptr);
    I.images.erase(it);
}

ImageViewHandle VulkanDevice::createImageView(ImageHandle image) {
    std::lock_guard lock(m_mutex);
    auto& I = *m_impl;
    auto it = I.images.find(image);
    if (it == I.images.end()) return 0;

    VkImageViewCreateInfo vci{};
    vci.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vci.image                           = it->second.image;
    vci.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    vci.format                          = it->second.format;
    vci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    vci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    vci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    vci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    vci.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    vci.subresourceRange.levelCount     = 1;
    vci.subresourceRange.layerCount     = 1;

    VkImageView view = VK_NULL_HANDLE;
    if (vkCreateImageView(I.device, &vci, nullptr, &view) != VK_SUCCESS) {
        FP4_ERROR(LogCategory::Vulkan) << "vkCreateImageView failed";
        return 0;
    }
    const auto h = I.nextHandle++;
    I.views[h] = { view };
    return h;
}

void VulkanDevice::destroyImageView(ImageViewHandle h) {
    std::lock_guard lock(m_mutex);
    auto& I = *m_impl;
    auto it = I.views.find(h);
    if (it == I.views.end()) return;
    vkDestroyImageView(I.device, it->second.imageView, nullptr);
    I.views.erase(it);
}

SamplerHandle VulkanDevice::createSampler(const SamplerDesc& desc) {
    std::lock_guard lock(m_mutex);
    auto& I = *m_impl;

    VkSamplerCreateInfo sci{};
    sci.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sci.magFilter    = desc.nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
    sci.minFilter    = desc.nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
    sci.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sci.addressModeU = desc.repeat ? VK_SAMPLER_ADDRESS_MODE_REPEAT
                                   : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeV = sci.addressModeU;
    sci.addressModeW = sci.addressModeU;
    sci.maxAnisotropy = desc.maxAniso;
    sci.anisotropyEnable = desc.maxAniso > 1.0f ? VK_TRUE : VK_FALSE;
    sci.maxLod = 16.0f;

    VkSampler s = VK_NULL_HANDLE;
    if (vkCreateSampler(I.device, &sci, nullptr, &s) != VK_SUCCESS) {
        FP4_ERROR(LogCategory::Vulkan) << "vkCreateSampler failed";
        return 0;
    }
    const auto h = I.nextHandle++;
    I.samplers[h] = { s };
    return h;
}

void VulkanDevice::destroySampler(SamplerHandle h) {
    std::lock_guard lock(m_mutex);
    auto& I = *m_impl;
    auto it = I.samplers.find(h);
    if (it == I.samplers.end()) return;
    vkDestroySampler(I.device, it->second.sampler, nullptr);
    I.samplers.erase(it);
}

// -------------------------------------------------------------------------
// Uploads
// -------------------------------------------------------------------------

bool VulkanDevice::uploadBuffer(BufferHandle h,
                                std::uint64_t  offset,
                                std::span<const std::byte> data) {
    std::lock_guard lock(m_mutex);
    auto& I = *m_impl;
    auto it = I.buffers.find(h);
    if (it == I.buffers.end()) return false;
    if (offset + data.size() > it->second.size) return false;

    void* mapped = nullptr;
    if (vkMapMemory(I.device, it->second.memory, offset, data.size(), 0, &mapped)
        != VK_SUCCESS) {
        FP4_ERROR(LogCategory::Vulkan) << "vkMapMemory(buffer) failed";
        return false;
    }
    std::memcpy(mapped, data.data(), data.size());
    vkUnmapMemory(I.device, it->second.memory);
    return true;
}

bool VulkanDevice::uploadImage(ImageHandle image, std::span<const std::byte> data) {
    std::lock_guard lock(m_mutex);
    auto& I = *m_impl;
    auto it = I.images.find(image);
    if (it == I.images.end()) return false;

    // Create a staging buffer.
    BufferDesc bd;
    bd.size  = data.size();
    bd.usage = ResourceUsage::Transfer;
    const auto staging = createBuffer(bd);
    if (!staging) return false;

    if (!uploadBuffer(staging, 0, data)) {
        destroyBuffer(staging);
        return false;
    }

    // One-shot: copy buffer -> image with layout transitions.
    VkCommandBufferAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool        = I.transferPool;
    ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    VkCommandBuffer cb = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(I.device, &ai, &cb);

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cb, &bi);

    VkImageMemoryBarrier b{};
    b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = it->second.image;
    b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    b.subresourceRange.levelCount = 1;
    b.subresourceRange.layerCount = 1;
    b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &b);

    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent.width  = it->second.width;
    region.imageExtent.height = it->second.height;
    region.imageExtent.depth  = 1;
    vkCmdCopyBufferToImage(cb, I.buffers[staging].buffer, it->second.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    VkImageMemoryBarrier b2 = b;
    b2.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    b2.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    b2.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    b2.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &b2);

    vkEndCommandBuffer(cb);

    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cb;
    VkFence fence = VK_NULL_HANDLE;
    VkFenceCreateInfo fci{};
    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    vkCreateFence(I.device, &fci, nullptr, &fence);
    vkQueueSubmit(I.graphicsQueue, 1, &si, fence);
    vkWaitForFences(I.device, 1, &fence, VK_TRUE, UINT64_MAX);
    vkDestroyFence(I.device, fence, nullptr);
    vkFreeCommandBuffers(I.device, I.transferPool, 1, &cb);

    destroyBuffer(staging);
    return true;
}

// -------------------------------------------------------------------------
// Shaders / pipelines
// -------------------------------------------------------------------------

ShaderHandle VulkanDevice::createShader(ShaderStage stage,
                                        std::span<const std::byte> spirv,
                                        std::string_view entry) {
    std::lock_guard lock(m_mutex);
    auto& I = *m_impl;

    VkShaderModuleCreateInfo sci{};
    sci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    sci.codeSize = spirv.size();
    sci.pCode    = reinterpret_cast<const std::uint32_t*>(spirv.data());

    VkShaderModule mod = VK_NULL_HANDLE;
    if (vkCreateShaderModule(I.device, &sci, nullptr, &mod) != VK_SUCCESS) {
        FP4_ERROR(LogCategory::Vulkan) << "vkCreateShaderModule failed";
        return 0;
    }
    (void)entry;
    const auto h = I.nextHandle++;
    I.shaders[h] = { mod, stage };
    return h;
}

void VulkanDevice::destroyShader(ShaderHandle h) {
    std::lock_guard lock(m_mutex);
    auto& I = *m_impl;
    auto it = I.shaders.find(h);
    if (it == I.shaders.end()) return;
    vkDestroyShaderModule(I.device, it->second.module, nullptr);
    I.shaders.erase(it);
}

PipelineHandle VulkanDevice::createGraphicsPipeline(ShaderHandle vs,
                                                    ShaderHandle fs) {
    std::lock_guard lock(m_mutex);
    auto& I = *m_impl;

    auto vsIt = I.shaders.find(vs);
    auto fsIt = I.shaders.find(fs);
    if (vsIt == I.shaders.end() || fsIt == I.shaders.end()) return 0;

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vsIt->second.module;
    stages[0].pName  = "main";
    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fsIt->second.module;
    stages[1].pName  = "main";

    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp{};
    vp.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount  = 1;

    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode    = VK_CULL_MODE_NONE;
    rs.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth   = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState cba{};
    cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments    = &cba;

    VkDynamicState dynStates[] = { VK_DYNAMIC_STATE_VIEWPORT,
                                   VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo ds{};
    ds.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    ds.dynamicStateCount = 2;
    ds.pDynamicStates    = dynStates;

    VkPipelineLayoutCreateInfo plci{};
    plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    vkCreatePipelineLayout(I.device, &plci, nullptr, &layout);

    VkGraphicsPipelineCreateInfo pci{};
    pci.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pci.stageCount          = 2;
    pci.pStages             = stages;
    pci.pVertexInputState   = &vi;
    pci.pInputAssemblyState = &ia;
    pci.pViewportState      = &vp;
    pci.pRasterizationState = &rs;
    pci.pMultisampleState   = &ms;
    pci.pColorBlendState    = &cb;
    pci.pDynamicState       = &ds;
    pci.layout              = layout;

    VkPipeline pipe = VK_NULL_HANDLE;
    VkResult r = vkCreateGraphicsPipelines(I.device, VK_NULL_HANDLE, 1,
                                           &pci, nullptr, &pipe);
    vkDestroyPipelineLayout(I.device, layout, nullptr);

    if (r != VK_SUCCESS) {
        FP4_ERROR(LogCategory::Vulkan) << "vkCreateGraphicsPipelines failed";
        return 0;
    }
    const auto h = I.nextHandle++;
    I.pipelines[h] = { pipe };
    return h;
}

void VulkanDevice::destroyPipeline(PipelineHandle h) {
    std::lock_guard lock(m_mutex);
    auto& I = *m_impl;
    auto it = I.pipelines.find(h);
    if (it == I.pipelines.end()) return;
    vkDestroyPipeline(I.device, it->second.pipeline, nullptr);
    I.pipelines.erase(it);
}

// -------------------------------------------------------------------------
// Clear / present
// -------------------------------------------------------------------------

void VulkanDevice::clearCurrentFrame(const ClearColor& c) {
    std::lock_guard lock(m_mutex);
    auto& I = *m_impl;
    if (!I.inFrame) return;

    VkClearColorValue color{};
    color.float32[0] = c.r;
    color.float32[1] = c.g;
    color.float32[2] = c.b;
    color.float32[3] = c.a;

    VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.levelCount = 1;
    range.layerCount = 1;

    vkCmdClearColorImage(I.cmdBuffers[I.frameIndex],
                         I.swapImages[I.swapIndex],
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         &color, 1, &range);
}

// -------------------------------------------------------------------------
// Synchronization
// -------------------------------------------------------------------------

FenceHandle VulkanDevice::createFence(bool signaled) {
    std::lock_guard lock(m_mutex);
    auto& I = *m_impl;
    VkFenceCreateInfo fci{};
    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fci.flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;
    VkFence f = VK_NULL_HANDLE;
    if (vkCreateFence(I.device, &fci, nullptr, &f) != VK_SUCCESS) return 0;
    const auto h = I.nextHandle++;
    I.fences[h] = { f };
    return h;
}

void VulkanDevice::destroyFence(FenceHandle h) {
    std::lock_guard lock(m_mutex);
    auto& I = *m_impl;
    auto it = I.fences.find(h);
    if (it == I.fences.end()) return;
    vkDestroyFence(I.device, it->second.fence, nullptr);
    I.fences.erase(it);
}

bool VulkanDevice::waitFence(FenceHandle h, std::uint64_t timeoutNs) {
    std::lock_guard lock(m_mutex);
    auto& I = *m_impl;
    auto it = I.fences.find(h);
    if (it == I.fences.end()) return false;
    return vkWaitForFences(I.device, 1, &it->second.fence, VK_TRUE,
                           timeoutNs) == VK_SUCCESS;
}

void VulkanDevice::resetFence(FenceHandle h) {
    std::lock_guard lock(m_mutex);
    auto& I = *m_impl;
    auto it = I.fences.find(h);
    if (it == I.fences.end()) return;
    vkResetFences(I.device, 1, &it->second.fence);
}

SemaphoreHandle VulkanDevice::createSemaphore() {
    std::lock_guard lock(m_mutex);
    auto& I = *m_impl;
    VkSemaphoreCreateInfo sci{};
    sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkSemaphore s = VK_NULL_HANDLE;
    if (vkCreateSemaphore(I.device, &sci, nullptr, &s) != VK_SUCCESS) return 0;
    const auto h = I.nextHandle++;
    I.semaphores[h] = { s };
    return h;
}

void VulkanDevice::destroySemaphore(SemaphoreHandle h) {
    std::lock_guard lock(m_mutex);
    auto& I = *m_impl;
    auto it = I.semaphores.find(h);
    if (it == I.semaphores.end()) return;
    vkDestroySemaphore(I.device, it->second.sem, nullptr);
    I.semaphores.erase(it);
}

} // namespace fusionps4::graphics::vulkan
