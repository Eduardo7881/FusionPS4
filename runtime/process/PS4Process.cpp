#include "runtime/process/PS4Process.hpp"
#include "isolation/IsolationConfig.hpp"
#include "runtime/GuestProcess.hpp"
#include "syscall/trap/TrapServer.hpp"
#include "sce/SceStubTable.hpp"
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

bool PS4Process::start(const isolation::IsolationConfig& iso) {
    std::lock_guard lock(m_mutex);
    if (m_state != ProcessState::Ready) return false;
    if (!m_mainModule) {
        FP4_ERROR(LogCategory::Loader)
            << "PS4Process::start without a loaded module";
        return false;
    }

    // Shared arena already exists inside AddressSpace; obtain its base and
    // size. The arena is the guest's virtual memory.
    const auto arenaBase = m_addressSpace.base();
    const auto arenaSize = m_addressSpace.capacity();

    // Create the trap server first so it can be bound to the guest before
    // the fork. Order matters: the server itself uses the ring; both live
    // in the arena.
    m_trapServer = std::make_unique<syscall::trap::TrapServer>(
        m_dispatcher,
        [](std::uint32_t id, const std::uint64_t args[6], bool* outIsError)
            -> std::int64_t {
            // SCE dispatch: look up by id in the SCE stub table.
            auto& table = sce::SceStubTable::instance();
            void* fn = table.resolveById(id);
            if (!fn) {
                if (outIsError) *outIsError = true;
                return -1;
            }
            using Fn = std::int64_t(*)(std::uint64_t, std::uint64_t,
                                        std::uint64_t, std::uint64_t,
                                        std::uint64_t, std::uint64_t);
            auto f = reinterpret_cast<Fn>(fn);
            if (outIsError) *outIsError = false;
            return f(args[0], args[1], args[2], args[3], args[4], args[5]);
        });

    m_guest = std::make_unique<GuestProcess>();
    if (!m_guest->configure(iso, arenaBase, arenaSize)) {
        m_guest.reset();
        m_trapServer.reset();
        return false;
    }
    m_trapServer->bindRing(&m_guest->ring());
    if (!m_trapServer->start()) {
        m_guest.reset();
        m_trapServer.reset();
        return false;
    }

    // Spawn the child. It will set up isolation and then jump to
    // guestEntryTrampoline with `this` as its argument.
    if (!m_guest->spawn(&PS4Process::guestEntryTrampoline, this)) {
        m_trapServer->stop();
        m_guest.reset();
        m_trapServer.reset();
        return false;
    }

    m_state = ProcessState::Running;
    return true;
}

int PS4Process::waitForGuest() {
    if (!m_guest) return 0;
    const int status = m_guest->wait();
    if (m_trapServer) {
        m_trapServer->stop();
    }
    m_state = ProcessState::Exited;
    return status;
}

void PS4Process::guestEntryTrampoline(void* arg) {
    auto* self = static_cast<PS4Process*>(arg);
    if (!self || !self->m_mainModule) {
        FP4_FATAL(LogCategory::Loader)
            << "guest trampoline invoked without a module";
        return;
    }
    const auto entry = self->m_mainModule->entryPoint();
    FP4_INFO(LogCategory::Loader)
        << "guest: jumping to entry " << reinterpret_cast<void*>(entry);

    using EntryFn = int(*)(int, char**, char**);
    auto fn = reinterpret_cast<EntryFn>(entry);
    const int rc = fn(0, nullptr, nullptr);
    FP4_INFO(LogCategory::Loader) << "guest: entry returned " << rc;
}

} // namespace fusionps4::runtime::process
