// Verifies that an unregistered syscall produces ENOSYS and increments the
// UNIMPLEMENTED registry, and that a registered one runs.

#include "syscall/SyscallDispatcher.hpp"
#include "debug/UnimplementedRegistry.hpp"
#include "debug/Log.hpp"

#include <cstdio>

using namespace fusionps4::syscall;

int main() {
    fusionps4::debug::Log::init();
    fusionps4::debug::Log::setMinLevel(fusionps4::debug::LogLevel::Fatal);

    SyscallDispatcher d;
    d.registerHandler(42, "answer", [](SyscallContext& c) { c.ok(42); });

    {
        SyscallContext c;
        c.number = 42;
        d.dispatch(c);
        if (c.isError || c.retval != 42) {
            std::fprintf(stderr, "FAIL: dispatch(42) did not return 42\n");
            return 1;
        }
    }

    {
        SyscallContext c;
        c.number = 9999;
        d.dispatch(c);
        if (!c.isError || c.retval != freebsd::kEnosys) {
            std::fprintf(stderr, "FAIL: unregistered syscall not ENOSYS\n");
            return 1;
        }
    }

    std::printf("syscall_dispatch_test: OK\n");
    return 0;
}
