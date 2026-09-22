#pragma once

#include "sce/SceLibrary.hpp"

namespace fusionps4::sce::camera {

class SceCamera : public SceLibrary {
public:
    static SceCamera& instance();

    const char* name() const override { return "libSceCamera"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;

private:
    SceCamera() = default;
    bool m_initialized = false;
};

} // namespace fusionps4::sce::camera
