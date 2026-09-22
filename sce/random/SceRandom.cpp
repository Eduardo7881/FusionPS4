#include "sce/random/SceRandom.hpp"

#include "debug/Log.hpp"
#include "runtime/RuntimeContext.hpp"
#include "sce/SceStubTable.hpp"

#include <array>
#include <cerrno>
#include <cstring>
#include <sys/syscall.h>
#include <unistd.h>

using fusionps4::debug::LogCategory;

namespace fusionps4::sce::random {

namespace {

// -------------------------------------------------------------------------
// ChaCha20 DRBG. Produces 64 bytes per block; reseeds every 1 MiB from the
// kernel RNG via getrandom(2). The state is thread-local so multiple guest
// threads do not contend.
// -------------------------------------------------------------------------

inline std::uint32_t rotl32(std::uint32_t v, int n) {
    return (v << n) | (v >> (32 - n));
}

inline void qr(std::uint32_t& a, std::uint32_t& b,
               std::uint32_t& c, std::uint32_t& d) {
    a += b; d ^= a; d = rotl32(d, 16);
    c += d; b ^= c; b = rotl32(b, 12);
    a += b; d ^= a; d = rotl32(d,  8);
    c += d; b ^= c; b = rotl32(b,  7);
}

void chacha20Block(const std::uint32_t in[16], std::uint32_t out[16]) {
    std::uint32_t x[16];
    std::memcpy(x, in, sizeof(x));
    for (int i = 0; i < 10; ++i) {
        qr(x[0], x[4], x[ 8], x[12]);
        qr(x[1], x[5], x[ 9], x[13]);
        qr(x[2], x[6], x[10], x[14]);
        qr(x[3], x[7], x[11], x[15]);
        qr(x[0], x[5], x[10], x[15]);
        qr(x[1], x[6], x[11], x[12]);
        qr(x[2], x[7], x[ 8], x[13]);
        qr(x[3], x[4], x[ 9], x[14]);
    }
    for (int i = 0; i < 16; ++i) out[i] = x[i] + in[i];
}

struct Drbg {
    std::uint32_t    key[8]    = {0};
    std::uint32_t    nonce[2]  = {0};
    std::uint64_t    counter  = 0;
    std::array<std::uint8_t, 64> buf{};
    std::size_t      bufPos   = 64;
    std::uint64_t    produced = 0;

    bool seeded = false;
};

Drbg& drbg() {
    thread_local Drbg d;
    return d;
}

bool reseedFromKernel(Drbg& d) {
    std::uint8_t seed[40];
    const long r = ::syscall(SYS_getrandom, seed, sizeof(seed), 0);
    if (r != static_cast<long>(sizeof(seed))) {
        FP4_ERROR(LogCategory::Sce)
            << "getrandom failed for DRBG reseed: "
            << std::strerror(errno);
        return false;
    }
    std::memcpy(d.key,   seed,      32);
    std::memcpy(d.nonce, seed + 32,  8);
    d.counter  = 0;
    d.bufPos   = 64;
    d.produced = 0;
    d.seeded   = true;
    return true;
}

void refill(Drbg& d) {
    std::uint32_t state[16] = {
        0x61707865, 0x3320646e, 0x79622d32, 0x6b206574,   // "expand 32-byte k"
        d.key[0], d.key[1], d.key[2], d.key[3],
        d.key[4], d.key[5], d.key[6], d.key[7],
        static_cast<std::uint32_t>(d.counter),
        static_cast<std::uint32_t>(d.counter >> 32),
        d.nonce[0], d.nonce[1],
    };
    std::uint32_t out[16];
    chacha20Block(state, out);
    std::memcpy(d.buf.data(), out, 64);
    d.counter++;
    d.bufPos = 0;
}

void fillBytes(void* dst, std::size_t n) {
    auto& d = drbg();
    if (!d.seeded) {
        if (!reseedFromKernel(d)) {
            // Absolute last resort: zero-fill and report.
            std::memset(dst, 0, n);
            FP4_FATAL(LogCategory::Sce)
                << "DRBG unavailable and no host entropy source works; "
                << "random bytes are zero";
            return;
        }
    }
    if (d.produced + n > (1u << 20)) {
        reseedFromKernel(d);
    }
    auto* p = static_cast<std::uint8_t*>(dst);
    while (n > 0) {
        if (d.bufPos >= 64) refill(d);
        const auto avail = 64 - d.bufPos;
        const auto take  = n < avail ? n : avail;
        std::memcpy(p, d.buf + d.bufPos, take);
        d.bufPos += take;
        d.produced += take;
        p += take;
        n -= take;
    }
}

} // namespace

SceRandom& SceRandom::instance() {
    static SceRandom s;
    return s;
}

bool SceRandom::initialize() {
    // Seed the current thread eagerly so the first call from a guest thread
    // never races on the lazy path.
    Drbg d;
    (void)reseedFromKernel(d);
    m_initialized = true;
    FP4_INFO(LogCategory::Sce)
        << "libSceRandom initialized (ChaCha20 DRBG, kernel-seeded)";
    return true;
}

void SceRandom::shutdown() { m_initialized = false; }

namespace {

extern "C" {

// sceRandomGetRandomNumber(void* dst, size_t n): fills `n` bytes.
int sceRandomGetRandomNumber(void* dst, std::size_t n) {
    if (!dst && n > 0) return static_cast<int>(0x80020005u);
    fillBytes(dst, n);
    return 0;
}

// sceRandomGetRandomValue returns a single 64-bit value.
std::uint64_t sceRandomGetRandomValue() {
    std::uint64_t v = 0;
    fillBytes(&v, sizeof(v));
    return v;
}

// sceRandomGetRandomFloat returns a float in [0, 1).
float sceRandomGetRandomFloat() {
    std::uint32_t v = 0;
    fillBytes(&v, sizeof(v));
    return static_cast<float>(v) / 4294967296.0f;
}

} // extern "C"

} // namespace

void SceRandom::registerExports(SceStubTable& t) {
    t.registerStub("libSceRandom", "sceRandomGetRandomNumber",
                   reinterpret_cast<void*>(&sceRandomGetRandomNumber));
    t.registerStub("libSceRandom", "sceRandomGetRandomValue",
                   reinterpret_cast<void*>(&sceRandomGetRandomValue));
    t.registerStub("libSceRandom", "sceRandomGetRandomFloat",
                   reinterpret_cast<void*>(&sceRandomGetRandomFloat));
}

} // namespace fusionps4::sce::random
