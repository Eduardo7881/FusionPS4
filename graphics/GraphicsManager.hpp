#pragma once

#include "graphics/abstraction/GraphicsDevice.hpp"

#include <memory>
#include <mutex>

namespace fusionps4::graphics {

// Selects, owns, and exposes the active GraphicsDevice. The Runtime holds
// exactly one GraphicsManager; the guest never sees this class directly.
class GraphicsManager {
public:
    GraphicsManager();
    ~GraphicsManager();

    GraphicsManager(const GraphicsManager&) = delete;
    GraphicsManager& operator=(const GraphicsManager&) = delete;

    // Initialize with the requested backend. On failure of the primary
    // backend, tries the alternate.
    bool init(Backend preferred, HostSurfaceFactory& surfaceFactory);

    void shutdown();

    GraphicsDevice* device() { return m_device.get(); }
    Backend         backend() const;

    // Swapchain helpers that go through the active backend.
    bool createSwapchain(const SwapchainDesc& desc);
    void destroySwapchain();
    void recreateSwapchain(std::uint32_t width, std::uint32_t height);

private:
    std::unique_ptr<GraphicsDevice> m_device;
    mutable std::mutex              m_mutex;
};

} // namespace fusionps4::graphics
