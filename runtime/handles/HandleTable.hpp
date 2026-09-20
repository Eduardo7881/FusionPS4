#pragma once

#include "runtime/handles/HandleObject.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace fusionps4::runtime::handles {

using Handle = std::int32_t;

inline constexpr Handle kInvalidHandle = -1;

class HandleTable {
public:
    HandleTable() = default;
    ~HandleTable() = default;

    HandleTable(const HandleTable&) = delete;
    HandleTable& operator=(const HandleTable&) = delete;

    // Register an object and return the new opaque PS4 handle.
    Handle registerObject(std::shared_ptr<HandleObject> obj);

    // Look up an object without removing it.
    std::shared_ptr<HandleObject> get(Handle h) const;

    // Explicitly close a handle. Returns true if it existed.
    bool close(Handle h);

    // Close everything (process teardown).
    void closeAll();

    std::vector<std::pair<Handle, std::shared_ptr<HandleObject>>> snapshot() const;

    std::size_t size() const;

private:
    mutable std::mutex m_mutex;
    Handle             m_next = 3;   // 0,1,2 reserved for std{in,out,err}
    std::unordered_map<Handle, std::shared_ptr<HandleObject>> m_objects;
};

} // namespace fusionps4::runtime::handles
