#pragma once

#include "runtime/thread/ThreadObject.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace fusionps4::runtime::thread {

class ThreadManager {
public:
    ThreadManager() = default;
    ~ThreadManager();

    ThreadManager(const ThreadManager&) = delete;
    ThreadManager& operator=(const ThreadManager&) = delete;

    // Creates a PS4 thread object and returns its id. Does not start it.
    ThreadId create(const std::string& name, ThreadObject::Entry entry);

    bool start(ThreadId id);
    void requestExit(ThreadId id);
    void suspend(ThreadId id);
    void resume(ThreadId id);
    void join(ThreadId id);

    std::shared_ptr<ThreadObject> get(ThreadId id) const;
    std::vector<std::shared_ptr<ThreadObject>> snapshot() const;

    // Requests exit for all threads and joins them. Called during process
    // teardown.
    void shutdownAll();

    std::size_t aliveCount() const;

private:
    mutable std::mutex m_mutex;
    ThreadId           m_next = 1;
    std::unordered_map<ThreadId, std::shared_ptr<ThreadObject>> m_threads;
};

} // namespace fusionps4::runtime::thread
