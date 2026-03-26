#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace switchpalace::install {

struct HFS0Header {
    char magic[4];        // "HFS0"
    uint32_t fileCount;
    uint32_t stringTableSize;
    uint32_t reserved;
};
static_assert(sizeof(HFS0Header) == 16);

struct HFS0FileEntry {
    uint64_t dataOffset;
    uint64_t dataSize;
    uint32_t stringTableOffset;
    uint32_t hashedSize;
    uint64_t reserved;
    uint8_t hash[0x20];   // SHA-256
};
static_assert(sizeof(HFS0FileEntry) == 0x40);

struct HFS0File {
    std::string name;
    uint64_t offset;  // absolute offset within the HFS0 partition
    uint64_t size;
    uint8_t hash[0x20];
};

class HFS0 {
public:
    /// Parse from raw header data. headerData must contain at least the
    /// HFS0 header + all file entries + the string table.
    bool parseHeader(const uint8_t* headerData, size_t headerDataSize);

    bool isValid() const;
    uint32_t getFileCount() const;
    const std::vector<HFS0File>& getFiles() const;

    /// Find a file by exact name
    const HFS0File* findFileByName(const std::string& name) const;

    /// Calculate the offset where actual file data begins
    uint64_t getDataStartOffset() const;

private:
    bool m_valid = false;
    uint32_t m_fileCount = 0;
    uint64_t m_dataStartOffset = 0;
    std::vector<HFS0File> m_files;
};

/// XCI-specific: root HFS0 starts at offset 0xF000 in the XCI file
constexpr uint64_t XCI_ROOT_HFS0_OFFSET = 0xF000;

} // namespace switchpalace::install
