#include "install/hfs0.hpp"

#include <cassert>
#include <cstring>
#include <cstdio>
#include <vector>

using namespace switchpalace::install;

// Helper: build an HFS0 binary blob in memory
static std::vector<uint8_t> buildHFS0(
    uint32_t fileCount,
    const std::vector<std::pair<std::string, uint64_t>>& files, // name, size pairs
    const std::vector<std::vector<uint8_t>>& hashes = {})       // optional SHA-256 hashes
{
    // Build string table
    std::vector<char> stringTable;
    std::vector<uint32_t> stringOffsets;
    for (const auto& f : files) {
        stringOffsets.push_back(static_cast<uint32_t>(stringTable.size()));
        stringTable.insert(stringTable.end(), f.first.begin(), f.first.end());
        stringTable.push_back('\0');
    }
    uint32_t stringTableSize = static_cast<uint32_t>(stringTable.size());

    // Build header
    HFS0Header header;
    std::memcpy(header.magic, "HFS0", 4);
    header.fileCount = fileCount;
    header.stringTableSize = stringTableSize;
    header.reserved = 0;

    // Build file entries
    std::vector<HFS0FileEntry> entries;
    uint64_t currentDataOffset = 0;
    for (size_t i = 0; i < files.size(); i++) {
        HFS0FileEntry entry;
        std::memset(&entry, 0, sizeof(entry));
        entry.dataOffset = currentDataOffset;
        entry.dataSize = files[i].second;
        entry.stringTableOffset = stringOffsets[i];
        entry.hashedSize = 0x200; // typical hashed size

        // Set hash if provided
        if (i < hashes.size() && hashes[i].size() == 0x20) {
            std::memcpy(entry.hash, hashes[i].data(), 0x20);
        } else {
            // Fill with a recognizable pattern
            std::memset(entry.hash, static_cast<int>(i + 0xAA), 0x20);
        }

        entries.push_back(entry);
        currentDataOffset += files[i].second;
    }

    // Assemble binary blob
    std::vector<uint8_t> data;
    data.resize(sizeof(HFS0Header) + entries.size() * sizeof(HFS0FileEntry) + stringTableSize);
    size_t offset = 0;

    std::memcpy(data.data() + offset, &header, sizeof(header));
    offset += sizeof(header);

    for (const auto& e : entries) {
        std::memcpy(data.data() + offset, &e, sizeof(e));
        offset += sizeof(e);
    }

    std::memcpy(data.data() + offset, stringTable.data(), stringTableSize);

    return data;
}

// Test 1: Valid HFS0 header with 2 files parses correctly
static void test_valid_hfs0_2_files()
{
    std::vector<std::pair<std::string, uint64_t>> files = {
        {"secure", 4194304},
        {"normal", 1048576}
    };

    auto data = buildHFS0(2, files);

    HFS0 hfs0;
    bool ok = hfs0.parseHeader(data.data(), data.size());

    assert(ok && "parseHeader should succeed");
    assert(hfs0.isValid() && "should be valid");
    assert(hfs0.getFileCount() == 2 && "should have 2 files");

    const auto& parsedFiles = hfs0.getFiles();
    assert(parsedFiles.size() == 2);

    assert(parsedFiles[0].name == "secure");
    assert(parsedFiles[0].size == 4194304);

    assert(parsedFiles[1].name == "normal");
    assert(parsedFiles[1].size == 1048576);

    // Verify findFileByName
    const HFS0File* found = hfs0.findFileByName("secure");
    assert(found != nullptr && "should find 'secure' partition");
    assert(found->size == 4194304);

    printf("  PASS: test_valid_hfs0_2_files\n");
}

// Test 2: Invalid magic returns isValid() == false
static void test_invalid_magic()
{
    std::vector<uint8_t> data(128, 0);
    std::memcpy(data.data(), "BAAD", 4); // bad magic

    HFS0 hfs0;
    bool ok = hfs0.parseHeader(data.data(), data.size());

    assert(!ok && "parseHeader should fail for bad magic");
    assert(!hfs0.isValid() && "should not be valid");

    printf("  PASS: test_invalid_magic\n");
}

// Test 3: File entries include hash field (verify 0x20 bytes copied)
static void test_hash_field_copied()
{
    std::vector<uint8_t> expectedHash(0x20);
    for (int i = 0; i < 0x20; i++)
        expectedHash[i] = static_cast<uint8_t>(i * 3 + 0x42);

    std::vector<std::pair<std::string, uint64_t>> files = {
        {"update", 2048}
    };
    std::vector<std::vector<uint8_t>> hashes = { expectedHash };

    auto data = buildHFS0(1, files, hashes);

    HFS0 hfs0;
    hfs0.parseHeader(data.data(), data.size());

    assert(hfs0.isValid());
    const auto& parsedFiles = hfs0.getFiles();
    assert(parsedFiles.size() == 1);

    // Verify all 0x20 hash bytes match
    bool hashMatch = (std::memcmp(parsedFiles[0].hash, expectedHash.data(), 0x20) == 0);
    assert(hashMatch && "hash field should be copied correctly (all 0x20 bytes)");

    printf("  PASS: test_hash_field_copied\n");
}

// Test 4: XCI_ROOT_HFS0_OFFSET constant is correct
static void test_xci_offset_constant()
{
    assert(XCI_ROOT_HFS0_OFFSET == 0xF000 &&
           "XCI root HFS0 offset should be 0xF000");
    printf("  PASS: test_xci_offset_constant\n");
}

int main()
{
    printf("Running HFS0 parser tests...\n");

    test_valid_hfs0_2_files();
    test_invalid_magic();
    test_hash_field_copied();
    test_xci_offset_constant();

    printf("All HFS0 tests PASSED!\n");
    return 0;
}
