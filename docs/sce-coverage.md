# SCE library coverage

Legend:

- **Full** — the whole exported surface is implemented with correct
  semantics for the underlying hardware.
- **Partial** — a useful subset works; the remainder returns
  `UNIMPLEMENTED` with a reason.
- **Stub** — every entry point returns success or a documented "not
  available" error; no real functionality.
- **Absent** — no exports registered; the guest sees a load failure.

| Library | Status | Notes |
|---|---|---|
| `libSceLibcInternal` | Partial | malloc/free/realloc/calloc/memalign, full `string.h`, `printf`/`fprintf`/`snprintf`/`puts`; `fopen` reports `UNIMPLEMENTED` (VFS integration pending). |
| `libSceUlt` | Partial | Pool create/destroy/alloc/free with atomic CAS; waiting-queue init/destroy; spinlock init. |
| `libSceKernel` | Partial | Semaphores, event queues, `scePthreadCreate`/`Join`, `sceKernelPrintf`, `sceKernelLoadStartModule`, timing. Memory APIs (`sceKernelAllocateFlexibleMemory`, `MapDirectMemory`, ...) are not implemented. |
| `libSceSysmodule` | Partial | Load/unload/is-loaded for runtime-implemented libraries; `GetModuleInfo`, `GetModuleList`. |
| `libSceSystemService` | Partial | `GetStatus`, `ReceiveEvent`, `HideSplashScreen`, `LaunchApp` (`NOT_SUPPORTED`), `GetDisplaySafeAreaInfo`, `ParamGetInt`, suspend/resume, `sceKernelGetProcessTime*`. |
| `libSceRandom` | Full | ChaCha20 DRBG seeded from `getrandom(2)`. |
| `libSceRtc` | Full | Tick ↔ civil conversion (PS4 epoch), all common entry points. |
| `libSceFiber` | Partial | Fiber lifecycle on `ucontext`, run/switch/return. Cross-fiber data passing is limited. |
| `libScePad` | Partial | Init/Open/Close/Read/ReadState/SetVibration, `GetControllerInformation`, motion sensor state, light bar. Touchpad and gyro available via SDL when the device supports them. |
| `libSceKeyboard` | Full | Via SDL keyboard state. |
| `libSceMouse` | Partial | Read works; capture toggles; no raw device path. |
| `libSceCamera` | Absent | Every entry point returns `NOT_CONNECTED` with a reason. |
| `libSceMove` | Absent | Every entry point returns `NOT_FOUND` with a reason. |
| `libSceAudioOut` | Partial | Open/Close/Output/SetVolume, single stereo port at 48 kHz. Batching and multi-port mixing pending. |
| `libSceNgs2` | Partial | System/Rack/Voice lifecycle and mixing; 3 of ~40 voice control ids implemented. |
| `libSceAjm` | Partial | Job queue + MP3 decode via libmpg123. AT9 returns `UNIMPLEMENTED`. |
| `libSceAudiodec` | Partial | Same backend as Ajm. |
| `libSceVideodec` | Absent | Every entry point returns `NOT_SUPPORTED` with a reason. |
| `libSceVoice` | Absent | Every entry point returns `NOT_CONNECTED`; no capture device is bound. |
| `libSceNet` | Partial | `socket`, `connect`, `send`, `recv`, `close`, plus `NetworkPolicy` enforcement. Server-side ops pending. |
| `libSceNetCtl` | Partial | State and info from `NetworkPolicy`. Callback registration is a no-op. |
| `libSceHttp` | Partial | Full template/connection/request/response flow via libcurl. `sceHttpAddRequestHeader` returns `NOT_SUPPORTED` (headers are not forwarded). |
| `libSceSsl` | Stub | `sceSslInit` returns a stable handle; `sceSslConnect` is `NOT_SUPPORTED`. TLS is provided inside `sceHttp*`. |
| `libSceJpeg` | Partial | Decode via libjpeg-turbo when linked. |
| `libScePng` | Partial | Decode via libpng when linked. |
| `libSceFont` | Partial | Library create/destroy, glyph rendering via FreeType. |
| `libSceFontFt` | Stub | Only lifecycle exports; everything else is behind `libSceFont`. |
| `libSceNpManager` | Partial | `sceNpGetOnlineId`, `GetAccountId`, `CheckNpReachability`, `IsPlusMember`. All data comes from the simulated `PsnBackend`. |
| `libSceNpCommon` | Stub | Request handle allocation. |
| `libSceNpTrophy` | Full | Context/handle lifecycle, unlock, get info, get game info — persisted on disk. |
| `libSceNpScore` | Partial | Submit and rank by range. Rank by NpId returns empty. |
| `libSceSaveData` | Partial | Container format, mount, commit, load, get/set memory. The container is not the SDAT format. |
| `libSceSaveDataDialog` | Full | Rendered by the runtime overlay. |
| `libSceCommonDialog` | Partial | Lifecycle and `IsUsed`. |
| `libSceMsgDialog` | Full | Open, status, update, result, close, abort. |
| `libSceErrorDialog` | Partial | Initialize, open (draws on overlay), status, close. |
| `libSceImeDialog` | Partial | Init, status, get-result, abort. Input captured by the overlay. |
| `libSceSigninDialog` | Partial | Non-interactive confirmation shown when signed in. |
| `libScePlayGo` | Stub | All content reported as fully installed. |
| `libSceAppContent` | Partial | Addcont mount real via VFS; delete refused by design. |
| `libSceGameUpdate` | Stub | Always "no update". |
| `libSceBgft` | Partial | Local VFS-to-VFS transfers with progress and cancel. |
| `libSceDownload` | Stub | Task management only; no network transfer. |
| `libSceScreenShot` | Full | Captures the last presented swapchain image and saves as JPEG or PNG. |
| `libSceShare` | Absent | Every entry point returns `NOT_SIGNED_IN` because PSN is simulated. |
| `libSceVideoOut` | Partial | Open/Close/RegisterBuffers/SubmitFlip/SetFlipRate. The runtime owns the actual swapchain. |
| `libSceGnm` | Partial | Resource creation, shader containers (SPIR-V only), pipeline creation, state tracking, `DrawIndex`/`DrawIndexAuto` (recorder-based). Raw command buffers return `UNIMPLEMENTED`. |
| `libSceGnmx` | Absent | The GNMX wrappers land in a later phase; titles that link them directly will see a load failure. |
| `libSceVideodec` | Absent | See above. |
