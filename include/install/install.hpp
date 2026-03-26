#pragma once

#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <cstdint>

// Forward declarations for Switch types (actual types from libnx)
// On host builds, these are stubbed
#ifdef __SWITCH__
#include <switch.h>
#else
// Stub types for host compilation
typedef uint8_t NcmContentId[0x10];
typedef uint8_t NcmPlaceHolderId[0x10];
typedef int NcmStorageId;
constexpr int NcmStorageId_SdCard = 2;
constexpr int NcmStorageId_BuiltInUser = 3;
#endif

namespace switchpalace::install {

enum class InstallDestination {
    SdCard,
    NAND
};

enum class FileFormat {
    NSP,
    XCI,
    NSZ,
    XCZ,
    Unknown
};

struct InstallProgress {
    uint64_t bytesWritten;
    uint64_t totalBytes;
    double speedBytesPerSec;
    uint64_t etaSeconds;
    std::string currentFile;
};

struct InstallResult {
    std::string filename;
    bool success;
    bool hashVerified;     // true if hash matched
    std::string errorMsg;  // empty if success
};

using ProgressCallback = std::function<void(const InstallProgress&)>;
using CancelToken = std::atomic<bool>;

class Install {
public:
    virtual ~Install() = default;

    /// Parse the container and prepare for installation
    virtual bool prepare() = 0;

    /// Execute the installation (streaming NCA data through NCM)
    /// Returns per-NCA results; caller aggregates into InstallResult
    virtual bool execute(InstallDestination dest, ProgressCallback onProgress, CancelToken& cancel) = 0;

    /// Roll back any created placeholders (called on failure/cancel)
    virtual void rollback() = 0;

    /// Get the total size of all NCAs to be installed
    virtual uint64_t getTotalSize() const = 0;

    /// Check if any title in this container is already installed at the destination
    virtual bool checkAlreadyInstalled(InstallDestination dest) const = 0;

    FileFormat getFormat() const { return m_format; }
    const std::string& getFilePath() const { return m_filePath; }

protected:
    Install(const std::string& filePath, FileFormat format)
        : m_filePath(filePath), m_format(format) {}

    std::string m_filePath;
    FileFormat m_format;
};

/// Detect format from file extension
FileFormat detectFormat(const std::string& filename);

} // namespace switchpalace::install
