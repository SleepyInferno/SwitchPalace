#include "util/crypto.hpp"

#include <cstring>

#ifdef __SWITCH__
#include <switch.h>
// libnx provides hardware-accelerated AES via spl service
// and mbedtls is available via switch-mbedtls or bundled with libnx
#include <mbedtls/sha256.h>
#include <mbedtls/aes.h>
#else
// Host build stubs for compilation
// In a real host-test scenario, we would link against mbedtls
#endif

namespace switchpalace::util {

// ============================================================================
// SHA256 implementation
// ============================================================================

// Internal context stored as opaque bytes to avoid exposing mbedtls in header
struct SHA256::Impl {
#ifdef __SWITCH__
    mbedtls_sha256_context ctx;
#else
    uint8_t placeholder[256]; // stub for host builds
#endif
    bool initialized = false;
};

// Static storage -- we use placement new with a static member approach
// To keep the header clean, we store the impl pointer via an opaque buffer

SHA256::SHA256() : m_impl(nullptr) {}

SHA256::~SHA256() {
    if (m_impl) {
#ifdef __SWITCH__
        mbedtls_sha256_free(&m_impl->ctx);
#endif
        delete m_impl;
        m_impl = nullptr;
    }
}

void SHA256::init() {
    if (m_impl) {
#ifdef __SWITCH__
        mbedtls_sha256_free(&m_impl->ctx);
#endif
        delete m_impl;
    }

    m_impl = new Impl();

#ifdef __SWITCH__
    mbedtls_sha256_init(&m_impl->ctx);
    mbedtls_sha256_starts(&m_impl->ctx, 0); // 0 = SHA-256 (not SHA-224)
#endif

    m_impl->initialized = true;
}

void SHA256::update(const uint8_t* data, size_t size) {
    if (!m_impl || !m_impl->initialized) return;

#ifdef __SWITCH__
    mbedtls_sha256_update(&m_impl->ctx, data, size);
#else
    (void)data;
    (void)size;
#endif
}

void SHA256::finish(uint8_t hash[32]) {
    if (!m_impl || !m_impl->initialized) {
        std::memset(hash, 0, 32);
        return;
    }

#ifdef __SWITCH__
    mbedtls_sha256_finish(&m_impl->ctx, hash);
    mbedtls_sha256_free(&m_impl->ctx);
#else
    std::memset(hash, 0, 32);
#endif

    m_impl->initialized = false;
}

// ============================================================================
// AES-CTR encryption for NCZ re-encryption
// ============================================================================

bool aesCtrEncrypt(const uint8_t* key, const uint8_t* counter,
                   const uint8_t* input, uint8_t* output, size_t size) {
    if (!key || !counter || !input || !output || size == 0) {
        return false;
    }

#ifdef __SWITCH__
    // Use libnx hardware-accelerated AES-CTR when available
    // Aes128CtrContext provides fast AES-CTR via the SPL service
    Aes128CtrContext ctx;
    aes128CtrContextCreate(&ctx, key, counter);
    aes128CtrCrypt(&ctx, output, input, size);
    return true;
#else
    // Host build stub -- no actual encryption
    std::memcpy(output, input, size);
    (void)key;
    (void)counter;
    return true;
#endif
}

} // namespace switchpalace::util
