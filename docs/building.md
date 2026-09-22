# Building

## Requirements

- CMake ≥ 3.20
- C++20 compiler (GCC ≥ 11 or Clang ≥ 14)
- SDL2 (required)
- Vulkan loader + headers (required for the primary backend)
- pkg-config

## Optional dependencies

These are detected at configure time. If missing, the corresponding SCE
library still registers its exports, but every entry point returns
`UNIMPLEMENTED` with the reason.

| Package | Enables | Macro |
|---|---|---|
| `libmpg123` | MP3 decoding (`libSceAjm`, `libSceAudiodec`) | `FUSIONPS4_HAVE_MPG123` |
| `libjpeg` (turbo) | JPEG decoding (`libSceJpeg`, `libSceScreenShot`) | `FUSIONPS4_HAVE_JPEG` |
| `libpng` | PNG decoding (`libScePng`, `libSceScreenShot`) | `FUSIONPS4_HAVE_PNG` |
| `freetype2` | Font rendering (`libSceFont`) and overlay atlas | `FUSIONPS4_HAVE_FREETYPE` |
| `libcurl` | HTTP (`libSceHttp`, `libSceSsl`) | `FUSIONPS4_HAVE_CURL` |
| `glslangValidator` | Overlay shaders (SPIR-V) | `FUSIONPS4_HAVE_OVERLAY_SPIRV` |

OpenSSL, VPX and avcodec are intentionally `0`. Their macros exist so a
downstream packager can flip them on without patching the source.

## Build

    cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
    cmake --build build -j

Artifacts:

    build/fusionps4
    build/fs4-inspect-elf
    build/fs4-unimpl-report
    build/test_elf_loader
    build/test_syscall_dispatch
    build/test_vfs_sandbox
    build/test_trampoline
    build/test_drbg

## Running tests

    cd build && ctest --output-on-failure

The tests do not require an actual PS4 binary. They exercise the loader,
the syscall dispatcher, the VFS sandbox, the trampoline emitter and the
DRBG with synthetic inputs.

## Docker

A `Dockerfile` is not included in this repository. To build in a
container, install the following Debian/Ubuntu packages:

    build-essential cmake pkg-config \
    libsdl2-dev libvulkan-dev \
    libmpg123-dev libjpeg-dev libpng-dev libfreetype-dev libcurl4-openssl-dev \
    glslang-tools

## Cross-compiling

FusionPS4 targets Linux x86_64 only. The `AddressSpace` design depends on
identity-mapped guest/host virtual addresses, which assumes the guest and
host share the same page size and address space layout. Porting to
another architecture would require either a same-architecture guest (for
example, ARM64 → ARM64) or a binary translator.
