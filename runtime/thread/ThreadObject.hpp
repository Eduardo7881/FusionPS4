#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <pthread.h>

namespace fusionps4::runtime::thread {

using ThreadId = std::uint32_t;

enum class ThreadState : std::uint32_t {
    Created = 0,
    Running,
    Suspended,
    Exited,
};

class ThreadObject {
public:
    using Entry = std::function<void()>;

    ThreadObject(ThreadId id, std::string name, Entry entry);
    ~ThreadObject();

    ThreadObject(const ThreadObject&) = delete;
    ThreadObject& operator=(const ThreadObject&) = delete;

    bool start();
    void requestExit();
    void suspend();
    void resume();
    void join();

    // Called from the pthread trampoline; do not call directly.
    void runEntry();

    ThreadId            id()     const { return m_id; }
    const std::string&  name()   const { return m_name; }
    ThreadState         state()  const { return m_state.load(); }
    pthread_t           native() const { return m_handle; }

    void setPriority(int prio);
    void setAffinity(std::uint64_t mask);

private:
    void waitWhileSuspended();

    ThreadId            m_id;
    std::string         m_name;
    Entry               m_entry;
    pthread_t           m_handle{};
    std::atomic<ThreadState> m_state{ThreadState::Created};
    std::atomic<bool>   m_exitRequested{false};
    std::atomic<bool>   m_suspended{false};

    std::mutex              m_suspendMutex;
    std::condition_variable m_suspendCv;
};

} // namespace fusionps4::runtime::thread
