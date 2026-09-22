#include "sce/sysmodule/SceSysmodule.hpp"

#include "debug/Log.hpp"
#include "loader/ElfLoader.hpp"
#include "runtime/RuntimeContext.hpp"
#include "runtime/process/PS4Process.hpp"
#include "sce/SceStubTable.hpp"

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

using fusionps4::debug::LogCategory;
using fusionps4::runtime::RuntimeContext;
using fusionps4::loader::modules::ModulePtr;

namespace fusionps4::sce::sysmodule {

SceSysmodule& SceSysmodule::instance() {
    static SceSysmodule s;
    return s;
}

bool SceSysmodule::initialize() {
    m_initialized = true;
    FP4_INFO(LogCategory::Sce) << "libSceSysmodule initialized";
    return true;
}

void SceSysmodule::shutdown() { m_initialized = false; }

namespace {

constexpr int kSceOk              = 0;
constexpr int kSceErrorInvalidArg = static_cast<int>(0x80020005u);
constexpr int kSceErrorNotLoaded  = static_cast<int>(0x80020004u);
constexpr int kSceErrorNoMemory   = static_cast<int>(0x80020002u);
constexpr int kSceErrorExists     = static_cast<int>(0x8002000Bu);

std::mutex                         g_loadMutex;
std::unordered_map<std::string, ModulePtr> g_loadedModules;

// Convert a PS4 module id (an integer identifying e.g. SCE_SYSMODULE_INTERNAL_PAD)
// to the library soname. Only libraries we implement are listed; anything
// else is refused with NOT_LOADED, which is what the guest expects when a
// module truly is not on the system.
struct ModuleMapping { int id; const char* lib; };
constexpr ModuleMapping kModuleMap[] = {
    {0x00000001, "libSceLibcInternal"},
    {0x00000002, "libSceUlt"},
    {0x00000003, "libSceKernel"},
    {0x0000000C, "libScePad"},
    {0x00000018, "libSceAudioOut"},
    {0x0000001C, "libSceNet"},
    {0x00000021, "libSceVideoOut"},
    {0x0000003C, "libSceGnm"},
    {0x00000048, "libSceSysmodule"},
};

const char* libForId(int id) {
    for (const auto& m : kModuleMap) if (m.id == id) return m.lib;
    return nullptr;
}

} // namespace

namespace {

extern "C" {

int sceSysmoduleLoadModule(int id) {
    const char* lib = libForId(id);
    if (!lib) {
        FP4_WARN(LogCategory::Sce)
            << "sceSysmoduleLoadModule: unknown module id 0x"
            << std::hex << id << std::dec;
        return kSceErrorNotLoaded;
    }

    std::lock_guard lock(g_loadMutex);
    if (g_loadedModules.count(lib)) {
        FP4_DEBUG(LogCategory::Sce)
            << "sceSysmoduleLoadModule: \"" << lib << "\" already loaded";
        return kSceErrorExists;
    }

    // The library is a runtime-implemented stub set. We mark it as loaded
    // so the guest's `sceSysmoduleIsLoaded` returns the expected value;
    // there is no ELF to load because the exports live in the runtime.
    auto& table = fusionps4::sce::SceStubTable::instance();
    const auto stubs = table.snapshot();
    bool found = false;
    const std::string prefix = std::string(lib) + "::";
    for (const auto& [key, addr] : stubs) {
        if (key.compare(0, prefix.size(), prefix) == 0) { found = true; break; }
    }
    if (!found) {
        FP4_WARN(LogCategory::Sce)
            << "sceSysmoduleLoadModule: \"" << lib
            << "\" has no registered stubs";
        return kSceErrorNotLoaded;
    }

    g_loadedModules[lib] = nullptr;   // nullptr means "runtime-implemented"
    FP4_INFO(LogCategory::Sce)
        << "sceSysmoduleLoadModule: \"" << lib << "\" loaded";
    return kSceOk;
}

int sceSysmoduleUnloadModule(int id) {
    const char* lib = libForId(id);
    if (!lib) return kSceErrorNotLoaded;
    std::lock_guard lock(g_loadMutex);
    auto it = g_loadedModules.find(lib);
    if (it == g_loadedModules.end()) return kSceErrorNotLoaded;
    // We do not actually unload runtime-implemented stubs because other
    // processes/modules may hold references to them. Mark as unloaded.
    g_loadedModules.erase(it);
    return kSceOk;
}

int sceSysmoduleIsLoaded(int id) {
    const char* lib = libForId(id);
    if (!lib) return kSceErrorNotLoaded;
    std::lock_guard lock(g_loadMutex);
    return g_loadedModules.count(lib) ? kSceOk : kSceErrorNotLoaded;
}

} // extern "C"

} // namespace

void SceSysmodule::registerExports(SceStubTable& t) {
    t.registerStub("libSceSysmodule", "sceSysmoduleLoadModule",
                   reinterpret_cast<void*>(&sceSysmoduleLoadModule));
    t.registerStub("libSceSysmodule", "sceSysmoduleUnloadModule",
                   reinterpret_cast<void*>(&sceSysmoduleUnloadModule));
    t.registerStub("libSceSysmodule", "sceSysmoduleIsLoaded",
                   reinterpret_cast<void*>(&sceSysmoduleIsLoaded));
}

} // namespace fusionps4::sce::sysmodule
