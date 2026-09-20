#include "runtime/handles/DirectoryHandle.hpp"

#include "debug/Log.hpp"

#include <unistd.h>

using fusionps4::debug::LogCategory;

namespace fusionps4::runtime::handles {

DirectoryHandle::DirectoryHandle(int         hostFd,
                                 std::string guestPath,
                                 std::string hostPath)
    : m_hostFd(hostFd),
      m_guestPath(std::move(guestPath)),
      m_hostPath(std::move(hostPath)) {}

DirectoryHandle::~DirectoryHandle() {
    if (m_hostFd >= 0) {
        ::close(m_hostFd);
        m_hostFd = -1;
    }
}

std::string DirectoryHandle::describe() const {
    return "Directory{fd=" + std::to_string(m_hostFd) + ", guest=\"" +
           m_guestPath + "\"}";
}

} // namespace fusionps4::runtime::handles
