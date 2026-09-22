#include "sce/savedata/SceSaveData.hpp"

#include "debug/Log.hpp"
#include "runtime/RuntimeContext.hpp"
#include "runtime/process/PS4Process.hpp"
#include "savedata/SaveContainer.hpp"
#include "sce/SceStubTable.hpp"
#include "filesystem/virtual/VirtualFileSystem.hpp"
#include "filesystem/policy/FsPolicy.hpp"

#include <cstring>
#include <fstream>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unordered_map>

using fusionps4::debug::LogCategory;
using fusionps4::runtime::RuntimeContext;
using fusionps4::savedata::SaveContainer;
namespace vfs = fusionps4::filesystem::virtual_fs;
namespace pol = fusionps4::filesystem::policy;

namespace fusionps4::sce::savedata {

SceSaveData& SceSaveData::instance() { static SceSaveData s; return s; }
bool SceSaveData::initialize() {
    m_initialized = true;
    FP4_INFO(LogCategory::Sce) << "libSceSaveData initialized";
    return true;
}
void SceSaveData::shutdown() { m_initialized = false; }

namespace {

constexpr int kOk             = 0;
constexpr int kErrInvalidArg  = static_cast<int>(0x80020005u);
constexpr int kErrNotFound    = static_cast<int>(0x80020004u);
constexpr int kErrNoMemory    = static_cast<int>(0x80020002u);
constexpr int kErrExists      = static_cast<int>(0x8002000Bu);

constexpr std::uint32_t kMaxDirName = 32;

struct SaveDir {
    std::string dirName;      // "0001", "TITLE-SAVEDATA-01", etc.
    std::string titleId;
    std::string subtitle;
    std::string detail;
    std::uint64_t createdAt = 0;
    std::uint64_t modifiedAt = 0;
    std::uint32_t numBlocks = 0;
    SaveContainer container;
};

std::mutex                                    g_mutex;
std::unordered_map<std::uint32_t, SaveDir>    g_saves;   // saveId -> SaveDir
std::uint32_t                                 g_nextSaveId = 1;

// Themed path used by the guest to look up a save: /save/<titleId>/<dirName>
// All file operations go through the runtime's VFS policy, so the guest
// cannot escape into host territory.
std::string resolveSavePath(const std::string& titleId,
                            const std::string& dirName) {
    // Compose the guest-visible path and let the VFS do the translation.
    return "/save/" + titleId + "/" + dirName + "/sd.img";
}

bool ensureHostDir(const std::string& hostDir) {
    std::string acc;
    for (std::size_t i = 0; i < hostDir.size(); ++i) {
        acc.push_back(hostDir[i]);
        if (hostDir[i] == '/' && acc.size() > 1) {
            ::mkdir(acc.substr(0, acc.size() - 1).c_str(), 0700);
        }
    }
    return ::mkdir(hostDir.c_str(), 0700) == 0 || errno == EEXIST;
}

std::string guestToHost(const std::string& guestPath) {
    auto* proc = RuntimeContext::instance().process();
    if (!proc) return {};
    std::int64_t perr = 0;
    auto tr = proc->virtualFileSystem().resolve(
        guestPath, pol::FsOp::Write, perr);
    return tr.ok ? tr.hostPath : std::string{};
}

// sceSaveDataSetupSaveDataMemory creates the runtime-side save object.
// The real PS4 API is elaborate; we support the common case where the
// title provides a directory name and a title id in the setup.
struct SetupParams {
    std::uint64_t titleId;      // hash of a title string
    std::uint32_t userId;
    std::uint32_t pad;
    char          dirName[kMaxDirName];
    char          subtitle[128];
    char          detail[256];
};
static_assert(sizeof(SetupParams) == 0x1A0, "SetupParams");

extern "C" {

int sceSaveDataInitialize(std::uint64_t /*poolSize*/, std::uint32_t /*flags*/,
                          void* /*ctx*/) {
    return kOk;
}
int sceSaveDataTerminate() { return kOk; }

int sceSaveDataSetupSaveDataMemory(std::uint32_t userId,
                                   std::uint64_t /*data*/,
                                   const SetupParams* params) {
    if (!params) return kErrInvalidArg;

    std::lock_guard lock(g_mutex);
    for (const auto& [id, sd] : g_saves) {
        if (sd.dirName == params->dirName &&
            sd.titleId == std::to_string(params->titleId)) {
            return static_cast<int>(id);
        }
    }

    SaveDir sd;
    sd.titleId  = std::to_string(params->titleId);
    sd.dirName  = params->dirName;
    sd.subtitle = params->subtitle;
    sd.detail   = params->detail;
    (void)userId;

    const auto id = g_nextSaveId++;
    g_saves[id] = std::move(sd);
    FP4_INFO(LogCategory::Sce)
        << "sceSaveDataSetupSaveDataMemory: dir=\"" << params->dirName
        << "\" title=" << params->titleId << " -> id=" << id;
    return static_cast<int>(id);
}

int sceSaveDataSaveIcon(std::uint32_t /*saveId*/, const void* /*icon*/,
                        std::size_t /*size*/) {
    return kOk;
}

// sceSaveDataCommitSaveData writes the container to the VFS.
int sceSaveDataCommitSaveData(std::uint32_t saveId) {
    std::lock_guard lock(g_mutex);
    auto it = g_saves.find(saveId);
    if (it == g_saves.end()) return kErrNotFound;

    auto& sd = it->second;
    sd.container.setTitleId(std::stoull(sd.titleId));
    sd.container.setSubtitle(sd.subtitle);
    sd.container.setDetail(sd.detail);

    const auto guestPath = resolveSavePath(sd.titleId, sd.dirName);
    const auto hostPath  = guestToHost(guestPath);
    if (hostPath.empty()) {
        FP4_ERROR(LogCategory::Sce)
            << "sceSaveDataCommitSaveData: VFS rejected path \""
            << guestPath << "\"";
        return kErrNotFound;
    }

    // Ensure the parent directory exists on the host.
    const auto parent = hostPath.substr(0, hostPath.rfind('/'));
    ensureHostDir(parent);

    const auto bytes = sd.container.serialize();
    std::ofstream f(hostPath, std::ios::binary | std::ios::trunc);
    if (!f) {
        FP4_ERROR(LogCategory::Sce)
            << "sceSaveDataCommitSaveData: cannot write \"" << hostPath << "\"";
        return kErrNoMemory;
    }
    f.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
    f.close();

    FP4_INFO(LogCategory::Sce)
        << "Save committed: \"" << guestPath << "\" (" << bytes.size()
        << " bytes, " << sd.container.entryCount() << " entries)";
    return kOk;
}

int sceSaveDataLoadSaveData(std::uint32_t saveId) {
    std::lock_guard lock(g_mutex);
    auto it = g_saves.find(saveId);
    if (it == g_saves.end()) return kErrNotFound;
    auto& sd = it->second;

    const auto guestPath = resolveSavePath(sd.titleId, sd.dirName);
    const auto hostPath  = guestToHost(guestPath);
    if (hostPath.empty()) return kErrNotFound;

    std::ifstream f(hostPath, std::ios::binary | std::ios::ate);
    if (!f) {
        // No file yet: empty container.
        sd.container = SaveContainer{};
        FP4_DEBUG(LogCategory::Sce)
            << "sceSaveDataLoadSaveData: no existing save at \"" << hostPath
            << "\"";
        return kOk;
    }
    const auto size = static_cast<std::size_t>(f.tellg());
    f.seekg(0);
    std::vector<std::uint8_t> bytes(size);
    f.read(reinterpret_cast<char*>(bytes.data()),
           static_cast<std::streamsize>(size));

    SaveContainer sc;
    if (!SaveContainer::deserialize(bytes.data(), bytes.size(), sc)) {
        FP4_ERROR(LogCategory::Sce)
            << "sceSaveDataLoadSaveData: corrupt container at \"" << hostPath
            << "\"";
        return kErrNotFound;
    }
    sd.container = std::move(sc);
    FP4_INFO(LogCategory::Sce)
        << "Save loaded: \"" << guestPath << "\" (" << sd.container.entryCount()
        << " entries)";
    return kOk;
}

// sceSaveDataGetSaveDataMemory and SetSaveDataMemory operate on the whole
// container as an opaque blob; the runtime serializes/deserializes on the
// fly. Titles that use the finer-grained entry APIs go through the helpers
// below.
int sceSaveDataGetSaveDataMemory(std::uint32_t saveId, void* outBuf,
                                 std::size_t outSize,
                                 std::size_t offset) {
    std::lock_guard lock(g_mutex);
    auto it = g_saves.find(saveId);
    if (it == g_saves.end()) return kErrNotFound;

    const auto bytes = it->second.container.serialize();
    if (offset >= bytes.size()) return kOk;   // nothing to read
    const auto take = std::min(outSize, bytes.size() - offset);
    std::memcpy(outBuf, bytes.data() + offset, take);
    return static_cast<int>(take);
}

int sceSaveDataSetSaveDataMemory(std::uint32_t saveId, const void* buf,
                                 std::size_t size, std::size_t offset) {
    std::lock_guard lock(g_mutex);
    auto it = g_saves.find(saveId);
    if (it == g_saves.end()) return kErrNotFound;

    auto bytes = it->second.container.serialize();
    if (offset + size > bytes.size()) bytes.resize(offset + size);
    std::memcpy(bytes.data() + offset, buf, size);

    SaveContainer sc;
    if (SaveContainer::deserialize(bytes.data(), bytes.size(), sc)) {
        it->second.container = std::move(sc);
        return kOk;
    }
    // If it did not parse, the guest wrote something we do not understand.
    // We keep the previous container and report the inconsistency.
    FP4_WARN(LogCategory::Sce)
        << "sceSaveDataSetSaveDataMemory: guest buffer does not parse as "
        << "a save container; data was not applied";
    return kErrInvalidArg;
}

int sceSaveDataDeleteSaveDataMemory(std::uint32_t saveId) {
    std::lock_guard lock(g_mutex);
    auto it = g_saves.find(saveId);
    if (it == g_saves.end()) return kErrNotFound;

    const auto guestPath = resolveSavePath(it->second.titleId,
                                           it->second.dirName);
    const auto hostPath = guestToHost(guestPath);
    if (!hostPath.empty()) ::unlink(hostPath.c_str());
    it->second.container = SaveContainer{};
    return kOk;
}

int sceSaveDataGetSaveDataMemorySize(std::uint32_t saveId, std::size_t* outSize) {
    if (!outSize) return kErrInvalidArg;
    std::lock_guard lock(g_mutex);
    auto it = g_saves.find(saveId);
    if (it == g_saves.end()) return kErrNotFound;
    *outSize = it->second.container.serialize().size();
    return kOk;
}

} // extern "C"

} // namespace

void SceSaveData::registerExports(SceStubTable& t) {
    t.registerStub("libSceSaveData", "sceSaveDataInitialize",
                   reinterpret_cast<void*>(&sceSaveDataInitialize));
    t.registerStub("libSceSaveData", "sceSaveDataTerminate",
                   reinterpret_cast<void*>(&sceSaveDataTerminate));
    t.registerStub("libSceSaveData", "sceSaveDataSetupSaveDataMemory",
                   reinterpret_cast<void*>(&sceSaveDataSetupSaveDataMemory));
    t.registerStub("libSceSaveData", "sceSaveDataSaveIcon",
                   reinterpret_cast<void*>(&sceSaveDataSaveIcon));
    t.registerStub("libSceSaveData", "sceSaveDataCommitSaveData",
                   reinterpret_cast<void*>(&sceSaveDataCommitSaveData));
    t.registerStub("libSceSaveData", "sceSaveDataLoadSaveData",
                   reinterpret_cast<void*>(&sceSaveDataLoadSaveData));
    t.registerStub("libSceSaveData", "sceSaveDataGetSaveDataMemory",
                   reinterpret_cast<void*>(&sceSaveDataGetSaveDataMemory));
    t.registerStub("libSceSaveData", "sceSaveDataSetSaveDataMemory",
                   reinterpret_cast<void*>(&sceSaveDataSetSaveDataMemory));
    t.registerStub("libSceSaveData", "sceSaveDataDeleteSaveDataMemory",
                   reinterpret_cast<void*>(&sceSaveDataDeleteSaveDataMemory));
    t.registerStub("libSceSaveData", "sceSaveDataGetSaveDataMemorySize",
                   reinterpret_cast<void*>(&sceSaveDataGetSaveDataMemorySize));
}

} // namespace fusionps4::sce::savedata
