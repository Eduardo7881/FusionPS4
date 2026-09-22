#pragma once

#include "sce/SceLibrary.hpp"

namespace fusionps4::sce::videodec {

class SceVideodec : public SceLibrary {
public:
    static SceVideodec& instance();
    const char* name() const override { return "libSceVideodec"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;

private:
    SceVideodec() = default;
    bool m_initialized = false;
};

} // namespace fusionps4::sce::videodec
