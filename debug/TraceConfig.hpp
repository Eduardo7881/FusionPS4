#pragma once

#include "debug/Log.hpp"

#include <string>
#include <vector>

namespace fusionps4::debug {

// Parses the FUSIONPS4_TRACE environment variable and reconfigures the
// Log's per-category enable flags. Accepted syntax:
//
//     FUSIONPS4_TRACE=syscall,sce,gnm          # enable only these
//     FUSIONPS4_TRACE=all                      # enable everything
//     FUSIONPS4_TRACE=none                     # disable everything
//     FUSIONPS4_TRACE=all,-syscall,-fs         # all except these
//
// Category names are the lowercase of LogCategory's name (host, loader,
// syscall, sce, fs, input, audio, network, memory, graphics, vulkan,
// opengl, thread, process, handle, error).
class TraceConfig {
public:
    // Apply the value of FUSIONPS4_TRACE if set. Idempotent.
    static void applyFromEnvironment();

    // Parse a specification string. Returns false if an unknown category
    // is named (still applies what it could).
    static bool apply(const std::string& spec);

    // Names of every known category, in LogCategory order.
    static std::vector<std::string> categoryNames();

private:
    static bool categoryFromName(const std::string& name, LogCategory& out);
};

} // namespace fusionps4::debug
