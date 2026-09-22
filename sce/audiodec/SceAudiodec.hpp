#pragma once

#include "sce/SceLibrary.hpp"

namespace fusionps4::sce::audiodec {

class SceAudiodec : public SceLibrary {
public:
    static SceAudiodec& instance();

    const char* name() const override { return "libSceAudiodec"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;

private:
    SceAudiodec() = default;
    bool m_initialized = false;
};

} // namespace fusionps4::sce::audiodec
