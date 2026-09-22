#pragma once
#include "sce/SceLibrary.hpp"
namespace fusionps4::sce::signin {
class SceSigninDialog : public SceLibrary {
public:
    static SceSigninDialog& instance();
    const char* name() const override { return "libSceSigninDialog"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;
private:
    SceSigninDialog() = default;
    bool m_initialized = false;
};
} // namespace
