#pragma once

#include "filesystem/policy/FsPolicy.hpp"
#include "filesystem/virtual/VirtualPath.hpp"

#include <cstdint>
#include <string>

namespace fusionps4::filesystem::virtual_fs {

// Result of translating a guest path into a host path plus the mount that
// governs it. Used by callers that need to apply policy before touching
// the host.
struct Translation {
    bool                             ok = false;
    std::string                      guestPath;
    std::string                      hostPath;
    const policy::MountPoint*        mount = nullptr;
};

class VirtualFileSystem {
public:
    VirtualFileSystem() = default;

    void configure(const std::string&                       vfsRoot,
                   const std::vector<policy::MountPoint>&   mounts);

    // Translate + policy check for a given op. On failure, fills outErrno
    // with the appropriate FreeBSD errno (already translated).
    Translation resolve(const std::string& guestPath,
                        policy::FsOp       op,
                        std::int64_t&      outErrno) const;

    const policy::FsPolicy& policy() const { return m_policy; }

private:
    policy::FsPolicy m_policy;
};

} // namespace fusionps4::filesystem::virtual_fs
