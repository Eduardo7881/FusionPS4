#pragma once

#include "runtime/handles/HandleObject.hpp"

#include <cstdint>
#include <string>

namespace fusionps4::runtime::handles {

// A FileHandle wraps a Linux fd obtained from the runtime's virtual
// filesystem. The guest never sees the fd number directly: it sees a PS4
// handle value assigned by HandleTable.
class FileHandle : public HandleObject {
public:
    FileHandle(int                hostFd,
               std::string        guestPath,
               std::string        hostPath,
               std::int32_t       openFlags);
    ~FileHandle() override;

    HandleType  type() const override { return HandleType::File; }
    const char* typeName() const override { return "File"; }
    std::string describe() const override;

    int          hostFd()      const { return m_hostFd; }
    const std::string& guestPath() const { return m_guestPath; }
    const std::string& hostPath()  const { return m_hostPath; }
    std::int32_t openFlags()   const { return m_openFlags; }

    bool isReadable() const;
    bool isWritable() const;

private:
    int          m_hostFd;
    std::string  m_guestPath;
    std::string  m_hostPath;
    std::int32_t m_openFlags;
};

} // namespace fusionps4::runtime::handles
