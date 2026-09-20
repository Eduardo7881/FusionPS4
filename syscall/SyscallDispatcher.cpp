#include "syscall/SyscallDispatcher.hpp"

#include "debug/Log.hpp"

#include <vector>

using fusionps4::debug::LogCategory;

namespace fusionps4::syscall {

SyscallDispatcher::SyscallDispatcher() = default;

void SyscallDispatcher::registerHandler(std::int64_t  number,
                                        std::string   name,
                                        SyscallHandler handler) {
    if (!handler) {
        FP4_ERROR(LogCategory::Syscall)
            << "refusing to register null handler for syscall " << number;
        return;
    }
    std::lock_guard lock(m_mutex);
    m_handlers[number] = Entry{std::move(name), std::move(handler)};
}

bool SyscallDispatcher::hasHandler(std::int64_t number) const {
    std::lock_guard lock(m_mutex);
    return m_handlers.find(number) != m_handlers.end();
}

void SyscallDispatcher::dispatch(SyscallContext& ctx) {
    Entry entry;
    {
        std::lock_guard lock(m_mutex);
        auto it = m_handlers.find(ctx.number);
        if (it == m_handlers.end()) {
            // Report and fail, but do not throw: guest expects a valid rax.
            std::string msg = "UNIMPLEMENTED syscall number=" +
                              std::to_string(ctx.number) +
                              " args=[" + std::to_string(ctx.args[0]) + "," +
                              std::to_string(ctx.args[1]) + "," +
                              std::to_string(ctx.args[2]) + "," +
                              std::to_string(ctx.args[3]) + "," +
                              std::to_string(ctx.args[4]) + "," +
                              std::to_string(ctx.args[5]) + "]";
            FP4_ERROR(LogCategory::Syscall) << msg;
            ctx.fail(freebsd::kEnosys);
            ctx.name = "unimplemented";
            return;
        }
        entry = it->second;
        ++m_callCounts[ctx.number];
    }

    ctx.name = entry.name.c_str();
    FP4_TRACE(LogCategory::Syscall)
        << entry.name << "(" << ctx.args[0] << ", " << ctx.args[1] << ", "
        << ctx.args[2] << ", " << ctx.args[3] << ", " << ctx.args[4] << ", "
        << ctx.args[5] << ")";

    try {
        entry.handler(ctx);
    } catch (const std::exception& ex) {
        FP4_ERROR(LogCategory::Syscall)
            << "handler for " << entry.name << " threw: " << ex.what();
        ctx.fail(freebsd::kEinval);
    } catch (...) {
        FP4_ERROR(LogCategory::Syscall)
            << "handler for " << entry.name << " threw unknown exception";
        ctx.fail(freebsd::kEinval);
    }

    if (ctx.isError) {
        FP4_TRACE(LogCategory::Syscall)
            << entry.name << " failed: "
            << freebsd::ErrnoName::name(ctx.retval) << " (" << ctx.retval << ")";
    } else {
        FP4_TRACE(LogCategory::Syscall)
            << entry.name << " returned " << ctx.retval;
    }
}

std::size_t SyscallDispatcher::handlerCount() const {
    std::lock_guard lock(m_mutex);
    return m_handlers.size();
}

std::vector<std::int64_t> SyscallDispatcher::registeredNumbers() const {
    std::lock_guard lock(m_mutex);
    std::vector<std::int64_t> out;
    out.reserve(m_handlers.size());
    for (const auto& [n, _] : m_handlers) out.push_back(n);
    return out;
}

} // namespace fusionps4::syscall
