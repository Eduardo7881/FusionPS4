#pragma once

#include "sce/SceLibrary.hpp"

namespace fusionps4::sce::videoout {

class SceVideoOut : public SceLibrary {
public:
    static SceVideoOut& instance();

    const char* name() const override { return "libSceVideoOut"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;

private:
    SceVideoOut() = default;
    bool m_initialized = false;
};

} // namespace fusionps4::sce::videoout
