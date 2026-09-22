#pragma once

#include "sce/SceLibrary.hpp"

namespace fusionps4::sce::move {

class SceMove : public SceLibrary {
public:
    static SceMove& instance();

    const char* name() const override { return "libSceMove"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;

private:
    SceMove() = default;
    bool m_initialized = false;
};

} // namespace fusionps4::sce::move
