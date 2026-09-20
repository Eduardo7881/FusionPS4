#include "host/window/HostWindow.hpp"

#include "debug/Log.hpp"

#include <SDL.h>
#include <vector>

using fusionps4::debug::LogCategory;

namespace fusionps4::host::window {

HostWindow::HostWindow() = default;

HostWindow::~HostWindow() {
    destroy();
}

bool HostWindow::create(const config::WindowConfig& cfg) {
    if (m_window) return false;

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        FP4_ERROR(LogCategory::Host)
            << "SDL_InitSubSystem(VIDEO) failed: " << SDL_GetError();
        return false;
    }

    Uint32 flags = SDL_WINDOW_VULKAN | SDL_WINDOW_ALLOW_HIGHDPI;

    switch (cfg.mode) {
        case config::WindowMode::Fullscreen:
            flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
            break;
        case config::WindowMode::FullscreenExclusive:
            flags |= SDL_WINDOW_FULLSCREEN;
            break;
        case config::WindowMode::Windowed:
        default:
            if (cfg.resizable) flags |= SDL_WINDOW_RESIZABLE;
            break;
    }

    m_window = SDL_CreateWindow(cfg.title.c_str(),
                                SDL_WINDOWPOS_CENTERED,
                                SDL_WINDOWPOS_CENTERED,
                                cfg.width, cfg.height, flags);
    if (!m_window) {
        FP4_ERROR(LogCategory::Host)
            << "SDL_CreateWindow failed: " << SDL_GetError();
        return false;
    }

    SDL_GetWindowSize(m_window, &m_width, &m_height);
    m_mode = cfg.mode;
    m_open = true;

    FP4_INFO(LogCategory::Host)
        << "Window created: \"" << cfg.title << "\" " << m_width << "x"
        << m_height << " mode=" << static_cast<int>(cfg.mode);
    return true;
}

void HostWindow::destroy() {
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    m_open = false;
}

void HostWindow::setTitle(const std::string& title) {
    if (m_window) SDL_SetWindowTitle(m_window, title.c_str());
}

void HostWindow::resize(int width, int height) {
    if (!m_window) return;
    SDL_SetWindowSize(m_window, width, height);
    m_width  = width;
    m_height = height;
}

void HostWindow::setMode(config::WindowMode mode) {
    if (!m_window) return;
    Uint32 flag = 0;
    switch (mode) {
        case config::WindowMode::Fullscreen:          flag = SDL_WINDOW_FULLSCREEN_DESKTOP; break;
        case config::WindowMode::FullscreenExclusive: flag = SDL_WINDOW_FULLSCREEN;         break;
        case config::WindowMode::Windowed:            flag = 0;                             break;
    }
    if (SDL_SetWindowFullscreen(m_window, flag) != 0) {
        FP4_WARN(LogCategory::Host)
            << "SDL_SetWindowFullscreen failed: " << SDL_GetError();
        return;
    }
    m_mode = mode;
}

void HostWindow::setEventHandlers(WindowEventHandlers handlers) {
    m_handlers = std::move(handlers);
}

bool HostWindow::processEvents() {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
            case SDL_QUIT:
                m_open = false;
                if (m_handlers.onClose) m_handlers.onClose();
                break;

            case SDL_WINDOWEVENT:
                if (ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                    ev.window.event == SDL_WINDOWEVENT_RESIZED) {
                    m_width  = ev.window.data1;
                    m_height = ev.window.data2;
                    if (m_handlers.onResize) m_handlers.onResize(m_width, m_height);
                } else if (ev.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                    if (m_handlers.onFocus) m_handlers.onFocus(true);
                } else if (ev.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                    if (m_handlers.onFocus) m_handlers.onFocus(false);
                } else if (ev.window.event == SDL_WINDOWEVENT_CLOSE) {
                    m_open = false;
                    if (m_handlers.onClose) m_handlers.onClose();
                }
                break;

            case SDL_KEYDOWN:
            case SDL_KEYUP:
                if (m_handlers.onKey) {
                    m_handlers.onKey(ev.key.keysym.scancode,
                                     ev.key.type == SDL_KEYDOWN);
                }
                break;

            case SDL_TEXTINPUT:
                if (m_handlers.onTextInput) {
                    m_handlers.onTextInput(ev.text.text);
                }
                break;

            default:
                break;
        }
    }
    return m_open;
}

void HostWindow::present() {
    // With SDL_WINDOW_VULKAN, presentation is done by the graphics backend
    // via VkSwapchainKHR. When OpenGL backend is selected, SDL_GL_SwapWindow
    // will be used here instead. This method exists so callers have a single
    // present entry point controlled by the runtime.
}

void HostWindow::collectVulkanExtensions(std::vector<const char*>& out) const {
    unsigned count = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(m_window, &count, nullptr)) {
        FP4_ERROR(LogCategory::Host)
            << "SDL_Vulkan_GetInstanceExtensions failed: " << SDL_GetError();
        return;
    }
    const std::size_t base = out.size();
    out.resize(base + count);
    SDL_Vulkan_GetInstanceExtensions(m_window, &count, out.data() + base);
}

} // namespace fusionps4::host::window
