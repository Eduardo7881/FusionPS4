# FreeBSD syscall map

The guest uses FreeBSD 9 syscall numbers. FusionPS4 implements a subset;
anything not in this table is dispatched by `SyscallDispatcher::dispatch`,
which returns `ENOSYS` and reports `UNIMPLEMENTED` with the syscall number
and arguments.

Legend:

- **✓** — implemented with correct semantics.
- **~** — implemented with a documented limitation.
- **✗** — not implemented; `ENOSYS` + `UNIMPLEMENTED`.

## File I/O

| # | Name | Status | Notes |
|---|---|---|---|
| 3 | read | ✓ | Handle-based, through `FileHandle`. |
| 4 | write | ✓ | Handle-based. |
| 5 | open | ✓ | VFS + `FsPolicy`; `O_DIRECTORY` yields a `DirectoryHandle`. |
| 6 | close | ✓ | Handle table; fd closed by the `HandleObject` destructor. |
| 10 | unlink | ✓ | Policy `Delete`. |
| 12 | chdir | ✗ | |
| 33 | access | ✗ | |
| 41 | dup | ✗ | |
| 42 | pipe | ✗ | |
| 58 | readlink | ✗ | |
| 92 | fcntl | ✗ | |
| 95 | fsync | ✓ | |
| 128 | rename | ✓ | Policy `Rename` on both sides. |
| 136 | mkdir | ✓ | Policy `Create`. |
| 137 | rmdir | ✓ | Policy `Delete`. |
| 188 | stat | ✓ | Translated to FreeBSD 9 `struct stat`. |
| 232 | getdirentries | ✗ | |
| 478 | lseek | ✓ | |
| 479 | truncate | ✗ | |
| 480 | ftruncate | ✓ | |
| 484 | fstat | ✓ | Accepts File/Directory/Socket handles. |

## Memory

| # | Name | Status | Notes |
|---|---|---|---|
| 69 | munmap | ✓ | |
| 70 | mprotect | ✓ | |
| 73 | mincore | ✗ | |
| 203 | mlock | ✗ | |
| 204 | munlock | ✗ | |
| 250 | minherit | ✗ | |
| 477 | mmap | ~ | Anonymous only. File-backed mappings return `ENOSYS`. |

## Threads and process

| # | Name | Status | Notes |
|---|---|---|---|
| 1 | exit | ✓ | Requests thread exit; runtime observes. |
| 20 | getpid | ✓ | Returns the `PS4Process` id. |
| 64 | getpagesize | ✓ | |
| 147 | setsid | ✗ | |
| 251 | rfork | ✗ | |
| 331 | thr_self | ✓ | |
| 332 | thr_exit | ✓ | |
| 433 | thr_kill | ✗ | |
| 434 | thr_suspend | ✓ | |
| 437 | thr_wake | ✓ | |
| 487 | thr_new | ✗ | |

## Time

| # | Name | Status | Notes |
|---|---|---|---|
| 116 | gettimeofday | ✓ | |

## Sockets

| # | Name | Status | Notes |
|---|---|---|---|
| 97  | socket | ~ | Handle-allocated; no policy check at socket creation. |
| 98  | connect | ✗ | The SCE path `sceNetConnect` handles policy. |
| 104 | bind | ✗ | |
| 106 | listen | ✗ | |

## SCE range

Numbers ≥ 500 are SCE syscalls. They are not registered; the SCE layer
uses trampolines that carry an SCE id in RAX instead of a syscall number.

## Implementing a new syscall

1. Add the number and name to `syscall/freebsd/FreeBsd.hpp`.
2. Write the handler in `syscall/handlers/Handlers.cpp`.
3. Register it in `registerFilesystemHandlers` / `registerMemoryHandlers`
   / `registerThreadHandlers` / `registerMiscHandlers`.
4. If the syscall touches a resource the runtime owns (filesystem,
   sockets, devices), route it through the corresponding manager rather
   than calling the Linux syscall directly.
5. Add a unit test in `tests/` if the handler has non-trivial logic.
