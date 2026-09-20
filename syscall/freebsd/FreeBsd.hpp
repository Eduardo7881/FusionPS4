#pragma once

#include <cstdint>

// PS4 runs a FreeBSD 9-derived kernel. This header collects the numeric
// constants and ABI-visible structures that PS4 userspace expects. The
// dispatcher (see syscall/SyscallDispatcher.hpp) uses these when converting
// between the guest's expectations and the Linux host's behaviour.

namespace fusionps4::syscall::freebsd {

// ---- x86_64 FreeBSD syscall numbers -------------------------------------
// The PS4 uses a superset of FreeBSD 9's x86_64 table. Numbers were
// cross-checked against the fail0verflow ps4-kernel disassembly and the
// public PS4 OpenSDK stubs. Numbers not listed here are treated as
// UNIMPLEMENTED by the dispatcher, rather than silently returning zero.
enum SyscallNumber : std::int64_t {
    kSysExit            = 1,
    kSysRead            = 3,
    kSysWrite           = 4,
    kSysOpen            = 5,
    kSysClose           = 6,
    kSysChdir           = 12,
    kSysGetPid          = 20,
    kSysAccess          = 33,
    kSysDup             = 41,
    kSysPipe            = 42,
    kSysIoctl           = 54,
    kSysReadLink        = 58,
    kSysUmask           = 60,
    kSysGetPageSize     = 64,
    kSysMunmap          = 69,
    kSysMprotect        = 70,
    kSysMincore         = 73,
    kSysGetGroups       = 74,
    kSysFcntl           = 92,
    kSysFsync           = 95,
    kSysSocket          = 97,
    kSysConnect         = 98,
    kSysBind            = 104,
    kSysListen          = 106,
    kSysGetTimeOfDay    = 116,
    kSysReadv           = 120,
    kSysWritev          = 121,
    kSysRename          = 128,
    kSysMkdir           = 136,
    kSysRmdir           = 137,
    kSysSetSid          = 147,
    kSysSysctl          = 202,
    kSysMlock           = 203,
    kSysMunlock         = 204,
    kSysPoll            = 209,
    kSysGetDirentries   = 232,
    kSysMinherit        = 250,
    kSysRfork           = 251,
    kSysIsSetUgid       = 253,
    kSysLchown          = 254,
    kSysThrSelf         = 331,   // FreeBSD thr_self
    kSysThrExit         = 332,
    kSysThrKill         = 433,
    kSysThrSuspend      = 434,
    kSysThrWake         = 437,
    kSysMmap            = 477,
    kSysLseek           = 478,
    kSysTruncate        = 479,
    kSysFtruncate       = 480,
    kSysFstat           = 484,
    kSysThrNew          = 487,
    // SCE-specific syscalls begin here. They are dispatched to the SCE
    // layer starting in Phase 4.
    kSysSceBase         = 500,
};

// ---- FreeBSD errno values -----------------------------------------------
// Kept in the same numeric order as FreeBSD <sys/errno.h> so that a direct
// cast of the guest's errno value can be mapped to its name in logs.
enum Errno : std::int64_t {
    kOk             = 0,
    kEperm          = 1,
    kEnoent         = 2,
    kEsrch          = 3,
    kEintr          = 4,
    kEio            = 5,
    kEnxio          = 6,
    kE2big          = 7,
    kEnoexec        = 8,
    kEbadf          = 9,
    kEchild         = 10,
    kEagain         = 11,
    kEnomem         = 12,
    kEacces         = 13,
    kEfault         = 14,
    kEnotblk        = 15,
    kEbusy          = 16,
    kEexists        = 17,
    kExdev          = 18,
    kEnodev         = 19,
    kEnotdir        = 20,
    kEisdir         = 21,
    kEinval         = 22,
    kEnfile         = 23,
    kEmfile         = 24,
    kEnotty         = 25,
    kEtxtbsy        = 26,
    kEfbig          = 27,
    kEnospc         = 28,
    kEspipe         = 29,
    kErofs          = 30,
    kEmlink         = 31,
    kEpipe          = 32,
    kEdom           = 33,
    kErange         = 34,
    kEdeadlk        = 35,
    kEnolck         = 77,
    kEnosys         = 78,
    kEnametoolong   = 63,
    kEnotempty      = 66,
    kElibbad        = 86,
    kEopnotsupp     = 45,
};

// Set the errno field of a SyscallContext. Handlers should prefer
// `ctx.fail(kEnoent)` over writing to ctx directly.
struct ErrnoName {
    static const char* name(std::int64_t e);
};

// ---- FreeBSD x86_64 ABI structures --------------------------------------

struct TimeSpec {
    std::int64_t tv_sec;
    std::int64_t tv_nsec;
};

struct TimeVal {
    std::int64_t tv_sec;
    std::int64_t tv_usec;
};

// struct stat as exposed to a FreeBSD 9 x86_64 userspace. Layout verified
// against <sys/stat.h> of FreeBSD 9.0-RELEASE; total size 144 bytes.
#pragma pack(push, 1)
struct Stat {
    std::uint64_t st_dev;
    std::uint64_t st_ino;
    std::uint64_t st_nlink;
    std::uint16_t st_mode;
    std::int16_t  st_padding0;
    std::uint32_t st_uid;
    std::uint32_t st_gid;
    std::int32_t  st_padding1;
    std::uint64_t st_rdev;
    TimeSpec      st_atim;
    TimeSpec      st_mtim;
    TimeSpec      st_ctim;
    std::int64_t  st_size;
    std::int64_t  st_blocks;
    std::uint32_t st_blksize;
    std::uint32_t st_flags;
    std::uint32_t st_gen;
    std::int32_t  st_lspare;
    std::int64_t  st_qspare[2];
};
#pragma pack(pop)

static_assert(sizeof(Stat) == 144, "FreeBSD 9 x86_64 stat must be 144 bytes");

// ---- FreeBSD open flags --------------------------------------------------
constexpr std::int32_t kO_RdOnly = 0x0000;
constexpr std::int32_t kO_WrOnly = 0x0001;
constexpr std::int32_t kO_RdWr   = 0x0002;
constexpr std::int32_t kO_AccMode= 0x0003;
constexpr std::int32_t kO_NonBlock = 0x0004;
constexpr std::int32_t kO_Append  = 0x0008;
constexpr std::int32_t kO_ShLock  = 0x0010;
constexpr std::int32_t kO_ExLock  = 0x0020;
constexpr std::int32_t kO_Async   = 0x0040;
constexpr std::int32_t kO_Direct  = 0x00010000;
constexpr std::int32_t kO_Directory = 0x00020000;
constexpr std::int32_t kO_Excl    = 0x00000800;
constexpr std::int32_t kO_Creat   = 0x00000200;
constexpr std::int32_t kO_Trunc   = 0x00000400;
constexpr std::int32_t kO_CloExec = 0x00100000;

// ---- FreeBSD mman flags --------------------------------------------------
constexpr std::int32_t kProtNone  = 0x0;
constexpr std::int32_t kProtRead  = 0x1;
constexpr std::int32_t kProtWrite = 0x2;
constexpr std::int32_t kProtExec  = 0x4;

constexpr std::int32_t kMapShared  = 0x0001;
constexpr std::int32_t kMapPrivate = 0x0002;
constexpr std::int32_t kMapFixed   = 0x0010;
constexpr std::int32_t kMapAnon    = 0x1000;

// ---- Mode bits -----------------------------------------------------------
constexpr std::uint16_t kS_Ifmt  = 0170000;
constexpr std::uint16_t kS_IfReg = 0100000;
constexpr std::uint16_t kS_IfDir = 0040000;
constexpr std::uint16_t kS_IfLnk = 0120000;
constexpr std::uint16_t kS_IfChr = 0020000;
constexpr std::uint16_t kS_IfBlk = 0060000;
constexpr std::uint16_t kS_IfFifo= 0010000;
constexpr std::uint16_t kS_IfSock= 0140000;

} // namespace fusionps4::syscall::freebsd
