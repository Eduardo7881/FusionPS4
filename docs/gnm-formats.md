# GNM formats and shader binaries

This document is the honest account of what FusionPS4 does and does not
know about the low-level PS4 graphics stack.

## GNMX (high-level)

The GNMX API is the wrapper layer over GNM. It uses C++ objects
(`Gnmx::VsShader`, `Gnmx::PsShader`, `Gnmx::RenderTarget`, ...) and
ultimately emits GNM command buffers through `sceGnm*` calls.

FusionPS4 implements the **wrapper-facing subset**:

- resource creation (`sceGnmCreateBuffer`, `sceGnmCreateTexture`,
  `sceGnmCreateRenderTarget`),
- shader container parsing for the SPIR-V case,
- pipeline creation (`sceGnmCreateShaderPipeline`),
- state setters (`sceGnmSetShaderPipeline`, `sceGnmSetRenderTarget`,
  `sceGnmSetVertexBuffer`, `sceGnmSetIndexBuffer`, `sceGnmSetViewport`,
  `sceGnmSetScissor`, `sceGnmSetClearColor`),
- draw submission (`sceGnmDrawIndexAuto`, `sceGnmDrawIndex`) which records
  into a runtime FIFO instead of submitting directly.

The runtime drains that FIFO inside `Runtime::tick` and translates each
recorded operation into a `GraphicsDevice::cmd*` call. This is what keeps
the guest out of the GPU.

## GNM (low-level command buffers)

Some engines bypass GNMX and submit raw GNM command buffers directly,
either through `sceGnmSubmitCommandBuffers` or through
`sceGnmSubmitAndFlipCommandBuffers`.

FusionPS4 **does not decode these command buffers**. The packet opcodes
are undocumented; reconstructing them requires deep reverse engineering
of the Sony driver that is not publicly available in a stable, verifiable
form.

Both entry points return `SCE_GNM_ERROR_NOT_SUPPORTED` with an
`UNIMPLEMENTED` report that includes the call count and the first DCB
pointer/size, so that an operator can see exactly how many calls a title
made and where they were aimed.

## ORBIS shaders

PS4 shader binaries in the ORBIS container wrap GCN ISA bytecode. To run
them on Vulkan, a runtime must recompile GCN → SPIR-V. This is a
multi-month project comparable to RPCS3's RSX recompiler.

FusionPS4 does **not** implement such a recompiler. When
`GnmShader::parse` inspects a shader blob:

- If the payload begins with the SPIR-V magic (`0x07230203`), it is
  forwarded to `GraphicsDevice::createShader` as SPIR-V.
- If the payload is not SPIR-V, the parser logs the first two header
  words, the shader size and the expected stage, and returns false. The
  caller returns `SCE_GNM_ERROR_NOT_SUPPORTED`.

The log entry looks like:

    UNIMPLEMENTED GnmShader::parse(ORBIS)
      size=8192 stage=0 header0=0x... header1=0x...
      reason=GCN ISA -> SPIR-V recompiler is out of scope for this phase

Titles that ship pre-translated SPIR-V (some middleware, and any shader
produced by the runtime's own tools) work.

## What works today

- GNMX wrapper path for engines that use it.
- SPIR-V shaders.
- Resource creation, pipeline creation, state tracking, draw submission
  through the recorder.
- Vulkan rendering into the swapchain, with a clear color per frame.

## What does not work

- Raw GNM command buffers.
- ORBIS shaders.
- GNMX features that depend on the Sony runtime internally, for example
  the `SceGnmxContext` lifecycle.

## Adding a shader recompiler

The recompiler would live in a new top-level directory, e.g.
`graphics/recompiler/gcn/`. It should:

1. Parse the ORBIS container (header, resource descriptors, ISA code).
2. Decode GCN SASS to an intermediate representation.
3. Emit SPIR-V from that IR (via a library such as `spirv-tools`).
4. Return a `ShaderHandle` through the existing GAL.

The output must be routed through `GraphicsDevice::createShader`; the SCE
stubs must not call Vulkan directly. Integration points are already
isolated in `sce/gnm/GnmShader.cpp`.
