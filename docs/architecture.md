# Architecture

## Execution model

FusionPS4 runs a PS4 application inside a *guest process*. The runtime
itself runs in a *host process*. Two execution modes are supported:

### In-process mode (default)

The guest runs inside the runtime process. Isolation is provided at the
language level by the `PS4Process` object: guest syscalls go through
`SyscallDispatcher`, guest SCE calls go through `SceStubTable`, guest
memory lives inside the `AddressSpace` (identity-mapped VMA over a memfd),
and guest handles live in a per-process `HandleTable`. This mode is used
for debugging and for building.

### Isolated mode (`FUSIONPS4_ISOLATED=1`)

The runtime forks a child process. Before jumping into the guest entry
point, the child:

1. creates a user namespace, PID namespace, mount namespace, network
   namespace, IPC namespace and UTS namespace;
2. chroots/pivot_roots into a tmpfs and mounts a fresh `/proc` (safe
   because the guest is in its own PID namespace);
3. drops every capability and sets `PR_SET_NO_NEW_PRIVS`;
4. installs a seccomp-BPF filter that traps every syscall except a short
   allowlist of syscalls the trap handler needs;
5. installs a `SIGSYS` handler that forwards trapped syscalls to the
   parent process through a shared lock-free ring.

The parent runs a `TrapServer` thread that drains the ring and dispatches
requests through the same `SyscallDispatcher` and `SceStubTable` used by
in-process mode.

Details are in [`isolation.md`](isolation.md).

## Memory

`AddressSpace` reserves 1 TiB at a fixed base (`0x0000'1000'0000'0000`)
using `MAP_FIXED_NOREPLACE`. The reservation is backed by a `memfd` that
is `MAP_SHARED`, so both the host process and (in isolated mode) the guest
process see the same bytes at the same virtual addresses.

This identity-mapping property is what allows PS4 code (native x86_64)
to be executed directly, without a binary translator: the guest can
dereference any pointer it constructs, and the runtime can inspect that
same memory at trap time by simple pointer arithmetic.

Guest VA space is partitioned into named regions by `AddressSpace::map`.
Every region is validated against the policy before being exposed to the
guest; the guest cannot allocate outside the arena.

## Frame lifecycle

    HostWindow events
        ↓
    InputManager::poll
        ↓
    Runtime::tick:
        ├── drain GnmCommandRecorder
        ├── dispatch each RecordedDraw via GraphicsDevice::cmd*
        ├── DialogManager::tick
        ├── OverlayRenderer (if a dialog is active)
        └── GraphicsDevice::presentFrame
        ↓
    repeat

`Runtime::tick` is the only place where GPU commands are submitted. The
guest's GNM stubs record into a FIFO; the runtime drains that FIFO inside
the frame's command buffer. The guest never sees a `VkCommandBuffer`.

## Subsystems

| Subsystem | Owner | Guest-visible API |
|---|---|---|
| Window | `Runtime` → `HostWindow` (SDL2) | `sceVideoOut*` |
| Rendering | `Runtime` → `GraphicsManager` → `VulkanDevice`/`OpenGLDevice` | `sceGnm*`, `sceGnmx*` |
| Filesystem | `PS4Process` → `VirtualFileSystem` | `sceLibcInternal::fopen`, syscalls |
| Input | `Runtime` → `InputManager` | `scePad*`, `sceKeyboard*`, `sceMouse*` |
| Audio | `Runtime` → `AudioManager` (SDL) | `sceAudioOut*`, `sceNgs2*`, `sceAjm*` |
| Networking | `Runtime` → `NetworkPolicy`, `HttpClient` | `sceNet*`, `sceHttp*` |
| Memory | `PS4Process` → `AddressSpace` | syscalls, `sceKernel*Memory` |
| Threads | `PS4Process` → `ThreadManager` | `scePthread*`, `sceFiber*` |
| Handles | `PS4Process` → `HandleTable` | opaque to the guest |
| Processes | `Runtime` → `PS4Process`, `GuestProcess` | `sceKernel*` |
| PSN | `Runtime` → `PsnBackend` (simulated) | `sceNp*`, `sceNpTrophy*` |
| Dialogs | `Runtime` → `DialogManager` + `OverlayRenderer` | `sce*Dialog` |

## SCE dispatch

Guest binaries import SCE functions by NID. The loader:

1. parses `DT_SCE_IMPORT_LIB` / `DT_SCE_IMPORT_LIB_ATTR` from the dynamic
   segment,
2. resolves each NID through `NidDatabase`,
3. looks up the qualified name (`libScePad::scePadOpen`) in
   `SceStubTable`,
4. writes a trampoline into a dedicated executable page of the guest's
   address space,
5. patches the guest's GOT slot with the trampoline address.

The trampoline is:

    mov r10, rcx
    mov eax, 0x50000000 | id
    syscall
    ret

The `syscall` instruction triggers `SIGSYS` in the isolated guest. The
trap handler reads RAX, sees the SCE marker, and forwards the call to the
runtime with the numeric id. The runtime invokes the registered C
function through `SceStubTable::resolveById`.

## What runs on which thread

| Thread | Work |
|---|---|
| Main | `Runtime::tick`, `HostWindow::processEvents`, GPU submission, overlay rendering |
| TrapServer | Drains `TrapRing`, dispatches syscalls and SCE calls |
| Ngs2 mixer | Mixes audio voices, pushes blocks into `AudioManager` |
| Ajm worker | Decodes audio jobs |
| Bgft worker | Performs VFS-to-VFS file transfers |
| Guest threads | One `pthread` per `scePthreadCreate` |

SCE calls arriving through the trap are serialized with the main thread
via a mutex, because they may touch the graphics device or the window.
Syscalls are dispatched without that mutex; they synchronize through
`AddressSpace` and `HandleTable`, which are already thread-safe.

## Overlay

`OverlayRenderer` draws dialogs on top of the guest frame:

- a `HostWindow`-owned swapchain image is the render target,
- vertices are pushed into a dynamic buffer,
- the font atlas is built once at init via FreeType from a system font,
- the pipeline is built from SPIR-V embedded at build time.

If any of these inputs is missing (no `glslangValidator` at build, no
FreeType, no system font), the overlay reports `UNIMPLEMENTED` with the
specific reason. Dialogs still complete logically (the guest does not
deadlock) but are not rendered.

## Logging and diagnostics

- `debug::Log` — categorized, level-filtered output.
- `debug::TraceConfig` — `FUSIONPS4_TRACE=cat1,cat2` runtime filter.
- `debug::UnimplementedRegistry` — aggregates every `FP4_UNIMPLEMENTED`.
  `Application::shutdown` dumps the report.
- `debug::CrashHandler` — `SIGSEGV`/`SIGBUS`/`SIGILL`/`SIGFPE`/`SIGABRT`
  handler with GPR dump, symbolicated backtrace via `ModuleRegistry`, and
  the current `UNIMPLEMENTED` report.
- `debug::Profiler` — `FUSIONPS4_PROFILE=1` enables per-frame timing.

## Design invariants

1. The guest never creates a window. The `HostWindow` is owned by the
   runtime; every present goes through `GraphicsDevice::presentFrame`.
2. The guest never touches a Linux file descriptor. Every file, socket
   and device is a PS4 handle in `HandleTable`.
3. The guest never calls a Linux syscall directly in isolated mode.
   The seccomp filter traps everything not in the allowlist.
4. The guest never allocates host memory. `AddressSpace` owns the arena;
   `malloc`/`free` (see `libSceLibcInternal`) operate on a sub-region.
5. Every unimplemented capability is reported. Nothing returns `0` to
   pretend success.
