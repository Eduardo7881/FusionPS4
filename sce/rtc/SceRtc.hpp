#pragma once

#include "sce/SceLibrary.hpp"

namespace fusionps4::sce::rtc {

// libSceRtc exposes the PS4's monotonic and wall-clock time services. The
// time base is 64-bit "ticks" measured in microseconds since 0001-01-01
// 00:00:00 UTC (proleptic Gregorian), matching the platform's convention.
class SceRtc : public SceLibrary {
public:
    static SceRtc& instance();

    const char* name() const override { return "libSceRtc"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;

private:
    SceRtc() = default;
    bool m_initialized = false;
};

} // namespace fusionps4::sce::rtc
