#include "loader/sce/NidDatabase.hpp"

#include "debug/Log.hpp"
#include "loader/sce/SceNid.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>

using fusionps4::debug::LogCategory;

namespace fusionps4::loader::sce {

NidDatabase& NidDatabase::instance() {
    static NidDatabase db;
    return db;
}

NidDatabase::NidDatabase() = default;

void NidDatabase::insert(std::uint64_t nid, std::string lib, std::string fn) {
    NidEntry e;
    e.nid      = nid;
    e.library  = std::move(lib);
    e.function = std::move(fn);
    m_entries[nid] = std::move(e);
}

bool NidDatabase::loadFromFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        FP4_WARN(LogCategory::Loader)
            << "NID database \"" << path << "\" not readable";
        return false;
    }

    std::lock_guard lock(m_mutex);
    std::string line;
    std::size_t added = 0, skipped = 0;
    while (std::getline(f, line)) {
        // Trim leading whitespace.
        std::size_t p = line.find_first_not_of(" \t\r\n");
        if (p == std::string::npos) continue;
        line = line.substr(p);
        if (line[0] == '#') continue;

        std::istringstream ss(line);
        std::string nidHex, lib, fn;
        if (!(ss >> nidHex >> lib >> fn)) { ++skipped; continue; }

        std::uint64_t nid = 0;
        if (!SceNid::fromHex(nidHex, nid)) { ++skipped; continue; }
        insert(nid, lib, fn);
        ++added;
    }

    FP4_INFO(LogCategory::Loader)
        << "NID database \"" << path << "\": " << added << " entries ("
        << skipped << " skipped)";
    return true;
}

void NidDatabase::loadBuiltinFallback() {
    std::lock_guard lock(m_mutex);
    if (m_builtinLoaded) return;
    m_builtinLoaded = true;

    // The built-in table covers only the functions whose NID we can compute
    // with the modern scheme (SHA256 of the plain name). PS4 uses several
    // NID schemes; any entry that doesn't match a real import in a real
    // binary simply never gets consulted, so this table is a convenience
    // for test binaries, not a substitute for a real database.
    struct Pair { const char* lib; const char* fn; };
    static const Pair kKnown[] = {
        {"libSceKernel",     "sceKernelCreateSema"},
        {"libSceKernel",     "sceKernelDeleteSema"},
        {"libSceKernel",     "sceKernelWaitSema"},
        {"libSceKernel",     "sceKernelPollSema"},
        {"libSceKernel",     "sceKernelSignalSema"},
        {"libSceKernel",     "sceKernelCreateEqueue"},
        {"libSceKernel",     "sceKernelDeleteEqueue"},
        {"libSceKernel",     "sceKernelWaitEqueue"},
        {"libSceKernel",     "sceKernelPrintf"},
        {"libSceKernel",     "sceKernelDebugOutText"},
        {"libSceKernel",     "sceKernelGetProcessTime"},
        {"libSceKernel",     "scePthreadCreate"},
        {"libSceKernel",     "scePthreadJoin"},
        {"libScePad",        "scePadInit"},
        {"libScePad",        "scePadOpen"},
        {"libScePad",        "scePadClose"},
        {"libScePad",        "scePadReadState"},
        {"libScePad",        "scePadRead"},
        {"libScePad",        "scePadSetVibration"},
        {"libSceAudioOut",   "sceAudioOutInit"},
        {"libSceAudioOut",   "sceAudioOutOpen"},
        {"libSceAudioOut",   "sceAudioOutClose"},
        {"libSceAudioOut",   "sceAudioOutOutput"},
        {"libSceAudioOut",   "sceAudioOutSetVolume"},
        {"libSceNet",        "sceNetInit"},
        {"libSceNet",        "sceNetTerm"},
        {"libSceNet",        "sceNetSocket"},
        {"libSceNet",        "sceNetClose"},
        {"libSceNet",        "sceNetConnect"},
        {"libSceNet",        "sceNetSend"},
        {"libSceNet",        "sceNetRecv"},
        {"libSceVideoOut",   "sceVideoOutOpen"},
        {"libSceVideoOut",   "sceVideoOutClose"},
        {"libSceVideoOut",   "sceVideoOutRegisterBuffers"},
        {"libSceVideoOut",   "sceVideoOutUnregisterBuffers"},
        {"libSceVideoOut",   "sceVideoOutSubmitFlip"},
        {"libSceVideoOut",   "sceVideoOutSetFlipRate"},
        {"libSceVideoOut",   "sceVideoOutGetFlipStatus"},
        {"libSceGnm",        "sceGnmInit"},
        {"libSceGnm",        "sceGnmTerminate"},
        {"libSceGnm",        "sceGnmSubmitDone"},
    };

    std::size_t n = 0;
    for (const auto& k : kKnown) {
        const auto nid = SceNid::computeLegacy(k.fn);
        insert(nid, k.lib, k.fn);
        ++n;
    }
    FP4_INFO(LogCategory::Loader)
        << "NID database: " << n << " built-in fallback entries loaded";
}

const NidEntry* NidDatabase::lookup(std::uint64_t nid) const {
    std::lock_guard lock(m_mutex);
    auto it = m_entries.find(nid);
    return it == m_entries.end() ? nullptr : &it->second;
}

std::size_t NidDatabase::size() const {
    std::lock_guard lock(m_mutex);
    return m_entries.size();
}

} // namespace fusionps4::loader::sce
