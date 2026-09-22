#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace fusionps4::savedata {

// A SaveContainer is the runtime's own save-file format. It is designed
// after PS4's SDAT but is not bit-for-bit compatible; titles that consume
// the raw bytes through sceSaveData* APIs see a valid container via
// translation, not the exact SDAT the PS4 produces.
//
// Layout:
//     SaveHeader
//     SaveEntry[entryCount]        (fixed-size descriptors)
//     payload bytes                (concatenated, in entry order)
//
// The container is self-describing, so a future change to the file format
// can migrate by version bump without breaking existing saves.
class SaveContainer {
public:
    static constexpr std::uint32_t kMagic   = 0x54414453u;  // "SDAT"
    static constexpr std::uint32_t kVersion = 1;

    struct Entry {
        std::string   name;
        std::vector<std::uint8_t> data;
        std::uint32_t flags = 0;
    };

    SaveContainer() = default;

    // Metadata.
    void setTitleId(std::uint64_t id) { m_titleId = id; }
    std::uint64_t titleId() const { return m_titleId; }

    void setTitleName(const std::string& s) { m_titleName = s; }
    const std::string& titleName() const { return m_titleName; }

    void setSubtitle(const std::string& s) { m_subtitle = s; }
    const std::string& subtitle() const { return m_subtitle; }

    void setDetail(const std::string& s) { m_detail = s; }
    const std::string& detail() const { return m_detail; }

    std::uint64_t createdAt()  const { return m_createdAt;  }
    std::uint64_t modifiedAt() const { return m_modifiedAt; }

    // ---- entries --------------------------------------------------------
    void addEntry(const std::string& name, const void* data, std::size_t size,
                  std::uint32_t flags = 0);
    bool hasEntry(const std::string& name) const;
    const Entry* entry(const std::string& name) const;
    std::vector<std::string> entryNames() const;
    std::size_t entryCount() const { return m_entries.size(); }

    // ---- serialization --------------------------------------------------
    std::vector<std::uint8_t> serialize() const;
    static bool deserialize(const void* data, std::size_t size,
                            SaveContainer& out);

private:
    std::uint64_t             m_titleId    = 0;
    std::string               m_titleName;
    std::string               m_subtitle;
    std::string               m_detail;
    std::uint64_t             m_createdAt  = 0;
    std::uint64_t             m_modifiedAt = 0;
    std::vector<Entry>        m_entries;
};

} // namespace fusionps4::savedata
