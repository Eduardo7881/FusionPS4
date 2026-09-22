#pragma once
#include "sce/SceLibrary.hpp"
namespace fusionps4::sce::share {
class SceShare : public SceLibrary {
public:
    static SceShare& instance();
    const char* name() const override { return "libSceShare"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;
private:
    SceShare() = default;
    bool m_initialized = false;
};
} // namespace
