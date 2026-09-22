#pragma once

#include <cstdint>
#include <string>

namespace fusionps4::loader::sce {

// On PS4, imported functions are identified by a NID (a 64-bit hash of the
// function's name, occasionally salted with the library name). Import
// tables in the ELF carry only the NID; the loader must map it back to a
// name to know which runtime stub to bind.
//
// We do not attempt to compute NIDs by guessing the salt. Instead we
// support:
//   1. A user-provided NID database file (text, one entry per line).
//   2. A small built-in fallback covering the libraries FusionPS4 already
//      implements (Part 4 + Part 9), so that simple test binaries work
//      without a database file.
//
// The database format is:
//
//     <nid-hex-16-chars> <library> <function>
//
// Lines starting with '#' are comments; blank lines are ignored. The
// library and function names use the PS4 convention ("libScePad",
// "scePadOpen").
class SceNid {
public:
    // Compute a NID assuming the modern scheme: NID = SHA256(name)[0:8],
    // interpreted as little-endian. Used by the built-in fallback table.
    static std::uint64_t computeLegacy(const std::string& functionName);

    // Convert to/from the 16-character lowercase hex form.
    static std::string   toHex(std::uint64_t nid);
    static bool          fromHex(const std::string& hex, std::uint64_t& out);
};

} // namespace fusionps4::loader::sce
