#pragma once
#include "sce/SceLibrary.hpp"
namespace fusionps4::sce::np {
class SceNpTrophy : public SceLibrary {
public:
    static SceNpTrophy& instance();
    const char* name() const override { return "libSceNpTrophy"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;
private:
    SceNpTrophy() = default;
    bool m_initialized = false;
};
} // namespace
