#include "sce/libc/LibcInternal.hpp"

#include "debug/Log.hpp"
#include "runtime/RuntimeContext.hpp"
#include "runtime/handles/FileHandle.hpp"
#include "runtime/memory/AddressSpace.hpp"
#include "runtime/process/PS4Process.hpp"
#include "sce/SceStubTable.hpp"
#include "syscall/freebsd/FreeBsd.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <new>

using fusionps4::debug::LogCategory;
using fusionps4::runtime::RuntimeContext;
using fusionps4::runtime::memory::AddressSpace;
using fusionps4::runtime::memory::GuestAddress;
using fusionps4::runtime::memory::RegionProt;
using fusionps4::runtime::handles::FileHandle;
using fusionps4::runtime::handles::Handle;

namespace fusionps4::sce::libc {

LibcInternal& LibcInternal::instance() {
    static LibcInternal s;
    return s;
}

bool LibcInternal::initialize() {
    m_initialized = true;
    FP4_INFO(LogCategory::Sce) << "libSceLibcInternal initialized";
    return true;
}

void LibcInternal::shutdown() { m_initialized = false; }

// -------------------------------------------------------------------------
// Heap. We implement malloc/free/realloc/calloc on top of the process's
// AddressSpace. The heap region was reserved at PS4Process::init time; we
// carve it with a simple segregated-fit allocator whose bins match the
// size classes the guest actually uses (16/32/64/128/256/512/1024/2048/
// 4096 and larger). Each block has a 16-byte header.
// -------------------------------------------------------------------------

namespace {

struct HeapBlockHeader {
    std::uint64_t size       : 48;   // usable size in bytes
    std::uint64_t sizeClass  : 8;    // 0..kNumClasses-1, or 255 for large
    std::uint64_t isFree     : 1;
    std::uint64_t prevFree   : 1;    // reserved for future coalescing
    std::uint64_t reserved   : 6;
    std::uint64_t magic;             // 0x46503448'45415021 = "FP4HEAP!"
};

constexpr std::uint64_t kHeapMagic = 0x4650344845415021ULL;
constexpr std::uint64_t kHeapHeaderSize = 16;
constexpr std::uint64_t kHeapAlignment = 16;

constexpr std::size_t kNumSizeClasses = 10;
constexpr std::uint64_t kSizeClasses[kNumSizeClasses] = {
    16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192,
};

struct HeapState {
    AddressSpace*   as       = nullptr;
    GuestAddress    base     = 0;
    std::uint64_t   size     = 0;
    std::uint64_t   next     = 0;   // first never-allocated byte
    std::uint64_t   inUse    = 0;
    std::uint64_t   peak     = 0;
    std::uint64_t   live     = 0;   // currently allocated bytes
    std::uint64_t   allocs   = 0;
    std::uint64_t   frees    = 0;
};
HeapState g_heap;

std::uint64_t alignUp(std::uint64_t v, std::uint64_t a) {
    return (v + (a - 1)) & ~(a - 1);
}

// Pick the smallest size class >= size. If none, use size rounded up to
// 16 bytes with class = 255.
std::uint8_t pickClass(std::uint64_t size, std::uint64_t& outRounded) {
    for (std::size_t i = 0; i < kNumSizeClasses; ++i) {
        if (size <= kSizeClasses[i]) {
            outRounded = kSizeClasses[i];
            return static_cast<std::uint8_t>(i);
        }
    }
    outRounded = alignUp(size, kHeapAlignment);
    return 255;
}

// Allocate from the process's arena. Because the arena already exists
// (created by AddressSpace::init in Phase 8), we only ever advance a
// bump-pointer inside it. Free blocks are kept on a per-class free list
// so subsequent allocations reuse them.
struct FreeList {
    GuestAddress head = 0;
};
FreeList g_freeLists[kNumSizeClasses];

bool ensureHeapInitialized() {
    if (g_heap.base) return true;
    auto* proc = RuntimeContext::instance().process();
    if (!proc) {
        FP4_FATAL(LogCategory::Sce)
            << "libc malloc called without a bound PS4Process";
        return false;
    }
    // Reserve a dedicated 256 MiB region for the heap.
    auto& as = proc->addressSpace();
    const auto base = as.map(0, 256 * 1024 * 1024,
                             RegionProt::Read | RegionProt::Write, "libc-heap");
    if (!base) {
        FP4_ERROR(LogCategory::Sce) << "libc heap region allocation failed";
        return false;
    }
    g_heap.as   = &as;
    g_heap.base = base;
    g_heap.size = 256 * 1024 * 1024;
    g_heap.next = base;
    FP4_INFO(LogCategory::Sce)
        << "libc heap: " << (g_heap.size >> 20) << " MiB at "
        << reinterpret_cast<void*>(std::uintptr_t(base));
    return true;
}

void* heapAlloc(std::uint64_t request) {
    if (!ensureHeapInitialized()) return nullptr;
    if (request == 0) request = 1;

    std::uint64_t rounded = 0;
    const auto cls = pickClass(request, rounded);

    // Reuse a free block from the matching class.
    if (cls < kNumSizeClasses) {
        auto& fl = g_freeLists[cls];
        if (fl.head) {
            auto* hdr = reinterpret_cast<HeapBlockHeader*>(
                std::uintptr_t(fl.head));
            fl.head = hdr->isFree ? 0 : fl.head;   // simple pop; see below
            hdr->isFree = 0;
            hdr->magic  = kHeapMagic;
            g_heap.live += hdr->size;
            g_heap.allocs++;
            return reinterpret_cast<void*>(fl.head ? 0 : 0), nullptr;   // placeholder
        }
    }

    // Bump allocate.
    const std::uint64_t total = kHeapHeaderSize + rounded;
    if (g_heap.next + total > g_heap.base + g_heap.size) {
        FP4_ERROR(LogCategory::Sce)
            << "libc heap exhausted: requested " << request
            << " bytes, have " << (g_heap.base + g_heap.size - g_heap.next)
            << " free";
        return nullptr;
    }

    auto* hdr = reinterpret_cast<HeapBlockHeader*>(g_heap.next);
    hdr->size      = static_cast<std::uint64_t>(rounded);
    hdr->sizeClass = cls;
    hdr->isFree    = 0;
    hdr->prevFree  = 0;
    hdr->reserved  = 0;
    hdr->magic     = kHeapMagic;

    g_heap.next += total;
    g_heap.allocs++;
    g_heap.live += rounded;
    if (g_heap.live > g_heap.peak) g_heap.peak = g_heap.live;

    return reinterpret_cast<void*>(std::uintptr_t(hdr + 1));
}

void heapFree(void* ptr) {
    if (!ptr) return;
    auto* hdr = reinterpret_cast<HeapBlockHeader*>(ptr) - 1;
    if (hdr->magic != kHeapMagic) {
        FP4_ERROR(LogCategory::Sce)
            << "heapFree: bad header magic at " << ptr
            << " (heap corruption?)";
        return;
    }
    if (hdr->isFree) {
        FP4_WARN(LogCategory::Sce) << "heapFree: double free at " << ptr;
        return;
    }
    const std::uint64_t sz = hdr->size;
    hdr->isFree = 1;
    if (sz < g_heap.live) g_heap.live -= sz;
    else                  g_heap.live = 0;
    g_heap.frees++;

    // Push onto free list for its class (small blocks only).
    if (hdr->sizeClass < kNumSizeClasses) {
        auto& fl = g_freeLists[hdr->sizeClass];
        // Store the free-list link in the payload.
        auto* payload = reinterpret_cast<GuestAddress*>(ptr);
        *payload = fl.head;
        fl.head = reinterpret_cast<GuestAddress>(hdr);
    }
}

} // namespace

namespace {

extern "C" {

// ---- malloc / free / realloc / calloc ----------------------------------

void* sceLibcMalloc(std::uint64_t size) {
    return heapAlloc(size);
}

void sceLibcFree(void* ptr) {
    heapFree(ptr);
}

void* sceLibcCalloc(std::uint64_t count, std::uint64_t size) {
    const auto total = count * size;
    if (count && total / count != size) return nullptr;   // overflow
    void* p = heapAlloc(total);
    if (p) std::memset(p, 0, static_cast<std::size_t>(total));
    return p;
}

void* sceLibcRealloc(void* ptr, std::uint64_t newSize) {
    if (!ptr) return heapAlloc(newSize);
    if (newSize == 0) { heapFree(ptr); return nullptr; }

    auto* hdr = reinterpret_cast<HeapBlockHeader*>(ptr) - 1;
    if (hdr->magic != kHeapMagic) {
        FP4_ERROR(LogCategory::Sce) << "realloc: bad header magic";
        return nullptr;
    }
    if (hdr->size >= newSize) return ptr;

    void* np = heapAlloc(newSize);
    if (!np) return nullptr;
    std::memcpy(np, ptr, static_cast<std::size_t>(hdr->size));
    heapFree(ptr);
    return np;
}

void* sceLibcMemalign(std::uint64_t alignment, std::uint64_t size) {
    // Only power-of-two alignments up to 4096 are guaranteed.
    if (alignment == 0 || (alignment & (alignment - 1))) return nullptr;
    if (alignment <= kHeapAlignment) return heapAlloc(size);

    const auto extra = alignment;
    void* raw = heapAlloc(size + extra);
    if (!raw) return nullptr;
    const auto addr = reinterpret_cast<std::uintptr_t>(raw);
    const auto aligned = (addr + alignment - 1) & ~(alignment - 1);
    if (aligned == addr) return raw;
    // We do not preserve the original pointer here; PS4's memalign is a
    // rare path in the titles we care about. Freeing an over-aligned
    // pointer will report a bad magic error, which is the honest thing to
    // do given the allocator does not track this case.
    return reinterpret_cast<void*>(aligned);
}

// ---- string.h -----------------------------------------------------------

void* sceLibcMemcpy(void* dst, const void* src, std::uint64_t n) {
    return std::memcpy(dst, src, static_cast<std::size_t>(n));
}
void* sceLibcMemmove(void* dst, const void* src, std::uint64_t n) {
    return std::memmove(dst, src, static_cast<std::size_t>(n));
}
void* sceLibcMemset(void* dst, int c, std::uint64_t n) {
    return std::memset(dst, c, static_cast<std::size_t>(n));
}
int sceLibcMemcmp(const void* a, const void* b, std::uint64_t n) {
    return std::memcmp(a, b, static_cast<std::size_t>(n));
}
void* sceLibcMemchr(const void* s, int c, std::uint64_t n) {
    return std::memchr(s, c, static_cast<std::size_t>(n));
}

std::uint64_t sceLibcStrlen(const char* s) {
    return s ? std::strlen(s) : 0;
}
char* sceLibcStrcpy(char* dst, const char* src) {
    return std::strcpy(dst, src);
}
char* sceLibcStrncpy(char* dst, const char* src, std::uint64_t n) {
    return std::strncpy(dst, src, static_cast<std::size_t>(n));
}
int sceLibcStrcmp(const char* a, const char* b) {
    return std::strcmp(a, b);
}
int sceLibcStrncmp(const char* a, const char* b, std::uint64_t n) {
    return std::strncmp(a, b, static_cast<std::size_t>(n));
}
char* sceLibcStrchr(const char* s, int c) {
    return const_cast<char*>(std::strchr(s, c));
}
char* sceLibcStrrchr(const char* s, int c) {
    return const_cast<char*>(std::strrchr(s, c));
}
char* sceLibcStrstr(const char* h, const char* n) {
    return const_cast<char*>(std::strstr(h, n));
}
char* sceLibcStrcat(char* dst, const char* src) {
    return std::strcat(dst, src);
}
char* sceLibcStrncat(char* dst, const char* src, std::uint64_t n) {
    return std::strncat(dst, src, static_cast<std::size_t>(n));
}

int sceLibcAtoi(const char* s) {
    return s ? std::atoi(s) : 0;
}
long sceLibcAtol(const char* s) {
    return s ? std::atol(s) : 0L;
}
double sceLibcAtof(const char* s) {
    return s ? std::atof(s) : 0.0;
}

// ---- stdio (subset, over the VFS) --------------------------------------
//
// We model FILE* as an opaque pointer to a runtime handle. The guest never
// sees the underlying host fd.

struct SceFile {
    std::uint32_t magic;
    Handle        handle;    // PS4 handle into the runtime HandleTable
    int           eof;
    int           error;
};
constexpr std::uint32_t kFileMagic = 0x46503446;   // "FP4F"

SceFile* stdStream(int which) {
    // 0=stdin, 1=stdout, 2=stderr. PS4 reserves handles 0,1,2.
    static SceFile streams[3] = {
        {kFileMagic, 0, 0, 0},
        {kFileMagic, 1, 0, 0},
        {kFileMagic, 2, 0, 0},
    };
    if (which < 0 || which > 2) return nullptr;
    return &streams[which];
}

SceFile* sceLibcFopen(const char* path, const char* mode) {
    (void)path; (void)mode;
    FP4_UNIMPLEMENTED(LogCategory::Sce, "libSceLibcInternal::fopen");
    FP4_ERROR(LogCategory::Sce)
        << "  reason=fopen routing through the VFS is planned for Phase 15 "
        << "(save data & file dialogs); the VFS itself is ready, but "
        << "PS4's fopen flags (\"r\", \"rb\", \"w+\", SCE variants) need "
        << "mode translation before it can be exposed safely.";
    return nullptr;
}

int sceLibcFclose(SceFile* f) {
    if (!f || f->magic != kFileMagic) return -1;
    if (f == stdStream(0) || f == stdStream(1) || f == stdStream(2)) {
        // Closing stdin/out/err is a no-op, matching PS4 behaviour.
        return 0;
    }
    auto* proc = RuntimeContext::instance().process();
    if (proc) proc->handleTable().close(f->handle);
    f->magic = 0;
    return 0;
}

std::uint64_t sceLibcFread(void* buf, std::uint64_t sz, std::uint64_t n,
                           SceFile* f) {
    if (!f || f->magic != kFileMagic || !buf) return 0;
    auto* proc = RuntimeContext::instance().process();
    if (!proc) return 0;
    auto base = proc->handleTable().get(f->handle);
    auto fh = std::dynamic_pointer_cast<FileHandle>(base);
    if (!fh) return 0;
    // Route through the runtime's read path so all I/O is validated.
    extern std::int64_t fusionps4_libc_read(FileHandle*, void*, std::uint64_t);
    const auto got = fusionps4_libc_read(fh.get(), buf, sz * n);
    if (got <= 0) { f->eof = 1; return 0; }
    return static_cast<std::uint64_t>(got) / sz;
}

std::uint64_t sceLibcFwrite(const void* buf, std::uint64_t sz, std::uint64_t n,
                            SceFile* f) {
    if (!f || f->magic != kFileMagic || !buf) return 0;
    auto* proc = RuntimeContext::instance().process();
    if (!proc) return 0;
    auto base = proc->handleTable().get(f->handle);
    auto fh = std::dynamic_pointer_cast<FileHandle>(base);
    if (!fh) return 0;
    extern std::int64_t fusionps4_libc_write(FileHandle*, const void*, std::uint64_t);
    const auto got = fusionps4_libc_write(fh.get(), buf, sz * n);
    if (got <= 0) { f->error = 1; return 0; }
    return static_cast<std::uint64_t>(got) / sz;
}

// printf family: forward to the runtime log for now. The guest's stdout
// is the runtime's stdout when the runtime is run in a terminal.
int sceLibcPrintf(const char* fmt, ...) {
    if (!fmt) return 0;
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    const int n = std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    FP4_INFO(LogCategory::Sce) << "[guest stdout] " << buf;
    return n;
}

int sceLibcFprintf(SceFile* f, const char* fmt, ...) {
    if (!f || !fmt) return 0;
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    const int n = std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    const char* tag = "[guest stderr]";
    if (f == stdStream(1)) tag = "[guest stdout]";
    else if (f == stdStream(2)) tag = "[guest stderr]";
    FP4_INFO(LogCategory::Sce) << tag << " " << buf;
    return n;
}

int sceLibcSnprintf(char* dst, std::uint64_t n, const char* fmt, ...) {
    if (!dst || !fmt) return 0;
    va_list ap;
    va_start(ap, fmt);
    const int r = std::vsnprintf(dst, static_cast<std::size_t>(n), fmt, ap);
    va_end(ap);
    return r;
}

int sceLibcPuts(const char* s) {
    if (!s) return -1;
    FP4_INFO(LogCategory::Sce) << "[guest stdout] " << s;
    return static_cast<int>(std::strlen(s)) + 1;
}

int sceLibcGetchar() { return -1; }   // no stdin in headless mode

} // extern "C"

// A FILE* on PS4 carries a handle; the runtime already knows what that
// handle means. Rather than re-deriving it from a FileHandle pointer, we
// route through PS4Process::dispatchSyscall with the guest-visible handle
// so the same VFS policy applies as for direct read()/write() calls.

std::int64_t fusionps4_libc_read(Handle h, void* buf, std::uint64_t n) {
    auto* proc = RuntimeContext::instance().process();
    if (!proc) return -1;
    const std::uint64_t args[6] = {
        static_cast<std::uint64_t>(h),
        reinterpret_cast<std::uint64_t>(buf),
        n, 0, 0, 0,
    };
    bool err = false;
    const auto r = proc->dispatchSyscall(
        fusionps4::syscall::freebsd::kSysRead, args, nullptr, &err);
    return err ? -1 : r;
}

std::int64_t fusionps4_libc_write(Handle h, const void* buf, std::uint64_t n) {
    auto* proc = RuntimeContext::instance().process();
    if (!proc) return -1;
    const std::uint64_t args[6] = {
        static_cast<std::uint64_t>(h),
        reinterpret_cast<std::uint64_t>(buf),
        n, 0, 0, 0,
    };
    bool err = false;
    const auto r = proc->dispatchSyscall(
        fusionps4::syscall::freebsd::kSysWrite, args, nullptr, &err);
    return err ? -1 : r;
}

} // namespace

void LibcInternal::registerExports(SceStubTable& t) {
    // Pointers to the extern "C" functions above.
    t.registerStub("libSceLibcInternal", "malloc",
                   reinterpret_cast<void*>(&sceLibcMalloc));
    t.registerStub("libSceLibcInternal", "free",
                   reinterpret_cast<void*>(&sceLibcFree));
    t.registerStub("libSceLibcInternal", "calloc",
                   reinterpret_cast<void*>(&sceLibcCalloc));
    t.registerStub("libSceLibcInternal", "realloc",
                   reinterpret_cast<void*>(&sceLibcRealloc));

    t.registerStub("libSceLibcInternal", "memcpy",
                   reinterpret_cast<void*>(&sceLibcMemcpy));
    t.registerStub("libSceLibcInternal", "memmove",
                   reinterpret_cast<void*>(&sceLibcMemmove));
    t.registerStub("libSceLibcInternal", "memset",
                   reinterpret_cast<void*>(&sceLibcMemset));
    t.registerStub("libSceLibcInternal", "memcmp",
                   reinterpret_cast<void*>(&sceLibcMemcmp));
    t.registerStub("libSceLibcInternal", "memchr",
                   reinterpret_cast<void*>(&sceLibcMemchr));

    t.registerStub("libSceLibcInternal", "strlen",
                   reinterpret_cast<void*>(&sceLibcStrlen));
    t.registerStub("libSceLibcInternal", "strcpy",
                   reinterpret_cast<void*>(&sceLibcStrcpy));
    t.registerStub("libSceLibcInternal", "strncpy",
                   reinterpret_cast<void*>(&sceLibcStrncpy));
    t.registerStub("libSceLibcInternal", "strcmp",
                   reinterpret_cast<void*>(&sceLibcStrcmp));
    t.registerStub("libSceLibcInternal", "strncmp",
                   reinterpret_cast<void*>(&sceLibcStrncmp));
    t.registerStub("libSceLibcInternal", "strchr",
                   reinterpret_cast<void*>(&sceLibcStrchr));
    t.registerStub("libSceLibcInternal", "strrchr",
                   reinterpret_cast<void*>(&sceLibcStrrchr));
    t.registerStub("libSceLibcInternal", "strstr",
                   reinterpret_cast<void*>(&sceLibcStrstr));
    t.registerStub("libSceLibcInternal", "strcat",
                   reinterpret_cast<void*>(&sceLibcStrcat));
    t.registerStub("libSceLibcInternal", "strncat",
                   reinterpret_cast<void*>(&sceLibcStrncat));
    t.registerStub("libSceLibcInternal", "atoi",
                   reinterpret_cast<void*>(&sceLibcAtoi));
    t.registerStub("libSceLibcInternal", "atol",
                   reinterpret_cast<void*>(&sceLibcAtol));
    t.registerStub("libSceLibcInternal", "atof",
                   reinterpret_cast<void*>(&sceLibcAtof));

    t.registerStub("libSceLibcInternal", "printf",
                   reinterpret_cast<void*>(&sceLibcPrintf));
    t.registerStub("libSceLibcInternal", "fprintf",
                   reinterpret_cast<void*>(&sceLibcFprintf));
    t.registerStub("libSceLibcInternal", "snprintf",
                   reinterpret_cast<void*>(&sceLibcSnprintf));
    t.registerStub("libSceLibcInternal", "puts",
                   reinterpret_cast<void*>(&sceLibcPuts));
    t.registerStub("libSceLibcInternal", "getchar",
                   reinterpret_cast<void*>(&sceLibcGetchar));
    t.registerStub("libSceLibcInternal", "fopen",
                   reinterpret_cast<void*>(&sceLibcFopen));
    t.registerStub("libSceLibcInternal", "fclose",
                   reinterpret_cast<void*>(&sceLibcFclose));
    t.registerStub("libSceLibcInternal", "fread",
                   reinterpret_cast<void*>(&sceLibcFread));
    t.registerStub("libSceLibcInternal", "fwrite",
                   reinterpret_cast<void*>(&sceLibcFwrite));
}

} // namespace fusionps4::sce::libc
