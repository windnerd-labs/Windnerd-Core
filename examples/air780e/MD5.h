#ifndef MD5_H
#define MD5_H

#include <cstddef>

class MD5 {
public:
    // Compute raw 16-byte MD5 hash of a NUL-terminated C string.
    // Caller must free() the returned buffer using free().
    static unsigned char* make_hash(char* input);

    // Convert raw hash bytes into a lowercase hex NUL-terminated string.
    // Caller must free() the returned buffer using free().
    static char* make_digest(const unsigned char* hash, std::size_t len);
};

#endif // MD5_H
