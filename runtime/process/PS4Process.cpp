#include "runtime/process/PS4Process.hpp"

#include "debug/Log.hpp"

using fusionps4::debug::LogCategory;
using fusionps4::runtime::memory::RegionProt;

namespace fusionps4::runtime::process {

PS4Process::PS4Process(ProcessId id, std::string name)
    : m_id(id), m_name(std::move(name)) {}

PS4Process::~PS4Process() {
    shutdown();
}

bool PS4Process::init() {
    std::lock_guard lock(m_mutex);
    if (m_state != ProcessState::Created) return false;

    if (!m_addressSpace.init()) {
        FP4_ERROR(LogCategory::Process)
            << "process " << m_id << " address space init failed";
        return false;
    }

    // Standard baseline mappings that any PS4 process expects.
    // Phase 2 will extend this with heap/stack/TLS/module regions.
    constexpr std::size_t kStackSize = 8u * 1024 * 1024;
    constexpr std::size_t kHeapSize  = 64u * 1024 * 1024;

    if (!m_addressSpace.map(0, kStackSize,
                            RegionProt::Read | RegionProt::Write,
                            "main-stack")) {
        return false;
    }
    if (!m_addressSpace.map(0, kHeapSize,
                            RegionProt::Read | RegionProt::Write,
                            "main-heap")) {
        return false;
    }

    m_state = ProcessState::Ready;
    FP4_INFO(LogCategory::Process)
        << "process " << m_id << " (\"" << m_name << "\") initialized";
    return true;
}

void PS4Process::shutdown() {
    std::lock_guard lock(m_mutex);
    if (m_state == ProcessState::Exited) return;

    m_threads.shutdownAll();
    m_handleTable.closeAll();
    m_addressSpace.destroy();

    m_state = ProcessState::Exited;
    FP4_INFO(LogCategory::Process) << "process " << m_id << " shut down";
}

bool PS4Process::loadExecutable(const std::string& hostPath) {
    // Phase 2 - ELF loader is responsible for mapping the PT_LOAD segments,
    // resolving symbols against the SCE stubs and building TLS.
    FP4_UNIMPLEMENTED(LogCategory::Loader, "PS4Process::loadExecutable");
    FP4_ERROR(LogCategory::Loader)
        << "  path=" << hostPath
        << "  reason=ELF loader not built into this binary yet";
    m_state = ProcessState::Loading;
    return false;
}

bool PS4Process::startMainThread(runtime::thread::ThreadObject::Entry entry) {
    std::lock_guard lock(m_mutex);
    if (m_state != ProcessState::Ready && m_state != ProcessState::Suspended) {
        return false;
    }
    m_mainThread = m_threads.create("main", std::move(entry));
    if (!m_threads.start(m_mainThread)) return false;
    m_state = ProcessState::Running;
    return true;
}

void PS4Process::suspend() {
    std::lock_guard lock(m_mutex);
    if (m_state != ProcessState::Running) return;
    for (auto& t : m_threads.snapshot()) t->suspend();
    m_state = ProcessState::Suspended;
}

void PS4Process::resume() {
    std::lock_guard lock(m_mutex);
    if (m_state != ProcessState::Suspended) return;
    for (auto& t : m_threads.snapshot()) t->resume();
    m_state = ProcessState::Running;
}

} // namespace fusionps4::runtime::process
