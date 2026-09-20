#pragma once

#include "syscall/SyscallDispatcher.hpp"

namespace fusionps4::runtime::process { class PS4Process; }

namespace fusionps4::syscall::handlers {

// Registers every built-in handler into the dispatcher. Each category
// installs only syscall numbers it actually implements; unknown numbers
// remain unregistered and are reported as UNIMPLEMENTED by the dispatcher.
void registerFilesystemHandlers(SyscallDispatcher& d,
                                runtime::process::PS4Process& process);

void registerMemoryHandlers(SyscallDispatcher& d,
                            runtime::process::PS4Process& process);

void registerThreadHandlers(SyscallDispatcher& d,
                            runtime::process::PS4Process& process);

void registerMiscHandlers(SyscallDispatcher& d,
                          runtime::process::PS4Process& process);

// Convenience entry point.
void registerAll(SyscallDispatcher& d,
                 runtime::process::PS4Process& process);

} // namespace fusionps4::syscall::handlers
