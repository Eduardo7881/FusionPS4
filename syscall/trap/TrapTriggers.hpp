#pragma once

#include <cstdint>
#include <ucontext.h>

namespace fusionps4::syscall::trap {

// Accessors for the x86_64 register file exposed by ucontext_t inside a
// SIGSYS handler. Kept in one place so the trap implementation does not
// sprinkle gregs[] indices everywhere.
struct TrapRegisters {
    ucontext_t* uc = nullptr;

    std::uint64_t syscallNumber() const {
        return uc->uc_mcontext.gregs[REG_RAX];
    }
    void setSyscallNumber(std::uint64_t v) {
        uc->uc_mcontext.gregs[REG_RAX] = static_cast<greg_t>(v);
    }

    std::uint64_t arg(int i) const {
        switch (i) {
            case 0: return uc->uc_mcontext.gregs[REG_RDI];
            case 1: return uc->uc_mcontext.gregs[REG_RSI];
            case 2: return uc->uc_mcontext.gregs[REG_RDX];
            case 3: return uc->uc_mcontext.gregs[REG_R10];
            case 4: return uc->uc_mcontext.gregs[REG_R8];
            case 5: return uc->uc_mcontext.gregs[REG_R9];
            default: return 0;
        }
    }

    void setResult(std::int64_t v) {
        uc->uc_mcontext.gregs[REG_RAX] = static_cast<greg_t>(v);
    }

    void setCarryFlag(bool on) {
        auto& eflags = uc->uc_mcontext.gregs[REG_EFL];
        if (on)  eflags |=  1;
        else     eflags &= ~1;
    }
};

} // namespace fusionps4::syscall::trap
