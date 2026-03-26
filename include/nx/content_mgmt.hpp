#pragma once

#include <cstdint>
#include <vector>
#include "install/install.hpp"

namespace switchpalace::nx {

class ContentManager {
public:
    bool openStorage(install::InstallDestination dest);
    void closeStorage();

    bool createPlaceholder(/* contentId, placeholderId, size */);
    bool writePlaceholder(/* placeholderId, offset, data, size */);
    bool registerContent(/* contentId, placeholderId */);
    bool deletePlaceholder(/* placeholderId */);

    bool commitMetaDatabase(/* metaKey, metaData, metaSize */);
    bool pushApplicationRecord(/* titleId, storageId */);

    bool hasContent(/* contentId */) const;
    int64_t getFreeSpace() const;

    /// Cleanup stale placeholders from previous failed runs
    void cleanupStalePlaceholders();
};

} // namespace switchpalace::nx
