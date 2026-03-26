#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace switchpalace::util {

struct FileEntry {
    std::string path;      // full path on SD (e.g., "sdmc:/games/title.nsp")
    std::string filename;  // display name (e.g., "title.nsp")
    std::string extension; // lowercase: "nsp", "xci", "nsz", "xcz"
    uint64_t fileSize;
};

/// Recursively scan sdmc:/ for installable files (.nsp, .xci, .nsz, .xcz)
/// Returns entries sorted alphabetically by filename (case-insensitive)
std::vector<FileEntry> scanForInstallableFiles();

/// Check if extension is a compressed format (nsz, xcz)
bool isCompressedFormat(const std::string& extension);

/// Format file size for display (e.g., "4.2 GB", "850 MB")
std::string formatFileSize(uint64_t bytes);

/// Format speed for display (e.g., "52.3 MB/s")
std::string formatSpeed(double bytesPerSecond);

/// Format ETA using largest applicable unit pair
/// "ETA {h}h {m}m" or "ETA {m}m {s}s" or "ETA {s}s"
std::string formatETA(uint64_t remainingSeconds);

} // namespace switchpalace::util
