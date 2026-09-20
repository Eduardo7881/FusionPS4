#include "sce/kernel/KernelObjects.hpp"

#include "debug/Log.hpp"

#include <chrono>

using fusionps4::debug::LogCategory;

namespace fusionps4::sce::kernel {

// ---- KernelSema ----------------------------------------------------------

KernelSema::KernelSema(std::string name, int initCount, int maxCount)
    : m_name(std::move(name)),
      m_max(maxCount > 0 ? maxCount : 1),
      m_count(initCount) {}

std::string KernelSema::describe() const {
    return "KernelSema{\"" + m_name + "\", count=" +
           std::to_string(currentCount()) + "/" + std::to_string(m_max) + "}";
}

bool KernelSema::wait(int need, int timeoutUs) {
    if (need <= 0) need = 1;
    std::unique_lock lock(m_mutex);

    auto pred = [&] { return m_count >= need; };

    if (timeoutUs < 0) {
        m_cv.wait(lock, pred);
    } else {
        using namespace std::chrono;
        if (!m_cv.wait_for(lock, microseconds(timeoutUs), pred)) {
            return false;
        }
    }
    m_count -= need;
    return true;
}

bool KernelSema::signal(int count) {
    if (count <= 0) count = 1;
    {
        std::lock_guard lock(m_mutex);
        if (m_count + count > m_max) {
            FP4_WARN(LogCategory::Sce)
                << "sema \"" << m_name << "\" overflow: " << m_count
                << " + " << count << " > " << m_max;
            return false;
        }
        m_count += count;
    }
    m_cv.notify_all();
    return true;
}

int KernelSema::currentCount() const {
    std::lock_guard lock(m_mutex);
    return m_count;
}

// ---- KernelMutex ---------------------------------------------------------

KernelMutex::KernelMutex(std::string name) : m_name(std::move(name)) {}

std::string KernelMutex::describe() const {
    return "KernelMutex{\"" + m_name + "\"}";
}

void KernelMutex::lock()              { m_mutex.lock(); }
bool KernelMutex::tryLock()           { return m_mutex.try_lock(); }
void KernelMutex::unlock()            { m_mutex.unlock(); }

} // namespace fusionps4::sce::kernel
