#pragma once

#include <cstdint>
#include <vector>
#include "install/install.hpp"

#ifdef __SWITCH__
#include <switch.h>
#else
// Stub types for host compilation
struct NcmContentStorage { int dummy; };
struct NcmContentMetaDatabase { int dummy; };
struct NcmContentMetaKey {
    uint64_t id;
    uint32_t version;
    uint8_t type;
    uint8_t install_type;
    uint8_t padding[2];
};
#endif

namespace switchpalace::nx {

class ContentManager {
public:
    ContentManager() = default;
    ~ContentManager();

    bool openStorage(install::InstallDestination dest);
    void closeStorage();

    // Placeholder lifecycle
    bool createPlaceholder(const NcmContentId& contentId, NcmPlaceHolderId& outPhId, int64_t size);
    bool writePlaceholder(const NcmPlaceHolderId& phId, uint64_t offset, const void* data, size_t size);
    bool registerContent(const NcmContentId& contentId, const NcmPlaceHolderId& phId);
    bool deletePlaceholder(const NcmPlaceHolderId& phId);

    // Metadata
    bool openMetaDatabase(install::InstallDestination dest);
    bool setMetaRecord(const NcmContentMetaKey& key, const void* data, size_t size);
    bool commitMetaDatabase();
    void closeMetaDatabase();

    // Application record
    bool pushApplicationRecord(uint64_t titleId, NcmStorageId storageId);

    // Queries
    bool hasContent(const NcmContentId& contentId) const;
    int64_t getFreeSpace() const;

    // Safety: clean up stale placeholders from prior failed runs
    void cleanupStalePlaceholders();

    // Rollback: delete all tracked placeholders
    void rollbackAll();

    // Track a placeholder for rollback
    void trackPlaceholder(const NcmPlaceHolderId& phId);

private:
    NcmContentStorage m_contentStorage = {};
    NcmContentMetaDatabase m_metaDb = {};
    bool m_storageOpen = false;
    bool m_metaDbOpen = false;
    NcmStorageId m_storageId = NcmStorageId_SdCard;
    std::vector<NcmPlaceHolderId> m_trackedPlaceholders;
};

} // namespace switchpalace::nx
