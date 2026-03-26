#pragma once

#include "install/install.hpp"
#include "install/pfs0.hpp"
#include "nx/content_mgmt.hpp"

#include <memory>

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
    std::vector<uint8_t> m_headerData; // Full PFS0 header for re-reading
    std::unique_ptr<nx::ContentManager> m_contentManager;
};

} // namespace switchpalace::install
