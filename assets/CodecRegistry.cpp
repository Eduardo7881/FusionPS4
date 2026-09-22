#include "assets/CodecRegistry.hpp"

#include "debug/Log.hpp"

#include <sstream>

using fusionps4::debug::LogCategory;

// Feature macros set by CMake based on find_package() results.
// They are defined as 0 when the dependency is not available.

#ifndef FUSIONPS4_HAVE_MPG123
#  define FUSIONPS4_HAVE_MPG123 0
#endif
#ifndef FUSIONPS4_HAVE_JPEG
#  define FUSIONPS4_HAVE_JPEG 0
#endif
#ifndef FUSIONPS4_HAVE_PNG
#  define FUSIONPS4_HAVE_PNG 0
#endif
#ifndef FUSIONPS4_HAVE_FREETYPE
#  define FUSIONPS4_HAVE_FREETYPE 0
#endif
#ifndef FUSIONPS4_HAVE_CURL
#  define FUSIONPS4_HAVE_CURL 0
#endif
#ifndef FUSIONPS4_HAVE_OPENSSL
#  define FUSIONPS4_HAVE_OPENSSL 0
#endif
#ifndef FUSIONPS4_HAVE_VPX
#  define FUSIONPS4_HAVE_VPX 0
#endif
#ifndef FUSIONPS4_HAVE_AVCODEC
#  define FUSIONPS4_HAVE_AVCODEC 0
#endif

namespace fusionps4::assets {

CodecRegistry& CodecRegistry::instance() {
    static CodecRegistry r;
    return r;
}

void CodecRegistry::scanBuildFeatures() {
    hasMp3      = FUSIONPS4_HAVE_MPG123   != 0;
    hasJpeg     = FUSIONPS4_HAVE_JPEG     != 0;
    hasPng      = FUSIONPS4_HAVE_PNG      != 0;
    hasFreetype = FUSIONPS4_HAVE_FREETYPE != 0;
    hasCurl     = FUSIONPS4_HAVE_CURL     != 0;
    hasOpenssl  = FUSIONPS4_HAVE_OPENSSL  != 0;
    hasVpx      = FUSIONPS4_HAVE_VPX      != 0;
    hasH264     = FUSIONPS4_HAVE_AVCODEC  != 0;

    // These are never available: no public AT9 decoder exists and the
    // runtime will not ship one invented from scratch.
    hasAt9      = false;
    hasVorbis   = false;

    FP4_INFO(LogCategory::Sce) << "Codec capabilities: " << summary();
}

std::string CodecRegistry::summary() const {
    auto mark = [](bool v) { return v ? "yes" : "no"; };
    std::ostringstream oss;
    oss << "mp3=" << mark(hasMp3)
        << " at9=" << mark(hasAt9)
        << " vorbis=" << mark(hasVorbis)
        << " jpeg=" << mark(hasJpeg)
        << " png=" << mark(hasPng)
        << " freetype=" << mark(hasFreetype)
        << " curl=" << mark(hasCurl)
        << " openssl=" << mark(hasOpenssl)
        << " vpx=" << mark(hasVpx)
        << " h264=" << mark(hasH264);
    return oss.str();
}

} // namespace fusionps4::assets
