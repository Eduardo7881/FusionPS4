#pragma once

#include "sce/SceLibrary.hpp"

namespace fusionps4::sce::sysmodule {

class SceSysmodule : public SceLibrary {
public:
    static SceSysmodule& instance();

    const char* name() const override { return "libSceSysmodule"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;

private:
    SceSysmodule() = default;
    bool m_initialized = false;
};

} // namespace fusionps4::sce::sysmodule
