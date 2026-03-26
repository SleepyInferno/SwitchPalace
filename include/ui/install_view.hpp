#pragma once

#include <borealis.hpp>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include "install/install.hpp"
#include "util/file_util.hpp"
#include "ui/progress_bar.hpp"

namespace switchpalace::ui {

class InstallProgressView : public brls::Box {
public:
    InstallProgressView(
        const std::vector<switchpalace::util::FileEntry>& files,
        install::InstallDestination destination
    );
    ~InstallProgressView();

private:
    void startInstallThread();
    void updateUI();
    void onCancelPressed();
    void transitionToSummary();

    ProgressBar* m_progressBar = nullptr;
    brls::Label* m_percentLabel = nullptr;
    brls::Label* m_filenameLabel = nullptr;
    brls::Label* m_speedLabel = nullptr;
    brls::Label* m_etaLabel = nullptr;
    brls::ScrollingFrame* m_statusList = nullptr;
    brls::Box* m_statusContainer = nullptr;
    brls::Box* m_nandWarningBanner = nullptr;
    brls::Box* m_cancelButton = nullptr;

    std::vector<switchpalace::util::FileEntry> m_files;
    install::InstallDestination m_destination;
    std::vector<install::InstallResult> m_results;
    size_t m_lastRenderedResult = 0;

    std::thread m_installThread;
    std::atomic<bool> m_cancel{false};
    std::mutex m_progressMutex;
    install::InstallProgress m_latestProgress;
    std::atomic<bool> m_installComplete{false};
    bool m_hasTransitioned = false;

    // UI update task (runs on main thread)
    class UIUpdateTask : public brls::RepeatingTask {
    public:
        UIUpdateTask(InstallProgressView* view);
        void run() override;
    private:
        InstallProgressView* m_view;
    };
    UIUpdateTask* m_updateTask = nullptr;
};

} // namespace switchpalace::ui
