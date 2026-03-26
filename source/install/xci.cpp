#include "install/xci.hpp"
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
// Utility: shared with nsp.cpp -- parse hex content ID from NCA filename
// ============================================================================

static bool hexCharToNibble(char c, uint8_t& out) {
    if (c >= '0' && c <= '9') { out = static_cast<uint8_t>(c - '0'); return true; }
    if (c >= 'a' && c <= 'f') { out = static_cast<uint8_t>(c - 'a' + 10); return true; }
    if (c >= 'A' && c <= 'F') { out = static_cast<uint8_t>(c - 'A' + 10); return true; }
    return false;
}

static bool parseContentIdFromFilename(const std::string& filename, NcmContentId& outId) {
    std::string baseName = filename;
    auto dotPos = baseName.find('.');
    if (dotPos != std::string::npos) {
        baseName = baseName.substr(0, dotPos);
    }

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

static bool endsWithCI(const std::string& str, const std::string& suffix) {
    if (str.size() < suffix.size()) return false;
    std::string tail = str.substr(str.size() - suffix.size());
    std::string suffLower = suffix;
    std::transform(tail.begin(), tail.end(), tail.begin(), ::tolower);
    std::transform(suffLower.begin(), suffLower.end(), suffLower.begin(), ::tolower);
    return tail == suffLower;
}

static uint64_t getBaseTitleId(uint64_t titleId) {
    return titleId & 0xFFFFFFFFFFFFE000ULL;
}

/// Find first HFS0File whose name ends with the given suffix
static const HFS0File* findHfs0FileBySuffix(const HFS0& hfs0, const std::string& suffix) {
    for (const auto& f : hfs0.getFiles()) {
        if (endsWithCI(f.name, suffix)) {
            return &f;
        }
    }
    return nullptr;
}

// ============================================================================
// XCIInstall
// ============================================================================

XCIInstall::XCIInstall(const std::string& filePath)
    : Install(filePath, FileFormat::XCI)
{
}

bool XCIInstall::prepare() {
    FILE* file = fopen(m_filePath.c_str(), "rb");
    if (!file) {
        return false;
    }

    // -----------------------------------------------------------------------
    // Step 1: Read root HFS0 at XCI_ROOT_HFS0_OFFSET (0xF000)
    // -----------------------------------------------------------------------
    fseek(file, static_cast<long>(XCI_ROOT_HFS0_OFFSET), SEEK_SET);

    HFS0Header rootHdr = {};
    if (fread(&rootHdr, sizeof(rootHdr), 1, file) != 1) {
        fclose(file);
        return false;
    }

    if (std::memcmp(rootHdr.magic, "HFS0", 4) != 0) {
        fclose(file);
        return false;
    }

    // Read full root HFS0 header
    size_t rootHeaderSize = sizeof(HFS0Header)
        + rootHdr.fileCount * sizeof(HFS0FileEntry)
        + rootHdr.stringTableSize;

    std::vector<uint8_t> rootHeaderData(rootHeaderSize);
    fseek(file, static_cast<long>(XCI_ROOT_HFS0_OFFSET), SEEK_SET);
    if (fread(rootHeaderData.data(), rootHeaderSize, 1, file) != 1) {
        fclose(file);
        return false;
    }

    if (!m_rootHfs0.parseHeader(rootHeaderData.data(), rootHeaderData.size())) {
        fclose(file);
        return false;
    }

    // -----------------------------------------------------------------------
    // Step 2: Find the "secure" partition in root HFS0
    // -----------------------------------------------------------------------
    const HFS0File* secureEntry = m_rootHfs0.findFileByName("secure");
    if (!secureEntry) {
        fclose(file);
        return false;
    }

    // Calculate secure partition absolute offset
    // secureOffset = XCI_ROOT_HFS0_OFFSET + rootHfs0.getDataStartOffset() + secureEntry->offset
    m_securePartitionOffset = XCI_ROOT_HFS0_OFFSET
        + m_rootHfs0.getDataStartOffset()
        + secureEntry->offset;

    // -----------------------------------------------------------------------
    // Step 3: Read and parse secure HFS0 header
    // -----------------------------------------------------------------------
    fseek(file, static_cast<long>(m_securePartitionOffset), SEEK_SET);

    HFS0Header secureHdr = {};
    if (fread(&secureHdr, sizeof(secureHdr), 1, file) != 1) {
        fclose(file);
        return false;
    }

    // Validate HFS0 magic at computed offset
    if (std::memcmp(secureHdr.magic, "HFS0", 4) != 0) {
        fclose(file);
        return false;
    }

    size_t secureHeaderSize = sizeof(HFS0Header)
        + secureHdr.fileCount * sizeof(HFS0FileEntry)
        + secureHdr.stringTableSize;

    m_secureHeaderData.resize(secureHeaderSize);
    fseek(file, static_cast<long>(m_securePartitionOffset), SEEK_SET);
    if (fread(m_secureHeaderData.data(), secureHeaderSize, 1, file) != 1) {
        fclose(file);
        return false;
    }

    if (!m_secureHfs0.parseHeader(m_secureHeaderData.data(), m_secureHeaderData.size())) {
        fclose(file);
        return false;
    }

    fclose(file);

    // -----------------------------------------------------------------------
    // Step 4: Calculate total install size from secure partition NCA files
    // -----------------------------------------------------------------------
    m_totalSize = 0;
    for (const auto& f : m_secureHfs0.getFiles()) {
        if (endsWithCI(f.name, ".nca") || endsWithCI(f.name, ".ncz")) {
            m_totalSize += f.size;
        }
    }

    // Verify we can find a CNMT NCA
    if (!findHfs0FileBySuffix(m_secureHfs0, ".cnmt.nca")) {
        return false;
    }

    return true;
}

bool XCIInstall::checkAlreadyInstalled(InstallDestination dest) const {
    nx::ContentManager cm;
    if (!cm.openStorage(dest)) {
        return false;
    }

    for (const auto& f : m_secureHfs0.getFiles()) {
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

bool XCIInstall::execute(InstallDestination dest, ProgressCallback onProgress, CancelToken& cancel) {
    m_contentManager = std::make_unique<nx::ContentManager>();

    if (!m_contentManager->openStorage(dest)) {
        return false;
    }
    if (!m_contentManager->openMetaDatabase(dest)) {
        return false;
    }

    // Safety net: clean up stale placeholders
    m_contentManager->cleanupStalePlaceholders();

    FILE* file = fopen(m_filePath.c_str(), "rb");
    if (!file) {
        return false;
    }

    // Absolute offset to the start of file data within the secure HFS0 partition
    uint64_t secureDataStart = m_securePartitionOffset + m_secureHfs0.getDataStartOffset();
    uint64_t totalBytesWritten = 0;
    auto startTime = std::chrono::steady_clock::now();

    // -----------------------------------------------------------------------
    // Step 1: Process CNMT NCA first
    // -----------------------------------------------------------------------
    const HFS0File* cnmtEntry = findHfs0FileBySuffix(m_secureHfs0, ".cnmt.nca");
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

        // Absolute offset for this NCA within the XCI file
        uint64_t cnmtAbsOffset = secureDataStart + cnmtEntry->offset;
        fseek(file, static_cast<long>(cnmtAbsOffset), SEEK_SET);

        util::SHA256 cnmtHasher;
        cnmtHasher.init();
        NcaWriter writer;
        uint64_t cnmtReadOffset = 0;

        auto cnmtReadFunc = [&](uint8_t* buffer, size_t maxSize) -> ssize_t {
            size_t toRead = std::min<size_t>(maxSize,
                static_cast<size_t>(cnmtEntry->size - cnmtReadOffset));
            if (toRead == 0) return 0;
            size_t bytesRead = fread(buffer, 1, toRead, file);
            cnmtReadOffset += bytesRead;
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

        uint8_t cnmtHash[32];
        cnmtHasher.finish(cnmtHash);

        if (!m_contentManager->registerContent(cnmtContentId, cnmtPhId)) {
            fclose(file);
            rollback();
            return false;
        }
    }

    // -----------------------------------------------------------------------
    // Step 2: Process each NCA in secure partition (excluding CNMT)
    // -----------------------------------------------------------------------
    for (const auto& entry : m_secureHfs0.getFiles()) {
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

        // Calculate absolute NCA offset within XCI file
        uint64_t ncaAbsOffset = secureDataStart + entry.offset;
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
            // NCZ: parse section headers and decompress
            uint8_t sectionHeaderBuf[4096];
            fseek(file, static_cast<long>(ncaAbsOffset + 0x4000), SEEK_SET);
            size_t sectionBytesRead = fread(sectionHeaderBuf, 1, sizeof(sectionHeaderBuf), file);

            std::vector<NczSectionEntry> sections;
            if (!parseNczSections(sectionHeaderBuf, sectionBytesRead, sections)) {
                fclose(file);
                rollback();
                return false;
            }

            // Reset to NCA start for the writer
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

        // Verify hash
        uint8_t hash[32];
        hasher.finish(hash);
        if (std::memcmp(hash, contentId.c, 16) != 0) {
            fclose(file);
            rollback();
            return false;
        }

        // Register content
        if (!m_contentManager->registerContent(contentId, phId)) {
            fclose(file);
            rollback();
            return false;
        }

        totalBytesWritten += entry.size;
    }

    // -----------------------------------------------------------------------
    // Step 3: Import tickets if found in secure partition (rare for XCI)
    // -----------------------------------------------------------------------
    {
        nx::TicketManager ticketMgr;

        for (const auto& entry : m_secureHfs0.getFiles()) {
            if (!endsWithCI(entry.name, ".tik")) {
                continue;
            }

            uint64_t tikAbsOffset = secureDataStart + entry.offset;
            std::vector<uint8_t> tikData(entry.size);
            fseek(file, static_cast<long>(tikAbsOffset), SEEK_SET);
            if (fread(tikData.data(), 1, entry.size, file) != entry.size) {
                continue;
            }

            // Find matching cert
            std::string certName = entry.name;
            auto tikExtPos = certName.rfind(".tik");
            if (tikExtPos != std::string::npos) {
                certName = certName.substr(0, tikExtPos) + ".cert";
            }

            const HFS0File* certEntry = m_secureHfs0.findFileByName(certName);
            if (!certEntry) {
                // Try any cert file
                certEntry = findHfs0FileBySuffix(m_secureHfs0, ".cert");
            }

            if (certEntry) {
                uint64_t certAbsOffset = secureDataStart + certEntry->offset;
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

void XCIInstall::rollback() {
    if (m_contentManager) {
        m_contentManager->rollbackAll();
    }
}

uint64_t XCIInstall::getTotalSize() const {
    return m_totalSize;
}

} // namespace switchpalace::install
