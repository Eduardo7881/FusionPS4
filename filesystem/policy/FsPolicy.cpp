#include "filesystem/policy/FsPolicy.hpp"

#include "debug/Log.hpp"

#include <algorithm>

using fusionps4::debug::LogCategory;

namespace fusionps4::filesystem::policy {

FsPolicy::FsPolicy() {
    // Default mounts; the runtime may override via configure().
    m_vfsRoot = "virtual_fs";
    m_mounts  = {
        {"/app",    "app",    /*ro*/ true,  /*cr*/ false, /*del*/ false},
        {"/data",   "data",   /*ro*/ false, /*cr*/ true,  /*del*/ true},
        {"/system", "system", /*ro*/ true,  /*cr*/ false, /*del*/ false},
        {"/temp",   "temp",   /*ro*/ false, /*cr*/ true,  /*del*/ true},
        {"/save",   "save",   /*ro*/ false, /*cr*/ true,  /*del*/ true},
        {"/user",   "user",   /*ro*/ false, /*cr*/ true,  /*del*/ true},
    };
}

void FsPolicy::configure(std::vector<MountPoint> mounts, std::string vfsRoot) {
    m_mounts  = std::move(mounts);
    m_vfsRoot = std::move(vfsRoot);

    FP4_INFO(LogCategory::Fs)
        << "FsPolicy configured: vfsRoot=\"" << m_vfsRoot << "\" mounts="
        << m_mounts.size();
}

const MountPoint* FsPolicy::match(const std::string& guestPath) const {
    const MountPoint* best = nullptr;
    std::size_t bestLen = 0;
    for (const auto& mp : m_mounts) {
        if (guestPath.size() < mp.guestPrefix.size()) continue;
        if (guestPath.compare(0, mp.guestPrefix.size(), mp.guestPrefix) != 0)
            continue;
        // Ensure a component boundary follows (either end-of-string or '/').
        if (guestPath.size() > mp.guestPrefix.size() &&
            guestPath[mp.guestPrefix.size()] != '/') {
            continue;
        }
        if (mp.guestPrefix.size() > bestLen) {
            bestLen = mp.guestPrefix.size();
            best = &mp;
        }
    }
    return best;
}

bool FsPolicy::isAllowed(const std::string& guestPath,
                         FsOp               op,
                         std::string*       outReason) const {
    const MountPoint* mp = match(guestPath);
    if (!mp) {
        if (outReason) *outReason = "no mount matches path";
        return false;
    }

    auto deny = [&](const char* why) {
        if (outReason) *outReason = why;
        return false;
    };

    switch (op) {
        case FsOp::Read:
        case FsOp::List:
            return true;   // all mounts are readable

        case FsOp::Write:
        case FsOp::Truncate:
            return mp->readOnly ? deny("mount is read-only") : true;

        case FsOp::Create:
            if (mp->readOnly)   return deny("mount is read-only");
            if (!mp->allowCreate) return deny("create not permitted on mount");
            return true;

        case FsOp::Delete:
        case FsOp::Rename:
            if (mp->readOnly)   return deny("mount is read-only");
            if (!mp->allowDelete) return deny("delete not permitted on mount");
            return true;

        case FsOp::Chmod:
        case FsOp::Chown:
            return mp->readOnly ? deny("mount is read-only") : true;
    }
    return deny("unknown FsOp");
}

} // namespace fusionps4::filesystem::policy
