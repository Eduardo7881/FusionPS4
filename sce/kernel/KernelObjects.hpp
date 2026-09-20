#pragma once

#include "runtime/handles/HandleObject.hpp"

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>

namespace fusionps4::sce::kernel {

// A counting semaphore, matching the semantics of sceKernelCreateSema /
// sceKernelWaitSema / sceKernelSignalSema.
class KernelSema : public runtime::handles::HandleObject {
public:
    KernelSema(std::string name, int initCount, int maxCount);

    HandleType  type() const override { return HandleType::Semaphore; }
    const char* typeName() const override { return "KernelSema"; }
    std::string describe() const override;

    // Blocks until the count reaches at least `need`. Returns false on
    // timeout (which never happens when timeoutUs < 0).
    bool wait(int need, int timeoutUs);

    // Adds `count` slots, up to max. Returns false on overflow.
    bool signal(int count);

    const std::string& name() const { return m_name; }
    int currentCount() const;

private:
    std::string             m_name;
    int                     m_max;
    int                     m_count;
    mutable std::mutex      m_mutex;
    std::condition_variable m_cv;
};

// A recursive-capable kernel mutex (libSceKernel pthread wrapper). Backed
// by a std::mutex + ownership tracking so it can be queried.
class KernelMutex : public runtime::handles::HandleObject {
public:
    explicit KernelMutex(std::string name);

    HandleType  type() const override { return HandleType::Mutex; }
    const char* typeName() const override { return "KernelMutex"; }
    std::string describe() const override;

    void lock();
    bool tryLock();
    void unlock();

private:
    std::string             m_name;
    std::mutex              m_mutex;
};

} // namespace fusionps4::sce::kernel
