#pragma once

#include "sce/SceLibrary.hpp"

namespace fusionps4::sce::ssl {

class SceSsl : public SceLibrary {
public:
    static SceSsl& instance();
    const char* name() const override { return "libSceSsl"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;

private:
    SceSsl() = default;
    bool m_initialized = false;
};

} // namespace fusionps4::sce::ssl
