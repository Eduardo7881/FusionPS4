#pragma once

#include "sce/SceLibrary.hpp"

namespace fusionps4::sce::net {

class SceNet : public SceLibrary {
public:
    static SceNet& instance();

    const char* name() const override { return "libSceNet"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;

private:
    SceNet() = default;
    bool m_initialized = false;
};

} // namespace fusionps4::sce::net
