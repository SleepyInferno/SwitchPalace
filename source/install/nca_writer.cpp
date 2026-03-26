#include "install/nca_writer.hpp"

#include <cstring>
#include <algorithm>
#include <memory>

#ifdef __SWITCH__
#include <zstd.h>
#else
// Host stubs for zstd types -- real builds link against switch-zstd
typedef struct ZSTD_DStream_s ZSTD_DStream;
typedef struct {
    const void* src;
    size_t size;
    size_t pos;
} ZSTD_inBuffer;
typedef struct {
    void* dst;
    size_t size;
    size_t pos;
} ZSTD_outBuffer;
// Stub functions for host compilation
inline ZSTD_DStream* ZSTD_createDStream() { return nullptr; }
inline size_t ZSTD_initDStream(ZSTD_DStream*) { return 0; }
inline size_t ZSTD_decompressStream(ZSTD_DStream*, ZSTD_outBuffer*, ZSTD_inBuffer*) { return 0; }
inline size_t ZSTD_freeDStream(ZSTD_DStream*) { return 0; }
inline unsigned ZSTD_isError(size_t code) { return code > 0x80000000UL; }
#endif

namespace switchpalace::install {

// NCZ header constants
static constexpr uint64_t NCZ_SECTION_MAGIC = 0x4E43535A4543544E; // "NCZSECTN" as uint64_t LE
static constexpr size_t NCZ_HEADER_SIZE = 0x4000; // First 0x4000 bytes are uncompressed NCA header

// ============================================================================
// Standard NCA writer -- direct stream to NCM placeholder
// ============================================================================

bool NcaWriter::writeStandardNca(
    nx::ContentManager& cm,
    const NcmPlaceHolderId& phId,
    uint64_t ncaSize,
    ReadFunc readFunc,
    util::SHA256& hasher,
    std::atomic<bool>& cancel,
    std::function<void(uint64_t bytesWritten)> onChunkWritten,
    size_t chunkSize)
{
    auto buffer = std::make_unique<uint8_t[]>(chunkSize);
    uint64_t offset = 0;

    while (offset < ncaSize) {
        if (cancel.load(std::memory_order_relaxed)) {
            return false;
        }

        size_t toRead = static_cast<size_t>(std::min<uint64_t>(chunkSize, ncaSize - offset));
        ssize_t bytesRead = readFunc(buffer.get(), toRead);
        if (bytesRead <= 0) {
            return false;
        }

        // Update hash with the data being written
        hasher.update(buffer.get(), static_cast<size_t>(bytesRead));

        // Write to NCM placeholder
        if (!cm.writePlaceholder(phId, offset, buffer.get(), static_cast<size_t>(bytesRead))) {
            return false;
        }

        offset += static_cast<uint64_t>(bytesRead);

        if (onChunkWritten) {
            onChunkWritten(offset);
        }
    }

    return true;
}

// ============================================================================
// NCZ NCA writer -- decompress zstd, re-encrypt per section, write
// ============================================================================

/// Determine which NCZ section a given offset falls into, and apply AES-CTR encryption
static bool encryptNczChunk(
    const uint8_t* input, uint8_t* output, size_t size,
    uint64_t dataOffset,
    const std::vector<NczSectionEntry>& sections)
{
    size_t processed = 0;

    while (processed < size) {
        uint64_t currentOffset = dataOffset + processed;

        // Find the section this offset belongs to
        const NczSectionEntry* matchedSection = nullptr;
        for (const auto& section : sections) {
            if (currentOffset >= section.offset &&
                currentOffset < section.offset + section.size) {
                matchedSection = &section;
                break;
            }
        }

        if (!matchedSection) {
            // No section covers this offset -- copy unencrypted
            // This can happen for padding/alignment regions
            size_t remaining = size - processed;
            std::memcpy(output + processed, input + processed, remaining);
            processed += remaining;
            continue;
        }

        // Calculate how many bytes in this section we can process
        uint64_t sectionEnd = matchedSection->offset + matchedSection->size;
        size_t bytesInSection = static_cast<size_t>(
            std::min<uint64_t>(sectionEnd - currentOffset, size - processed));

        if (matchedSection->cryptoType == 0) {
            // No encryption needed for this section (cryptoType 0 = none)
            std::memcpy(output + processed, input + processed, bytesInSection);
        } else {
            // Apply AES-CTR encryption with the section's key and counter
            // The counter is incremented based on the block position within the section
            uint8_t adjustedCounter[16];
            std::memcpy(adjustedCounter, matchedSection->cryptoCounter, 16);

            // Adjust the counter for the current position within the section
            // AES-CTR counter increments per 16-byte block
            uint64_t blockOffset = (currentOffset - matchedSection->offset) / 16;
            // Add block offset to the counter (big-endian increment on last 8 bytes)
            uint64_t counterVal = 0;
            for (int i = 0; i < 8; i++) {
                counterVal = (counterVal << 8) | adjustedCounter[8 + i];
            }
            counterVal += blockOffset;
            for (int i = 7; i >= 0; i--) {
                adjustedCounter[8 + i] = static_cast<uint8_t>(counterVal & 0xFF);
                counterVal >>= 8;
            }

            if (!util::aesCtrEncrypt(matchedSection->cryptoKey, adjustedCounter,
                                     input + processed, output + processed, bytesInSection)) {
                return false;
            }
        }

        processed += bytesInSection;
    }

    return true;
}

bool NcaWriter::writeNczNca(
    nx::ContentManager& cm,
    const NcmPlaceHolderId& phId,
    uint64_t ncaSize,
    ReadFunc readFunc,
    const std::vector<NczSectionEntry>& sections,
    util::SHA256& hasher,
    std::atomic<bool>& cancel,
    std::function<void(uint64_t bytesWritten)> onChunkWritten,
    size_t chunkSize)
{
    // Step 1: Read and write the first 0x4000 bytes (uncompressed NCA header)
    auto headerBuf = std::make_unique<uint8_t[]>(NCZ_HEADER_SIZE);
    size_t headerRead = 0;

    while (headerRead < NCZ_HEADER_SIZE) {
        ssize_t bytesRead = readFunc(headerBuf.get() + headerRead,
                                     NCZ_HEADER_SIZE - headerRead);
        if (bytesRead <= 0) {
            return false;
        }
        headerRead += static_cast<size_t>(bytesRead);
    }

    // Write NCA header directly to placeholder (no decompression needed)
    hasher.update(headerBuf.get(), NCZ_HEADER_SIZE);
    if (!cm.writePlaceholder(phId, 0, headerBuf.get(), NCZ_HEADER_SIZE)) {
        return false;
    }

    if (onChunkWritten) {
        onChunkWritten(NCZ_HEADER_SIZE);
    }

    // Step 2: Initialize zstd decompression stream
    ZSTD_DStream* dstream = ZSTD_createDStream();
    if (!dstream) {
        return false;
    }

    size_t initResult = ZSTD_initDStream(dstream);
    if (ZSTD_isError(initResult)) {
        ZSTD_freeDStream(dstream);
        return false;
    }

    // Step 3: Streaming decompress, re-encrypt, and write
    auto compressedBuf = std::make_unique<uint8_t[]>(chunkSize);
    auto decompressedBuf = std::make_unique<uint8_t[]>(chunkSize);
    auto encryptedBuf = std::make_unique<uint8_t[]>(chunkSize);

    uint64_t writeOffset = NCZ_HEADER_SIZE; // Start writing after the NCA header
    bool done = false;

    while (!done && writeOffset < ncaSize) {
        if (cancel.load(std::memory_order_relaxed)) {
            ZSTD_freeDStream(dstream);
            return false;
        }

        // Read compressed data
        ssize_t bytesRead = readFunc(compressedBuf.get(), chunkSize);
        if (bytesRead < 0) {
            ZSTD_freeDStream(dstream);
            return false;
        }
        if (bytesRead == 0) {
            done = true;
            break;
        }

        ZSTD_inBuffer inBuf = { compressedBuf.get(), static_cast<size_t>(bytesRead), 0 };

        // Decompress all available input
        while (inBuf.pos < inBuf.size && writeOffset < ncaSize) {
            if (cancel.load(std::memory_order_relaxed)) {
                ZSTD_freeDStream(dstream);
                return false;
            }

            size_t outSize = std::min<size_t>(
                chunkSize, static_cast<size_t>(ncaSize - writeOffset));
            ZSTD_outBuffer outBuf = { decompressedBuf.get(), outSize, 0 };

            size_t ret = ZSTD_decompressStream(dstream, &outBuf, &inBuf);
            if (ZSTD_isError(ret)) {
                ZSTD_freeDStream(dstream);
                return false;
            }

            if (outBuf.pos > 0) {
                // Re-encrypt the decompressed data per section entry
                if (!encryptNczChunk(decompressedBuf.get(), encryptedBuf.get(),
                                     outBuf.pos, writeOffset, sections)) {
                    ZSTD_freeDStream(dstream);
                    return false;
                }

                // Update hash with the re-encrypted data (this is what NCM sees)
                hasher.update(encryptedBuf.get(), outBuf.pos);

                // Write to NCM placeholder
                if (!cm.writePlaceholder(phId, writeOffset, encryptedBuf.get(), outBuf.pos)) {
                    ZSTD_freeDStream(dstream);
                    return false;
                }

                writeOffset += outBuf.pos;

                if (onChunkWritten) {
                    onChunkWritten(writeOffset);
                }
            }

            // If ret == 0, the frame is complete
            if (ret == 0) {
                done = true;
                break;
            }
        }
    }

    ZSTD_freeDStream(dstream);
    return true;
}

// ============================================================================
// Parse NCZ section headers
// ============================================================================

bool parseNczSections(const uint8_t* data, size_t dataSize,
                      std::vector<NczSectionEntry>& outSections) {
    outSections.clear();

    // Minimum size: magic (8 bytes) + sectionCount (8 bytes)
    if (dataSize < 16) {
        return false;
    }

    // Validate magic: "NCZSECTN" = 0x4E43535A4543544E
    uint64_t magic = 0;
    std::memcpy(&magic, data, sizeof(magic));
    if (magic != NCZ_SECTION_MAGIC) {
        return false;
    }

    // Read section count
    uint64_t sectionCount = 0;
    std::memcpy(&sectionCount, data + 8, sizeof(sectionCount));

    // Sanity check section count (prevent excessive allocation)
    if (sectionCount > 64 || sectionCount == 0) {
        return false;
    }

    // Check we have enough data for all section entries
    size_t requiredSize = 16 + sectionCount * sizeof(NczSectionEntry);
    if (dataSize < requiredSize) {
        return false;
    }

    // Read section entries
    outSections.resize(static_cast<size_t>(sectionCount));
    std::memcpy(outSections.data(), data + 16,
                static_cast<size_t>(sectionCount) * sizeof(NczSectionEntry));

    return true;
}

} // namespace switchpalace::install
