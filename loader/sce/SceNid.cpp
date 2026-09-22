#include "loader/sce/SceNid.hpp"

#include <cstdio>
#include <cstring>

// We need SHA-256 for computeLegacy(). Rather than pull in OpenSSL, we
// embed a compact public-domain implementation. If the runtime was built
// against OpenSSL the translation unit will not be compiled; see
// CMakeLists for the conditional. For now we always build the embedded
// version to avoid a hard dependency.

namespace fusionps4::loader::sce {

namespace {

// -------------------------------------------------------------------------
// Minimal SHA-256 (public-domain, based on the reference implementation
// released by Brad Conte, 2015). Kept here to avoid an OpenSSL dependency
// solely for hashing 32-byte strings.
// -------------------------------------------------------------------------

struct Sha256Ctx {
    std::uint8_t  data[64];
    std::uint32_t datalen = 0;
    std::uint64_t bitlen   = 0;
    std::uint32_t state[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
    };
};

constexpr std::uint32_t kSha256K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

inline std::uint32_t rotr(std::uint32_t x, std::uint32_t n) {
    return (x >> n) | (x << (32 - n));
}

void sha256Transform(Sha256Ctx& ctx, const std::uint8_t* chunk) {
    std::uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
        w[i] = (std::uint32_t(chunk[i * 4]) << 24) |
               (std::uint32_t(chunk[i * 4 + 1]) << 16) |
               (std::uint32_t(chunk[i * 4 + 2]) << 8) |
               (std::uint32_t(chunk[i * 4 + 3]));
    }
    for (int i = 16; i < 64; ++i) {
        const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    std::uint32_t a = ctx.state[0], b = ctx.state[1], c = ctx.state[2], d = ctx.state[3];
    std::uint32_t e = ctx.state[4], f = ctx.state[5], g = ctx.state[6], h = ctx.state[7];
    for (int i = 0; i < 64; ++i) {
        const std::uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        const std::uint32_t ch = (e & f) ^ (~e & g);
        const std::uint32_t t1 = h + S1 + ch + kSha256K[i] + w[i];
        const std::uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t t2 = S0 + maj;
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    ctx.state[0] += a; ctx.state[1] += b; ctx.state[2] += c; ctx.state[3] += d;
    ctx.state[4] += e; ctx.state[5] += f; ctx.state[6] += g; ctx.state[7] += h;
}

void sha256Update(Sha256Ctx& ctx, const std::uint8_t* data, std::size_t len) {
    for (std::size_t i = 0; i < len; ++i) {
        ctx.data[ctx.datalen++] = data[i];
        if (ctx.datalen == 64) {
            sha256Transform(ctx, ctx.data);
            ctx.bitlen += 512;
            ctx.datalen = 0;
        }
    }
}

void sha256Final(Sha256Ctx& ctx, std::uint8_t out[32]) {
    std::uint32_t i = ctx.datalen;
    if (ctx.datalen < 56) {
        ctx.data[i++] = 0x80;
        while (i < 56) ctx.data[i++] = 0;
    } else {
        ctx.data[i++] = 0x80;
        while (i < 64) ctx.data[i++] = 0;
        sha256Transform(ctx, ctx.data);
        std::memset(ctx.data, 0, 56);
    }
    ctx.bitlen += std::uint64_t(ctx.datalen) * 8;
    for (int j = 7; j >= 0; --j) {
        ctx.data[56 + (7 - j)] = std::uint8_t((ctx.bitlen >> (j * 8)) & 0xFF);
    }
    sha256Transform(ctx, ctx.data);
    for (int j = 0; j < 8; ++j) {
        out[j * 4]     = std::uint8_t((ctx.state[j] >> 24) & 0xFF);
        out[j * 4 + 1] = std::uint8_t((ctx.state[j] >> 16) & 0xFF);
        out[j * 4 + 2] = std::uint8_t((ctx.state[j] >>  8) & 0xFF);
        out[j * 4 + 3] = std::uint8_t((ctx.state[j]      ) & 0xFF);
    }
}

} // namespace

std::uint64_t SceNid::computeLegacy(const std::string& functionName) {
    Sha256Ctx ctx;
    sha256Update(ctx, reinterpret_cast<const std::uint8_t*>(functionName.data()),
                 functionName.size());
    std::uint8_t digest[32];
    sha256Final(ctx, digest);
    // Little-endian interpretation of the first 8 bytes.
    std::uint64_t v = 0;
    for (int i = 0; i < 8; ++i) {
        v |= std::uint64_t(digest[i]) << (i * 8);
    }
    return v;
}

std::string SceNid::toHex(std::uint64_t nid) {
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016llx", (unsigned long long)nid);
    return buf;
}

bool SceNid::fromHex(const std::string& hex, std::uint64_t& out) {
    if (hex.size() != 16) return false;
    std::uint64_t v = 0;
    for (char c : hex) {
        v <<= 4;
        if (c >= '0' && c <= '9') v |= (c - '0');
        else if (c >= 'a' && c <= 'f') v |= (c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= (c - 'A' + 10);
        else return false;
    }
    out = v;
    return true;
}

} // namespace fusionps4::loader::sce
