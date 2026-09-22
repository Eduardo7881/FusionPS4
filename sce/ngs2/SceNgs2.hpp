#pragma once

#include "sce/SceLibrary.hpp"

namespace fusionps4::sce::ngs2 {

class SceNgs2 : public SceLibrary {
public:
    static SceNgs2& instance();

    const char* name() const override { return "libSceNgs2"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;

    // Runtime-side access for other subsystems (sceAudioOut and sceAjm
    // route through the same graph so the guest sees consistent mixing).
    audio::Ngs2Graph& graph() { return *m_graph; }

private:
    SceNgs2() = default;
    bool                             m_initialized = false;
    std::unique_ptr<audio::Ngs2Graph> m_graph;
};

} // namespace fusionps4::sce::ngs2
