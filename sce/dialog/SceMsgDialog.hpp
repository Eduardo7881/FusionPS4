#pragma once
#include "sce/SceLibrary.hpp"
namespace fusionps4::sce::dialog {
class SceMsgDialog : public SceLibrary {
public:
    static SceMsgDialog& instance();
    const char* name() const override { return "libSceMsgDialog"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;
private:
    SceMsgDialog() = default;
    bool m_initialized = false;
};
} // namespace
