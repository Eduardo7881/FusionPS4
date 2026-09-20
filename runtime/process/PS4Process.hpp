#pragma once

#include "runtime/handles/HandleTable.hpp"
#include "runtime/memory/AddressSpace.hpp"
#include "runtime/thread/ThreadManager.hpp"

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

private:
    ProcessId  m_id;
    std::string m_name;

    ProcessState                    m_state = ProcessState::Created;
    runtime::memory::AddressSpace   m_addressSpace;
    runtime::handles::HandleTable   m_handleTable;
    runtime::thread::ThreadManager  m_threads;
    runtime::thread::ThreadId       m_mainThread = 0;

    mutable std::mutex m_mutex;
};

} // namespace fusionps4::runtime::process
