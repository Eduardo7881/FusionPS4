#pragma once

#include "sce/SceLibrary.hpp"

namespace fusionps4::sce::input {

class SceKeyboard : public SceLibrary {
public:
    static SceKeyboard& instance();

    const char* name() const override { return "libSceKeyboard"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;

private:
    SceKeyboard() = default;
    bool m_initialized = false;
};

} // namespace fusionps4::sce::input
