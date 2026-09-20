#pragma once

#include "syscall/SyscallContext.hpp"

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

namespace fusionps4::syscall {

using SyscallHandler = std::function<void(SyscallContext&)>;

// The SyscallDispatcher is per-process. It exposes a small registration API
// so that phase-specific handlers (fs, memory, thread, sce) can attach
// themselves without the dispatcher needing to know about them.
class SyscallDispatcher {
public:
    SyscallDispatcher();
    ~SyscallDispatcher() = default;

    SyscallDispatcher(const SyscallDispatcher&) = delete;
    SyscallDispatcher& operator=(const SyscallDispatcher&) = delete;

    // Register a handler for a given syscall number. Replaces any previous
    // handler. `name` is used for logging and diagnostics.
    void registerHandler(std::int64_t  number,
                         std::string   name,
                         SyscallHandler handler);

    bool hasHandler(std::int64_t number) const;

    // Dispatch a single call. On unknown numbers, sets an error and reports
    // UNIMPLEMENTED through the logging system.
    void dispatch(SyscallContext& ctx);

    // Number of handlers currently registered.
    std::size_t handlerCount() const;

    // Snapshot of registered numbers for diagnostics.
    std::vector<std::int64_t> registeredNumbers() const;

private:
    struct Entry {
        std::string    name;
        SyscallHandler handler;
    };

    mutable std::mutex                         m_mutex;
    std::unordered_map<std::int64_t, Entry>    m_handlers;
    std::unordered_map<std::int64_t, std::uint64_t> m_callCounts;
};

} // namespace fusionps4::syscall
