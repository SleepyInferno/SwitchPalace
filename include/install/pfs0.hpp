#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace switchpalace::install {

struct PFS0Header {
    char magic[4];        // "PFS0"
    uint32_t fileCount;
    uint32_t stringTableSize;
    uint32_t reserved;
};
static_assert(sizeof(PFS0Header) == 16);

struct PFS0FileEntry {
    uint64_t dataOffset;
    uint64_t dataSize;
    uint32_t stringTableOffset;
    uint32_t reserved;
};
static_assert(sizeof(PFS0FileEntry) == 24);

struct PFS0File {
    std::string name;
    uint64_t offset;  // absolute offset in container
    uint64_t size;
};

class PFS0 {
public:
    /// Parse from raw header data. headerData must contain at least the
    /// PFS0 header + all file entries + the string table.
    bool parseHeader(const uint8_t* headerData, size_t headerDataSize);

    bool isValid() const;
    uint32_t getFileCount() const;
    const std::vector<PFS0File>& getFiles() const;

    /// Find a file by exact name
    const PFS0File* findFileByName(const std::string& name) const;

    /// Find the first file whose name ends with the given suffix
    /// (e.g., ".cnmt.nca")
    const PFS0File* findFileBySuffix(const std::string& suffix) const;

    /// Calculate the offset where actual file data begins
    uint64_t getDataStartOffset() const;

private:
    bool m_valid = false;
    uint32_t m_fileCount = 0;
    uint64_t m_dataStartOffset = 0;
    std::vector<PFS0File> m_files;
};

} // namespace switchpalace::install
