#pragma once

#include <cstdint>
#include <cstddef>

namespace switchpalace::util {

/// SHA-256 hash computation (streaming)
class SHA256 {
public:
    void init();
    void update(const uint8_t* data, size_t size);
    void finish(uint8_t hash[32]);
};

/// AES-CTR encryption for NCZ re-encryption
bool aesCtrEncrypt(const uint8_t* key, const uint8_t* counter,
                   const uint8_t* input, uint8_t* output, size_t size);

} // namespace switchpalace::util
