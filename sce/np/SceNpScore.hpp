#pragma once
#include "sce/SceLibrary.hpp"
namespace fusionps4::sce::np {
class SceNpScore : public SceLibrary {
public:
    static SceNpScore& instance();
    const char* name() const override { return "libSceNpScore"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;
private:
    SceNpScore() = default;
    bool m_initialized = false;
};
} // namespace
