#pragma once

#include "sce/SceLibrary.hpp"

namespace fusionps4::sce::random {

// libSceRandom provides sceRandomGetRandomNumber and friends. Games use it
// for seeding their own PRNGs and for session tokens; we cannot afford to
// return predictable values, so this is a ChaCha20-based DRBG seeded from
// the host's getrandom(2).
class SceRandom : public SceLibrary {
public:
    static SceRandom& instance();

    const char* name() const override { return "libSceRandom"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;

private:
    SceRandom() = default;
    bool m_initialized = false;
};

} // namespace fusionps4::sce::random
