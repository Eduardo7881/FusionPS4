#include "filesystem/virtual/VirtualFileSystem.hpp"

#include "debug/Log.hpp"
#include "syscall/freebsd/FreeBsd.hpp"

using fusionps4::debug::LogCategory;
using fusionps4::syscall::freebsd::kEacces;
using fusionps4::syscall::freebsd::kEnoent;
using fusionps4::syscall::freebsd::kEinval;
using fusionps4::syscall::freebsd::kEnotdir;

namespace fusionps4::filesystem::virtual_fs {

void VirtualFileSystem::configure(
    const std::string&                     vfsRoot,
    const std::vector<policy::MountPoint>& mounts) {
    m_policy.configure(mounts, vfsRoot);
}

Translation VirtualFileSystem::resolve(const std::string& guestPath,
                                       policy::FsOp       op,
                                       std::int64_t&      outErrno) const {
    Translation tr;
    tr.guestPath = guestPath;
    outErrno = 0;

    std::string canonical;
    if (!VirtualPath::normalize(guestPath, canonical)) {
        FP4_WARN(LogCategory::Fs)
            << "invalid guest path: \"" << guestPath << "\"";
        outErrno = kEinval;
        return tr;
    }

    const auto* mp = m_policy.match(canonical);
    if (!mp) {
        FP4_DEBUG(LogCategory::Fs)
            << "no mount for \"" << canonical << "\"";
        outErrno = kEnoent;
        return tr;
    }

    // Strip the mount prefix; the remainder (with leading '/') is the
    // suffix passed to VirtualPath::translate.
    std::string suffix = canonical.substr(mp->guestPrefix.size());
    if (suffix.empty()) suffix = "/";

    std::string hostPath;
    if (!VirtualPath::translate(suffix, m_policy.vfsRoot(),
                                mp->hostSubdir, hostPath)) {
        outErrno = kEinval;
        return tr;
    }

    std::string reason;
    if (!m_policy.isAllowed(canonical, op, &reason)) {
        FP4_WARN(LogCategory::Fs)
            << "policy denied " << static_cast<int>(op) << " on \""
            << canonical << "\": " << reason;
        // Distinguish "no permission" from "not found" so the guest sees
        // sane semantics. Mount mismatches are reported by match() above.
        outErrno = kEacces;
        return tr;
    }

    tr.ok       = true;
    tr.hostPath = hostPath;
    tr.mount    = mp;
    return tr;
}

} // namespace fusionps4::filesystem::virtual_fs
