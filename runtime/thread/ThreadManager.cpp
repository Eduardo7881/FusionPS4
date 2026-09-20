#include "runtime/thread/ThreadManager.hpp"

#include "debug/Log.hpp"

using fusionps4::debug::LogCategory;

namespace fusionps4::runtime::thread {

ThreadManager::~ThreadManager() {
    shutdownAll();
}

ThreadId ThreadManager::create(const std::string& name, ThreadObject::Entry entry) {
    std::lock_guard lock(m_mutex);
    const ThreadId id = m_next++;
    auto obj = std::make_shared<ThreadObject>(id, name, std::move(entry));
    m_threads.emplace(id, std::move(obj));
    FP4_TRACE(LogCategory::Thread)
        << "created thread " << id << " \"" << name << "\"";
    return id;
}

bool ThreadManager::start(ThreadId id) {
    auto t = get(id);
    if (!t) return false;
    return t->start();
}

void ThreadManager::requestExit(ThreadId id) {
    if (auto t = get(id)) t->requestExit();
}

void ThreadManager::suspend(ThreadId id) {
    if (auto t = get(id)) t->suspend();
}

void ThreadManager::resume(ThreadId id) {
    if (auto t = get(id)) t->resume();
}

void ThreadManager::join(ThreadId id) {
    if (auto t = get(id)) t->join();
}

std::shared_ptr<ThreadObject> ThreadManager::get(ThreadId id) const {
    std::lock_guard lock(m_mutex);
    auto it = m_threads.find(id);
    return it == m_threads.end() ? nullptr : it->second;
}

std::vector<std::shared_ptr<ThreadObject>> ThreadManager::snapshot() const {
    std::lock_guard lock(m_mutex);
    std::vector<std::shared_ptr<ThreadObject>> out;
    out.reserve(m_threads.size());
    for (const auto& [_, t] : m_threads) out.push_back(t);
    return out;
}

void ThreadManager::shutdownAll() {
    std::vector<std::shared_ptr<ThreadObject>> threads;
    {
        std::lock_guard lock(m_mutex);
        threads.reserve(m_threads.size());
        for (auto& [_, t] : m_threads) threads.push_back(t);
    }
    for (auto& t : threads) {
        if (t) t->requestExit();
    }
    for (auto& t : threads) {
        if (t && t->state() != ThreadState::Created) {
            t->join();
        }
    }
    std::lock_guard lock(m_mutex);
    m_threads.clear();
}

std::size_t ThreadManager::aliveCount() const {
    std::lock_guard lock(m_mutex);
    std::size_t n = 0;
    for (const auto& [_, t] : m_threads) {
        if (t && t->state() != ThreadState::Exited) ++n;
    }
    return n;
}

} // namespace fusionps4::runtime::thread
