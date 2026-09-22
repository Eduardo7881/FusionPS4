#pragma once
#include "sce/SceLibrary.hpp"
namespace fusionps4::sce::dialog {
class SceErrorDialog : public SceLibrary {
public:
    static SceErrorDialog& instance();
    const char* name() const override { return "libSceErrorDialog"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;
private:
    SceErrorDialog() = default;
    bool m_initialized = false;
};
} // namespace
