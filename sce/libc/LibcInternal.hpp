#pragma once

#include "sce/SceLibrary.hpp"

namespace fusionps4::sce::libc {

// libSceLibcInternal is what every PS4 title links against for its libc
// needs. It is not libc itself: it is a runtime library that forwards to
// the PS4 kernel's syscall layer. Here we reimplement it on top of the
// runtime's VFS + AddressSpace + HandleTable, so the guest sees correct
// semantics without ever touching the host libc.
class LibcInternal : public SceLibrary {
public:
    static LibcInternal& instance();

    const char* name() const override { return "libSceLibcInternal"; }
    bool initialize() override;
    void shutdown() override;
    void registerExports(SceStubTable& table) override;

private:
    LibcInternal() = default;
    bool m_initialized = false;
};

} // namespace fusionps4::sce::libc
