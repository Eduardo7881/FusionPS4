#pragma once

#include "sce/SceLibrary.hpp"

namespace fusionps4::sce::pad {

class ScePad : public SceLibrary {
public:
    static ScePad& instance();

    const char* name() const override { return "libScePad"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;

private:
    ScePad() = default;
    bool m_initialized = false;
};

} // namespace fusionps4::sce::pad
