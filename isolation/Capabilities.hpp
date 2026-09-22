#pragma once

namespace fusionps4::isolation {

// Removes every capability from the calling process and forbids regaining
// any via execve. This is a one-way operation; the caller must not need
// privileges afterwards.
class Capabilities {
public:
    // Returns true on success. On failure, logs the errno and returns false.
    // The caller is expected to abort if the IsolationConfig demands strict
    // isolation.
    static bool dropAll();

    // Sets PR_SET_NO_NEW_PRIVS. Idempotent.
    static bool setNoNewPrivs();
};

} // namespace fusionps4::isolation
