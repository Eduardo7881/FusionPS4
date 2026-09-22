#pragma once

#include "sce/SceLibrary.hpp"

namespace fusionps4::sce::np {

class SceNpManager : public SceLibrary {
public:
    static SceNpManager& instance();
    const char* name() const override { return "libSceNpManager"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;

private:
    SceNpManager() = default;
    bool m_initialized = false;
};

} // namespace fusionps4::sce::np
