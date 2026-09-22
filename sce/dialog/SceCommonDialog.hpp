#pragma once
#include "sce/SceLibrary.hpp"
namespace fusionps4::sce::dialog {
class SceCommonDialog : public SceLibrary {
public:
    static SceCommonDialog& instance();
    const char* name() const override { return "libSceCommonDialog"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;
private:
    SceCommonDialog() = default;
    bool m_initialized = false;
};
} // namespace
