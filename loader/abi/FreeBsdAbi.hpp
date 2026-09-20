#pragma once

#include <cstdint>

// PS4 runs a FreeBSD-derived kernel. Its userland code (games, system
// libraries) assumes FreeBSD errno values, FreeBSD syscall semantics, and
// the FreeBSD x86_64 ABI (System V with FreeBSD-specific tweaks).
//
// This header collects the constants we need to translate between the
// guest's expectations and the Linux host. The syscall *dispatcher* itself
// lives in syscall/ and will be added in Phase 3; here we provide just the
// error codes so loader-adjacent code can express failures the way the
// guest expects.

namespace fusionps4::loader::abi {

// FreeBSD errno values. Only the ones relevant to loader-adjacent APIs are
// listed; Phase 3 will extend this set.
enum class Errno : int {
    Ok              = 0,
    Perm            = 1,
    NoEnt           = 2,
    Srch            = 3,
    Intr            = 4,
    Io              = 5,
    NxIo            = 6,
    TooBig          = 7,
    NoExec          = 8,
    BadF            = 9,
    NoChild         = 10,
    Again           = 11,
    NoMem           = 12,
    Acces           = 13,
    Fault           = 14,
    NotBlk          = 15,
    Busy            = 16,
    Exists          = 17,
    XDev            = 18,
    NoDev           = 19,
    NotDir          = 20,
    IsDir           = 21,
    Inval           = 22,
    NFile           = 23,
    MFile           = 24,
    NotTty          = 25,
    TxtBsy          = 26,
    FBig            = 27,
    NoSpc           = 28,
    Spipe           = 29,
    Rofs            = 30,
    MLink           = 31,
    Pipe            = 32,
    Dom             = 33,
    Range           = 34,
    NoMsg           = 35,
    // Keep this code above in sync with FreeBSD <sys/errno.h>.
};

// PS4's kernel is FreeBSD 9-based. The syscall numbers themselves will be
// defined in syscall/freebsd/SyscallNumbers.hpp during Phase 3, since they
// belong to the dispatcher, not the loader.

// Program header PT_NOTE type used by PS4 for module metadata.
constexpr std::uint32_t kNoteTypeSceModuleInfo  = 0x61000001;

} // namespace fusionps4::loader::abi
