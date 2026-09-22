#pragma once

#include "sce/SceLibrary.hpp"

namespace fusionps4::sce::screenshot {

class SceScreenShot : public SceLibrary {
public:
    static SceScreenShot& instance();
    const char* name() const override { return "libSceScreenShot"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;

private:
    SceScreenShot() = default;
    bool m_initialized = false;
};

} // namespace fusionps4::sce::screenshot
