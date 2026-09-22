# Running

## Basic invocation

    ./fusionps4

The runtime opens a window, initializes the graphics backend, sets up the
virtual filesystem, starts the input/audio/network managers, and waits.
If `FUSIONPS4_ISOLATED=1`, it also forks the guest process and jumps into
the entry point of the loaded PS4 executable.

## Environment variables

| Variable | Default | Effect |
|---|---|---|
| `FUSIONPS4_ISOLATED` | `0` | If `1`, run the guest in an isolated child process |
| `FUSIONPS4_PSN_ACCOUNT` | `Player1` | Online ID for the simulated PSN account |
| `FUSIONPS4_TRACE` | (unset) | Category filter, e.g. `syscall,sce,gnm` or `all,-fs` |
| `FUSIONPS4_LOG` | `DEBUG` | Minimum log level (`TRACE`/`DEBUG`/`INFO`/`WARN`/`ERROR`) |
| `FUSIONPS4_LOGFILE` | (unset) | Also write logs to this file |
| `FUSIONPS4_TITLE` | `FusionPS4` | Window title |
| `FUSIONPS4_WIDTH` | `1280` | Window width in windowed mode |
| `FUSIONPS4_HEIGHT` | `720` | Window height in windowed mode |
| `FUSIONPS4_FULLSCREEN` | `0` | If `1`, start fullscreen |
| `FUSIONPS4_VFS` | `virtual_fs` | Virtual filesystem root directory |
| `FUSIONPS4_NID_DB` | (unset) | Path to an external NID database |
| `FUSIONPS4_PROFILE` | `0` | If `1`, enable the frame profiler |

### Isolation-specific variables

| Variable | Default | Effect |
|---|---|---|
| `FUSIONPS4_ISO_USER_NS` | `1` | Create user namespace |
| `FUSIONPS4_ISO_PID_NS` | `1` | Create PID namespace |
| `FUSIONPS4_ISO_MOUNT_NS` | `1` | Create mount namespace |
| `FUSIONPS4_ISO_NET_NS` | `1` | Create network namespace |
| `FUSIONPS4_ISO_IPC_NS` | `1` | Create IPC namespace |
| `FUSIONPS4_ISO_UTS_NS` | `1` | Create UTS namespace |
| `FUSIONPS4_ISO_DROP_CAPS` | `1` | Drop every capability |
| `FUSIONPS4_ISO_NO_NEW_PRIVS` | `1` | Set `PR_SET_NO_NEW_PRIVS` |
| `FUSIONPS4_ISO_TRAP_ALL` | `1` | Install the seccomp trap filter |
| `FUSIONPS4_ISO_HOSTNAME` | `PS4` | Guest hostname |
| `FUSIONPS4_ISO_JAIL` | `/tmp/fusionps4-jail` | Jail root path |
| `FUSIONPS4_ISO_TRACE_SYSCALLS` | `0` | Trace every trapped syscall to stderr |
| `FUSIONPS4_ISO_TRACE_SCE` | `0` | Trace every trapped SCE call to stderr |
| `FUSIONPS4_ISO_STRICT` | `1` | Abort the guest if any isolation step fails |

## On-disk layout

FusionPS4 uses paths relative to the current working directory:

    .
    ├── virtual_fs/
    │   ├── app/           (read-only; game data)
    │   ├── data/          (read-write; user-visible data)
    │   ├── system/        (read-only; system data)
    │   ├── temp/          (read-write; scratch)
    │   ├── save/          (read-write; game saves)
    │   └── user/          (read-write; album, screenshots)
    ├── apps/              (host directory for PS4 executables)
    ├── config/            (runtime configuration)
    └── persist/
        └── <onlineId>/
            ├── account.dat
            ├── trophies/
            │   └── <titleId>.dat
            └── scores/
                └── <boardId>.dat

The virtual filesystem is *not* the host filesystem. The guest sees
`/app`, `/data`, `/system`, `/temp`, `/save`, `/user` as its root. Any
attempt to open `/etc/passwd`, `/home/...` or `/dev/...` is rejected with
`ENOENT` by `FsPolicy`.

## Common invocations

    # Trace syscalls but not SCE calls.
    FUSIONPS4_TRACE=syscall ./fusionps4

    # Everything except audio.
    FUSIONPS4_TRACE=all,-audio ./fusionps4

    # Full isolation with a custom account, logging to a file.
    FUSIONPS4_ISOLATED=1 FUSIONPS4_PSN_ACCOUNT=Alice \
        FUSIONPS4_LOGFILE=/tmp/fusionps4.log ./fusionps4

    # 1920x1080 fullscreen, profiling enabled.
    FUSIONPS4_WIDTH=1920 FUSIONPS4_HEIGHT=1080 FUSIONPS4_FULLSCREEN=1 \
        FUSIONPS4_PROFILE=1 ./fusionps4

## Shutdown

The runtime exits when:

- the `HostWindow` is closed by the user (window manager close button);
- the isolated guest exits (in `FUSIONPS4_ISOLATED=1` mode);
- the runtime receives `SIGINT` or `SIGTERM`.

On shutdown, `Application::shutdown`:

1. stops the guest process and reaps it,
2. drains and reports the `UNIMPLEMENTED` registry,
3. flushes the profiler report if enabled,
4. tears down the graphics backend,
5. destroys the window.
