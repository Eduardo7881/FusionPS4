#pragma once

#include "sce/SceLibrary.hpp"

namespace fusionps4::sce::playgo {

class ScePlayGo : public SceLibrary {
public:
    static ScePlayGo& instance();
    const char* name() const override { return "libScePlayGo"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;

private:
    ScePlayGo() = default;
    bool m_initialized = false;
};

} // namespace fusionps4::sce::playgo
