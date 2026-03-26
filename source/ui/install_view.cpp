#include "ui/install_view.hpp"
#include "ui/summary_view.hpp"
#include "ui/dialogs.hpp"
#include "install/nsp.hpp"
#include "install/xci.hpp"
#include "util/file_util.hpp"

namespace switchpalace::ui {

InstallProgressView::InstallProgressView(
    const std::vector<switchpalace::util::FileEntry>& files,
    install::InstallDestination destination)
    : m_files(files), m_destination(destination)
{
    setAxis(brls::Axis::COLUMN);
    setWidth(brls::View::AUTO);
    setHeight(brls::View::AUTO);
    setGrow(1.0f);
    setBackgroundColor(nvgRGB(0x1A, 0x1A, 0x1A));
    setPadding(32.0f, 32.0f, 32.0f, 32.0f);

    // NAND warning banner
    m_nandWarningBanner = new brls::Box();
    m_nandWarningBanner->setWidth(brls::View::AUTO);
    m_nandWarningBanner->setHeight(40.0f);
    m_nandWarningBanner->setShrink(0.0f);
    m_nandWarningBanner->setBackgroundColor(nvgRGB(0xFF, 0x9F, 0x0A));
    m_nandWarningBanner->setAlignItems(brls::AlignItems::CENTER);
    m_nandWarningBanner->setJustifyContent(brls::JustifyContent::CENTER);
    m_nandWarningBanner->setPadding(8.0f, 16.0f, 8.0f, 16.0f);

    auto* nandLabel = new brls::Label();
    nandLabel->setText("Installing to NAND system memory");
    nandLabel->setFontSize(18.0f);
    nandLabel->setTextColor(nvgRGB(0x1A, 0x1A, 0x1A));
    m_nandWarningBanner->addView(nandLabel);

    if (destination != install::InstallDestination::NAND)
        m_nandWarningBanner->setVisibility(brls::Visibility::GONE);

    addView(m_nandWarningBanner);

    // Progress percentage label
    m_percentLabel = new brls::Label();
    m_percentLabel->setText("0%");
    m_percentLabel->setFontSize(22.0f);
    m_percentLabel->setTextColor(nvgRGB(0x3D, 0x9C, 0xF0));
    m_percentLabel->setHorizontalAlign(brls::HorizontalAlign::RIGHT);
    m_percentLabel->setMargins(16.0f, 0.0f, 8.0f, 0.0f);
    addView(m_percentLabel);

    // Progress bar
    m_progressBar = new ProgressBar();
    m_progressBar->setMargins(0.0f, 0.0f, 12.0f, 0.0f);
    addView(m_progressBar);

    // Current filename
    m_filenameLabel = new brls::Label();
    m_filenameLabel->setText("");
    m_filenameLabel->setFontSize(20.0f);
    m_filenameLabel->setTextColor(nvgRGB(0xFF, 0xFF, 0xFF));
    m_filenameLabel->setMargins(0.0f, 0.0f, 4.0f, 0.0f);
    addView(m_filenameLabel);

    // Speed and ETA row
    auto* speedEtaRow = new brls::Box(brls::Axis::ROW);
    speedEtaRow->setWidth(brls::View::AUTO);
    speedEtaRow->setJustifyContent(brls::JustifyContent::SPACE_BETWEEN);
    speedEtaRow->setMargins(0.0f, 0.0f, 16.0f, 0.0f);

    m_speedLabel = new brls::Label();
    m_speedLabel->setText("0.0 MB/s");
    m_speedLabel->setFontSize(18.0f);
    m_speedLabel->setTextColor(nvgRGB(0xAD, 0xAD, 0xAD));
    speedEtaRow->addView(m_speedLabel);

    m_etaLabel = new brls::Label();
    m_etaLabel->setText("ETA --");
    m_etaLabel->setFontSize(18.0f);
    m_etaLabel->setTextColor(nvgRGB(0xAD, 0xAD, 0xAD));
    m_etaLabel->setHorizontalAlign(brls::HorizontalAlign::RIGHT);
    speedEtaRow->addView(m_etaLabel);

    addView(speedEtaRow);

    // Per-file status list
    m_statusList = new brls::ScrollingFrame();
    m_statusList->setGrow(1.0f);

    m_statusContainer = new brls::Box(brls::Axis::COLUMN);
    m_statusContainer->setWidth(brls::View::AUTO);
    m_statusList->addView(m_statusContainer);
    addView(m_statusList);

    // Cancel button
    auto* bottomBar = new brls::Box(brls::Axis::ROW);
    bottomBar->setWidth(brls::View::AUTO);
    bottomBar->setHeight(48.0f);
    bottomBar->setShrink(0.0f);
    bottomBar->setJustifyContent(brls::JustifyContent::FLEX_END);
    bottomBar->setMargins(16.0f, 0.0f, 0.0f, 0.0f);

    m_cancelButton = new brls::Box();
    m_cancelButton->setBorderColor(nvgRGB(0xFF, 0x45, 0x3A));
    m_cancelButton->setBorderThickness(2.0f);
    m_cancelButton->setCornerRadius(6.0f);
    m_cancelButton->setHeight(48.0f);
    m_cancelButton->setPadding(0.0f, 24.0f, 0.0f, 24.0f);
    m_cancelButton->setAlignItems(brls::AlignItems::CENTER);
    m_cancelButton->setJustifyContent(brls::JustifyContent::CENTER);
    m_cancelButton->setFocusable(true);

    auto* cancelLabel = new brls::Label();
    cancelLabel->setText("Cancel");
    cancelLabel->setFontSize(20.0f);
    cancelLabel->setTextColor(nvgRGB(0xFF, 0x45, 0x3A));
    m_cancelButton->addView(cancelLabel);

    m_cancelButton->registerClickAction([this](brls::View* view) {
        onCancelPressed();
        return true;
    });

    bottomBar->addView(m_cancelButton);
    addView(bottomBar);

    // Start the install thread
    startInstallThread();

    // Start UI update task (every 100ms on main thread)
    m_updateTask = new UIUpdateTask(this);
    m_updateTask->start();
}

InstallProgressView::~InstallProgressView()
{
    if (m_updateTask)
    {
        m_updateTask->stop();
        delete m_updateTask;
        m_updateTask = nullptr;
    }

    if (m_installThread.joinable())
    {
        m_cancel = true;
        m_installThread.join();
    }
}

// UIUpdateTask implementation
InstallProgressView::UIUpdateTask::UIUpdateTask(InstallProgressView* view)
    : brls::RepeatingTask(100), m_view(view)
{
}

void InstallProgressView::UIUpdateTask::run()
{
    if (m_view)
        m_view->updateUI();
}

void InstallProgressView::startInstallThread()
{
    m_installThread = std::thread([this]() {
        uint64_t totalBytesAllFiles = 0;
        for (const auto& file : m_files)
        {
            auto format = install::detectFormat(file.filename);
            std::unique_ptr<install::Install> installer;

            if (format == install::FileFormat::NSP || format == install::FileFormat::NSZ)
                installer = std::make_unique<install::NSPInstall>(file.path);
            else if (format == install::FileFormat::XCI || format == install::FileFormat::XCZ)
                installer = std::make_unique<install::XCIInstall>(file.path);
            else
                continue;

            if (installer->prepare())
                totalBytesAllFiles += installer->getTotalSize();
        }

        uint64_t cumulativeBytes = 0;

        for (size_t i = 0; i < m_files.size(); i++)
        {
            if (m_cancel.load())
                break;

            const auto& file = m_files[i];
            auto format = install::detectFormat(file.filename);
            std::unique_ptr<install::Install> installer;

            if (format == install::FileFormat::NSP || format == install::FileFormat::NSZ)
                installer = std::make_unique<install::NSPInstall>(file.path);
            else if (format == install::FileFormat::XCI || format == install::FileFormat::XCZ)
                installer = std::make_unique<install::XCIInstall>(file.path);
            else
            {
                install::InstallResult result;
                result.filename = file.filename;
                result.success = false;
                result.hashVerified = false;
                result.errorMsg = "Unknown format";
                std::lock_guard<std::mutex> lock(m_progressMutex);
                m_results.push_back(result);
                continue;
            }

            if (!installer->prepare())
            {
                install::InstallResult result;
                result.filename = file.filename;
                result.success = false;
                result.hashVerified = false;
                result.errorMsg = "Failed to parse container";
                std::lock_guard<std::mutex> lock(m_progressMutex);
                m_results.push_back(result);
                continue;
            }

            uint64_t fileSize = installer->getTotalSize();
            uint64_t baseCumulative = cumulativeBytes;

            auto progressCb = [this, baseCumulative, totalBytesAllFiles, &file](const install::InstallProgress& p) {
                std::lock_guard<std::mutex> lock(m_progressMutex);
                m_latestProgress.bytesWritten = baseCumulative + p.bytesWritten;
                m_latestProgress.totalBytes = totalBytesAllFiles;
                m_latestProgress.speedBytesPerSec = p.speedBytesPerSec;
                m_latestProgress.etaSeconds = p.etaSeconds;
                m_latestProgress.currentFile = file.filename;
            };

            bool success = installer->execute(m_destination, progressCb, m_cancel);

            if (m_cancel.load())
            {
                installer->rollback();

                install::InstallResult result;
                result.filename = file.filename;
                result.success = false;
                result.hashVerified = false;
                result.errorMsg = "Cancelled";
                std::lock_guard<std::mutex> lock(m_progressMutex);
                m_results.push_back(result);
                break;
            }

            if (success)
            {
                cumulativeBytes += fileSize;
                install::InstallResult result;
                result.filename = file.filename;
                result.success = true;
                result.hashVerified = true;
                result.errorMsg = "";
                std::lock_guard<std::mutex> lock(m_progressMutex);
                m_results.push_back(result);
            }
            else
            {
                installer->rollback();
                install::InstallResult result;
                result.filename = file.filename;
                result.success = false;
                result.hashVerified = false;
                result.errorMsg = "Installation failed";
                std::lock_guard<std::mutex> lock(m_progressMutex);
                m_results.push_back(result);
            }
        }

        m_installComplete = true;
    });

    // Register a repeating timer to update the UI from the main thread
    registerAction("", brls::ControllerButton::BUTTON_B, [](brls::View* view) {
        // B does nothing during install (cancel requires explicit button)
        return true;
    }, true);
}

void InstallProgressView::updateUI()
{
    install::InstallProgress progress;
    std::vector<install::InstallResult> newResults;

    {
        std::lock_guard<std::mutex> lock(m_progressMutex);
        progress = m_latestProgress;

        for (size_t i = m_lastRenderedResult; i < m_results.size(); i++)
            newResults.push_back(m_results[i]);
    }

    if (progress.totalBytes > 0)
    {
        float percent = static_cast<float>(progress.bytesWritten) / static_cast<float>(progress.totalBytes) * 100.0f;
        m_progressBar->setProgress(percent);

        char buf[16];
        snprintf(buf, sizeof(buf), "%.0f%%", percent);
        m_percentLabel->setText(buf);
    }

    m_filenameLabel->setText(progress.currentFile);
    m_speedLabel->setText(switchpalace::util::formatSpeed(progress.speedBytesPerSec));
    m_etaLabel->setText(switchpalace::util::formatETA(progress.etaSeconds));

    // Render new per-file results
    for (const auto& result : newResults)
    {
        auto* row = new brls::Box(brls::Axis::ROW);
        row->setWidth(brls::View::AUTO);
        row->setPadding(8.0f, 0.0f, 8.0f, 0.0f);
        row->setAlignItems(brls::AlignItems::CENTER);

        auto* icon = new brls::Label();
        auto* nameLabel = new brls::Label();
        nameLabel->setFontSize(20.0f);
        nameLabel->setMargins(0.0f, 0.0f, 0.0f, 8.0f);

        if (result.success)
        {
            icon->setText("\u2713"); // Checkmark
            icon->setFontSize(20.0f);
            icon->setTextColor(nvgRGB(0x30, 0xD1, 0x58));
            nameLabel->setText(result.filename);
            nameLabel->setTextColor(nvgRGB(0xFF, 0xFF, 0xFF));
        }
        else
        {
            icon->setText("\u2717"); // X mark
            icon->setFontSize(20.0f);
            icon->setTextColor(nvgRGB(0xFF, 0x45, 0x3A));
            nameLabel->setText(result.filename);
            nameLabel->setTextColor(nvgRGB(0xFF, 0xFF, 0xFF));
        }

        row->addView(icon);
        row->addView(nameLabel);

        if (!result.success && !result.errorMsg.empty())
        {
            auto* errLabel = new brls::Label();
            errLabel->setText(" - " + result.errorMsg);
            errLabel->setFontSize(18.0f);
            errLabel->setTextColor(nvgRGB(0xFF, 0x45, 0x3A));
            row->addView(errLabel);
        }

        m_statusContainer->addView(row);
        m_lastRenderedResult++;
    }

    if (m_installComplete)
    {
        transitionToSummary();
    }
}

void InstallProgressView::onCancelPressed()
{
    showCancelDialog([this]() {
        m_cancel = true;
    });
}

void InstallProgressView::transitionToSummary()
{
    std::vector<install::InstallResult> results;
    {
        std::lock_guard<std::mutex> lock(m_progressMutex);
        results = m_results;
    }

    auto* summaryView = new SummaryView(results);
    brls::Application::pushActivity(new brls::Activity(summaryView));
}

} // namespace switchpalace::ui
