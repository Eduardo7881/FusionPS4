#pragma once

#include "sce/SceLibrary.hpp"

namespace fusionps4::sce::gameupdate {

class SceGameUpdate : public SceLibrary {
public:
    static SceGameUpdate& instance();
    const char* name() const override { return "libSceGameUpdate"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;

private:
    SceGameUpdate() = default;
    bool m_initialized = false;
};

} // namespace fusionps4::sce::gameupdate
