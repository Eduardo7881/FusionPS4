#pragma once

#include "runtime/handles/HandleObject.hpp"

#include <string>

namespace fusionps4::runtime::handles {

// A directory opened via open(O_DIRECTORY) or opendir(). Wraps a Linux fd.
class DirectoryHandle : public HandleObject {
public:
    DirectoryHandle(int         hostFd,
                    std::string guestPath,
                    std::string hostPath);
    ~DirectoryHandle() override;

    HandleType  type() const override { return HandleType::Directory; }
    const char* typeName() const override { return "Directory"; }
    std::string describe() const override;

    int hostFd() const { return m_hostFd; }
    const std::string& guestPath() const { return m_guestPath; }
    const std::string& hostPath()  const { return m_hostPath; }

private:
    int         m_hostFd;
    std::string m_guestPath;
    std::string m_hostPath;
};

} // namespace fusionps4::runtime::handles
