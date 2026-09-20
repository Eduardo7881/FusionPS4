#include "graphics/opengl/OpenGLDevice.hpp"

#include "debug/Log.hpp"

#include <SDL.h>
#include <SDL_opengl.h>

#include <cstring>

using fusionps4::debug::LogCategory;

namespace fusionps4::graphics::opengl {

struct OpenGLDevice::Impl {
    SDL_Window*   window = nullptr;
    SDL_GLContext context = nullptr;

    std::uint32_t width  = 0;
    std::uint32_t height = 0;

    std::uint64_t nextHandle = 1;

    struct BufferEntry  { GLuint id = 0; std::uint64_t size = 0; };
    struct ImageEntry   { GLuint id = 0; std::uint32_t w = 0, h = 0; };
    struct SamplerEntry { GLuint id = 0; };
    struct FenceEntry   { bool signaled = true; };
    struct SemaphoreEntry { bool signaled = false; };

    std::unordered_map<BufferHandle,    BufferEntry>    buffers;
    std::unordered_map<ImageHandle,     ImageEntry>     images;
    std::unordered_map<ImageViewHandle, ImageHandle>    views;
    std::unordered_map<SamplerHandle,   SamplerEntry>   samplers;
    std::unordered_map<FenceHandle,     FenceEntry>     fences;
    std::unordered_map<SemaphoreHandle, SemaphoreEntry> semaphores;

    ClearColor pendingClear{};
    bool       hasPendingClear = false;

    // The OpenGL backend cannot use SPIR-V directly; shaders would need a
    // SPIR-V -> GLSL translator. This backend is used as a fallback when
    // Vulkan is unavailable; it can still present cleared frames reliably.
};

OpenGLDevice::OpenGLDevice() : m_impl(std::make_unique<Impl>()) {}
OpenGLDevice::~OpenGLDevice() { shutdown(); }

bool OpenGLDevice::initialize(HostSurfaceFactory& surfaceFactory) {
    std::lock_guard lock(m_mutex);
    auto& I = *m_impl;

    I.window = static_cast<SDL_Window*>(surfaceFactory.nativeWindowHandle());
    if (!I.window) {
        FP4_ERROR(LogCategory::OpenGL)
            << "HostSurfaceFactory did not provide a native window";
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    I.context = SDL_GL_CreateContext(I.window);
    if (!I.context) {
        FP4_ERROR(LogCategory::OpenGL)
            << "SDL_GL_CreateContext failed: " << SDL_GetError();
        return false;
    }
    SDL_GL_MakeCurrent(I.window, I.context);

    FP4_INFO(LogCategory::OpenGL)
        << "OpenGL backend initialized: "
        << reinterpret_cast<const char*>(glGetString(GL_VERSION));
    return true;
}

void OpenGLDevice::shutdown() {
    std::lock_guard lock(m_mutex);
    auto& I = *m_impl;
    if (I.context) {
        SDL_GL_DeleteContext(I.context);
        I.context = nullptr;
    }
    I.window = nullptr;
}

bool OpenGLDevice::createSwapchain(const SwapchainDesc& desc) {
    std::lock_guard lock(m_mutex);
    auto& I = *m_impl;
    I.width  = desc.width;
    I.height = desc.height;
    if (I.window) SDL_GL_MakeCurrent(I.window, I.context);
    if (desc.vsync) SDL_GL_SetSwapInterval(1);
    else            SDL_GL_SetSwapInterval(0);
    FP4_INFO(LogCategory::OpenGL)
        << "swapchain: " << desc.width << "x" << desc.height
        << " vsync=" << desc.vsync;
    return true;
}

void OpenGLDevice::destroySwapchain() {
    // GL has no swapchain object; the window owns it.
}

void OpenGLDevice::recreateSwapchain(std::uint32_t width, std::uint32_t height) {
    std::lock_guard lock(m_mutex);
    m_impl->width  = width;
    m_impl->height = height;
}

bool OpenGLDevice::beginFrame(FrameContext& outFrame) {
    std::lock_guard lock(m_mutex);
    auto& I = *m_impl;
    if (!I.context) return false;

    SDL_GL_MakeCurrent(I.window, I.context);

    int w = 0, h = 0;
    SDL_GL_GetDrawableSize(I.window, &w, &h);
    glViewport(0, 0, w, h);

    outFrame.imageView  = 0;
    outFrame.imageIndex = 0;
    outFrame.width      = static_cast<std::uint32_t>(w);
    outFrame.height     = static_cast<std::uint32_t>(h);
    outFrame.format     = Format::R8G8B8A8_Unorm;

    I.hasPendingClear = false;
    return true;
}

bool OpenGLDevice::presentFrame() {
    std::lock_guard lock(m_mutex);
    auto& I = *m_impl;
    if (!I.context) return false;
    SDL_GL_SwapWindow(I.window);
    return true;
}

void OpenGLDevice::clearCurrentFrame(const ClearColor& c) {
    std::lock_guard lock(m_mutex);
    auto& I = *m_impl;
    if (!I.context) return false;
    (void)0;
    SDL_GL_MakeCurrent(I.window, I.context);
    glClearColor(c.r, c.g, c.b, c.a);
    glClear(GL_COLOR_BUFFER_BIT);
    I.hasPendingClear = true;
}

BufferHandle OpenGLDevice::createBuffer(const BufferDesc& desc) {
    std::lock_guard lock(m_mutex);
    auto& I = *m_impl;
    if (!I.context) return 0;
    GLuint id = 0;
    glGenBuffers(1, &id);
    glBindBuffer(GL_ARRAY_BUFFER, id);
    glBufferData(GL_ARRAY_BUFFER, desc.size, nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    const auto h = I.nextHandle++;
    I.buffers[h] = { id, desc.size };
    return h;
}

void OpenGLDevice::destroyBuffer(BufferHandle h) {
    std::lock_guard lock(m_mutex);
    auto& I = *m_impl;
    auto it = I.buffers.find(h);
    if (it == I.buffers.end()) return;
    if (I.context) {
        SDL_GL_MakeCurrent(I.window, I.context);
        glDeleteBuffers(1, &it->second.id);
    }
    I.buffers.erase(it);
}

ImageHandle OpenGLDevice::createImage(const ImageDesc& desc) {
    std::lock_guard lock(m_mutex);
    auto& I = *m_impl;
    if (!I.context) return 0;
    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 static_cast<GLsizei>(desc.width),
                 static_cast<GLsizei>(desc.height), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
    const auto h = I.nextHandle++;
    I.images[h] = { id, desc.width, desc.height };
    return h;
}

void OpenGLDevice::destroyImage(ImageHandle h) {
    std::lock_guard lock(m_mutex);
    auto& I = *m_impl;
    auto it = I.images.find(h);
    if (it == I.images.end()) return;
    if (I.context) {
        SDL_GL_MakeCurrent(I.window, I.context);
        glDeleteTextures(1, &it->second.id);
    }
    I.images.erase(it);
}

ImageViewHandle OpenGLDevice::createImageView(ImageHandle image) {
    std::lock_guard lock(m_mutex);
    auto& I = *m_impl;
    if (I.images.find(image) == I.images.end()) return 0;
    const auto h = I.nextHandle++;
    I.views[h] = image;
    return h;
}

void OpenGLDevice::destroyImageView(ImageViewHandle h) {
    std::lock_guard lock(m_mutex);
    m_impl->views.erase(h);
}

SamplerHandle OpenGLDevice::createSampler(const SamplerDesc& desc) {
    std::lock_guard lock(m_mutex);
    auto& I = *m_impl;
    if (!I.context) return 0;
    GLuint id = 0;
    glGenSamplers(1, &id);
    glSamplerParameteri(id, GL_TEXTURE_MIN_FILTER,
                        desc.nearest ? GL_NEAREST : GL_LINEAR);
    glSamplerParameteri(id, GL_TEXTURE_MAG_FILTER,
                        desc.nearest ? GL_NEAREST : GL_LINEAR);
    glSamplerParameteri(id, GL_TEXTURE_WRAP_S,
                        desc.repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    glSamplerParameteri(id, GL_TEXTURE_WRAP_T,
                        desc.repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    const auto h = I.nextHandle++;
    I.samplers[h] = { id };
    return h;
}

void OpenGLDevice::destroySampler(SamplerHandle h) {
    std::lock_guard lock(m_mutex);
    auto& I = *m_impl;
    auto it = I.samplers.find(h);
    if (it == I.samplers.end()) return;
    if (I.context) {
        SDL_GL_MakeCurrent(I.window, I.context);
        glDeleteSamplers(1, &it->second.id);
    }
    I.samplers.erase(it);
}

bool OpenGLDevice::uploadBuffer(BufferHandle h, std::uint64_t offset,
                                std::span<const std::byte> data) {
    std::lock_guard lock(m_mutex);
    auto& I = *m_impl;
    auto it = I.buffers.find(h);
    if (it == I.buffers.end()) return false;
    if (offset + data.size() > it->second.size) return false;
    SDL_GL_MakeCurrent(I.window, I.context);
    glBindBuffer(GL_ARRAY_BUFFER, it->second.id);
    glBufferSubData(GL_ARRAY_BUFFER,
                    static_cast<GLintptr>(offset),
                    static_cast<GLsizeiptr>(data.size()),
                    data.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return true;
}

bool OpenGLDevice::uploadImage(ImageHandle image,
                               std::span<const std::byte> data) {
    std::lock_guard lock(m_mutex);
    auto& I = *m_impl;
    auto it = I.images.find(image);
    if (it == I.images.end()) return false;
    SDL_GL_MakeCurrent(I.window, I.context);
    glBindTexture(GL_TEXTURE_2D, it->second.id);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                    static_cast<GLsizei>(it->second.w),
                    static_cast<GLsizei>(it->second.h),
                    GL_RGBA, GL_UNSIGNED_BYTE, data.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

ShaderHandle OpenGLDevice::createShader(ShaderStage /*stage*/,
                                        std::span<const std::byte> /*spirv*/,
                                        std::string_view /*entry*/) {
    FP4_ERROR(LogCategory::OpenGL)
        << "UNIMPLEMENTED OpenGLDevice::createShader: the OpenGL backend "
        << "does not consume SPIR-V; a SPIR-V -> GLSL translator is required.";
    return 0;
}

void OpenGLDevice::destroyShader(ShaderHandle /*h*/) {}

PipelineHandle OpenGLDevice::createGraphicsPipeline(ShaderHandle /*vs*/,
                                                     ShaderHandle /*fs*/) {
    FP4_ERROR(LogCategory::OpenGL)
        << "UNIMPLEMENTED OpenGLDevice::createGraphicsPipeline";
    return 0;
}

void OpenGLDevice::destroyPipeline(PipelineHandle /*h*/) {}

FenceHandle OpenGLDevice::createFence(bool signaled) {
    std::lock_guard lock(m_mutex);
    auto& I = *m_impl;
    const auto h = I.nextHandle++;
    I.fences[h] = { signaled };
    return h;
}

void OpenGLDevice::destroyFence(FenceHandle h) {
    std::lock_guard lock(m_mutex);
    m_impl->fences.erase(h);
}

bool OpenGLDevice::waitFence(FenceHandle h, std::uint64_t /*timeoutNs*/) {
    std::lock_guard lock(m_mutex);
    auto it = m_impl->fences.find(h);
    if (it == m_impl->fences.end()) return false;
    // OpenGL commands are implicitly ordered; a fence is effectively
    // always considered signaled here. Real fences require GLsync objects,
    // added in Phase 7 when the GL backend is fully wired.
    it->second.signaled = true;
    return true;
}

void OpenGLDevice::resetFence(FenceHandle h) {
    std::lock_guard lock(m_mutex);
    auto it = m_impl->fences.find(h);
    if (it != m_impl->fences.end()) it->second.signaled = false;
}

SemaphoreHandle OpenGLDevice::createSemaphore() {
    std::lock_guard lock(m_mutex);
    auto& I = *m_impl;
    const auto h = I.nextHandle++;
    I.semaphores[h] = { false };
    return h;
}

void OpenGLDevice::destroySemaphore(SemaphoreHandle h) {
    std::lock_guard lock(m_mutex);
    m_impl->semaphores.erase(h);
}

} // namespace fusionps4::graphics::opengl
