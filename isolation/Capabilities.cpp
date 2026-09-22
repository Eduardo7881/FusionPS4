#include "isolation/Capabilities.hpp"

#include "debug/Log.hpp"

#include <cerrno>
#include <cstring>
#include <linux/capability.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

using fusionps4::debug::LogCategory;

namespace fusionps4::isolation {

bool Capabilities::setNoNewPrivs() {
    if (::prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
        FP4_ERROR(LogCategory::Process)
            << "prctl(PR_SET_NO_NEW_PRIVS) failed: " << std::strerror(errno);
        return false;
    }
    return true;
}

bool Capabilities::dropAll() {
    // ---- 1. Drop every bounding-set capability ---------------------------
    // The bounding set limits what can ever be acquired through execve.
    // Iterate from 0 to the highest valid cap. CAP_LAST_CAP is read from
    // /proc; if we cannot read it, use a conservative upper bound.
    int lastCap = 63;
    if (FILE* f = std::fopen("/proc/sys/kernel/cap_last_cap", "r")) {
        if (std::fscanf(f, "%d", &lastCap) != 1) lastCap = 63;
        std::fclose(f);
    }
    for (int cap = 0; cap <= lastCap; ++cap) {
        if (::prctl(PR_CAPBSET_DROP, cap, 0, 0, 0) != 0) {
            if (errno == EINVAL) continue;    // cap not supported on kernel
            FP4_WARN(LogCategory::Process)
                << "PR_CAPBSET_DROP(" << cap << ") failed: "
                << std::strerror(errno);
        }
    }

    // ---- 2. Clear all effective, permitted and inheritable caps ----------
    struct __user_cap_header_struct hdr{};
    struct __user_cap_data_struct data[2]{};
    hdr.version = _LINUX_CAPABILITY_VERSION_3;
    hdr.pid = 0;
    if (::syscall(SYS_capset, &hdr, data) != 0) {
        FP4_ERROR(LogCategory::Process)
            << "capset() failed: " << std::strerror(errno);
        return false;
    }

    FP4_INFO(LogCategory::Process)
        << "capabilities dropped (bounding set cleared, "
        << "effective/permitted/inheritable zeroed)";
    return true;
}

} // namespace fusionps4::isolation
