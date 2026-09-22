#include "isolation/SharedArena.hpp"

#include "debug/Log.hpp"

#include <cerrno>
#include <cstring>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

using fusionps4::debug::LogCategory;

#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC 0x0001U
#endif

namespace fusionps4::isolation {

namespace {

int memfdCreate(const char* name, unsigned int flags) {
    return static_cast<int>(::syscall(SYS_memfd_create, name, flags));
}

} // namespace

SharedArena::~SharedArena() {
    destroy();
}

bool SharedArena::init(std::uintptr_t base, std::size_t size) {
    std::lock_guard lock(m_mutex);
    if (m_base != 0) return true;

    m_fd = memfdCreate("fusionps4-arena", 0);
    if (m_fd < 0) {
        FP4_ERROR(LogCategory::Memory)
            << "memfd_create failed: " << std::strerror(errno);
        return false;
    }

    if (::ftruncate(m_fd, static_cast<off_t>(size)) != 0) {
        FP4_ERROR(LogCategory::Memory)
            << "ftruncate(memfd, " << size << ") failed: "
            << std::strerror(errno);
        ::close(m_fd);
        m_fd = -1;
        return false;
    }

    // Parent maps the arena RW so it can inspect guest memory at will.
    void* r = ::mmap(reinterpret_cast<void*>(base), size,
                     PROT_READ | PROT_WRITE,
                     MAP_SHARED | MAP_FIXED_NOREPLACE,
                     m_fd, 0);
    if (r == MAP_FAILED) {
        FP4_ERROR(LogCategory::Memory)
            << "mmap(arena) failed: " << std::strerror(errno);
        ::close(m_fd);
        m_fd = -1;
        return false;
    }
    if (reinterpret_cast<std::uintptr_t>(r) != base) {
        FP4_ERROR(LogCategory::Memory)
            << "mmap(arena) returned unexpected base "
            << reinterpret_cast<void*>(r);
        ::munmap(r, size);
        ::close(m_fd);
        m_fd = -1;
        return false;
    }

    m_base = base;
    m_size = size;

    FP4_INFO(LogCategory::Memory)
        << "SharedArena: [" << reinterpret_cast<void*>(m_base) << ", "
        << reinterpret_cast<void*>(m_base + m_size) << ") size="
        << (m_size >> 30) << " GiB fd=" << m_fd;
    return true;
}

void SharedArena::destroy() {
    std::lock_guard lock(m_mutex);
    if (m_base) {
        ::munmap(reinterpret_cast<void*>(m_base), m_size);
        m_base = 0;
        m_size = 0;
    }
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
}

bool SharedArena::contains(std::uintptr_t addr, std::size_t len) const {
    if (addr < m_base) return false;
    const auto off = addr - m_base;
    if (off > m_size) return false;
    if (len > m_size - off) return false;
    return true;
}

void* SharedArena::hostPointer(std::uintptr_t guestAddr) {
    if (!contains(guestAddr, 1)) return nullptr;
    return reinterpret_cast<void*>(guestAddr);
}

bool SharedArena::protect(std::uintptr_t addr, std::size_t len, int prot) {
    if (!contains(addr, len)) return false;
    if (::mprotect(reinterpret_cast<void*>(addr), len, prot) != 0) {
        FP4_ERROR(LogCategory::Memory)
            << "mprotect(" << reinterpret_cast<void*>(addr) << ", " << len
            << ", " << prot << ") failed: " << std::strerror(errno);
        return false;
    }
    return true;
}

} // namespace fusionps4::isolation
