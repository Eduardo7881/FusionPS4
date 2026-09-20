#include "runtime/handles/FileHandle.hpp"

#include "debug/Log.hpp"
#include "syscall/freebsd/FreeBsd.hpp"

#include <unistd.h>

using fusionps4::debug::LogCategory;
namespace fs = fusionps4::syscall::freebsd;

namespace fusionps4::runtime::handles {

FileHandle::FileHandle(int                hostFd,
                       std::string        guestPath,
                       std::string        hostPath,
                       std::int32_t       openFlags)
    : m_hostFd(hostFd),
      m_guestPath(std::move(guestPath)),
      m_hostPath(std::move(hostPath)),
      m_openFlags(openFlags) {}

FileHandle::~FileHandle() {
    if (m_hostFd >= 0) {
        ::close(m_hostFd);
        FP4_TRACE(LogCategory::Fs)
            << "closed fd=" << m_hostFd << " for \"" << m_guestPath << "\"";
        m_hostFd = -1;
    }
}

std::string FileHandle::describe() const {
    return "File{fd=" + std::to_string(m_hostFd) + ", guest=\"" +
           m_guestPath + "\"}";
}

bool FileHandle::isReadable() const {
    const auto m = m_openFlags & fs::kO_AccMode;
    return m == fs::kO_RdOnly || m == fs::kO_RdWr;
}

bool FileHandle::isWritable() const {
    const auto m = m_openFlags & fs::kO_AccMode;
    return m == fs::kO_WrOnly || m == fs::kO_RdWr;
}

} // namespace fusionps4::runtime::handles
