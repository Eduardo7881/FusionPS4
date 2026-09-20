#pragma once

#include <string>

namespace fusionps4::filesystem::virtual_fs {

struct ResolvedPath {
    // The path as the guest wrote it, normalized (leading '/', no "." or
    // ".." components remaining, no doubled slashes).
    std::string guestPath;

    // The corresponding host path, relative to the current working
    // directory, or empty if the path failed to resolve.
    std::string hostPath;

    bool ok = false;
};

// Path translation rules:
//   * The guest filesystem is rooted at "/". Every guest path is looked up
//     in the mount table (see FsPolicy).
//   * "." and ".." are resolved lexically, before mount matching, using a
//     stack algorithm. ".." never escapes the guest root.
//   * Paths containing embedded NUL, empty components (except the leading
//     root), or non-printable characters in components are rejected.
//   * Linux-specific constructs are NOT translated: "/etc", "/proc",
//     "/sys", "/dev", and anything else outside the mount table fail with
//     ENOENT — the guest cannot see them.
class VirtualPath {
public:
    // Normalize a guest path. Returns false if it is invalid (empty, has
    // embedded NUL, ...). On success, `out` receives the canonical form.
    static bool normalize(const std::string& input, std::string& out);

    // Translate to a host path using the mount table's `hostSubdir`,
    // anchored at `vfsRoot`. The result is always inside `vfsRoot`, thanks
    // to the earlier normalization step.
    static bool translate(const std::string& guestPath,
                          const std::string& vfsRoot,
                          const std::string& hostSubdir,
                          std::string&       outHostPath);
};

} // namespace fusionps4::filesystem::virtual_fs
