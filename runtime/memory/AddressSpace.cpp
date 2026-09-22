#include "runtime/memory/AddressSpace.hpp"
#include "isolation/SharedArena.hpp"
#include "debug/Log.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <sys/mman.h>

using fusionps4::debug::LogCategory;

namespace fusionps4::runtime::memory {

namespace {

constexpr std::uint64_t kGuestBase = 0x0000'1000'0000'0000ULL;
constexpr std::uint64_t kGuestSize = 0x0000'0100'0000'0000ULL; // 1 TiB

int toHostProt(RegionProt p) {
    int r = 0;
    if (hasProt(p, RegionProt::Read))    r |= PROT_READ;
    if (hasProt(p, RegionProt::Write))   r |= PROT_WRITE;
    if (hasProt(p, RegionProt::Execute)) r |= PROT_EXEC;
    return r;
}

constexpr std::size_t kPageSize = 0x1000;

std::size_t alignUp(std::size_t v, std::size_t a) {
    return (v + a - 1) & ~(a - 1);
}

} // namespace

AddressSpace::AddressSpace() = default;

AddressSpace::~AddressSpace() {
    destroy();
}

bool AddressSpace::init() {
    std::lock_guard lock(m_mutex);
    if (m_base) return true;

    m_arena = std::make_unique<isolation::SharedArena>();
    if (!m_arena->init(kGuestBase, static_cast<std::size_t>(kGuestSize))) {
        FP4_ERROR(LogCategory::Memory)
            << "SharedArena initialization failed";
        m_arena.reset();
        return false;
    }

    m_base     = kGuestBase;
    m_capacity = static_cast<std::size_t>(kGuestSize);
    m_nextHint = m_base + 0x10000;
    return true;
}

void AddressSpace::destroy() {
    std::lock_guard lock(m_mutex);
    if (m_arena) {
        m_arena->destroy();
        m_arena.reset();
    }
    m_base = 0;
    m_capacity = 0;
    m_nextHint = 0;
    m_regions.clear();
}

GuestAddress AddressSpace::findFreeGap(std::size_t size,
                                       GuestAddress hint) const {
    // Try the caller's hint first.
    if (hint != 0 && hint >= m_base && hint + size <= m_base + m_capacity) {
        bool clash = false;
        for (const auto& r : m_regions) {
            if (!(hint + size <= r.base || hint >= r.base + r.size)) {
                clash = true;
                break;
            }
        }
        if (!clash) return hint;
    }

    // First-fit from m_nextHint over sorted regions.
    auto sorted = m_regions;
    std::sort(sorted.begin(), sorted.end(),
              [](const MemoryRegion& a, const MemoryRegion& b) {
                  return a.base < b.base;
              });

    GuestAddress cur = m_nextHint;
    for (const auto& r : sorted) {
        if (cur + size <= r.base) return cur;
        cur = std::max(cur, r.base + r.size);
    }
    if (cur + size <= m_base + m_capacity) return cur;
    return 0;
}

AddressSpace::map(GuestAddress hint,
                               std::size_t  size,
                               RegionProt   prot,
                               const std::string& name) {
    std::lock_guard lock(m_mutex);
    if (!m_base || !m_arena) return 0;

    size = alignUp(size, kPageSize);
    const GuestAddress where = findFreeGap(size, hint);
    if (!where) return 0;

    // The arena is already mapped RW in the parent. Just record the region;
    // the guest's view is changed separately by the child's trap handler.
    MemoryRegion region;
    region.base    = where;
    region.size    = size;
    region.prot    = prot;
    region.hostPtr = m_arena->hostPointer(where);
    region.name    = name;
    m_regions.push_back(region);
    if (where == m_nextHint) m_nextHint = where + size;
    return where;
}

bool AddressSpace::unmap(GuestAddress addr, std::size_t size) {
    std::lock_guard lock(m_mutex);
    for (auto it = m_regions.begin(); it != m_regions.end(); ++it) {
        if (it->base == addr) {
            ::munmap(reinterpret_cast<void*>(it->base), it->size);
            m_regions.erase(it);
            (void)size;
            return true;
        }
    }
    return false;
}

bool AddressSpace::protect(GuestAddress addr, std::size_t size, RegionProt prot) {
    std::lock_guard lock(m_mutex);
    for (auto& r : m_regions) {
        if (addr >= r.base && addr < r.base + r.size) {
            if (::mprotect(reinterpret_cast<void*>(r.base), r.size,
                           toHostProt(prot)) != 0) {
                FP4_ERROR(LogCategory::Memory)
                    << "mprotect failed at " << reinterpret_cast<void*>(r.base)
                    << ": " << std::strerror(errno);
                return false;
            }
            r.prot = prot;
            (void)size;
            return true;
        }
    }
    return false;
}

std::optional<MemoryRegion> AddressSpace::find(GuestAddress addr) const {
    std::lock_guard lock(m_mutex);
    for (const auto& r : m_regions) {
        if (addr >= r.base && addr < r.base + r.size) return r;
    }
    return std::nullopt;
}

std::vector<MemoryRegion> AddressSpace::regions() const {
    std::lock_guard lock(m_mutex);
    return m_regions;
}

void* AddressSpace::hostPointer(GuestAddress addr) {
    if (find(addr)) return reinterpret_cast<void*>(addr);
    return nullptr;
}

} // namespace fusionps4::runtime::memory
