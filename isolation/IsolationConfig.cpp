#include "isolation/IsolationConfig.hpp"

#include "debug/Log.hpp"

#include <cstdlib>
#include <cstring>

using fusionps4::debug::LogCategory;

namespace fusionps4::isolation {

namespace {

bool envBool(const char* name, bool fallback) {
    const char* v = std::getenv(name);
    if (!v) return fallback;
    return std::strcmp(v, "0") != 0 && std::strcmp(v, "false") != 0 &&
           std::strcmp(v, "no") != 0;
}

std::string envStr(const char* name, const std::string& fallback) {
    const char* v = std::getenv(name);
    return v ? v : fallback;
}

} // namespace

IsolationConfig loadIsolationConfigFromEnvironment() {
    IsolationConfig c;

    c.use_user_ns  = envBool("FUSIONPS4_ISO_USER_NS",  c.use_user_ns);
    c.use_pid_ns   = envBool("FUSIONPS4_ISO_PID_NS",   c.use_pid_ns);
    c.use_mount_ns = envBool("FUSIONPS4_ISO_MOUNT_NS", c.use_mount_ns);
    c.use_net_ns   = envBool("FUSIONPS4_ISO_NET_NS",   c.use_net_ns);
    c.use_ipc_ns   = envBool("FUSIONPS4_ISO_IPC_NS",   c.use_ipc_ns);
    c.use_uts_ns   = envBool("FUSIONPS4_ISO_UTS_NS",   c.use_uts_ns);

    c.drop_capabilities = envBool("FUSIONPS4_ISO_DROP_CAPS",   c.drop_capabilities);
    c.no_new_privs      = envBool("FUSIONPS4_ISO_NO_NEW_PRIVS", c.no_new_privs);
    c.trap_all_syscalls = envBool("FUSIONPS4_ISO_TRAP_ALL",     c.trap_all_syscalls);

    c.guest_hostname = envStr("FUSIONPS4_ISO_HOSTNAME", c.guest_hostname);
    c.jail_root      = envStr("FUSIONPS4_ISO_JAIL",     c.jail_root);

    c.trace_syscalls   = envBool("FUSIONPS4_ISO_TRACE_SYSCALLS", false);
    c.trace_sce_calls  = envBool("FUSIONPS4_ISO_TRACE_SCE",      false);

    c.abort_on_isolation_failure =
        envBool("FUSIONPS4_ISO_STRICT", true);

    FP4_INFO(LogCategory::Process)
        << "Isolation config: user=" << c.use_user_ns
        << " pid=" << c.use_pid_ns
        << " mount=" << c.use_mount_ns
        << " net=" << c.use_net_ns
        << " ipc=" << c.use_ipc_ns
        << " uts=" << c.use_uts_ns
        << " drop_caps=" << c.drop_capabilities
        << " no_new_privs=" << c.no_new_privs
        << " strict=" << c.abort_on_isolation_failure;
    return c;
}

} // namespace fusionps4::isolation
