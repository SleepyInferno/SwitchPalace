#include "install/pfs0.hpp"

#include <cassert>
#include <cstring>
#include <cstdio>
#include <vector>

using namespace switchpalace::install;

// Helper: build a PFS0 binary blob in memory
static std::vector<uint8_t> buildPFS0(
    uint32_t fileCount,
    const std::vector<std::pair<std::string, uint64_t>>& files) // name, size pairs
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
    PFS0Header header;
    std::memcpy(header.magic, "PFS0", 4);
    header.fileCount = fileCount;
    header.stringTableSize = stringTableSize;
    header.reserved = 0;

    // Build file entries
    std::vector<PFS0FileEntry> entries;
    uint64_t currentDataOffset = 0;
    for (size_t i = 0; i < files.size(); i++) {
        PFS0FileEntry entry;
        entry.dataOffset = currentDataOffset;
        entry.dataSize = files[i].second;
        entry.stringTableOffset = stringOffsets[i];
        entry.reserved = 0;
        entries.push_back(entry);
        currentDataOffset += files[i].second;
    }

    // Assemble binary blob
    std::vector<uint8_t> data;
    data.resize(sizeof(PFS0Header) + entries.size() * sizeof(PFS0FileEntry) + stringTableSize);
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

// Test 1: Valid PFS0 header with 3 files parses correctly
static void test_valid_pfs0_3_files()
{
    std::vector<std::pair<std::string, uint64_t>> files = {
        {"program.nca", 1048576},
        {"data.nca", 2097152},
        {"content.cnmt.nca", 4096}
    };

    auto data = buildPFS0(3, files);

    PFS0 pfs0;
    bool ok = pfs0.parseHeader(data.data(), data.size());

    assert(ok && "parseHeader should succeed");
    assert(pfs0.isValid() && "should be valid");
    assert(pfs0.getFileCount() == 3 && "should have 3 files");

    const auto& parsedFiles = pfs0.getFiles();
    assert(parsedFiles.size() == 3);

    assert(parsedFiles[0].name == "program.nca");
    assert(parsedFiles[0].size == 1048576);

    assert(parsedFiles[1].name == "data.nca");
    assert(parsedFiles[1].size == 2097152);

    assert(parsedFiles[2].name == "content.cnmt.nca");
    assert(parsedFiles[2].size == 4096);

    printf("  PASS: test_valid_pfs0_3_files\n");
}

// Test 2: Invalid magic returns isValid() == false
static void test_invalid_magic()
{
    std::vector<uint8_t> data(64, 0);
    std::memcpy(data.data(), "XXXX", 4); // bad magic

    PFS0 pfs0;
    bool ok = pfs0.parseHeader(data.data(), data.size());

    assert(!ok && "parseHeader should fail for bad magic");
    assert(!pfs0.isValid() && "should not be valid");

    printf("  PASS: test_invalid_magic\n");
}

// Test 3: Zero file count returns empty file list but isValid() == true
static void test_zero_file_count()
{
    auto data = buildPFS0(0, {});

    PFS0 pfs0;
    bool ok = pfs0.parseHeader(data.data(), data.size());

    assert(ok && "parseHeader should succeed with zero files");
    assert(pfs0.isValid() && "should be valid");
    assert(pfs0.getFileCount() == 0 && "should have 0 files");
    assert(pfs0.getFiles().empty() && "files list should be empty");

    printf("  PASS: test_zero_file_count\n");
}

// Test 4: findFileBySuffix(".cnmt.nca") returns correct entry
static void test_find_file_by_suffix()
{
    std::vector<std::pair<std::string, uint64_t>> files = {
        {"program.nca", 1048576},
        {"data.nca", 2097152},
        {"content.cnmt.nca", 4096}
    };

    auto data = buildPFS0(3, files);

    PFS0 pfs0;
    pfs0.parseHeader(data.data(), data.size());

    const PFS0File* found = pfs0.findFileBySuffix(".cnmt.nca");
    assert(found != nullptr && "should find .cnmt.nca file");
    assert(found->name == "content.cnmt.nca" && "should match the cnmt file");
    assert(found->size == 4096);

    const PFS0File* notFound = pfs0.findFileBySuffix(".nonexistent");
    assert(notFound == nullptr && "should not find nonexistent suffix");

    printf("  PASS: test_find_file_by_suffix\n");
}

// Test 5: getDataStartOffset() computes correctly
static void test_data_start_offset()
{
    std::vector<std::pair<std::string, uint64_t>> files = {
        {"file_a.nca", 100},
        {"file_b.nca", 200}
    };

    auto data = buildPFS0(2, files);

    PFS0 pfs0;
    pfs0.parseHeader(data.data(), data.size());

    // string table: "file_a.nca\0file_b.nca\0" = 22 bytes
    uint32_t stringTableSize = 11 + 11; // each name + null terminator
    uint64_t expected = sizeof(PFS0Header) + 2 * sizeof(PFS0FileEntry) + stringTableSize;

    assert(pfs0.getDataStartOffset() == expected &&
           "data start offset should be header + entries + string table");

    // Verify absolute offsets of files
    const auto& parsedFiles = pfs0.getFiles();
    assert(parsedFiles[0].offset == expected && "first file at data start");
    assert(parsedFiles[1].offset == expected + 100 && "second file offset");

    printf("  PASS: test_data_start_offset\n");
}

int main()
{
    printf("Running PFS0 parser tests...\n");

    test_valid_pfs0_3_files();
    test_invalid_magic();
    test_zero_file_count();
    test_find_file_by_suffix();
    test_data_start_offset();

    printf("All PFS0 tests PASSED!\n");
    return 0;
}
