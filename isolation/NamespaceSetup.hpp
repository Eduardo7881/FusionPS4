#pragma once

#include "isolation/IsolationConfig.hpp"

namespace fusionps4::isolation {

// Sets up Linux namespaces for the child process. Must be called after
// fork() and before seccomp is installed. All operations are best-effort
// if the corresponding config flag is false; if a flag is true and the
// operation fails, the function returns false.
class NamespaceSetup {
public:
    // Creates namespaces via unshare(2). This does not need a helper
    // process because CLONE_NEWUSER grants the child privileges inside the
    // new user namespace, which allows the other namespaces to be created
    // afterwards.
    static bool unshareAll(const IsolationConfig& cfg);

    // Sets the guest hostname (UTS namespace must already exist).
    static bool setHostname(const IsolationConfig& cfg);

    // Mounts a fresh tmpfs as the new root and pivot_root's into it. The
    // virtual filesystem content is bind-mounted into the new root at
    // well-known paths. After this call, the guest cannot see any host
    // path outside the jail.
    static bool pivotIntoJail(const IsolationConfig& cfg);
};

} // namespace fusionps4::isolation
