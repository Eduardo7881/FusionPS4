# FusionPS4

![Status](https://img.shields.io/badge/status-WIP-red)
![Platform](https://img.shields.io/badge/platform-Linux-blue)
![License](https://img.shields.io/badge/license-MIT-green)
![Version](https://img.shields.io/badge/version-0.1--dev-orange)

FusionPS4 is an experimental compatibility layer that aims to run
PlayStation 4 applications on Linux systems.

## Quickstart

    cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
    cmake --build build -j

    # Default mode (no isolation): the guest runs in the same process.
    ./build/fusionps4

    # Isolated mode: the guest runs in a child process, isolated by
    # namespaces + seccomp. This is the hypervisor-equivalent mode.
    FUSIONPS4_ISOLATED=1 ./build/fusionps4

    # Simulate a PSN account.
    FUSIONPS4_PSN_ACCOUNT=MyPlayer FUSIONPS4_ISOLATED=1 ./build/fusionps4

    # Reduce the log to specific categories.
    FUSIONPS4_TRACE=syscall,sce,gnm ./build/fusionps4

## Documentation

- [Architecture](docs/architecture.md)
- [Building](docs/building.md)
- [Running](docs/running.md)
- [Isolation model](docs/isolation.md)
- [SCE coverage](docs/sce-coverage.md)
- [Syscall map](docs/syscall-map.md)
- [GNM formats](docs/gnm-formats.md)
- [UNIMPLEMENTED report](docs/unimplemented-report.md)
- [Troubleshooting](docs/troubleshooting.md)
- [Contributing](CONTRIBUTING.md)

## Status

FusionPS4 is a research-grade runtime. It provides:

- a real ELF loader with SCE import handling,
- a real trap mechanism (seccomp + SIGSYS + shared ring),
- a real virtual filesystem with policy,
- a real Vulkan backend with command recording and frame capture,
- real SCE libraries for the majority of the low-level surface,
- honest reporting of what is not implemented.

What it does **not** provide:

- a GCN ISA → SPIR-V shader recompiler (see `docs/gnm-formats.md`),
- a decoder for raw GNM command buffers (see `docs/gnm-formats.md`),
- real PSN connectivity (see `docs/sce-coverage.md`),
- video decode,
- camera or PlayStation Move input.

## ⚠️ Status

This project is heavily work-in-progress.

It is NOT capable of running commercial PS4 games yet.

## 🎯 Goals

- Run simple PS4 executables
- Build a functional runtime environment
- Expand compatibility over time

## 🚀 Roadmap

- [x] ELF Loader
- [ ] Basic syscall translation
- [ ] Minimal rendering
- [x] Input system
- [ ] Game compatibility (long-term)

## 🐧 Platform

- Linux only
- Windows support is not planned anytime soon

## ⚖️ Legal

This project does NOT include any Sony code or proprietary assets.

See DISCLAIMER.md for more information.
