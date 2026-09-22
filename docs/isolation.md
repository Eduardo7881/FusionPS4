# Isolation model

FusionPS4 can run the guest in a child process that is isolated from the
host by Linux namespaces and seccomp. This mode is enabled by
`FUSIONPS4_ISOLATED=1`.

The goal is to make the runtime behave, from the guest's perspective, like
a hypervisor: the guest executes natively, but every interaction with the
host is mediated.

## Layers

### 1. Namespaces

The child creates the following namespaces via `unshare(2)`:

| Namespace | Effect |
|---|---|
| `CLONE_NEWUSER` | The child gets its own user namespace; it can then create the others without host privileges. |
| `CLONE_NEWPID` | The guest sees only its own processes. |
| `CLONE_NEWNS` | The guest's mount table is private. |
| `CLONE_NEWNET` | The guest has no interfaces; every network operation must go through the runtime. |
| `CLONE_NEWIPC` | No shared SysV IPC with the host. |
| `CLONE_NEWUTS` | The guest has its own hostname. |

### 2. Mount isolation

The child pivots into a tmpfs mount at `FUSIONPS4_ISO_JAIL` (default
`/tmp/fusionps4-jail`). It creates the standard directory skeleton
(`/app`, `/data`, `/system`, `/temp`, `/save`, `/user`, `/dev`, `/proc`,
`/tmp`) and bind-mounts a safe subset of `/dev`:

    /dev/null, /dev/zero, /dev/random, /dev/urandom

No other host path is visible. `/etc`, `/home`, `/usr`, `/var`,
`/dev/dri`, `/dev/snd`, `/dev/input` are all outside the jail.

If `pivot_root(2)` fails (for example, when the child lacks
`CAP_SYS_ADMIN` even inside its user namespace), the runtime falls back to
`chroot(2)`. This is logged as a warning: `chroot` is weaker (a
privileged process could escape) but combined with the capability drop it
remains effective.

### 3. Capabilities

`Capabilities::dropAll()`:

1. iterates from `0` to `/proc/sys/kernel/cap_last_cap` and drops each
   capability from the bounding set (`PR_CAPBSET_DROP`);
2. clears the effective, permitted and inheritable sets via `capset(2)`.

`Capabilities::setNoNewPrivs()` sets `PR_SET_NO_NEW_PRIVS`, which makes
`execve` never grant extra privileges.

### 4. seccomp-BPF

`SeccompFilter::install` installs a BPF program that returns
`SECCOMP_RET_TRAP` for every syscall not in the allowlist. The allowlist
contains only syscalls required by the trap handler itself:

    futex, rt_sigreturn, sigaltstack, restart_syscall, rseq,
    mmap, mprotect, munmap, exit, exit_group

None of these can access host resources.

The filter uses `SECCOMP_FILTER_FLAG_TSYNC`, so all threads of the child
inherit it. This makes it impossible for the guest to escape by spawning
a new thread.

### 5. Trap gate

`TrapGate::install` installs a `SIGSYS` handler with `SA_SIGINFO |
SA_ONSTACK | SA_NODEFER` and configures an alternate signal stack. Every
trapped syscall reaches this handler, which:

1. reads the syscall number and arguments from `ucontext_t`;
2. writes them into the shared `TrapRing`;
3. blocks until the runtime writes a response;
4. writes the response into RAX and sets or clears the carry flag to match
   the FreeBSD error convention.

### 6. Shared ring

`TrapRing` lives inside the `SharedArena` (the same `memfd` that backs the
guest's virtual memory). It has 256 slots. Producers (guest threads)
claim a slot with a CAS on a `std::atomic<uint32_t>` state field, write a
`TrapRequest`, and wake the server with a futex. The server writes a
`TrapResponse`, sets the slot state to `Done`, and wakes the guest thread
with another futex.

Memory ordering is enforced by the atomic on the state field: the request
payload is written before the state store with release semantics; the
response payload is read after the state load with acquire semantics.

## Request kinds

`TrapRequest.kind` distinguishes:

- `0` — a real FreeBSD syscall. Dispatched through `SyscallDispatcher`.
- `1` — an SCE function call. Dispatched through `SceStubTable::resolveById`.

SCE calls use `SCE_CALL_BASE = 0x50000000` as a marker; the guest's
trampolines write `SCE_CALL_BASE | id` into EAX before executing the
`syscall` instruction.

## What the guest cannot do (in isolated mode)

| Action | Result |
|---|---|
| `open("/etc/passwd")` | The path does not exist inside the jail; rejected at `FsPolicy` too. |
| `kill(host_pid, ...)` | The PID namespace makes host PIDs invisible; `kill` cannot address them. |
| `socket(AF_INET)` + `connect` to a host address | The network namespace has no interfaces; every connection must be through `sceNetConnect`. |
| `mmap` a file from `/dev/dri` | The file does not exist in the jail. |
| Call any Linux syscall not in the allowlist | `SIGSYS` fires; the request is served by the runtime with policy checks. |
| Read host memory | Only the arena is shared. Everything else is COW after `fork`. |
| `ptrace` another process | Not allowlisted; trapped. |
| `openat` with `AT_FDCWD` to escape the jail | `openat` is trapped; the runtime translates the path through the VFS. |

## Limits of the model

- `pivot_root` requires `CAP_SYS_ADMIN` inside the child's user namespace.
  On kernels without unprivileged user namespaces, the runtime falls back
  to `chroot` and logs the weaker mode.
- The trap gate runs the handler on the guest's CPU. A guest that spends
  all its time in syscalls will be dominated by the trap round-trip. The
  design tolerates this because real workloads are dominated by
  computation and graphics.
- seccomp-BPF is inherited across `fork`. The runtime does not try to
  spawn sub-processes on the guest's behalf while the filter is active.

## Reference

- `isolation/IsolationConfig.hpp` — policy flags and defaults.
- `isolation/NamespaceSetup.cpp` — namespace creation and jail setup.
- `isolation/Capabilities.cpp` — capability drop.
- `isolation/SeccompFilter.cpp` — BPF program construction.
- `isolation/SharedArena.cpp` — memfd arena.
- `syscall/trap/TrapRing.cpp` — shared ring.
- `syscall/trap/TrapGate.cpp` — SIGSYS handler.
- `syscall/trap/TrapServer.cpp` — runtime-side dispatcher thread.
- `runtime/GuestProcess.cpp` — `fork`, isolation, entry jump.
