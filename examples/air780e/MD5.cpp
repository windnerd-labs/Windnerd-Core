#include "MD5.h"
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdio>

namespace {

inline uint32_t leftrotate(uint32_t x, uint32_t c) {
    return (x << c) | (x >> (32 - c));
}

void md5(const unsigned char* initial_msg, size_t initial_len, unsigned char* digest) {
    static const uint32_t r[] = {
        7,12,17,22, 7,12,17,22, 7,12,17,22, 7,12,17,22,
        5,9,14,20, 5,9,14,20, 5,9,14,20, 5,9,14,20,
        4,11,16,23, 4,11,16,23, 4,11,16,23, 4,11,16,23,
        6,10,15,21, 6,10,15,21, 6,10,15,21, 6,10,15,21
    };

    static const uint32_t k[] = {
        0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,
        0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
        0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,
        0x6b901122,0xfd987193,0xa679438e,0x49b40821,
        0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,
        0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
        0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,
        0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
        0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,
        0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
        0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,
        0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
        0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,
        0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
        0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,
        0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391
    };

    uint32_t h0 = 0x67452301;
    uint32_t h1 = 0xefcdab89;
    uint32_t h2 = 0x98badcfe;
    uint32_t h3 = 0x10325476;

    size_t new_len = (((initial_len + 8) / 64) + 1) * 64;
    unsigned char* msg = (unsigned char*)calloc(new_len + 64, 1);
    if (!msg) return;
    memcpy(msg, initial_msg, initial_len);
    msg[initial_len] = 0x80;

    uint64_t bits_len = (uint64_t)initial_len * 8;
    memcpy(msg + new_len - 8, &bits_len, 8);

    for (size_t offset = 0; offset < new_len; offset += 64) {
        uint32_t w[16];
        for (int i = 0; i < 16; ++i) {
            const unsigned char* p = msg + offset + i*4;
            w[i] = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
        }

        uint32_t a = h0;
        uint32_t b = h1;
        uint32_t c = h2;
        uint32_t d = h3;

        for (uint32_t i = 0; i < 64; ++i) {
            uint32_t f, g;
            if (i < 16) {
                f = (b & c) | ((~b) & d);
                g = i;
            } else if (i < 32) {
                f = (d & b) | ((~d) & c);
                g = (5*i + 1) % 16;
            } else if (i < 48) {
                f = b ^ c ^ d;
                g = (3*i + 5) % 16;
            } else {
                f = c ^ (b | (~d));
                g = (7*i) % 16;
            }
            uint32_t tmp = d;
            d = c;
            c = b;
            uint32_t to_add = a + f + k[i] + w[g];
            b = b + leftrotate(to_add, r[i]);
            a = tmp;
        }

        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
    }

    free(msg);

    memcpy(digest + 0, &h0, 4);
    memcpy(digest + 4, &h1, 4);
    memcpy(digest + 8, &h2, 4);
    memcpy(digest + 12, &h3, 4);
}

} // namespace

unsigned char* MD5::make_hash(char* input) {
    if (!input) return nullptr;
    size_t len = strlen(input);
    unsigned char* out = (unsigned char*)malloc(16);
    if (!out) return nullptr;
    md5((const unsigned char*)input, len, out);
    return out;
}

char* MD5::make_digest(const unsigned char* hash, std::size_t len) {
    if (!hash || len == 0) return nullptr;
    const char hex[] = "0123456789abcdef";
    size_t out_len = len * 2 + 1;
    char* out = (char*)malloc(out_len);
    if (!out) return nullptr;
    for (size_t i = 0; i < len; ++i) {
        unsigned char v = hash[i];
        out[i*2] = hex[(v >> 4) & 0x0F];
        out[i*2 + 1] = hex[v & 0x0F];
    }
    out[out_len - 1] = '\0';
    return out;
}
