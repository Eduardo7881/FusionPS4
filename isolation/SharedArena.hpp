#pragma once

#include <cstdint>
#include <mutex>
#include <string>

namespace fusionps4::isolation {

// A single shared mapping used as the guest's virtual address space.
//
// Because the arena is backed by a memfd and mapped MAP_SHARED at a fixed
// VA in both the runtime (parent) and the guest (child), the runtime can
// inspect guest memory directly using the same pointer arithmetic as if
// the guest were in-process. Crucially, the runtime keeps PROT_READ |
// PROT_WRITE on the whole arena, while the guest's view starts PROT_NONE
// and is only relaxed per-region at the runtime's command.
//
// This is the mechanism that lets us keep the guest *out* of the runtime's
// address space while still being able to read/write guest memory at
// syscall time.
class SharedArena {
public:
    SharedArena() = default;
    ~SharedArena();

    SharedArena(const SharedArena&) = delete;
    SharedArena& operator=(const SharedArena&) = delete;

    // Reserves `size` bytes at `base` and creates the backing memfd. The
    // calling process (parent) gets PROT_READ | PROT_WRITE.
    bool init(std::uintptr_t base, std::size_t size);
    void destroy();

    bool           valid() const { return m_base != 0; }
    std::uintptr_t base()  const { return m_base; }
    std::size_t    size()  const { return m_size; }
    int            fd()    const { return m_fd; }

    // Returns true if [addr, addr+len) is within the arena. Does not check
    // protection.
    bool contains(std::uintptr_t addr, std::size_t len) const;

    // Translate a guest VA to a host pointer usable by the runtime. Because
    // the runtime maps the arena at the same VA, this is a simple cast,
    // but we validate the range first.
    void* hostPointer(std::uintptr_t guestAddr);

    // Change the calling process's view of a sub-range. Used by the runtime
    // to hide/unhide regions from the guest; has no effect on the child
    // unless the child calls it too (the child's trap handler does that on
    // behalf of mmap/mprotect).
    bool protect(std::uintptr_t addr, std::size_t len, int prot);

private:
    int            m_fd   = -1;
    std::uintptr_t m_base = 0;
    std::size_t    m_size = 0;
    mutable std::mutex m_mutex;
};

} // namespace fusionps4::isolation
