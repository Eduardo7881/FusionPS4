#pragma once
#include "sce/SceLibrary.hpp"
namespace fusionps4::sce::netctl {
class SceNetCtl : public SceLibrary {
public:
    static SceNetCtl& instance();
    const char* name() const override { return "libSceNetCtl"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;
private:
    SceNetCtl() = default;
    bool m_initialized = false;
};
} // namespace
