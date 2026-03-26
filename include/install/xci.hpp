#pragma once

#include "install/install.hpp"
#include "install/hfs0.hpp"
#include "nx/content_mgmt.hpp"

#include <memory>

namespace switchpalace::install {

class XCIInstall : public Install {
public:
    explicit XCIInstall(const std::string& filePath);

    bool prepare() override;
    bool execute(InstallDestination dest, ProgressCallback onProgress, CancelToken& cancel) override;
    void rollback() override;
    uint64_t getTotalSize() const override;
    bool checkAlreadyInstalled(InstallDestination dest) const override;

private:
    HFS0 m_rootHfs0;
    HFS0 m_secureHfs0;
    uint64_t m_totalSize = 0;
    uint64_t m_securePartitionOffset = 0; // Absolute offset to secure HFS0 data
    std::vector<uint8_t> m_secureHeaderData;
    std::unique_ptr<nx::ContentManager> m_contentManager;
};

} // namespace switchpalace::install
