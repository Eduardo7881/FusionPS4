#pragma once

#include "sce/SceLibrary.hpp"

namespace fusionps4::sce::gnm {

// The GNM layer tracks device state that SCE titles expect to persist
// across sceGnm* calls. It translates the calls into the Graphics
// Abstraction Layer. Phase 5 wires the basic surface: device lifecycle,
// render target binding, and a matching clear/draw call. Resource
// creation, shaders, pipelines and command buffer submission are
// Phase 6.
class SceGnm : public SceLibrary {
public:
    static SceGnm& instance();

    const char* name() const override { return "libSceGnm"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;

private:
    SceGnm() = default;
    bool m_initialized = false;
};

} // namespace fusionps4::sce::gnm
