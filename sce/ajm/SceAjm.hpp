#pragma once

#include "sce/SceLibrary.hpp"

namespace fusionps4::sce::ajm {

class SceAjm : public SceLibrary {
public:
    static SceAjm& instance();

    const char* name() const override { return "libSceAjm"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;

private:
    SceAjm() = default;
    bool m_initialized = false;
};

} // namespace fusionps4::sce::ajm
