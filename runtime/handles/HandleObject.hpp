#pragma once

#include <cstdint>
#include <string>

namespace fusionps4::runtime::handles {

enum class HandleType : std::uint32_t {
    Unknown = 0,
    File,
    Directory,
    Socket,
    Event,
    Semaphore,
    Mutex,
    Thread,
    Process,
    SharedMemory,
    Device,
};

class HandleObject {
public:
    virtual ~HandleObject() = default;

    virtual HandleType  type() const = 0;
    virtual const char* typeName() const = 0;
    virtual std::string describe() const { return typeName(); }
};

} // namespace fusionps4::runtime::handles
