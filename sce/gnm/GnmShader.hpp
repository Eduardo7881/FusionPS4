#pragma once

#include "graphics/abstraction/GraphicsTypes.hpp"

#include <cstdint>
#include <span>
#include <string>

namespace fusionps4::sce::gnm {

// PS4 shader binaries come in two flavours we must distinguish before
// attempting to use them:
//
//   * ORBIS: the native PS4 shader format, whose payload is GCN ISA
//     bytecode plus a header containing the shader type, register usage,
//     and input/output signature. Translating this to SPIR-V requires a
//     full GCN-to-SPIR-V recompiler, which is out of scope for this
//     phase. We reject it with UNIMPLEMENTED, identifying the exact
//     reason.
//
//   * SPIR-V: some engines (and all shaders produced by our own tools)
//     carry a pre-translated SPIR-V module inside the container. We
//     detect the SPIR-V magic number (0x07230203, little-endian) at the
//     start of the payload and pass it straight to the GAL.
class GnmShader {
public:
    enum class Source {
        Unknown,
        Orbis,     // native PS4 shader binary
        Spirv,     // pre-translated SPIR-V payload
    };

    struct Parsed {
        Source                            source = Source::Unknown;
        graphics::ShaderStage             stage  = graphics::ShaderStage::Vertex;
        std::span<const std::byte>        payload;   // points into the guest buffer
        std::string                       reason;    // populated on failure
    };

    // Inspect a guest-provided shader blob. `expectedStage` is used to
    // disambiguate formats that do not carry stage information in their
    // header. On success, `out` is fully populated. On failure, `out.reason`
    // describes the problem in a log-friendly way.
    static bool parse(const void* blob, std::size_t size,
                      graphics::ShaderStage expectedStage,
                      Parsed& out);
};

} // namespace fusionps4::sce::gnm
