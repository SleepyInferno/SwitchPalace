#include "install/pfs0.hpp"
#include <cstring>

namespace switchpalace::install {

bool PFS0::parseHeader(const uint8_t* headerData, size_t headerDataSize)
{
    m_valid = false;
    m_files.clear();
    m_fileCount = 0;
    m_dataStartOffset = 0;

    // Need at least the header
    if (!headerData || headerDataSize < sizeof(PFS0Header))
        return false;

    const PFS0Header* header = reinterpret_cast<const PFS0Header*>(headerData);

    // Validate magic
    if (std::memcmp(header->magic, "PFS0", 4) != 0)
        return false;

    m_fileCount = header->fileCount;

    // Calculate data start offset
    m_dataStartOffset = sizeof(PFS0Header)
                      + static_cast<uint64_t>(m_fileCount) * sizeof(PFS0FileEntry)
                      + header->stringTableSize;

    // Validate we have enough data for all entries + string table
    if (headerDataSize < m_dataStartOffset)
        return false;

    const PFS0FileEntry* entries = reinterpret_cast<const PFS0FileEntry*>(
        headerData + sizeof(PFS0Header));

    const char* stringTable = reinterpret_cast<const char*>(
        headerData + sizeof(PFS0Header) + m_fileCount * sizeof(PFS0FileEntry));

    uint32_t stringTableSize = header->stringTableSize;

    m_files.reserve(m_fileCount);

    for (uint32_t i = 0; i < m_fileCount; i++)
    {
        PFS0File file;

        // Read name from string table
        if (entries[i].stringTableOffset < stringTableSize)
        {
            file.name = std::string(stringTable + entries[i].stringTableOffset);
        }

        // Absolute offset = data start + entry's relative offset
        file.offset = m_dataStartOffset + entries[i].dataOffset;
        file.size = entries[i].dataSize;

        m_files.push_back(std::move(file));
    }

    m_valid = true;
    return true;
}

bool PFS0::isValid() const
{
    return m_valid;
}

uint32_t PFS0::getFileCount() const
{
    return m_fileCount;
}

const std::vector<PFS0File>& PFS0::getFiles() const
{
    return m_files;
}

const PFS0File* PFS0::findFileByName(const std::string& name) const
{
    for (const auto& file : m_files)
    {
        if (file.name == name)
            return &file;
    }
    return nullptr;
}

const PFS0File* PFS0::findFileBySuffix(const std::string& suffix) const
{
    for (const auto& file : m_files)
    {
        if (file.name.size() >= suffix.size() &&
            file.name.compare(file.name.size() - suffix.size(), suffix.size(), suffix) == 0)
        {
            return &file;
        }
    }
    return nullptr;
}

uint64_t PFS0::getDataStartOffset() const
{
    return m_dataStartOffset;
}

} // namespace switchpalace::install
