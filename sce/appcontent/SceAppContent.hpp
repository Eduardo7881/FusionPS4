#pragma once

#include "sce/SceLibrary.hpp"

namespace fusionps4::sce::appcontent {

class SceAppContent : public SceLibrary {
public:
    static SceAppContent& instance();
    const char* name() const override { return "libSceAppContent"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;

private:
    SceAppContent() = default;
    bool m_initialized = false;
};

} // namespace fusionps4::sce::appcontent
