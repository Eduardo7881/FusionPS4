#pragma once

#include "sce/SceLibrary.hpp"

namespace fusionps4::sce::http {

class SceHttp : public SceLibrary {
public:
    static SceHttp& instance();
    const char* name() const override { return "libSceHttp"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;

private:
    SceHttp() = default;
    bool m_initialized = false;
};

} // namespace fusionps4::sce::http
