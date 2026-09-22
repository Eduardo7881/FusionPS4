#include "audio/AjmDecoder.hpp"

#include "assets/CodecRegistry.hpp"
#include "debug/Log.hpp"

#if FUSIONPS4_HAVE_MPG123
#  include <mpg123.h>
#endif

using fusionps4::debug::LogCategory;

namespace fusionps4::audio {

#if FUSIONPS4_HAVE_MPG123
namespace {

class Mp3Decoder : public AjmDecoder {
public:
    Mp3Decoder() {
        static bool initDone = false;
        if (!initDone) {
            mpg123_init();
            initDone = true;
        }
        int err = 0;
        m_handle = mpg123_new(nullptr, &err);
    }

    ~Mp3Decoder() override {
        if (m_handle) mpg123_delete(m_handle);
    }

    AjmCodec codec() const override { return AjmCodec::Mp3; }

    bool decode(const void* in, std::size_t inSize,
                AjmPcmResult& out) override {
        if (!m_handle || !in || inSize == 0) return false;

        int err = 0;
        if (mpg123_open_feed(m_handle) != MPG123_OK) {
            FP4_ERROR(LogCategory::Audio) << "mpg123_open_feed failed";
            return false;
        }

        out.samples.clear();
        out.channels   = 0;
        out.sampleRate = 0;

        const auto rc = mpg123_feed(m_handle,
                                    static_cast<const unsigned char*>(in),
                                    inSize);
        if (rc != MPG123_OK) {
            FP4_ERROR(LogCategory::Audio) << "mpg123_feed failed";
            return false;
        }

        long rate = 0;
        int  channels = 0, enc = 0;
        if (mpg123_getformat(m_handle, &rate, &channels, &enc) != MPG123_OK) {
            FP4_ERROR(LogCategory::Audio) << "mpg123_getformat failed";
            return false;
        }
        // Force signed 16-bit interleaved.
        mpg123_format_none(m_handle);
        mpg123_format(m_handle, rate, channels, MPG123_ENC_SIGNED_16);

        out.channels   = static_cast<std::uint32_t>(channels);
        out.sampleRate = static_cast<std::uint32_t>(rate);

        constexpr std::size_t kChunkSamples = 1152 * 4;
        std::vector<std::uint8_t> buf(kChunkSamples * 2);
        for (;;) {
            std::size_t got = 0;
            const int r = mpg123_read(m_handle, buf.data(), buf.size(), &got);
            if (got > 0) {
                const auto n = got / sizeof(std::int16_t);
                const auto* src = reinterpret_cast<const std::int16_t*>(
                    buf.data());
                out.samples.insert(out.samples.end(), src, src + n);
            }
            if (r == MPG123_DONE) break;
            if (r != MPG123_OK && r != MPG123_NEW_FORMAT) {
                FP4_WARN(LogCategory::Audio)
                    << "mpg123_read returned " << r;
                break;
            }
            if (got == 0) break;
        }

        FP4_DEBUG(LogCategory::Audio)
            << "Ajm MP3 decode: " << inSize << " bytes -> "
            << out.samples.size() << " samples @ " << out.channels << "ch, "
            << out.sampleRate << "Hz";
        return !out.samples.empty();
    }

private:
    mpg123_handle* m_handle = nullptr;
};

} // namespace
#endif  // FUSIONPS4_HAVE_MPG123

std::unique_ptr<AjmDecoder> AjmDecoder::create(AjmCodec codec) {
    switch (codec) {
        case AjmCodec::Mp3:
#if FUSIONPS4_HAVE_MPG123
            return std::make_unique<Mp3Decoder>();
#else
            FP4_ERROR(LogCategory::Audio)
                << "AjmDecoder: MP3 requested but libmpg123 is not linked "
                << "into this build (FUSIONPS4_HAVE_MPG123=0). "
                << "Rebuild with libmpg123 available to enable MP3 decode.";
            return nullptr;
#endif
        case AjmCodec::At9:
            FP4_UNIMPLEMENTED(LogCategory::Audio, "AjmDecoder(AT9)");
            FP4_ERROR(LogCategory::Audio)
                << "  reason=no public AT9 decoder exists; the runtime "
                << "will not implement a codec without specification. "
                << "Titles that rely on AT9 audio streams will produce "
                << "silence and report UNIMPLEMENTED for each decode call.";
            return nullptr;
        default:
            return nullptr;
    }
}

} // namespace fusionps4::audio
