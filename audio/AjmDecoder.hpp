#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace fusionps4::audio {

enum class AjmCodec {
    Unknown,
    Mp3,
    At9,
};

struct AjmPcmResult {
    std::vector<std::int16_t> samples;   // interleaved
    std::uint32_t             channels = 0;
    std::uint32_t             sampleRate = 0;
};

// Backend for decoding compressed audio. Delegates to libmpg123 for MP3;
// reports UNIMPLEMENTED for AT9 (no public decoder).
class AjmDecoder {
public:
    virtual ~AjmDecoder() = default;

    virtual bool decode(const void* in, std::size_t inSize,
                        AjmPcmResult& out) = 0;
    virtual AjmCodec codec() const = 0;

    // Factory: returns nullptr if the codec is not available.
    static std::unique_ptr<AjmDecoder> create(AjmCodec codec);
};

} // namespace fusionps4::audio
