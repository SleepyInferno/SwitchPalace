#pragma once

#include "install/install.hpp"
#include "install/hfs0.hpp"

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
    std::vector<uint8_t> m_placeholderIds;
};

} // namespace switchpalace::install
