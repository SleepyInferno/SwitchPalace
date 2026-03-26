#pragma once

#include <cstdint>
#include <vector>
#include <functional>
#include <atomic>
#include "nx/content_mgmt.hpp"
#include "util/crypto.hpp"

namespace switchpalace::install {

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
    /// Read function: called with (buffer, maxSize) -> returns bytes read, or -1 on error
    using ReadFunc = std::function<ssize_t(uint8_t* buffer, size_t maxSize)>;

    /// Write a standard NCA from source stream to NCM placeholder
    /// chunkSize: streaming buffer size (1MB applet, 8MB application)
    bool writeStandardNca(
        nx::ContentManager& cm,
        const NcmPlaceHolderId& phId,
        uint64_t ncaSize,
        ReadFunc readFunc,
        util::SHA256& hasher,
        std::atomic<bool>& cancel,
        std::function<void(uint64_t bytesWritten)> onChunkWritten,
        size_t chunkSize = 0x800000 // 8MB default
    );

    /// Write an NCZ (compressed NCA) -- decompress with zstd, re-encrypt, write
    /// First 0x4000 bytes are uncompressed NCA header -- write directly
    /// Then parse NCZSECTN entries, decompress zstd stream, re-encrypt per section
    bool writeNczNca(
        nx::ContentManager& cm,
        const NcmPlaceHolderId& phId,
        uint64_t ncaSize, // decompressed output size
        ReadFunc readFunc,
        const std::vector<NczSectionEntry>& sections,
        util::SHA256& hasher,
        std::atomic<bool>& cancel,
        std::function<void(uint64_t bytesWritten)> onChunkWritten,
        size_t chunkSize = 0x800000
    );
};

/// Parse NCZ section headers from the data at offset 0x4000
/// Returns the sections and advances the read position past the section header
bool parseNczSections(const uint8_t* data, size_t dataSize,
                      std::vector<NczSectionEntry>& outSections);

} // namespace switchpalace::install
