#pragma once

#include "sce/SceLibrary.hpp"

namespace fusionps4::sce::ult {

// libSceUlt provides low-level primitives used by every other SCE library
// on PS4: memory pools (fixed-size block allocators used for GPU buffers
// and DMA rings), waiting queues (lightweight intra-process event
// primitives that live on the caller's stack), and spinlocks.
//
// Because these primitives are supposed to be inlined into the caller on
// PS4, the guest typically calls only the initialization helpers; the hot
// path runs on guest memory. We provide the helpers plus enough state
// tracking that misuse is detected and reported, rather than silently
// corrupting guest memory.
class SceUlt : public SceLibrary {
public:
    static SceUlt& instance();

    const char* name() const override { return "libSceUlt"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;

private:
    SceUlt() = default;
    bool m_initialized = false;
};

} // namespace fusionps4::sce::ult
