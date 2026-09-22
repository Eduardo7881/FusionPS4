#include "sce/font/SceFont.hpp"

#include "assets/CodecRegistry.hpp"
#include "debug/Log.hpp"
#include "runtime/RuntimeContext.hpp"
#include "runtime/process/PS4Process.hpp"
#include "sce/SceStubTable.hpp"

#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#if FUSIONPS4_HAVE_FREETYPE
#  include <ft2build.h>
#  include FT_FREETYPE_H
#endif

using fusionps4::debug::LogCategory;
using fusionps4::runtime::RuntimeContext;

namespace fusionps4::sce::font {

SceFont& SceFont::instance() {
    static SceFont s;
    return s;
}

bool SceFont::initialize() {
    m_initialized = true;
    const auto& caps = assets::CodecRegistry::instance();
    FP4_INFO(LogCategory::Sce)
        << "libSceFont initialized (freetype=" << caps.hasFreetype << ")";
    return true;
}

void SceFont::shutdown() { m_initialized = false; }

namespace {

constexpr int kOk            = 0;
constexpr int kErrInvalidArg = static_cast<int>(0x80020005u);
constexpr int kErrNotFound   = static_cast<int>(0x80020004u);
constexpr int kErrNotSupport = static_cast<int>(0x80020003u);

#if FUSIONPS4_HAVE_FREETYPE

FT_Library g_ft = nullptr;

struct FontEntry {
    FT_Face face = nullptr;
};

std::mutex                                    g_mutex;
std::unordered_map<std::uint64_t, FontEntry>  g_fonts;
std::uint64_t                                 g_nextHandle = 1;

bool ensureFreetype() {
    if (g_ft) return true;
    if (FT_Init_FreeType(&g_ft) != 0) {
        FP4_ERROR(LogCategory::Sce) << "FT_Init_FreeType failed";
        return false;
    }
    return true;
}

#endif  // FUSIONPS4_HAVE_FREETYPE

extern "C" {

int sceFontInit() { return kOk; }
int sceFontDone() { return kOk; }

// sceFontCreateLibraryFromMemory supports a caller-provided library object
// (typically on the guest's stack) followed by a font file. We allocate a
// runtime handle and return it in *outLibrary.
int sceFontCreateLibraryFromMemory(void** outLibrary,
                                   const void* fontData,
                                   std::size_t fontDataSize,
                                   std::uint32_t /*userFlags*/) {
#if FUSIONPS4_HAVE_FREETYPE
    if (!outLibrary || !fontData || fontDataSize == 0) return kErrInvalidArg;
    if (!ensureFreetype()) return kErrNotSupport;

    std::lock_guard lock(g_mutex);
    FT_Face face = nullptr;
    if (FT_New_Memory_Face(g_ft,
                           static_cast<const FT_Byte*>(fontData),
                           static_cast<FT_Long>(fontDataSize),
                           0, &face) != 0) {
        FP4_ERROR(LogCategory::Sce)
            << "FreeType rejected the font data (" << fontDataSize << " bytes)";
        return kErrInvalidArg;
    }
    const auto h = g_nextHandle++;
    g_fonts[h] = FontEntry{ face };
    *outLibrary = reinterpret_cast<void*>(static_cast<std::uintptr_t>(h));
    FP4_DEBUG(LogCategory::Sce)
        << "sceFontCreateLibraryFromMemory -> handle=" << h;
    return kOk;
#else
    (void)outLibrary; (void)fontData; (void)fontDataSize;
    FP4_UNIMPLEMENTED(LogCategory::Sce, "sceFontCreateLibraryFromMemory");
    FP4_ERROR(LogCategory::Sce)
        << "  reason=freetype not linked into this build "
        << "(FUSIONPS4_HAVE_FREETYPE=0); rebuild with freetype2";
    return kErrNotSupport;
#endif
}

int sceFontDestroyLibrary(void* library) {
#if FUSIONPS4_HAVE_FREETYPE
    if (!library) return kErrInvalidArg;
    const auto h = static_cast<std::uint64_t>(
        reinterpret_cast<std::uintptr_t>(library));
    std::lock_guard lock(g_mutex);
    auto it = g_fonts.find(h);
    if (it == g_fonts.end()) return kErrNotFound;
    FT_Done_Face(it->second.face);
    g_fonts.erase(it);
    return kOk;
#else
    (void)library;
    return kErrNotSupport;
#endif
}

// sceFontGetCharGlyphImage renders one glyph into a caller buffer.
// Layout of the guest parameter block is SDK-specific; we accept the
// common form [library ptr][codepoint u32][size pt u32][out ptr]
int sceFontGetCharGlyphImage(void* library, std::uint32_t codepoint,
                             std::uint32_t pixelSize,
                             void* outBitmap, std::uint32_t outStride,
                             std::uint32_t outHeight) {
#if FUSIONPS4_HAVE_FREETYPE
    if (!library || !outBitmap) return kErrInvalidArg;
    const auto h = static_cast<std::uint64_t>(
        reinterpret_cast<std::uintptr_t>(library));

    std::lock_guard lock(g_mutex);
    auto it = g_fonts.find(h);
    if (it == g_fonts.end()) return kErrNotFound;

    if (FT_Set_Pixel_Sizes(it->second.face, 0, pixelSize) != 0) {
        return kErrInvalidArg;
    }
    if (FT_Load_Char(it->second.face, codepoint, FT_LOAD_RENDER) != 0) {
        return kErrInvalidArg;
    }

    const auto& bmp = it->second.face->glyph->bitmap;
    const auto rows = std::min<std::uint32_t>(bmp.rows, outHeight);
    auto* dst = static_cast<std::uint8_t*>(outBitmap);
    for (std::uint32_t y = 0; y < rows; ++y) {
        const auto src = bmp.buffer + y * bmp.pitch;
        std::memcpy(dst + y * outStride, src, bmp.width);
        if (outStride > bmp.width) {
            std::memset(dst + y * outStride + bmp.width, 0,
                        outStride - bmp.width);
        }
    }
    return kOk;
#else
    (void)library; (void)codepoint; (void)pixelSize;
    (void)outBitmap; (void)outStride; (void)outHeight;
    return kErrNotSupport;
#endif
}

} // extern "C"

} // namespace

void SceFont::registerExports(SceStubTable& t) {
    t.registerStub("libSceFont", "sceFontInit",
                   reinterpret_cast<void*>(&sceFontInit));
    t.registerStub("libSceFont", "sceFontDone",
                   reinterpret_cast<void*>(&sceFontDone));
    t.registerStub("libSceFont", "sceFontCreateLibraryFromMemory",
                   reinterpret_cast<void*>(&sceFontCreateLibraryFromMemory));
    t.registerStub("libSceFont", "sceFontDestroyLibrary",
                   reinterpret_cast<void*>(&sceFontDestroyLibrary));
    t.registerStub("libSceFont", "sceFontGetCharGlyphImage",
                   reinterpret_cast<void*>(&sceFontGetCharGlyphImage));
}

} // namespace fusionps4::sce::font
