#pragma once

#include "graphics/abstraction/GraphicsTypes.hpp"

#include <cstdint>

namespace fusionps4::sce::gnm {

// GNM resource identifiers. On PS4 these are opaque 32-bit handles that
// index into the GPU's resource descriptor table. Our registry mirrors
// that: an ID is valid within the process that created it, and the runtime
// keeps the mapping to GAL objects.
using GnmBufferId       = std::uint32_t;
using GnmTextureId      = std::uint32_t;
using GnmRenderTargetId = std::uint32_t;
using GnmVsShaderId     = std::uint32_t;
using GnmPsShaderId     = std::uint32_t;
using GnmPipelineId     = std::uint32_t;

constexpr std::uint32_t kGnmInvalidId = 0;

// ---- texture formats ----------------------------------------------------
// A subset of GNM texture format values. Only formats that have a direct
// GAL counterpart are listed; the translator rejects the rest with
// UNIMPLEMENTED at sceGnm* call time.
enum GnmTextureFormat : std::uint32_t {
    kGnmTextureFormatInvalid          = 0x00,
    kGnmTextureFormatB8G8R8A8Unorm    = 0x01,
    kGnmTextureFormatB8G8R8A8Srgb     = 0x03,
    kGnmTextureFormatR8G8B8A8Unorm    = 0x0A,
    kGnmTextureFormatR8G8B8A8Srgb     = 0x0C,
    kGnmTextureFormatR16G16B16A16Float= 0x12,
    kGnmTextureFormatR32G32B32A32Float= 0x18,
    kGnmTextureFormatD32Float         = 0x30,
};

// ---- data formats (vertex attributes) ----------------------------------
// A subset used by sceGnmx; unsupported ones surface as UNIMPLEMENTED in
// the vertex-input pipeline builder.
enum GnmDataFormat : std::uint32_t {
    kGnmDataFormatInvalid     = 0x00,
    kGnmDataFormat8UNorm      = 0x01,
    kGnmDataFormat8SNorm      = 0x02,
    kGnmDataFormat8UInt       = 0x04,
    kGnmDataFormat8SInt       = 0x05,
    kGnmDataFormat16UNorm     = 0x07,
    kGnmDataFormat16SNorm     = 0x08,
    kGnmDataFormat16UInt      = 0x0A,
    kGnmDataFormat16SInt      = 0x0B,
    kGnmDataFormat16Float     = 0x0C,
    kGnmDataFormat32UInt      = 0x0E,
    kGnmDataFormat32SInt      = 0x0F,
    kGnmDataFormat32Float     = 0x10,
};

// ---- GNMX pipeline state ------------------------------------------------
// A flattened snapshot of the shader+render state that sceGnmxSetup* would
// configure on PS4. Kept POD so it can be queued through the recorder.

struct GnmVertexInput {
    std::uint32_t binding   = 0;
    std::uint32_t stride    = 0;
    GnmDataFormat format    = kGnmDataFormatInvalid;
    std::uint32_t offset    = 0;
    std::uint32_t location  = 0;
    std::uint32_t numComps  = 1;
};

struct GnmRenderTargetInfo {
    std::uint32_t width  = 0;
    std::uint32_t height = 0;
    GnmTextureFormat format = kGnmTextureFormatInvalid;
};

// Translate a GNM texture format to a GAL format. Returns Unknown on
// unsupported values; callers report UNIMPLEMENTED.
graphics::Format gnmTextureFormatToGal(GnmTextureFormat f);

} // namespace fusionps4::sce::gnm
