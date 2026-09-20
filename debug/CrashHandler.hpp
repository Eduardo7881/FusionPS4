#pragma once

#include "debug/Symbolizer.hpp"

#include <cstdint>

namespace fusionps4::debug {

// Installs signal handlers for SIGSEGV, SIGBUS, SIGILL, SIGFPE and
// SIGABRT. On crash, prints:
//   * the signal, address, and faulting PC,
//   * a hex dump of the x86_64 general-purpose registers plus RIP/EFLAGS,
//   * a symbolicated backtrace using the provided Symbolizer (which the
//     caller binds to the process's ModuleRegistry),
//   * the current UNIMPLEMENTED report (frequently the cause of a crash is
//     an unimplemented stub that returned garbage).
// Then re-raises the signal so the kernel produces a core dump.
class CrashHandler {
public:
    // Must be called once at startup. `sym` is copied; its module registry
    // pointer must remain valid for the process lifetime.
    static void install(Symbolizer sym);

    // Optional: called before the report, so a caller can pause audio and
    // flush logs.
    using PreReportHook = void(*)();
    static void setPreReportHook(PreReportHook h);

    // Optional: writes the report to a file (in addition to stderr).
    static void setReportFile(const char* path);
};

} // namespace fusionps4::debug
