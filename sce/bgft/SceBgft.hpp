#pragma once
#include "sce/SceLibrary.hpp"
namespace fusionps4::sce::bgft {
class SceBgft : public SceLibrary {
public:
    static SceBgft& instance();
    const char* name() const override { return "libSceBgft"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;
private:
    SceBgft() = default;
    bool m_initialized = false;
};
} // namespace
