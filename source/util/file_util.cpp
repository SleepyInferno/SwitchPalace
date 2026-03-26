#include "util/file_util.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <sstream>
#include <iomanip>

#ifdef __SWITCH__
#include <dirent.h>
#include <sys/stat.h>
#else
// Host build stubs
#include <dirent.h>
#include <sys/stat.h>
#endif

namespace switchpalace::util {

namespace {

std::string toLower(const std::string& s)
{
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

std::string getExtension(const std::string& filename)
{
    auto pos = filename.rfind('.');
    if (pos == std::string::npos || pos == filename.size() - 1)
        return "";
    return toLower(filename.substr(pos + 1));
}

bool isInstallableExtension(const std::string& ext)
{
    return ext == "nsp" || ext == "xci" || ext == "nsz" || ext == "xcz";
}

void scanDirectory(const std::string& dirPath, std::vector<FileEntry>& results)
{
    DIR* dir = opendir(dirPath.c_str());
    if (!dir)
        return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        std::string name = entry->d_name;
        if (name == "." || name == "..")
            continue;

        std::string fullPath = dirPath;
        if (fullPath.back() != '/')
            fullPath += '/';
        fullPath += name;

        struct stat st;
        if (stat(fullPath.c_str(), &st) != 0)
            continue;

        if (S_ISDIR(st.st_mode))
        {
            scanDirectory(fullPath, results);
        }
        else if (S_ISREG(st.st_mode))
        {
            std::string ext = getExtension(name);
            if (isInstallableExtension(ext))
            {
                FileEntry fe;
                fe.path = fullPath;
                fe.filename = name;
                fe.extension = ext;
                fe.fileSize = static_cast<uint64_t>(st.st_size);
                results.push_back(std::move(fe));
            }
        }
    }

    closedir(dir);
}

} // anonymous namespace

std::vector<FileEntry> scanForInstallableFiles()
{
    std::vector<FileEntry> results;

#ifdef __SWITCH__
    scanDirectory("sdmc:/", results);
#else
    // Host build: scan current directory for testing
    scanDirectory(".", results);
#endif

    // Sort alphabetically by filename (case-insensitive)
    std::sort(results.begin(), results.end(),
              [](const FileEntry& a, const FileEntry& b) {
                  return toLower(a.filename) < toLower(b.filename);
              });

    return results;
}

bool isCompressedFormat(const std::string& extension)
{
    std::string ext = toLower(extension);
    return ext == "nsz" || ext == "xcz";
}

std::string formatFileSize(uint64_t bytes)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);

    if (bytes >= 1073741824ULL) // 1 GB
    {
        oss << (static_cast<double>(bytes) / 1073741824.0) << " GB";
    }
    else if (bytes >= 1048576ULL) // 1 MB
    {
        oss << (static_cast<double>(bytes) / 1048576.0) << " MB";
    }
    else if (bytes >= 1024ULL) // 1 KB
    {
        oss << (static_cast<double>(bytes) / 1024.0) << " KB";
    }
    else
    {
        oss << bytes << " B";
    }

    return oss.str();
}

std::string formatSpeed(double bytesPerSecond)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);
    oss << (bytesPerSecond / 1048576.0) << " MB/s";
    return oss.str();
}

std::string formatETA(uint64_t remainingSeconds)
{
    if (remainingSeconds == 0)
        return "ETA 0s";

    uint64_t hours = remainingSeconds / 3600;
    uint64_t minutes = (remainingSeconds % 3600) / 60;
    uint64_t seconds = remainingSeconds % 60;

    std::ostringstream oss;
    oss << "ETA ";

    if (hours > 0)
    {
        oss << hours << "h " << minutes << "m";
    }
    else if (minutes > 0)
    {
        oss << minutes << "m " << seconds << "s";
    }
    else
    {
        oss << seconds << "s";
    }

    return oss.str();
}

} // namespace switchpalace::util
