#include "savedata/SaveContainer.hpp"

#include "debug/Log.hpp"

#include <chrono>
#include <cstring>

using fusionps4::debug::LogCategory;

namespace fusionps4::savedata {

namespace {

#pragma pack(push, 1)
struct HeaderOnDisk {
    std::uint32_t magic;
    std::uint32_t version;
    std::uint64_t titleId;
    std::uint64_t createdAt;
    std::uint64_t modifiedAt;
    std::uint32_t entryCount;
    std::uint32_t reserved;
    char          titleName[64];
    char          subtitle[128];
    char          detail[256];
    std::uint32_t payloadSize;
    std::uint32_t payloadCrc;
};

struct EntryOnDisk {
    std::uint64_t offset;
    std::uint64_t size;
    std::uint32_t flags;
    std::uint32_t reserved;
    char          name[64];
};
#pragma pack(pop)

std::uint32_t crc32(const std::uint8_t* data, std::size_t n) {
    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < n; ++i) {
        crc ^= data[i];
        for (int k = 0; k < 8; ++k) {
            crc = (crc >> 1) ^ (0xEDB88320u & (~((crc & 1) - 1)));
        }
    }
    return ~crc;
}

std::uint64_t nowMicros() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
}

void copyStr(char* dst, std::size_t cap, const std::string& s) {
    const auto n = std::min(cap - 1, s.size());
    std::memcpy(dst, s.data(), n);
    dst[n] = '\0';
}

std::string readStr(const char* src, std::size_t cap) {
    std::size_t n = 0;
    while (n < cap && src[n] != '\0') ++n;
    return std::string(src, n);
}

} // namespace

void SaveContainer::addEntry(const std::string& name, const void* data,
                             std::size_t size, std::uint32_t flags) {
    Entry e;
    e.name  = name;
    e.flags = flags;
    e.data.resize(size);
    if (size > 0 && data) std::memcpy(e.data.data(), data, size);
    if (m_createdAt == 0) m_createdAt = nowMicros();
    m_modifiedAt = nowMicros();
    m_entries.push_back(std::move(e));
}

bool SaveContainer::hasEntry(const std::string& name) const {
    return entry(name) != nullptr;
}

const SaveContainer::Entry* SaveContainer::entry(const std::string& name) const {
    for (const auto& e : m_entries) if (e.name == name) return &e;
    return nullptr;
}

std::vector<std::string> SaveContainer::entryNames() const {
    std::vector<std::string> out;
    out.reserve(m_entries.size());
    for (const auto& e : m_entries) out.push_back(e.name);
    return out;
}

std::vector<std::uint8_t> SaveContainer::serialize() const {
    HeaderOnDisk h{};
    h.magic      = kMagic;
    h.version    = kVersion;
    h.titleId    = m_titleId;
    h.createdAt  = m_createdAt;
    h.modifiedAt = m_modifiedAt;
    h.entryCount = static_cast<std::uint32_t>(m_entries.size());
    copyStr(h.titleName, sizeof(h.titleName), m_titleName);
    copyStr(h.subtitle,  sizeof(h.subtitle),  m_subtitle);
    copyStr(h.detail,    sizeof(h.detail),    m_detail);

    std::vector<EntryOnDisk> entries;
    entries.reserve(m_entries.size());
    std::vector<std::uint8_t> payload;
    std::uint64_t offset = 0;
    for (const auto& e : m_entries) {
        EntryOnDisk ed{};
        ed.offset = offset;
        ed.size   = e.data.size();
        ed.flags  = e.flags;
        copyStr(ed.name, sizeof(ed.name), e.name);
        entries.push_back(ed);
        payload.insert(payload.end(), e.data.begin(), e.data.end());
        offset += e.data.size();
    }

    h.payloadSize = static_cast<std::uint32_t>(payload.size());
    h.payloadCrc  = crc32(payload.data(), payload.size());

    const std::size_t total = sizeof(h) +
                              entries.size() * sizeof(EntryOnDisk) +
                              payload.size();
    std::vector<std::uint8_t> out(total);
    std::size_t cursor = 0;
    std::memcpy(out.data() + cursor, &h, sizeof(h)); cursor += sizeof(h);
    if (!entries.empty()) {
        std::memcpy(out.data() + cursor, entries.data(),
                    entries.size() * sizeof(EntryOnDisk));
        cursor += entries.size() * sizeof(EntryOnDisk);
    }
    if (!payload.empty()) {
        std::memcpy(out.data() + cursor, payload.data(), payload.size());
    }
    return out;
}

bool SaveContainer::deserialize(const void* data, std::size_t size,
                                SaveContainer& out) {
    if (!data || size < sizeof(HeaderOnDisk)) return false;

    const auto* bytes = static_cast<const std::uint8_t*>(data);
    HeaderOnDisk h{};
    std::memcpy(&h, bytes, sizeof(h));

    if (h.magic != kMagic) {
        FP4_WARN(LogCategory::Fs)
            << "SaveContainer: bad magic 0x" << std::hex << h.magic << std::dec;
        return false;
    }
    if (h.version != kVersion) {
        FP4_WARN(LogCategory::Fs)
            << "SaveContainer: unsupported version " << h.version;
        return false;
    }

    const std::size_t entriesBytes =
        static_cast<std::size_t>(h.entryCount) * sizeof(EntryOnDisk);
    if (sizeof(h) + entriesBytes + h.payloadSize > size) return false;

    out = SaveContainer{};
    out.m_titleId    = h.titleId;
    out.m_createdAt  = h.createdAt;
    out.m_modifiedAt = h.modifiedAt;
    out.m_titleName  = readStr(h.titleName, sizeof(h.titleName));
    out.m_subtitle   = readStr(h.subtitle,  sizeof(h.subtitle));
    out.m_detail     = readStr(h.detail,    sizeof(h.detail));

    const auto* entryBytes = bytes + sizeof(h);
    const auto* payload    = entryBytes + entriesBytes;

    if (crc32(payload, h.payloadSize) != h.payloadCrc) {
        FP4_ERROR(LogCategory::Fs)
            << "SaveContainer: payload CRC mismatch; the save is corrupt";
        return false;
    }

    out.m_entries.reserve(h.entryCount);
    for (std::uint32_t i = 0; i < h.entryCount; ++i) {
        EntryOnDisk ed{};
        std::memcpy(&ed, entryBytes + i * sizeof(ed), sizeof(ed));
        if (ed.offset + ed.size > h.payloadSize) return false;
        Entry e;
        e.name  = readStr(ed.name, sizeof(ed.name));
        e.flags = ed.flags;
        e.data.resize(ed.size);
        if (ed.size > 0) {
            std::memcpy(e.data.data(), payload + ed.offset, ed.size);
        }
        out.m_entries.push_back(std::move(e));
    }
    return true;
}

} // namespace fusionps4::savedata
