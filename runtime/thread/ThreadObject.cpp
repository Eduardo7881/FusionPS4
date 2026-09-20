#include "runtime/thread/ThreadObject.hpp"

#include "debug/Log.hpp"

#include <cerrno>
#include <cstring>
#include <pthread.h>
#include <sched.h>

using fusionps4::debug::LogCategory;

namespace fusionps4::runtime::thread {

namespace {

struct TrampolineCtx {
    ThreadObject* self;
};

extern "C" void* fusionps4_thread_trampoline(void* arg) {
    auto* ctx = static_cast<TrampolineCtx*>(arg);
    ThreadObject* self = ctx->self;
    delete ctx;
    self->runEntry();
    return nullptr;
}

} // namespace

ThreadObject::ThreadObject(ThreadId id, std::string name, Entry entry)
    : m_id(id), m_name(std::move(name)), m_entry(std::move(entry)) {}

ThreadObject::~ThreadObject() {
    if (m_state.load() == ThreadState::Running) {
        requestExit();
    }
}

bool ThreadObject::start() {
    if (m_state.load() != ThreadState::Created) return false;

    auto* ctx = new TrampolineCtx{this};

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    const int rc = pthread_create(&m_handle, &attr,
                                  fusionps4_thread_trampoline, ctx);
    pthread_attr_destroy(&attr);

    if (rc != 0) {
        delete ctx;
        FP4_ERROR(LogCategory::Thread)
            << "pthread_create(\"" << m_name << "\") failed: "
            << std::strerror(rc);
        return false;
    }

    pthread_setname_np(m_handle, m_name.c_str());
    m_state.store(ThreadState::Running);
    FP4_DEBUG(LogCategory::Thread)
        << "thread " << m_id << " (\"" << m_name << "\") started";
    return true;
}

void ThreadObject::runEntry() {
    // Suspend-aware wrapper: while a thread is suspended it blocks before
    // running user code, and resume() wakes it up.
    waitWhileSuspended();

    if (!m_exitRequested.load()) {
        try {
            if (m_entry) m_entry();
        } catch (const std::exception& ex) {
            FP4_ERROR(LogCategory::Thread)
                << "thread " << m_id << " threw: " << ex.what();
        } catch (...) {
            FP4_ERROR(LogCategory::Thread)
                << "thread " << m_id << " threw unknown exception";
        }
    }
    m_state.store(ThreadState::Exited);
    FP4_DEBUG(LogCategory::Thread) << "thread " << m_id << " exited";
}

void ThreadObject::requestExit() {
    m_exitRequested.store(true);
    resume();
}

void ThreadObject::suspend() {
    std::lock_guard lock(m_suspendMutex);
    m_suspended.store(true);
    m_state.store(ThreadState::Suspended);
}

void ThreadObject::resume() {
    {
        std::lock_guard lock(m_suspendMutex);
        m_suspended.store(false);
        if (m_state.load() == ThreadState::Suspended) {
            m_state.store(ThreadState::Running);
        }
    }
    m_suspendCv.notify_all();
}

void ThreadObject::waitWhileSuspended() {
    std::unique_lock lock(m_suspendMutex);
    m_suspendCv.wait(lock, [this] { return !m_suspended.load(); });
}

void ThreadObject::join() {
    if (m_state.load() == ThreadState::Created) return;
    pthread_join(m_handle, nullptr);
}

void ThreadObject::setPriority(int prio) {
    sched_param sp{};
    sp.sched_priority = prio;
    const int rc = pthread_setschedparam(m_handle, SCHED_OTHER, &sp);
    if (rc != 0) {
        FP4_WARN(LogCategory::Thread)
            << "pthread_setschedparam failed for thread " << m_id << ": "
            << std::strerror(rc);
    }
}

void ThreadObject::setAffinity(std::uint64_t mask) {
    cpu_set_t set;
    CPU_ZERO(&set);
    for (int i = 0; i < 64; ++i) {
        if (mask & (1ull << i)) CPU_SET(i, &set);
    }
    const int rc = pthread_setaffinity_np(m_handle, sizeof(set), &set);
    if (rc != 0) {
        FP4_WARN(LogCategory::Thread)
            << "pthread_setaffinity_np failed for thread " << m_id << ": "
            << std::strerror(rc);
    }
}

} // namespace fusionps4::runtime::thread
