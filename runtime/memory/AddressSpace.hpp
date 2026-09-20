#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace fusionps4::runtime::memory {

using GuestAddress = std::uint64_t;

enum class RegionProt : std::uint32_t {
    None    = 0,
    Read    = 1u << 0,
    Write   = 1u << 1,
    Execute = 1u << 2,
};

inline RegionProt operator|(RegionProt a, RegionProt b) {
    return static_cast<RegionProt>(static_cast<std::uint32_t>(a) |
                                   static_cast<std::uint32_t>(b));
}
inline bool hasProt(RegionProt v, RegionProt f) {
    return (static_cast<std::uint32_t>(v) &
            static_cast<std::uint32_t>(f)) != 0u;
}

struct MemoryRegion {
    GuestAddress base    = 0;
    std::size_t  size    = 0;
    RegionProt   prot    = RegionProt::None;
    void*        hostPtr = nullptr;
    std::string  name;
};

class AddressSpace {
public:
    AddressSpace();
    ~AddressSpace();

    AddressSpace(const AddressSpace&) = delete;
    AddressSpace& operator=(const AddressSpace&) = delete;

    bool init();
    void destroy();

    // Map a new anonymous region inside the guest VA range.
    GuestAddress map(GuestAddress hint,
                     std::size_t  size,
                     RegionProt   prot,
                     const std::string& name = {});

    bool unmap(GuestAddress addr, std::size_t size);
    bool protect(GuestAddress addr, std::size_t size, RegionProt prot);

    std::optional<MemoryRegion> find(GuestAddress addr) const;
    std::vector<MemoryRegion>   regions() const;

    // Since guest VAs are identity-mapped to host, this is just a cast,
    // but we validate the region first to catch bugs.
    void* hostPointer(GuestAddress addr);

    GuestAddress base()     const { return m_base; }
    std::size_t  capacity() const { return m_capacity; }

private:
    GuestAddress findFreeGap(std::size_t size, GuestAddress hint) const;

    GuestAddress m_base     = 0;
    std::size_t  m_capacity = 0;
    GuestAddress m_nextHint = 0;

    mutable std::mutex        m_mutex;
    std::vector<MemoryRegion> m_regions;
};

} // namespace fusionps4::runtime::memory
