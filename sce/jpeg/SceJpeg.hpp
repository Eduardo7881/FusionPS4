#pragma once

#include "sce/SceLibrary.hpp"

namespace fusionps4::sce::jpeg {

class SceJpeg : public SceLibrary {
public:
    static SceJpeg& instance();
    const char* name() const override { return "libSceJpeg"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;

private:
    SceJpeg() = default;
    bool m_initialized = false;
};

} // namespace fusionps4::sce::jpeg
