#include "sce/png/ScePng.hpp"

#include "assets/CodecRegistry.hpp"
#include "debug/Log.hpp"
#include "sce/SceStubTable.hpp"

#if FUSIONPS4_HAVE_PNG
#  include <png.h>
#endif

#include <cstring>

using fusionps4::debug::LogCategory;

namespace fusionps4::sce::png {

ScePng& ScePng::instance() {
    static ScePng s;
    return s;
}

bool ScePng::initialize() {
    m_initialized = true;
    const auto& caps = assets::CodecRegistry::instance();
    FP4_INFO(LogCategory::Sce)
        << "libScePng initialized (libpng=" << caps.hasPng << ")";
    return true;
}

void ScePng::shutdown() { m_initialized = false; }

namespace {

constexpr int kOk            = 0;
constexpr int kErrInvalidArg = static_cast<int>(0x80020005u);
constexpr int kErrNotSupport = static_cast<int>(0x80020003u);

#if FUSIONPS4_HAVE_PNG

struct PngReadState {
    const std::uint8_t* data;
    std::size_t         size;
    std::size_t         offset;
};

void pngReadCallback(png_structp png, png_bytep out, png_size_t len) {
    auto* state = static_cast<PngReadState*>(png_get_io_ptr(png));
    if (state->offset + len > state->size) {
        png_error(png, "read past end of PNG buffer");
        return;
    }
    std::memcpy(out, state->data + state->offset, len);
    state->offset += len;
}

bool decodePng(const void* in, std::size_t inSize,
               void* out, std::size_t outCapacity,
               std::uint32_t& width, std::uint32_t& height) {
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                             nullptr, nullptr, nullptr);
    if (!png) return false;

    png_infop info = png_create_info_struct(png);
    if (!info) { png_destroy_read_struct(&png, nullptr, nullptr); return false; }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, nullptr);
        FP4_ERROR(LogCategory::Sce) << "libpng error while decoding";
        return false;
    }

    PngReadState state{
        static_cast<const std::uint8_t*>(in), inSize, 0};
    png_set_read_fn(png, &state, pngReadCallback);
    png_read_info(png, info);

    width  = png_get_image_width(png, info);
    height = png_get_image_height(png, info);
    auto colorType = png_get_color_type(png, info);
    auto bitDepth  = png_get_bit_depth(png, info);

    if (bitDepth == 16) png_set_strip_16(png);
    if (colorType == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (colorType == PNG_COLOR_TYPE_GRAY && bitDepth < 8) {
        png_set_expand_gray_1_2_4_to_8(png);
    }
    if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
    if (colorType == PNG_COLOR_TYPE_GRAY ||
        colorType == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png);
    }
    if (colorType != PNG_COLOR_TYPE_RGB_ALPHA) {
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    }
    png_read_update_info(png, info);

    const auto rowBytes = png_get_rowbytes(png, info);
    const auto needed = static_cast<std::size_t>(rowBytes) * height;
    if (needed > outCapacity) {
        FP4_ERROR(LogCategory::Sce)
            << "PNG output buffer too small: need " << needed
            << " have " << outCapacity;
        png_destroy_read_struct(&png, &info, nullptr);
        return false;
    }

    std::vector<png_bytep> rows(height);
    auto* dst = static_cast<std::uint8_t*>(out);
    for (std::uint32_t y = 0; y < height; ++y) {
        rows[y] = dst + y * rowBytes;
    }
    png_read_image(png, rows.data());
    png_read_end(png, nullptr);
    png_destroy_read_struct(&png, &info, nullptr);
    return true;
}

#endif  // FUSIONPS4_HAVE_PNG

extern "C" {

int scePngInit(std::uint32_t /*version*/) { return kOk; }
int scePngFinish() { return kOk; }

// scePngDecode(in, inSize, out, outCapacity, outInfo*)
// outInfo: [0..3] = width u32, [4..7] = height u32, [8..11] = channels u32
int scePngDecode(const void* in, std::size_t inSize,
                 void* out, std::size_t outCapacity,
                 void* outInfo) {
#if FUSIONPS4_HAVE_PNG
    if (!in || !out || inSize == 0) return kErrInvalidArg;
    std::uint32_t w = 0, h = 0;
    if (!decodePng(in, inSize, out, outCapacity, w, h)) return kErrInvalidArg;
    if (outInfo) {
        auto* info = static_cast<std::uint32_t*>(outInfo);
        info[0] = w;
        info[1] = h;
        info[2] = 4;    // RGBA8 after conversion
    }
    return kOk;
#else
    (void)in; (void)inSize; (void)out; (void)outCapacity; (void)outInfo;
    FP4_UNIMPLEMENTED(LogCategory::Sce, "scePngDecode");
    FP4_ERROR(LogCategory::Sce)
        << "  reason=libpng not linked into this build (FUSIONPS4_HAVE_PNG=0); "
        << "rebuild with libpng to enable PNG decode";
    return kErrNotSupport;
#endif
}

} // extern "C"

} // namespace

void ScePng::registerExports(SceStubTable& t) {
    t.registerStub("libScePng", "scePngInit",
                   reinterpret_cast<void*>(&scePngInit));
    t.registerStub("libScePng", "scePngFinish",
                   reinterpret_cast<void*>(&scePngFinish));
    t.registerStub("libScePng", "scePngDecode",
                   reinterpret_cast<void*>(&scePngDecode));
}

} // namespace fusionps4::sce::png
