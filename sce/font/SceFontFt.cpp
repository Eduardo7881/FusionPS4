#include "sce/font/SceFontFt.hpp"

#include "assets/CodecRegistry.hpp"
#include "debug/Log.hpp"
#include "sce/SceStubTable.hpp"

using fusionps4::debug::LogCategory;

namespace fusionps4::sce::font {

SceFontFt& SceFontFt::instance() {
    static SceFontFt s;
    return s;
}

bool SceFontFt::initialize() {
    m_initialized = true;
    const auto& caps = assets::CodecRegistry::instance();
    if (caps.hasFreetype) {
        FP4_INFO(LogCategory::Sce)
            << "libSceFontFt initialized (backed by shared FreeType)";
    } else {
        FP4_WARN(LogCategory::Sce)
            << "libSceFontFt initialized without FreeType; "
            << "every entry point will report UNIMPLEMENTED";
    }
    return true;
}

void SceFontFt::shutdown() { m_initialized = false; }

namespace {

constexpr int kOk            = 0;
constexpr int kErrNotSupport = static_cast<int>(0x80020003u);

extern "C" {

// libSceFontFt is a thin shim over FreeType; we expose the two entry points
// the SDK actually advertises as stable. Everything else lives behind the
// libSceFont wrappers above.

int sceFontFtInit() {
    if (!assets::CodecRegistry::instance().hasFreetype) {
        FP4_UNIMPLEMENTED(LogCategory::Sce, "sceFontFtInit");
        FP4_ERROR(LogCategory::Sce)
            << "  reason=freetype not linked into this build";
        return kErrNotSupport;
    }
    return kOk;
}

int sceFontFtDone() { return kOk; }

} // extern "C"

} // namespace

void SceFontFt::registerExports(SceStubTable& t) {
    t.registerStub("libSceFontFt", "sceFontFtInit",
                   reinterpret_cast<void*>(&sceFontFtInit));
    t.registerStub("libSceFontFt", "sceFontFtDone",
                   reinterpret_cast<void*>(&sceFontFtDone));
}

} // namespace fusionps4::sce::font
