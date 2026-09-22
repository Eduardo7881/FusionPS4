# Troubleshooting

## The window opens but is black

The guest has not submitted a frame yet, or `beginFrame` returned false
and the swapchain was recreated. Check the log:

- `[VULKAN] swapchain: WxH images=N format=...` — the swapchain exists.
- `[VULKAN] vkQueuePresentKHR failed` — the present failed.
- `[HOST] createSwapchain failed` — the swapchain was never created.

Common causes:

- No Vulkan ICD installed. Install `mesa-vulkan-drivers` or your GPU
  vendor's driver.
- Window created without `SDL_WINDOW_VULKAN`. This is handled by
  `HostWindow::create`; if it is not, the runtime reports a fatal error at
  init.

## The guest exits immediately

Enable tracing:

    FUSIONPS4_TRACE=loader,syscall,sce ./fusionps4

Look for:

- `UNIMPLEMENTED symbol lookup for "..."` — the loader could not resolve
  an import. Add the missing function to the corresponding SCE library
  or to `NidDatabase`.
- `UNIMPLEMENTED syscall number=...` — the guest issued a syscall that
  `SyscallDispatcher` does not implement. Add a handler (see
  `docs/syscall-map.md`).
- `process N loaded "..." entry=...` followed by an immediate exit — the
  guest's `main` returned or crashed. Check the crash report.

## The crash handler fires but there is no stack trace

The handler walks the frame pointer chain. If the guest was compiled with
`-fomit-frame-pointer` (which is common), the walk terminates early. This
is expected: the report still contains the faulting PC and the module it
belongs to.

To get more, build the guest with `-fno-omit-frame-pointer` or run under
`gdb` with `catch signal SIGSEGV`.

## `mmap` file-backed returns ENOSYS

That is correct. File-backed `mmap` is intentionally unimplemented until
the file handle model gains lifetime pinning. See `syscall-map.md`.

## Video playback produces no output

`libSceVideodec` is not implemented. Every entry point returns
`NOT_SUPPORTED` with the reason "no public VP9/H.264 hardware decode
available". Titles that support a fallback path will use it; titles that
require the decoder will not play video.

## The overlay does not render dialogs

Check the build:

- `FUSIONPS4_HAVE_OVERLAY_SPIRV=1` requires `glslangValidator` at build
  time.
- `FUSIONPS4_HAVE_FREETYPE=1` requires FreeType.
- A system font must exist at one of the paths listed in
  `OverlayRenderer::ensureFontLoaded`.

If any is missing, the runtime logs:

    UNIMPLEMENTED OverlayRenderer::initialize
      reason=overlay shaders not compiled

Dialogs still complete logically; the guest sees their result on the next
status poll.

## Screenshots fail with NOT_SUPPORTED

Either:

- no frame has been presented yet (`captureFrame` requires at least one
  prior `presentFrame`), or
- neither libjpeg nor libpng is linked.

Check the log for `Codec capabilities:` at startup.

## The guest cannot reach the network

`NetworkPolicy` defaults to `LocalhostOnly`. To allow other destinations,
either set the mode to `FullAccess` in `Runtime::init` or add specific
endpoints via `NetworkPolicy::allowEndpoint`. Denied connections are
logged:

    [NETWORK] connect to example.com:443 denied: only localhost connections permitted

## The isolated guest aborts at startup

Check the log for the isolation step that failed:

- `namespace setup failed` — the kernel refused `unshare`. On kernels with
  unprivileged user namespaces disabled
  (`kernel.unprivileged_userns_clone=0`), user namespaces cannot be
  created. Either enable them or set `FUSIONPS4_ISO_USER_NS=0` (the other
  namespaces still work if the process has `CAP_SYS_ADMIN`).
- `pivot into jail failed` — `pivot_root` and `chroot` both failed. The
  child does not have `CAP_SYS_ADMIN` even in its user namespace.
- `seccomp install failed` — seccomp is not available in this kernel.

Setting `FUSIONPS4_ISO_STRICT=0` makes the child continue past these
failures with reduced isolation. Do not do this in production.

## The NID database has no entries

FusionPS4 ships a built-in fallback table covering the functions from
Parts 4, 9, 10 and 11. Real PS4 titles require a full NID database
covering the imports they use. Point `FUSIONPS4_NID_DB` at a file:

    <nid-hex-16-chars> <library> <function>

Lines beginning with `#` are ignored. The runtime logs how many entries
were loaded at startup.
