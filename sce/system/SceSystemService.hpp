#pragma once

#include "sce/SceLibrary.hpp"

#include <cstdint>

namespace fusionps4::sce::system {

class SceSystemService : public SceLibrary {
public:
    static SceSystemService& instance();

    const char* name() const override { return "libSceSystemService"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;

private:
    SceSystemService() = default;
    bool m_initialized = false;
};

} // namespace fusionps4::sce::system
