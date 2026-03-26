#include "install/hfs0.hpp"
#include <cstring>

namespace switchpalace::install {

bool HFS0::parseHeader(const uint8_t* headerData, size_t headerDataSize)
{
    m_valid = false;
    m_files.clear();
    m_fileCount = 0;
    m_dataStartOffset = 0;

    // Need at least the header
    if (!headerData || headerDataSize < sizeof(HFS0Header))
        return false;

    const HFS0Header* header = reinterpret_cast<const HFS0Header*>(headerData);

    // Validate magic
    if (std::memcmp(header->magic, "HFS0", 4) != 0)
        return false;

    m_fileCount = header->fileCount;

    // Calculate data start offset
    m_dataStartOffset = sizeof(HFS0Header)
                      + static_cast<uint64_t>(m_fileCount) * sizeof(HFS0FileEntry)
                      + header->stringTableSize;

    // Validate we have enough data for all entries + string table
    if (headerDataSize < m_dataStartOffset)
        return false;

    const HFS0FileEntry* entries = reinterpret_cast<const HFS0FileEntry*>(
        headerData + sizeof(HFS0Header));

    const char* stringTable = reinterpret_cast<const char*>(
        headerData + sizeof(HFS0Header) + m_fileCount * sizeof(HFS0FileEntry));

    uint32_t stringTableSize = header->stringTableSize;

    m_files.reserve(m_fileCount);

    for (uint32_t i = 0; i < m_fileCount; i++)
    {
        HFS0File file;

        // Read name from string table
        if (entries[i].stringTableOffset < stringTableSize)
        {
            file.name = std::string(stringTable + entries[i].stringTableOffset);
        }

        // Absolute offset = data start + entry's relative offset
        file.offset = m_dataStartOffset + entries[i].dataOffset;
        file.size = entries[i].dataSize;

        // Copy hash
        std::memcpy(file.hash, entries[i].hash, 0x20);

        m_files.push_back(std::move(file));
    }

    m_valid = true;
    return true;
}

bool HFS0::isValid() const
{
    return m_valid;
}

uint32_t HFS0::getFileCount() const
{
    return m_fileCount;
}

const std::vector<HFS0File>& HFS0::getFiles() const
{
    return m_files;
}

const HFS0File* HFS0::findFileByName(const std::string& name) const
{
    for (const auto& file : m_files)
    {
        if (file.name == name)
            return &file;
    }
    return nullptr;
}

uint64_t HFS0::getDataStartOffset() const
{
    return m_dataStartOffset;
}

} // namespace switchpalace::install
