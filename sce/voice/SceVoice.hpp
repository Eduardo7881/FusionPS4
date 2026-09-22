#pragma once

#include "sce/SceLibrary.hpp"

namespace fusionps4::sce::voice {

class SceVoice : public SceLibrary {
public:
    static SceVoice& instance();
    const char* name() const override { return "libSceVoice"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;

private:
    SceVoice() = default;
    bool m_initialized = false;
};

} // namespace fusionps4::sce::voice
