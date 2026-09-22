#pragma once

#include "sce/SceLibrary.hpp"

namespace fusionps4::sce::png {

class ScePng : public SceLibrary {
public:
    static ScePng& instance();
    const char* name() const override { return "libScePng"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;

private:
    ScePng() = default;
    bool m_initialized = false;
};

} // namespace fusionps4::sce::png
