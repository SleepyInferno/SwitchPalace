#pragma once

#include <cstdint>
#include <vector>
#include <functional>

namespace switchpalace::install {

/// NCZ section entry for re-encryption after decompression
struct NczSectionEntry {
    uint64_t offset;
    uint64_t size;
    uint64_t cryptoType;
    uint64_t padding;
    uint8_t cryptoKey[16];
    uint8_t cryptoCounter[16];
};

class NcaWriter {
public:
    /// Write NCA data to NCM placeholder in chunks.
    /// For NCZ: decompress zstd stream, re-encrypt per section, then write.
    /// Returns false on failure (caller should rollback).
    bool writeNca(/* NCM handles, source stream, size, isNcz, sections */);

    /// Compute SHA-256 hash of written data for verification
    bool verifyHash(const uint8_t expectedHash[0x20]) const;

private:
    std::vector<uint8_t> m_hashAccumulator;
};

} // namespace switchpalace::install
