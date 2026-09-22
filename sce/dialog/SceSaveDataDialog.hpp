#pragma once
#include "sce/SceLibrary.hpp"
namespace fusionps4::sce::dialog {
class SceSaveDataDialog : public SceLibrary {
public:
    static SceSaveDataDialog& instance();
    const char* name() const override { return "libSceSaveDataDialog"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;
private:
    SceSaveDataDialog() = default;
    bool m_initialized = false;
};
} // namespace
