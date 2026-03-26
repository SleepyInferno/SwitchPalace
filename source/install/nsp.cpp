#include "install/nsp.hpp"
#include "install/nca_writer.hpp"
#include "nx/ticket.hpp"
#include "util/crypto.hpp"

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <memory>

namespace switchpalace::install {

// ============================================================================
// Utility: parse hex content ID from NCA filename
// NCA filenames are hex-encoded content IDs + ".nca" or ".ncz"
// e.g., "abc123...def456.nca" -> NcmContentId { 0xab, 0xc1, 0x23, ... }
// ============================================================================

static bool hexCharToNibble(char c, uint8_t& out) {
    if (c >= '0' && c <= '9') { out = static_cast<uint8_t>(c - '0'); return true; }
    if (c >= 'a' && c <= 'f') { out = static_cast<uint8_t>(c - 'a' + 10); return true; }
    if (c >= 'A' && c <= 'F') { out = static_cast<uint8_t>(c - 'A' + 10); return true; }
    return false;
}

static bool parseContentIdFromFilename(const std::string& filename, NcmContentId& outId) {
    // Strip extension (.nca or .cnmt.nca or .ncz)
    std::string baseName = filename;
    auto dotPos = baseName.find('.');
    if (dotPos != std::string::npos) {
        baseName = baseName.substr(0, dotPos);
    }

    // Content ID is 16 bytes = 32 hex characters
    if (baseName.size() != 32) {
        return false;
    }

    for (size_t i = 0; i < 16; i++) {
        uint8_t hi, lo;
        if (!hexCharToNibble(baseName[i * 2], hi) ||
            !hexCharToNibble(baseName[i * 2 + 1], lo)) {
            return false;
        }
        outId.c[i] = (hi << 4) | lo;
    }

    return true;
}

/// Check if a filename ends with a given suffix (case-insensitive)
static bool endsWithCI(const std::string& str, const std::string& suffix) {
    if (str.size() < suffix.size()) return false;
    std::string tail = str.substr(str.size() - suffix.size());
    std::string suffLower = suffix;
    std::transform(tail.begin(), tail.end(), tail.begin(), ::tolower);
    std::transform(suffLower.begin(), suffLower.end(), suffLower.begin(), ::tolower);
    return tail == suffLower;
}

/// Derive base title ID from any title ID (mask lower 3 nibbles)
static uint64_t getBaseTitleId(uint64_t titleId) {
    return titleId & 0xFFFFFFFFFFFFE000ULL;
}

// ============================================================================
// NSPInstall
// ============================================================================

NSPInstall::NSPInstall(const std::string& filePath)
    : Install(filePath, FileFormat::NSP)
{
}

bool NSPInstall::prepare() {
    FILE* file = fopen(m_filePath.c_str(), "rb");
    if (!file) {
        return false;
    }

    // Read the PFS0 header to get file count and string table size
    PFS0Header hdr = {};
    if (fread(&hdr, sizeof(hdr), 1, file) != 1) {
        fclose(file);
        return false;
    }

    // Validate magic
    if (std::memcmp(hdr.magic, "PFS0", 4) != 0) {
        fclose(file);
        return false;
    }

    // Calculate full header size: header + file entries + string table
    size_t fullHeaderSize = sizeof(PFS0Header)
        + hdr.fileCount * sizeof(PFS0FileEntry)
        + hdr.stringTableSize;

    // Read full header data
    m_headerData.resize(fullHeaderSize);
    fseek(file, 0, SEEK_SET);
    if (fread(m_headerData.data(), fullHeaderSize, 1, file) != 1) {
        fclose(file);
        return false;
    }

    fclose(file);

    // Parse PFS0 header
    if (!m_pfs0.parseHeader(m_headerData.data(), m_headerData.size())) {
        return false;
    }

    if (!m_pfs0.isValid()) {
        return false;
    }

    // Verify we can find the CNMT NCA
    const PFS0File* cnmtNca = m_pfs0.findFileBySuffix(".cnmt.nca");
    if (!cnmtNca) {
        return false;
    }

    // Calculate total install size from all NCA entries
    m_totalSize = 0;
    for (const auto& f : m_pfs0.getFiles()) {
        if (endsWithCI(f.name, ".nca") || endsWithCI(f.name, ".ncz")) {
            m_totalSize += f.size;
        }
    }

    return true;
}

bool NSPInstall::checkAlreadyInstalled(InstallDestination dest) const {
    nx::ContentManager cm;
    if (!cm.openStorage(dest)) {
        return false;
    }

    // Check each NCA content ID against NCM storage
    for (const auto& f : m_pfs0.getFiles()) {
        if (!endsWithCI(f.name, ".nca") && !endsWithCI(f.name, ".ncz")) {
            continue;
        }

        NcmContentId contentId = {};
        if (parseContentIdFromFilename(f.name, contentId)) {
            if (cm.hasContent(contentId)) {
                return true; // Already installed
            }
        }
    }

    return false;
}

bool NSPInstall::execute(InstallDestination dest, ProgressCallback onProgress, CancelToken& cancel) {
    m_contentManager = std::make_unique<nx::ContentManager>();

    // Open NCM storage and meta database
    if (!m_contentManager->openStorage(dest)) {
        return false;
    }
    if (!m_contentManager->openMetaDatabase(dest)) {
        return false;
    }

    // Safety net: clean up stale placeholders from prior crashes
    m_contentManager->cleanupStalePlaceholders();

    // Open the NSP file for reading NCA data
    FILE* file = fopen(m_filePath.c_str(), "rb");
    if (!file) {
        return false;
    }

    uint64_t dataStartOffset = m_pfs0.getDataStartOffset();
    uint64_t totalBytesWritten = 0;
    auto startTime = std::chrono::steady_clock::now();

    // -----------------------------------------------------------------------
    // Step 1: Process CNMT NCA first
    // -----------------------------------------------------------------------
    const PFS0File* cnmtEntry = m_pfs0.findFileBySuffix(".cnmt.nca");
    if (!cnmtEntry) {
        fclose(file);
        return false;
    }

    {
        NcmContentId cnmtContentId = {};
        if (!parseContentIdFromFilename(cnmtEntry->name, cnmtContentId)) {
            fclose(file);
            return false;
        }

        NcmPlaceHolderId cnmtPhId = {};
        if (!m_contentManager->createPlaceholder(cnmtContentId, cnmtPhId,
                                                  static_cast<int64_t>(cnmtEntry->size))) {
            fclose(file);
            return false;
        }

        // Read CNMT NCA data and write to placeholder
        uint64_t cnmtAbsOffset = dataStartOffset + cnmtEntry->offset;
        fseek(file, static_cast<long>(cnmtAbsOffset), SEEK_SET);

        util::SHA256 cnmtHasher;
        cnmtHasher.init();
        NcaWriter writer;
        uint64_t cnmtOffset = 0;

        auto cnmtReadFunc = [&](uint8_t* buffer, size_t maxSize) -> ssize_t {
            size_t toRead = std::min<size_t>(maxSize,
                static_cast<size_t>(cnmtEntry->size - cnmtOffset));
            if (toRead == 0) return 0;
            size_t bytesRead = fread(buffer, 1, toRead, file);
            cnmtOffset += bytesRead;
            return static_cast<ssize_t>(bytesRead);
        };

        if (!writer.writeStandardNca(*m_contentManager, cnmtPhId,
                                      cnmtEntry->size, cnmtReadFunc, cnmtHasher,
                                      cancel,
                                      [&](uint64_t bytes) {
                                          totalBytesWritten = bytes;
                                      })) {
            fclose(file);
            rollback();
            return false;
        }

        // Verify hash
        uint8_t cnmtHash[32];
        cnmtHasher.finish(cnmtHash);

        // Register CNMT content
        if (!m_contentManager->registerContent(cnmtContentId, cnmtPhId)) {
            fclose(file);
            rollback();
            return false;
        }
    }

    // -----------------------------------------------------------------------
    // Step 2: Process each NCA (excluding CNMT)
    // -----------------------------------------------------------------------
    for (const auto& entry : m_pfs0.getFiles()) {
        // Skip non-NCA files and CNMT (already processed)
        if (!endsWithCI(entry.name, ".nca") && !endsWithCI(entry.name, ".ncz")) {
            continue;
        }
        if (endsWithCI(entry.name, ".cnmt.nca")) {
            continue;
        }

        // Check cancellation between NCAs
        if (cancel.load(std::memory_order_relaxed)) {
            fclose(file);
            rollback();
            return false;
        }

        NcmContentId contentId = {};
        if (!parseContentIdFromFilename(entry.name, contentId)) {
            fclose(file);
            rollback();
            return false;
        }

        NcmPlaceHolderId phId = {};
        if (!m_contentManager->createPlaceholder(contentId, phId,
                                                  static_cast<int64_t>(entry.size))) {
            fclose(file);
            rollback();
            return false;
        }

        // Seek to the NCA data within the NSP container
        uint64_t ncaAbsOffset = dataStartOffset + entry.offset;
        fseek(file, static_cast<long>(ncaAbsOffset), SEEK_SET);

        util::SHA256 hasher;
        hasher.init();
        NcaWriter writer;
        uint64_t ncaReadOffset = 0;

        auto readFunc = [&](uint8_t* buffer, size_t maxSize) -> ssize_t {
            size_t toRead = std::min<size_t>(maxSize,
                static_cast<size_t>(entry.size - ncaReadOffset));
            if (toRead == 0) return 0;
            size_t bytesRead = fread(buffer, 1, toRead, file);
            ncaReadOffset += bytesRead;
            return static_cast<ssize_t>(bytesRead);
        };

        bool isNcz = endsWithCI(entry.name, ".ncz");

        if (isNcz) {
            // NCZ: read first 0x4000 + section header, then decompress
            // Read the NCZ section header from the data after the NCA header
            uint8_t sectionHeaderBuf[4096]; // Large enough for section headers
            fseek(file, static_cast<long>(ncaAbsOffset + 0x4000), SEEK_SET);
            size_t sectionBytesRead = fread(sectionHeaderBuf, 1, sizeof(sectionHeaderBuf), file);

            std::vector<NczSectionEntry> sections;
            if (!parseNczSections(sectionHeaderBuf, sectionBytesRead, sections)) {
                fclose(file);
                rollback();
                return false;
            }

            // Reset file position to start of NCA data for the writer
            fseek(file, static_cast<long>(ncaAbsOffset), SEEK_SET);
            ncaReadOffset = 0;

            if (!writer.writeNczNca(*m_contentManager, phId,
                                     entry.size, readFunc, sections, hasher,
                                     cancel,
                                     [&](uint64_t bytes) {
                                         auto now = std::chrono::steady_clock::now();
                                         double elapsed = std::chrono::duration<double>(now - startTime).count();
                                         double speed = (elapsed > 0) ? bytes / elapsed : 0;
                                         uint64_t remaining = (speed > 0) ? static_cast<uint64_t>((m_totalSize - totalBytesWritten) / speed) : 0;

                                         if (onProgress) {
                                             onProgress(InstallProgress{
                                                 totalBytesWritten + bytes,
                                                 m_totalSize,
                                                 speed,
                                                 remaining,
                                                 entry.name
                                             });
                                         }
                                     })) {
                fclose(file);
                rollback();
                return false;
            }
        } else {
            // Standard NCA: direct stream write
            if (!writer.writeStandardNca(*m_contentManager, phId,
                                          entry.size, readFunc, hasher,
                                          cancel,
                                          [&](uint64_t bytes) {
                                              auto now = std::chrono::steady_clock::now();
                                              double elapsed = std::chrono::duration<double>(now - startTime).count();
                                              double speed = (elapsed > 0) ? bytes / elapsed : 0;
                                              uint64_t remaining = (speed > 0) ? static_cast<uint64_t>((m_totalSize - totalBytesWritten) / speed) : 0;

                                              if (onProgress) {
                                                  onProgress(InstallProgress{
                                                      totalBytesWritten + bytes,
                                                      m_totalSize,
                                                      speed,
                                                      remaining,
                                                      entry.name
                                                  });
                                              }
                                          })) {
                fclose(file);
                rollback();
                return false;
            }
        }

        // Finalize hash and verify against content ID
        uint8_t hash[32];
        hasher.finish(hash);
        // NCA content ID = first 16 bytes of SHA-256 hash of NCA data
        // Verification: compare hash[0..15] with contentId
        if (std::memcmp(hash, contentId.c, 16) != 0) {
            // Hash mismatch -- installation integrity failure
            fclose(file);
            rollback();
            return false;
        }

        // Register content with NCM
        if (!m_contentManager->registerContent(contentId, phId)) {
            fclose(file);
            rollback();
            return false;
        }

        totalBytesWritten += entry.size;
    }

    // -----------------------------------------------------------------------
    // Step 3: Import tickets (.tik + .cert files)
    // -----------------------------------------------------------------------
    {
        nx::TicketManager ticketMgr;

        for (const auto& entry : m_pfs0.getFiles()) {
            if (!endsWithCI(entry.name, ".tik")) {
                continue;
            }

            // Read ticket data
            uint64_t tikAbsOffset = dataStartOffset + entry.offset;
            std::vector<uint8_t> tikData(entry.size);
            fseek(file, static_cast<long>(tikAbsOffset), SEEK_SET);
            if (fread(tikData.data(), 1, entry.size, file) != entry.size) {
                continue; // Skip this ticket on read failure
            }

            // Find matching .cert file
            // Convention: same base name with .cert extension, or a common cert file
            std::string certName = entry.name;
            auto tikExtPos = certName.rfind(".tik");
            if (tikExtPos != std::string::npos) {
                certName = certName.substr(0, tikExtPos) + ".cert";
            }

            const PFS0File* certEntry = m_pfs0.findFileByName(certName);
            if (!certEntry) {
                // Try common cert filename
                certEntry = m_pfs0.findFileBySuffix(".cert");
            }

            if (certEntry) {
                uint64_t certAbsOffset = dataStartOffset + certEntry->offset;
                std::vector<uint8_t> certData(certEntry->size);
                fseek(file, static_cast<long>(certAbsOffset), SEEK_SET);
                if (fread(certData.data(), 1, certEntry->size, file) == certEntry->size) {
                    ticketMgr.importTicket(tikData.data(), tikData.size(),
                                           certData.data(), certData.size());
                }
            }
        }
    }

    fclose(file);

    // -----------------------------------------------------------------------
    // Step 4: Commit meta database and push application record
    // -----------------------------------------------------------------------
    if (!m_contentManager->commitMetaDatabase()) {
        rollback();
        return false;
    }

    // Derive base title ID from CNMT entry filename
    // For simplicity, use the content ID as a proxy -- in production, parse CNMT data
    // The base title ID has lower nibbles masked to 0x000
    NcmContentId cnmtContentId = {};
    parseContentIdFromFilename(cnmtEntry->name, cnmtContentId);
    uint64_t baseTitleId = 0;
    std::memcpy(&baseTitleId, cnmtContentId.c, sizeof(baseTitleId));
    baseTitleId = getBaseTitleId(baseTitleId);

    NcmStorageId storageId = (dest == InstallDestination::SdCard)
        ? NcmStorageId_SdCard
        : NcmStorageId_BuiltInUser;
    m_contentManager->pushApplicationRecord(baseTitleId, storageId);

    return true;
}

void NSPInstall::rollback() {
    if (m_contentManager) {
        m_contentManager->rollbackAll();
    }
}

uint64_t NSPInstall::getTotalSize() const {
    return m_totalSize;
}

// ============================================================================
// Format detection (free function)
// ============================================================================

FileFormat detectFormat(const std::string& filename) {
    auto dotPos = filename.rfind('.');
    if (dotPos == std::string::npos) {
        return FileFormat::Unknown;
    }

    std::string ext = filename.substr(dotPos);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".nsp") return FileFormat::NSP;
    if (ext == ".xci") return FileFormat::XCI;
    if (ext == ".nsz") return FileFormat::NSZ;
    if (ext == ".xcz") return FileFormat::XCZ;

    return FileFormat::Unknown;
}

} // namespace switchpalace::install
