#pragma once

#include "install/install.hpp"
#include "install/pfs0.hpp"

namespace switchpalace::install {

class NSPInstall : public Install {
public:
    explicit NSPInstall(const std::string& filePath);

    bool prepare() override;
    bool execute(InstallDestination dest, ProgressCallback onProgress, CancelToken& cancel) override;
    void rollback() override;
    uint64_t getTotalSize() const override;
    bool checkAlreadyInstalled(InstallDestination dest) const override;

private:
    PFS0 m_pfs0;
    uint64_t m_totalSize = 0;
    // Placeholder tracking for rollback -- populated during execute()
    std::vector<uint8_t> m_placeholderIds; // serialized NcmPlaceHolderIds
};

} // namespace switchpalace::install
