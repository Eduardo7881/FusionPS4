#pragma once
#include "sce/SceLibrary.hpp"
namespace fusionps4::sce::download {
class SceDownload : public SceLibrary {
public:
    static SceDownload& instance();
    const char* name() const override { return "libSceDownload"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;
private:
    SceDownload() = default;
    bool m_initialized = false;
};
} // namespace
