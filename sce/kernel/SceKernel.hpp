#pragma once

#include "sce/SceLibrary.hpp"

namespace fusionps4::sce::kernel {

// Implements libSceKernel's user-visible surface for the pieces that do
// not belong to the syscall layer: semaphores, event queues, thread
// creation wrappers, and the sceKernelPrintf family.
class SceKernel : public SceLibrary {
public:
    static SceKernel& instance();

    const char* name() const override { return "libSceKernel"; }

    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;

private:
    SceKernel() = default;
    bool m_initialized = false;
};

} // namespace fusionps4::sce::kernel
