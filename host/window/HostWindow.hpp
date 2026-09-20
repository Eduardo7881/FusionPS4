#pragma once

#include "config/RuntimeConfig.hpp"

#include <cstdint>
#include <functional>
#include <string>

struct SDL_Window;

namespace fusionps4::host::window {

struct WindowEventHandlers {
    std::function<void(int width, int height)>      onResize;
    std::function<void()>                           onClose;
    std::function<void(bool focused)>               onFocus;
    std::function<void(const std::string& text)>    onTextInput;
    std::function<void(int scancode, bool down)>    onKey;
};

class HostWindow {
public:
    HostWindow();
    ~HostWindow();

    HostWindow(const HostWindow&) = delete;
    HostWindow& operator=(const HostWindow&) = delete;

    bool create(const config::WindowConfig& cfg);
    void destroy();

    bool isOpen() const { return m_open; }

    void setTitle(const std::string& title);
    void resize(int width, int height);
    void setMode(config::WindowMode mode);

    void setEventHandlers(WindowEventHandlers handlers);

    // Pumps SDL events; returns false if the window was requested to close.
    bool processEvents();

    // Present the current backbuffer to the display. Actual GPU work is
    // performed by the graphics layer; here we just swap the window surface.
    void present();

    int width()  const { return m_width; }
    int height() const { return m_height; }

    SDL_Window* nativeHandle() const { return m_window; }

    // Extensions required by the graphics backend to create a VkSurfaceKHR.
    void collectVulkanExtensions(std::vector<const char*>& out) const;

private:
    SDL_Window*             m_window = nullptr;
    int                     m_width  = 0;
    int                     m_height = 0;
    bool                    m_open   = false;
    config::WindowMode      m_mode   = config::WindowMode::Windowed;
    WindowEventHandlers     m_handlers;
};

} // namespace fusionps4::host::window
