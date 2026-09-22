#include "sce/ult/SceUlt.hpp"

#include "debug/Log.hpp"
#include "runtime/RuntimeContext.hpp"
#include "runtime/memory/AddressSpace.hpp"
#include "runtime/process/PS4Process.hpp"
#include "sce/SceStubTable.hpp"

#include <atomic>
#include <cstring>

using fusionps4::debug::LogCategory;
using fusionps4::runtime::RuntimeContext;

namespace fusionps4::sce::ult {

SceUlt& SceUlt::instance() {
    static SceUlt s;
    return s;
}

bool SceUlt::initialize() {
    m_initialized = true;
    FP4_INFO(LogCategory::Sce) << "libSceUlt initialized";
    return true;
}

void SceUlt::shutdown() { m_initialized = false; }

namespace {

constexpr int kUltOk             = 0;
constexpr int kUltErrorInvalidArg= static_cast<int>(0x80020005u);
constexpr int kUltErrorNoMemory  = static_cast<int>(0x80020002u);
constexpr int kUltErrorBusy      = static_cast<int>(0x8002000Bu);

// PS4 SceUlt memory-pool object. Layout is not public; we treat it as an
// opaque 64-byte structure the guest allocates (typically on the stack)
// and we manage entirely from our side.
constexpr std::size_t kUltPoolObjSize = 64;

// SceUltWaitingQueue — a futex-like primitive. PS4 lays it out as 4
// 32-bit words; the first is the counter, the rest are platform-reserved.
constexpr std::size_t kUltWaitingQueueSize = 16;

struct PoolRegistry {
    std::uint64_t poolAddr   = 0;
    std::uint64_t backingAddr= 0;
    std::uint32_t blockSize  = 0;
    std::uint32_t blockCount = 0;
    std::uint32_t used       = 0;
};

std::vector<PoolRegistry> g_pools;

PoolRegistry* findPool(std::uint64_t addr) {
    for (auto& p : g_pools) if (p.poolAddr == addr) return &p;
    return nullptr;
}

bool validateGuestWrite(std::uint64_t addr, std::size_t size) {
    auto* proc = RuntimeContext::instance().process();
    if (!proc) return false;
    auto& as = proc->addressSpace();
    auto reg = as.find(addr);
    if (!reg) return false;
    if (!hasProt(reg->prot, fusionps4::runtime::memory::RegionProt::Write))
        return false;
    if (addr + size > reg->base + reg->size) return false;
    return true;
}

extern "C" {

// ---- memory pools -------------------------------------------------------
//
// sceUltPoolCreate(obj, blockSize, blockCount, alignment, backingAddr)
//
// The guest supplies both the pool object (typically a stack structure)
// and the backing memory (typically GPU-visible). We record the mapping so
// subsequent Alloc/Free can be validated.

int sceUltPoolCreate(void* poolObj, std::uint32_t blockSize,
                     std::uint32_t blockCount, std::uint32_t /*alignment*/,
                     void* backing) {
    if (!poolObj || !backing || blockSize == 0 || blockCount == 0)
        return kUltErrorInvalidArg;

    const auto objAddr = reinterpret_cast<std::uint64_t>(poolObj);
    const auto backAddr = reinterpret_cast<std::uint64_t>(backing);

    if (!validateGuestWrite(objAddr, kUltPoolObjSize)) {
        FP4_ERROR(LogCategory::Sce)
            << "sceUltPoolCreate: pool object at "
            << poolObj << " is not writable guest memory";
        return kUltErrorInvalidArg;
    }

    if (auto* existing = findPool(objAddr)) {
        FP4_WARN(LogCategory::Sce)
            << "sceUltPoolCreate: pool already initialized at " << poolObj;
        return kUltErrorBusy;
    }

    PoolRegistry r;
    r.poolAddr    = objAddr;
    r.backingAddr = backAddr;
    r.blockSize   = blockSize;
    r.blockCount  = blockCount;
    r.used        = 0;
    g_pools.push_back(r);

    // The pool object itself stores a free-list head that the guest will
    // manipulate inline; initialize it to the first backing block.
    std::memset(poolObj, 0, kUltPoolObjSize);
    auto* word0 = reinterpret_cast<std::uint64_t*>(poolObj);
    word0[0] = backAddr;                 // free list head
    word0[1] = blockSize;
    word0[2] = blockCount;
    word0[3] = 0;

    FP4_DEBUG(LogCategory::Sce)
        << "sceUltPoolCreate: " << blockCount << " x " << blockSize
        << " at " << backing;
    return kUltOk;
}

int sceUltPoolDestroy(void* poolObj) {
    if (!poolObj) return kUltErrorInvalidArg;
    const auto objAddr = reinterpret_cast<std::uint64_t>(poolObj);
    for (auto it = g_pools.begin(); it != g_pools.end(); ++it) {
        if (it->poolAddr == objAddr) {
            g_pools.erase(it);
            return kUltOk;
        }
    }
    return kUltErrorInvalidArg;
}

// The PS4 inline implementation of alloc/free is a single CAS on the
// free-list head. The guest does this itself; our export exists for cases
// where the compiler chose not to inline. We perform the same CAS.

void* sceUltPoolAlloc(void* poolObj) {
    if (!poolObj) return nullptr;
    auto* r = findPool(reinterpret_cast<std::uint64_t>(poolObj));
    if (!r) return nullptr;

    auto* head = reinterpret_cast<std::uint64_t*>(poolObj);
    std::uint64_t current = head[0];
    for (;;) {
        if (current == 0) {
            FP4_WARN(LogCategory::Sce) << "sceUltPoolAlloc: pool exhausted";
            return nullptr;
        }
        // Each block starts with the next-free pointer in its first 8 bytes.
        auto* block = reinterpret_cast<std::uint64_t*>(
            static_cast<std::uintptr_t>(current));
        const std::uint64_t next = block[0];
        if (__atomic_compare_exchange_n(&head[0], &current, next,
                                        false, __ATOMIC_ACQ_REL,
                                        __ATOMIC_ACQUIRE)) {
            block[0] = 0;
            return block;
        }
        // current was updated by the failed CAS.
    }
}

void sceUltPoolFree(void* poolObj, void* block) {
    if (!poolObj || !block) return;
    auto* r = findPool(reinterpret_cast<std::uint64_t>(poolObj));
    if (!r) return;

    auto* head = reinterpret_cast<std::uint64_t*>(poolObj);
    auto* b = reinterpret_cast<std::uint64_t*>(block);
    std::uint64_t current = head[0];
    for (;;) {
        b[0] = current;
        if (__atomic_compare_exchange_n(&head[0], &current,
                                        reinterpret_cast<std::uint64_t>(b),
                                        false, __ATOMIC_ACQ_REL,
                                        __ATOMIC_ACQUIRE)) {
            return;
        }
    }
}

// ---- waiting queues -----------------------------------------------------
//
// sceUltWaitingQueueInit(queue): initializes a 16-byte object. The inline
// wait/signal path uses an atomic on word 0 and a futex on the same word
// in the runtime's address space. Because the guest's queue lives in the
// shared arena, the runtime can futex-wake it directly.

int sceUltWaitingQueueInit(void* queue) {
    if (!queue) return kUltErrorInvalidArg;
    if (!validateGuestWrite(reinterpret_cast<std::uint64_t>(queue),
                            kUltWaitingQueueSize)) {
        return kUltErrorInvalidArg;
    }
    std::memset(queue, 0, kUltWaitingQueueSize);
    return kUltOk;
}

int sceUltWaitingQueueDestroy(void* queue) {
    if (!queue) return kUltErrorInvalidArg;
    // Wake any waiters so they can observe destruction.
    auto* word = reinterpret_cast<std::uint32_t*>(queue);
    __atomic_store_n(word, 1u, __ATOMIC_RELEASE);
    return kUltOk;
}

// ---- spinlocks ----------------------------------------------------------
//
// PS4 exposes an ultra-light spinlock via inline asm; the exported
// function is only used at initialization.

int sceUltSpinlockInit(void* lock) {
    if (!lock) return kUltErrorInvalidArg;
    if (!validateGuestWrite(reinterpret_cast<std::uint64_t>(lock), 4)) {
        return kUltErrorInvalidArg;
    }
    *reinterpret_cast<std::uint32_t*>(lock) = 0;
    return kUltOk;
}

} // extern "C"

} // namespace

void SceUlt::registerExports(SceStubTable& t) {
    t.registerStub("libSceUlt", "sceUltPoolCreate",
                   reinterpret_cast<void*>(&sceUltPoolCreate));
    t.registerStub("libSceUlt", "sceUltPoolDestroy",
                   reinterpret_cast<void*>(&sceUltPoolDestroy));
    t.registerStub("libSceUlt", "sceUltPoolAlloc",
                   reinterpret_cast<void*>(&sceUltPoolAlloc));
    t.registerStub("libSceUlt", "sceUltPoolFree",
                   reinterpret_cast<void*>(&sceUltPoolFree));
    t.registerStub("libSceUlt", "sceUltWaitingQueueInit",
                   reinterpret_cast<void*>(&sceUltWaitingQueueInit));
    t.registerStub("libSceUlt", "sceUltWaitingQueueDestroy",
                   reinterpret_cast<void*>(&sceUltWaitingQueueDestroy));
    t.registerStub("libSceUlt", "sceUltSpinlockInit",
                   reinterpret_cast<void*>(&sceUltSpinlockInit));
}

} // namespace fusionps4::sce::ult
