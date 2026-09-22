#include "sce/jpeg/SceJpeg.hpp"

#include "assets/CodecRegistry.hpp"
#include "debug/Log.hpp"
#include "sce/SceStubTable.hpp"

#if FUSIONPS4_HAVE_JPEG
#  include <jpeglib.h>
#  include <setjmp.h>
#endif

#include <cstring>

using fusionps4::debug::LogCategory;

namespace fusionps4::sce::jpeg {

SceJpeg& SceJpeg::instance() {
    static SceJpeg s;
    return s;
}

bool SceJpeg::initialize() {
    m_initialized = true;
    const auto& caps = assets::CodecRegistry::instance();
    FP4_INFO(LogCategory::Sce)
        << "libSceJpeg initialized (libjpeg=" << caps.hasJpeg << ")";
    return true;
}

void SceJpeg::shutdown() { m_initialized = false; }

namespace {

constexpr int kOk             = 0;
constexpr int kErrInvalidArg  = static_cast<int>(0x80020005u);
constexpr int kErrNotSupport  = static_cast<int>(0x80020003u);

#if FUSIONPS4_HAVE_JPEG

// libjpeg uses setjmp/longjmp for error handling; we need a per-call
// error manager that does not call exit().
struct JpegErrorMgr {
    jpeg_error_mgr pub;
    jmp_buf        jump;
    char           message[JMSG_LENGTH_MAX];
};

void jpegErrorExit(j_common_ptr cinfo) {
    auto* mgr = reinterpret_cast<JpegErrorMgr*>(cinfo->err);
    (*cinfo->err->format_message)(cinfo, mgr->message);
    longjmp(mgr->jump, 1);
}

struct DecodeResult {
    std::uint32_t width  = 0;
    std::uint32_t height = 0;
    std::uint32_t channels = 0;
};

bool decodeJpeg(const void* in, std::size_t inSize,
                void* out, std::size_t outCapacity,
                DecodeResult& res) {
    jpeg_decompress_struct cinfo{};
    JpegErrorMgr jerr{};
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = jpegErrorExit;

    if (setjmp(jerr.jump)) {
        FP4_ERROR(LogCategory::Sce)
            << "libjpeg error: " << jerr.message;
        jpeg_destroy_decompress(&cinfo);
        return false;
    }

    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo,
                 static_cast<const unsigned char*>(in),
                 static_cast<unsigned long>(inSize));
    jpeg_read_header(&cinfo, TRUE);
    cinfo.out_color_space = JCS_RGB;
    jpeg_start_decompress(&cinfo);

    const auto w = cinfo.output_width;
    const auto h = cinfo.output_height;
    const auto c = cinfo.output_components;
    const auto needed = static_cast<std::size_t>(w) * h * c;
    if (needed > outCapacity) {
        FP4_ERROR(LogCategory::Sce)
            << "JPEG output buffer too small: need " << needed
            << " have " << outCapacity;
        jpeg_destroy_decompress(&cinfo);
        return false;
    }

    const auto rowStride = cinfo.output_width * cinfo.output_components;
    JSAMPROW rowPtr = nullptr;
    auto* dst = static_cast<std::uint8_t*>(out);
    while (cinfo.output_scanline < cinfo.output_height) {
        rowPtr = dst + (cinfo.output_scanline * rowStride);
        jpeg_read_scanlines(&cinfo, &rowPtr, 1);
    }
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    res.width    = w;
    res.height   = h;
    res.channels = c;
    return true;
}
#endif  // FUSIONPS4_HAVE_JPEG

extern "C" {

int sceJpegInitJpeg(int /*memSize*/) { return kOk; }
int sceJpegFinishJpeg() { return kOk; }

int sceJpegDecodeJpeg(const void* in, std::size_t inSize,
                      void* out, std::size_t outCapacity,
                      std::uint32_t* outWidth, std::uint32_t* outHeight) {
#if FUSIONPS4_HAVE_JPEG
    if (!in || !out || inSize == 0) return kErrInvalidArg;
    DecodeResult r;
    if (!decodeJpeg(in, inSize, out, outCapacity, r)) return kErrInvalidArg;
    if (outWidth)  *outWidth  = r.width;
    if (outHeight) *outHeight = r.height;
    return kOk;
#else
    (void)in; (void)inSize; (void)out; (void)outCapacity;
    (void)outWidth; (void)outHeight;
    FP4_UNIMPLEMENTED(LogCategory::Sce, "sceJpegDecodeJpeg");
    FP4_ERROR(LogCategory::Sce)
        << "  reason=libjpeg-turbo not linked into this build "
        << "(FUSIONPS4_HAVE_JPEG=0); rebuild with libjpeg to enable JPEG decode";
    return kErrNotSupport;
#endif
}

int sceJpegCreateDecoder(std::uint32_t /*version*/) { return kOk; }
int sceJpegDeleteDecoder() { return kOk; }
int sceJpegDecodeJpegInternal(std::uint64_t /*handle*/, const void* /*in*/,
                              std::size_t /*inSize*/, void* /*out*/,
                              std::size_t /*outCapacity*/) {
    return kErrNotSupport;
}

} // extern "C"

} // namespace

void SceJpeg::registerExports(SceStubTable& t) {
    t.registerStub("libSceJpeg", "sceJpegInitJpeg",
                   reinterpret_cast<void*>(&sceJpegInitJpeg));
    t.registerStub("libSceJpeg", "sceJpegFinishJpeg",
                   reinterpret_cast<void*>(&sceJpegFinishJpeg));
    t.registerStub("libSceJpeg", "sceJpegDecodeJpeg",
                   reinterpret_cast<void*>(&sceJpegDecodeJpeg));
    t.registerStub("libSceJpeg", "sceJpegCreateDecoder",
                   reinterpret_cast<void*>(&sceJpegCreateDecoder));
    t.registerStub("libSceJpeg", "sceJpegDeleteDecoder",
                   reinterpret_cast<void*>(&sceJpegDeleteDecoder));
    t.registerStub("libSceJpeg", "sceJpegDecodeJpegInternal",
                   reinterpret_cast<void*>(&sceJpegDecodeJpegInternal));
}

} // namespace fusionps4::sce::jpeg
