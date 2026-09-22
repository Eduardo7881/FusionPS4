#pragma once

#include "runtime/handles/HandleTable.hpp"
#include "runtime/memory/AddressSpace.hpp"
#include "runtime/thread/ThreadManager.hpp"
#include "runtime/GuestProcess.hpp"
#include "syscall/trap/TrapServer.hpp"

#include <memory>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

namespace fusionps4::runtime::process {

using ProcessId = std::uint32_t;

enum class ProcessState {
    Created,
    Loading,
    Ready,
    Running,
    Suspended,
    Exited,
};

// A PS4Process owns:
//   - a virtual address space (guest VA identity-mapped to host VA),
//   - a handle table whose values are opaque PS4 handles,
//   - a thread manager for the guest threads,
//   - module / environment metadata (populated by the loader in Phase 2).
class PS4Process {
public:
    PS4Process(ProcessId id, std::string name);
    ~PS4Process();

    PS4Process(const PS4Process&) = delete;
    PS4Process& operator=(const PS4Process&) = delete;

    bool init();
    void shutdown();

    // Phase 2 will implement ELF load, module resolution, TLS setup,
    // relocations. Phase 1 reports UNIMPLEMENTED and returns false.
    bool loadExecutable(const std::string& hostPath);

    bool startMainThread(runtime::thread::ThreadObject::Entry entry);

    void suspend();
    void resume();

    ProcessId            id()   const { return m_id; }
    const std::string&   name() const { return m_name; }
    ProcessState         state() const { return m_state; }

    runtime::memory::AddressSpace&   addressSpace()   { return m_addressSpace; }
    runtime::handles::HandleTable&   handleTable()    { return m_handleTable; }
    runtime::thread::ThreadManager&  threadManager()  { return m_threads; }

    runtime::thread::ThreadId mainThreadId() const { return m_mainThread; }
    
    // Owns the forked child that actually executes PS4 code. Null until
    // start() is called.
    GuestProcess* guestProcess() { return m_guest.get(); }

    // Runtime-side trap server that services the shared ring.
    syscall::trap::TrapServer* trapServer() { return m_trapServer.get(); }

    // Installs isolation, forks the guest, wires up the trap server, and
    // jumps the guest to the loaded module's entry point. Returns false if
    // isolation cannot be established.
    bool start(const isolation::IsolationConfig& iso);

    // Blocks until the guest exits.
    int waitForGuest();

private:
    ProcessId  m_id;
    std::string m_name;

    ProcessState                    m_state = ProcessState::Created;
    runtime::memory::AddressSpace   m_addressSpace;
    runtime::handles::HandleTable   m_handleTable;
    runtime::thread::ThreadManager  m_threads;
    runtime::thread::ThreadId       m_mainThread = 0;
    
    std::unique_ptr<GuestProcess>          m_guest;
    std::unique_ptr<syscall::trap::TrapServer> m_trapServer;

    // Static trampoline that the child's trap gate jumps into. Calls the
    // loaded module's entry point.
    static void guestEntryTrampoline(void* arg);

    mutable std::mutex m_mutex;
};

} // namespace fusionps4::runtime::process
