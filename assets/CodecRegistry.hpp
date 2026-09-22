#pragma once

#include <string>

namespace fusionps4::assets {

// Central point that tells the SCE libraries which optional third-party
// backends are actually linked into this build. Populated once during
// Runtime::init() based on the compile-time feature macros set by CMake.
//
// The registry is intentionally a plain struct of booleans instead of a
// dynamic capability query: a library that is missing at build time cannot
// appear at runtime, and pretending otherwise would be a lie.
struct CodecRegistry {
    static CodecRegistry& instance();

    // Populated by scanBuildFeatures() at startup.
    bool hasMp3      = false;   // libmpg123
    bool hasAt9      = false;   // native AT9 decoder (currently unsupported)
    bool hasVorbis   = false;   // libvorbis
    bool hasJpeg     = false;   // libjpeg-turbo
    bool hasPng      = false;   // libpng
    bool hasFreetype = false;   // freetype2
    bool hasCurl     = false;   // libcurl
    bool hasOpenssl  = false;   // OpenSSL (used indirectly by libcurl)
    bool hasVpx      = false;   // libvpx (VP8/VP9)
    bool hasH264     = false;   // libavcodec / openh264

    // Human-readable summary, used in the startup log and in
    // UNIMPLEMENTED reports.
    std::string summary() const;

    // Fills the flags from compile-time macros. Called once, idempotent.
    void scanBuildFeatures();

private:
    CodecRegistry() = default;
};

} // namespace fusionps4::assets
