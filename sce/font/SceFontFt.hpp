#pragma once

#include "sce/SceLibrary.hpp"

namespace fusionps4::sce::font {

// libSceFontFt exposes a subset of FreeType's own API so that titles which
// link FreeType directly can use the same font objects. We route every
// call through the same FreeType instance used by libSceFont, so faces
// opened through either API are interchangeable.
class SceFontFt : public SceLibrary {
public:
    static SceFontFt& instance();
    const char* name() const override { return "libSceFontFt"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;

private:
    SceFontFt() = default;
    bool m_initialized = false;
};

} // namespace fusionps4::sce::font
