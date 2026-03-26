#pragma once

#include <borealis.hpp>
#include <vector>
#include "util/file_util.hpp"
#include "install/install.hpp"

namespace switchpalace::ui {

class FileBrowserView : public brls::Box {
public:
    FileBrowserView();

    void refreshFileList();
    std::vector<switchpalace::util::FileEntry> getSelectedFiles() const;
    install::InstallDestination getSelectedDestination() const;

private:
    void onInstallPressed();
    void setupAppletBadge();
    void setupActionBar();
    void setupEmptyState();
    void buildFileList();
    void updateInstallButton();
    void toggleDestination();

    brls::ScrollingFrame* m_scrollFrame = nullptr;
    brls::Box* m_listContainer = nullptr;
    brls::Box* m_actionBar = nullptr;
    brls::Box* m_sdOption = nullptr;
    brls::Box* m_nandOption = nullptr;
    brls::Label* m_sdLabel = nullptr;
    brls::Label* m_nandLabel = nullptr;
    brls::Box* m_installButton = nullptr;
    brls::Label* m_installLabel = nullptr;

    std::vector<switchpalace::util::FileEntry> m_allFiles;
    std::vector<bool> m_selected;
    install::InstallDestination m_destination = install::InstallDestination::SdCard;
    bool m_isAppletMode = false;
};

} // namespace switchpalace::ui
