#pragma once

namespace fusionps4::sce {

class SceStubTable;

// Every SCE library the runtime implements exposes its exports through this
// interface. The runtime constructs the full set during init and drives
// them in order.
class SceLibrary {
public:
    virtual ~SceLibrary() = default;

    virtual const char* name() const = 0;

    // Called once during PS4Process::init(), before guest code runs.
    // Return false to abort process init.
    virtual bool initialize() = 0;

    // Called during process shutdown, in reverse order of initialize().
    virtual void shutdown() = 0;

    // Called once during init, after initialize(). Must register every
    // exported stub into the table.
    virtual void registerExports(SceStubTable& table) = 0;
};

} // namespace fusionps4::sce
