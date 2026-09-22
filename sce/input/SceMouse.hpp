#pragma once

#include "sce/SceLibrary.hpp"

namespace fusionps4::sce::input {

class SceMouse : public SceLibrary {
public:
    static SceMouse& instance();

    const char* name() const override { return "libSceMouse"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;

private:
    SceMouse() = default;
    bool m_initialized = false;
};

} // namespace fusionps4::sce::input
