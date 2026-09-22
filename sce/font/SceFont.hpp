#pragma once

#include "sce/SceLibrary.hpp"

namespace fusionps4::sce::font {

class SceFont : public SceLibrary {
public:
    static SceFont& instance();
    const char* name() const override { return "libSceFont"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;

private:
    SceFont() = default;
    bool m_initialized = false;
};

} // namespace fusionps4::sce::font
