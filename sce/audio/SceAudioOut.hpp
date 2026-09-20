#pragma once

#include "sce/SceLibrary.hpp"

namespace fusionps4::sce::audio {

class SceAudioOut : public SceLibrary {
public:
    static SceAudioOut& instance();

    const char* name() const override { return "libSceAudioOut"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;

private:
    SceAudioOut() = default;
    bool m_initialized = false;
};

} // namespace fusionps4::sce::audio
