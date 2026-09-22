#pragma once

#include "sce/SceLibrary.hpp"

namespace fusionps4::sce::fiber {

class SceFiber : public SceLibrary {
public:
    static SceFiber& instance();

    const char* name() const override { return "libSceFiber"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;

private:
    SceFiber() = default;
    bool m_initialized = false;
};

} // namespace fusionps4::sce::fiber
