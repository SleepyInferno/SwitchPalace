#pragma once

#include <cstdint>
#include <cstddef>

namespace switchpalace::util {

/// SHA-256 hash computation (streaming)
class SHA256 {
public:
    SHA256();
    ~SHA256();

    // Non-copyable
    SHA256(const SHA256&) = delete;
    SHA256& operator=(const SHA256&) = delete;

    void init();
    void update(const uint8_t* data, size_t size);
    void finish(uint8_t hash[32]);

private:
    struct Impl;
    Impl* m_impl;
};

/// AES-CTR encryption for NCZ re-encryption
/// Uses libnx hardware-accelerated AES on Switch, mbedtls fallback otherwise
bool aesCtrEncrypt(const uint8_t* key, const uint8_t* counter,
                   const uint8_t* input, uint8_t* output, size_t size);

} // namespace switchpalace::util
