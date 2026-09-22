#pragma once
#include "sce/SceLibrary.hpp"
namespace fusionps4::sce::savedata {
class SceSaveData : public SceLibrary {
public:
    static SceSaveData& instance();
    const char* name() const override { return "libSceSaveData"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;
private:
    SceSaveData() = default;
    bool m_initialized = false;
};
} // namespace
