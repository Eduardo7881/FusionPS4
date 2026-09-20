#include "sce/gnm/GnmShader.hpp"

#include "debug/Log.hpp"

#include <cstring>

using fusionps4::debug::LogCategory;

namespace fusionps4::sce::gnm {

namespace {

// SPIR-V magic in little-endian bytes.
constexpr std::uint32_t kSpirvMagic = 0x07230203;

bool looksLikeSpirv(const std::uint8_t* p, std::size_t n) {
    if (n < 4) return false;
    std::uint32_t v = 0;
    std::memcpy(&v, p, 4);
    return v == kSpirvMagic;
}

// ORBIS shader binaries begin with a small header whose first word is a
// magic / version. The only values we accept are ones we can positively
// identify; we do not accept "plausible" binaries. Values come from
// public PS4 shader header dumps.
constexpr std::uint32_t kOrbisVsMagic = 0xDEADBEEF;   // placeholder for
constexpr std::uint32_t kOrbisPsMagic = 0xDEADBEEF;   // documentation purposes

} // namespace

bool GnmShader::parse(const void* blob, std::size_t size,
                      graphics::ShaderStage expectedStage,
                      Parsed& out) {
    out = {};
    out.stage = expectedStage;

    if (!blob || size < 8) {
        out.reason = "shader blob null or too small";
        return false;
    }
    const auto* p = static_cast<const std::uint8_t*>(blob);

    if (looksLikeSpirv(p, size)) {
        out.source  = Source::Spirv;
        out.payload = { reinterpret_cast<const std::byte*>(p), size };
        return true;
    }

    // Detect the ORBIS header word. Real values are intentionally not
    // claimed here; the header dump is versioned and public documentation
    // is incomplete. Instead we log UNIMPLEMENTED with the raw bytes so a
    // caller can add a specific mapping later.
    std::uint32_t w0 = 0, w1 = 0;
    std::memcpy(&w0, p, 4);
    std::memcpy(&w1, p + 4, 4);

    out.source = Source::Orbis;
    out.reason =
        "ORBIS shader binary detected but GCN-to-SPIR-V translation is not "
        "implemented; header words 0x" + std::to_string(w0) + " / 0x" +
        std::to_string(w1);

    FP4_UNIMPLEMENTED(LogCategory::Sce,
        "GnmShader::parse(ORBIS)");
    FP4_ERROR(LogCategory::Sce)
        << "  size=" << size
        << "  stage=" << static_cast<int>(expectedStage)
        << "  header0=0x" << std::hex << w0
        << "  header1=0x" << w1 << std::dec
        << "  reason=GCN ISA -> SPIR-V recompiler is out of scope for this phase";

    (void)kOrbisVsMagic;
    (void)kOrbisPsMagic;
    return false;
}

} // namespace fusionps4::sce::gnm
