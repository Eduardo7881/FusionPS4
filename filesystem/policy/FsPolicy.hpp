#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace fusionps4::filesystem::policy {

// Mount points exposed to the guest. Each mount maps a guest-visible path
// prefix to a subdirectory below the runtime's virtual_fs root.
struct MountPoint {
    std::string guestPrefix;   // e.g. "/app"
    std::string hostSubdir;    // e.g. "app" (relative to virtual_fs root)
    bool        readOnly = false;
    bool        allowCreate = false;
    bool        allowDelete = false;
};

// Operations the policy understands. Kept small on purpose: the policy is
// consulted at a handful of well-defined points (open, unlink, rename,
// mkdir, chmod) rather than on every byte of I/O.
enum class FsOp {
    Read,
    Write,
    Create,
    Delete,
    Rename,
    List,
    Chmod,
    Chown,
    Truncate,
};

class FsPolicy {
public:
    FsPolicy();

    // Configure the mount table. Must be called before first use.
    void configure(std::vector<MountPoint> mounts,
                   std::string           vfsRoot);

    const std::vector<MountPoint>& mounts() const { return m_mounts; }
    const std::string&             vfsRoot() const { return m_vfsRoot; }

    // Returns the mount whose guest prefix matches `guestPath`, longest
    // match wins. Returns nullptr if nothing matched.
    const MountPoint* match(const std::string& guestPath) const;

    // Check whether the guest is allowed to perform `op` on `guestPath`.
    // Fills `outReason` on failure.
    bool isAllowed(const std::string& guestPath,
                   FsOp               op,
                   std::string*       outReason = nullptr) const;

private:
    std::vector<MountPoint> m_mounts;
    std::string             m_vfsRoot;
};

} // namespace fusionps4::filesystem::policy
