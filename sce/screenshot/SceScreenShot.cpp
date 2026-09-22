#include "sce/screenshot/SceScreenShot.hpp"

#include "assets/CodecRegistry.hpp"
#include "debug/Log.hpp"
#include "graphics/abstraction/GraphicsDevice.hpp"
#include "runtime/RuntimeContext.hpp"
#include "runtime/process/PS4Process.hpp"
#include "sce/SceStubTable.hpp"

#include <cstring>
#include <fstream>
#include <string>

#if FUSIONPS4_HAVE_PNG
#  include <png.h>
#endif
#if FUSIONPS4_HAVE_JPEG
#  include <jpeglib.h>
#  include <setjmp.h>
#endif

using fusionps4::debug::LogCategory;
using fusionps4::runtime::RuntimeContext;

namespace fusionps4::sce::screenshot {

SceScreenShot& SceScreenShot::instance() {
    static SceScreenShot s;
    return s;
}

bool SceScreenShot::initialize() {
    m_initialized = true;
    const auto& caps = assets::CodecRegistry::instance();
    FP4_INFO(LogCategory::Sce)
        << "libSceScreenShot initialized (png=" << caps.hasPng
        << " jpeg=" << caps.hasJpeg << ")";
    return true;
}

void SceScreenShot::shutdown() { m_initialized = false; }

namespace {

constexpr int kOk             = 0;
constexpr int kErrInvalidArg  = static_cast<int>(0x80020005u);
constexpr int kErrNotFound    = static_cast<int>(0x80020004u);
constexpr int kErrNotSupport  = static_cast<int>(0x80020003u);
constexpr int kErrNoMemory    = static_cast<int>(0x80020002u);

// Filenames are user-provided strings in the guest's memory. We require
// them to start with "/user/" (photo album root); anything else is
// rejected so the guest cannot write outside the sandbox.
bool isAlbumPath(const std::string& p) {
    return p.compare(0, 6, "/user/") == 0;
}

std::string hostPathFromGuest(const std::string& guestPath,
                              std::int64_t& outErr) {
    auto* proc = RuntimeContext::instance().process();
    if (!proc) { outErr = kErrNotFound; return {}; }
    std::int64_t perr = 0;
    auto tr = proc->virtualFileSystem().resolve(
        guestPath, fusionps4::filesystem::policy::FsOp::Create, perr);
    if (!tr.ok) { outErr = perr; return {}; }
    outErr = kOk;
    return tr.hostPath;
}

#if FUSIONPS4_HAVE_PNG
bool writePng(const std::string& path,
              const fusionps4::graphics::CapturedFrame& frame) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                              nullptr, nullptr, nullptr);
    if (!png) { std::fclose(f); return false; }
    png_infop info = png_create_info_struct(png);
    if (!info) { png_destroy_write_struct(&png, nullptr); std::fclose(f); return false; }
    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        std::fclose(f);
        return false;
    }
    png_init_io(png, f);
    png_set_IHDR(png, info, frame.width, frame.height, 8,
                 PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);

    std::vector<png_bytep> rows(frame.height);
    for (std::uint32_t y = 0; y < frame.height; ++y) {
        rows[y] = const_cast<png_bytep>(
            frame.rgba.data() + static_cast<std::size_t>(y) *
                                    frame.width * 4);
    }
    png_write_image(png, rows.data());
    png_write_end(png, nullptr);
    png_destroy_write_struct(&png, &info);
    std::fclose(f);
    return true;
}
#endif

#if FUSIONPS4_HAVE_JPEG
struct JpegErrorMgr {
    jpeg_error_mgr pub;
    jmp_buf        jump;
    char           msg[JMSG_LENGTH_MAX];
};
void jpegErr(j_common_ptr cinfo) {
    auto* m = reinterpret_cast<JpegErrorMgr*>(cinfo->err);
    (*cinfo->err->format_message)(cinfo, m->msg);
    longjmp(m->jump, 1);
}
bool writeJpeg(const std::string& path,
               const fusionps4::graphics::CapturedFrame& frame,
               int quality) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;

    jpeg_compress_struct cinfo{};
    JpegErrorMgr jerr{};
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = jpegErr;
    if (setjmp(jerr.jump)) {
        jpeg_destroy_compress(&cinfo);
        std::fclose(f);
        return false;
    }
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, f);
    cinfo.image_width      = frame.width;
    cinfo.image_height     = frame.height;
    cinfo.input_components = 4;
    cinfo.in_color_space   = JCS_EXT_RGBA;
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);
    jpeg_start_compress(&cinfo, TRUE);

    std::vector<JSAMPROW> rows(frame.height);
    for (std::uint32_t y = 0; y < frame.height; ++y) {
        rows[y] = const_cast<JSAMPROW>(
            frame.rgba.data() + static_cast<std::size_t>(y) *
                                    frame.width * 4);
    }
    while (cinfo.next_scanline < cinfo.image_height) {
        JSAMPROW row = rows[cinfo.next_scanline];
        jpeg_write_scanlines(&cinfo, &row, 1);
    }
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    std::fclose(f);
    return true;
}
#endif

extern "C" {

int sceScreenShotInit(std::uint32_t /*poolSize*/) { return kOk; }
int sceScreenShotTerm() { return kOk; }

// sceScreenShotCapture saves the last presented frame to disk. The guest
// provides a directory and filename (in its own VFS namespace); the
// runtime reads the swapchain back, encodes as JPEG, and writes it.
int sceScreenShotCapture(const char* guestDir, const char* guestFile) {
    if (!guestDir || !guestFile) return kErrInvalidArg;

    const std::string dir(guestDir);
    const std::string file(guestFile);
    const std::string path = dir + "/" + file;
    if (!isAlbumPath(path)) {
        FP4_WARN(LogCategory::Sce)
            << "sceScreenShotCapture: path \"" << path
            << "\" outside /user/ album root; rejected";
        return kErrNotSupport;
    }

    auto* dev = RuntimeContext::instance().graphicsDevice();
    if (!dev) {
        FP4_ERROR(LogCategory::Sce)
            << "sceScreenShotCapture: no GraphicsDevice bound";
        return kErrNotSupport;
    }

    fusionps4::graphics::CapturedFrame frame;
    if (!dev->captureFrame(frame)) {
        FP4_ERROR(LogCategory::Sce)
            << "sceScreenShotCapture: captureFrame failed (no presented "
            << "frame yet, or backend readback unsupported)";
        return kErrNotSupport;
    }

    std::int64_t perr = 0;
    const auto hostPath = hostPathFromGuest(path, perr);
    if (hostPath.empty()) {
        FP4_ERROR(LogCategory::Sce)
            << "sceScreenShotCapture: VFS refused \"" << path << "\"";
        return kErrNotFound;
    }

    // Ensure the parent directory exists.
    {
        std::string acc;
        for (std::size_t i = 0; i < hostPath.size(); ++i) {
            acc.push_back(hostPath[i]);
            if (hostPath[i] == '/') ::mkdir(acc.c_str(), 0755);
        }
    }

    const auto& caps = assets::CodecRegistry::instance();

#if FUSIONPS4_HAVE_JPEG
    if (caps.hasJpeg) {
        if (writeJpeg(hostPath, frame, 92)) {
            FP4_INFO(LogCategory::Sce)
                << "Screenshot saved (JPEG): " << path
                << " (" << frame.width << "x" << frame.height << ")";
            return kOk;
        }
    }
#endif

#if FUSIONPS4_HAVE_PNG
    if (caps.hasPng) {
        if (writePng(hostPath, frame)) {
            FP4_INFO(LogCategory::Sce)
                << "Screenshot saved (PNG): " << path
                << " (" << frame.width << "x" << frame.height << ")";
            return kOk;
        }
    }
#endif

    FP4_ERROR(LogCategory::Sce)
        << "sceScreenShotCapture: no usable image encoder in this build "
        << "(JPEG or PNG must be linked)";
    return kErrNotSupport;
}

int sceScreenShotDisable()  { return kOk; }
int sceScreenShotEnable()   { return kOk; }
int sceScreenShotSetParam(const void* /*p*/) { return kOk; }

} // extern "C"

} // namespace

void SceScreenShot::registerExports(SceStubTable& t) {
    t.registerStub("libSceScreenShot", "sceScreenShotInit",
                   reinterpret_cast<void*>(&sceScreenShotInit));
    t.registerStub("libSceScreenShot", "sceScreenShotTerm",
                   reinterpret_cast<void*>(&sceScreenShotTerm));
    t.registerStub("libSceScreenShot", "sceScreenShotCapture",
                   reinterpret_cast<void*>(&sceScreenShotCapture));
    t.registerStub("libSceScreenShot", "sceScreenShotDisable",
                   reinterpret_cast<void*>(&sceScreenShotDisable));
    t.registerStub("libSceScreenShot", "sceScreenShotEnable",
                   reinterpret_cast<void*>(&sceScreenShotEnable));
    t.registerStub("libSceScreenShot", "sceScreenShotSetParam",
                   reinterpret_cast<void*>(&sceScreenShotSetParam));
}

} // namespace fusionps4::sce::screenshot
