#pragma once
#include "sce/SceLibrary.hpp"
namespace fusionps4::sce::dialog {
class SceImeDialog : public SceLibrary {
public:
    static SceImeDialog& instance();
    const char* name() const override { return "libSceImeDialog"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;
private:
    SceImeDialog() = default;
    bool m_initialized = false;
};
} // namespace
