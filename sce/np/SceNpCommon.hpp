#pragma once
#include "sce/SceLibrary.hpp"
namespace fusionps4::sce::np {
class SceNpCommon : public SceLibrary {
public:
    static SceNpCommon& instance();
    const char* name() const override { return "libSceNpCommon"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;
private:
    SceNpCommon() = default;
    bool m_initialized = false;
};
} // namespace
