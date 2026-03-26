#include "nx/content_mgmt.hpp"

#ifdef __SWITCH__
#include <switch.h>
#endif

#include <cstring>

namespace switchpalace::nx {

ContentManager::~ContentManager() {
    closeMetaDatabase();
    closeStorage();
}

bool ContentManager::openStorage(install::InstallDestination dest) {
    if (m_storageOpen) {
        closeStorage();
    }

    m_storageId = (dest == install::InstallDestination::SdCard)
        ? NcmStorageId_SdCard
        : NcmStorageId_BuiltInUser;

#ifdef __SWITCH__
    Result rc = ncmOpenContentStorage(&m_contentStorage, m_storageId);
    if (R_FAILED(rc)) {
        return false;
    }
#endif

    m_storageOpen = true;
    return true;
}

void ContentManager::closeStorage() {
    if (m_storageOpen) {
#ifdef __SWITCH__
        ncmContentStorageClose(&m_contentStorage);
#endif
        m_storageOpen = false;
    }
}

bool ContentManager::createPlaceholder(const NcmContentId& contentId,
                                       NcmPlaceHolderId& outPhId,
                                       int64_t size) {
    if (!m_storageOpen) return false;

#ifdef __SWITCH__
    Result rc = ncmContentStorageGeneratePlaceHolderId(&m_contentStorage, &outPhId);
    if (R_FAILED(rc)) {
        return false;
    }

    rc = ncmContentStorageCreatePlaceHolder(&m_contentStorage, &contentId, &outPhId, size);
    if (R_FAILED(rc)) {
        return false;
    }

    trackPlaceholder(outPhId);
#endif

    return true;
}

bool ContentManager::writePlaceholder(const NcmPlaceHolderId& phId,
                                      uint64_t offset,
                                      const void* data,
                                      size_t size) {
    if (!m_storageOpen) return false;

#ifdef __SWITCH__
    Result rc = ncmContentStorageWritePlaceHolder(&m_contentStorage, &phId, offset, data, size);
    if (R_FAILED(rc)) {
        return false;
    }
#endif

    return true;
}

bool ContentManager::registerContent(const NcmContentId& contentId,
                                     const NcmPlaceHolderId& phId) {
    if (!m_storageOpen) return false;

#ifdef __SWITCH__
    Result rc = ncmContentStorageRegister(&m_contentStorage, &contentId, &phId);
    if (R_FAILED(rc)) {
        return false;
    }
#endif

    return true;
}

bool ContentManager::deletePlaceholder(const NcmPlaceHolderId& phId) {
    if (!m_storageOpen) return false;

#ifdef __SWITCH__
    Result rc = ncmContentStorageDeletePlaceHolder(&m_contentStorage, &phId);
    if (R_FAILED(rc)) {
        return false;
    }
#endif

    return true;
}

bool ContentManager::openMetaDatabase(install::InstallDestination dest) {
    if (m_metaDbOpen) {
        closeMetaDatabase();
    }

    NcmStorageId storageId = (dest == install::InstallDestination::SdCard)
        ? NcmStorageId_SdCard
        : NcmStorageId_BuiltInUser;

#ifdef __SWITCH__
    Result rc = ncmOpenContentMetaDatabase(&m_metaDb, storageId);
    if (R_FAILED(rc)) {
        return false;
    }
#endif

    m_metaDbOpen = true;
    return true;
}

bool ContentManager::setMetaRecord(const NcmContentMetaKey& key,
                                   const void* data,
                                   size_t size) {
    if (!m_metaDbOpen) return false;

#ifdef __SWITCH__
    Result rc = ncmContentMetaDatabaseSet(&m_metaDb, &key, data, size);
    if (R_FAILED(rc)) {
        return false;
    }
#endif

    return true;
}

bool ContentManager::commitMetaDatabase() {
    if (!m_metaDbOpen) return false;

#ifdef __SWITCH__
    Result rc = ncmContentMetaDatabaseCommit(&m_metaDb);
    if (R_FAILED(rc)) {
        return false;
    }
#endif

    return true;
}

void ContentManager::closeMetaDatabase() {
    if (m_metaDbOpen) {
#ifdef __SWITCH__
        ncmContentMetaDatabaseClose(&m_metaDb);
#endif
        m_metaDbOpen = false;
    }
}

bool ContentManager::pushApplicationRecord(uint64_t titleId, NcmStorageId storageId) {
#ifdef __SWITCH__
    Result rc = nsInitialize();
    if (R_FAILED(rc)) {
        return false;
    }

    // Build a storage record for the application
    NsApplicationContentMetaStatus storageRecord = {};
    storageRecord.application_id = titleId;
    storageRecord.storageID = static_cast<uint8_t>(storageId);

    // libnx 4.12+: NS auto-registers the application record after NCM meta commit.
    // nsPushApplicationRecord was removed from the public NS API.
    (void)storageRecord;
    nsExit();
#else
    (void)titleId;
    (void)storageId;
#endif

    return true;
}

bool ContentManager::hasContent(const NcmContentId& contentId) const {
    if (!m_storageOpen) return false;

#ifdef __SWITCH__
    bool hasOut = false;
    Result rc = ncmContentStorageHas(
        const_cast<NcmContentStorage*>(&m_contentStorage), &hasOut, &contentId);
    if (R_FAILED(rc)) {
        return false;
    }
    return hasOut;
#else
    (void)contentId;
    return false;
#endif
}

int64_t ContentManager::getFreeSpace() const {
    if (!m_storageOpen) return -1;

#ifdef __SWITCH__
    int64_t freeSpace = 0;
    Result rc = ncmContentStorageGetFreeSpaceSize(
        const_cast<NcmContentStorage*>(&m_contentStorage), &freeSpace);
    if (R_FAILED(rc)) {
        return -1;
    }
    return freeSpace;
#else
    return 0;
#endif
}

void ContentManager::cleanupStalePlaceholders() {
    if (!m_storageOpen) return;

#ifdef __SWITCH__
    ncmContentStorageCleanupAllPlaceHolder(&m_contentStorage);
#endif
}

void ContentManager::rollbackAll() {
    for (auto& phId : m_trackedPlaceholders) {
        deletePlaceholder(phId);
    }
    m_trackedPlaceholders.clear();
}

void ContentManager::trackPlaceholder(const NcmPlaceHolderId& phId) {
    m_trackedPlaceholders.push_back(phId);
}

} // namespace switchpalace::nx
