#include "ui/file_browser.hpp"
#include <cstdio>
#include "ui/install_view.hpp"
#include "ui/dialogs.hpp"
#include "nx/applet.hpp"
#include "nx/content_mgmt.hpp"
#include "install/nsp.hpp"
#include "install/xci.hpp"
#include "util/file_util.hpp"

#include <algorithm>

namespace switchpalace::ui {

static void fbDebugLog(const char* msg)
{
#ifdef __SWITCH__
    FILE* f = fopen("sdmc:/switchpalace_debug.log", "a");
    if (f) { fprintf(f, "%s\n", msg); fclose(f); }
#else
    (void)msg;
#endif
}

FileBrowserView::FileBrowserView()
{
    fbDebugLog("FileBrowserView: ctor start");
    setAxis(brls::Axis::COLUMN);
    setWidth(brls::View::AUTO);
    setHeight(brls::View::AUTO);
    setGrow(1.0f);
    setBackgroundColor(nvgRGB(0x1A, 0x1A, 0x1A));

    m_isAppletMode = switchpalace::nx::isAppletMode();
    fbDebugLog("FileBrowserView: base layout set");

    // Title bar
    auto* titleBar = new brls::Box(brls::Axis::ROW);
    titleBar->setHeight(64.0f);
    titleBar->setWidth(brls::View::AUTO);
    titleBar->setShrink(0.0f);
    titleBar->setPadding(16.0f, 32.0f, 16.0f, 32.0f);
    titleBar->setJustifyContent(brls::JustifyContent::SPACE_BETWEEN);
    titleBar->setAlignItems(brls::AlignItems::CENTER);

    auto* titleLabel = new brls::Label();
    titleLabel->setText("SwitchPalace");
    titleLabel->setFontSize(28.0f);
    titleLabel->setTextColor(nvgRGB(0xFF, 0xFF, 0xFF));
    titleBar->addView(titleLabel);

    addView(titleBar);
    fbDebugLog("FileBrowserView: titleBar added");

    // APPLET badge (top-right in title bar)
    setupAppletBadge();
    fbDebugLog("FileBrowserView: appletMode check");
    if (m_isAppletMode)
    {
        fbDebugLog("FileBrowserView: badge: new Box");
        auto* badge = new brls::Box();
        fbDebugLog("FileBrowserView: badge: setBackgroundColor");
        badge->setBackgroundColor(nvgRGB(0xFF, 0xD6, 0x0A));
        fbDebugLog("FileBrowserView: badge: setCornerRadius");
        badge->setCornerRadius(4.0f);
        fbDebugLog("FileBrowserView: badge: setPadding");
        badge->setPadding(4.0f, 8.0f, 4.0f, 8.0f);
        fbDebugLog("FileBrowserView: badge: setAlignItems");
        badge->setAlignItems(brls::AlignItems::CENTER);
        fbDebugLog("FileBrowserView: badge: setJustifyContent");
        badge->setJustifyContent(brls::JustifyContent::CENTER);

        fbDebugLog("FileBrowserView: badge: new Label");
        auto* badgeLabel = new brls::Label();
        fbDebugLog("FileBrowserView: badge: setText");
        badgeLabel->setText("APPLET");
        fbDebugLog("FileBrowserView: badge: setFontSize");
        badgeLabel->setFontSize(18.0f);
        fbDebugLog("FileBrowserView: badge: setTextColor");
        badgeLabel->setTextColor(nvgRGB(0x1A, 0x1A, 0x1A));
        fbDebugLog("FileBrowserView: badge: addView(badgeLabel)");
        badge->addView(badgeLabel);
        fbDebugLog("FileBrowserView: badge: titleBar->addView(badge)");
        titleBar->addView(badge);
        fbDebugLog("FileBrowserView: badge: done");
    }

    // Scrolling file list
    fbDebugLog("FileBrowserView: new ScrollingFrame");
    m_scrollFrame = new brls::ScrollingFrame();
    m_scrollFrame->setScrollingBehavior(brls::ScrollingBehavior::CENTERED);
    fbDebugLog("FileBrowserView: scrollFrame: setGrow");
    m_scrollFrame->setGrow(1.0f);

    fbDebugLog("FileBrowserView: new listContainer");
    m_listContainer = new brls::Box(brls::Axis::COLUMN);
    fbDebugLog("FileBrowserView: listContainer: setWidth");
    m_listContainer->setWidth(brls::View::AUTO);
    fbDebugLog("FileBrowserView: listContainer: setPadding");
    m_listContainer->setPadding(8.0f, 32.0f, 8.0f, 32.0f);
    fbDebugLog("FileBrowserView: listContainer: setBackgroundColor");
    m_listContainer->setBackgroundColor(nvgRGB(0x24, 0x24, 0x24));
    fbDebugLog("FileBrowserView: listContainer: setCornerRadius");
    m_listContainer->setCornerRadius(8.0f);

    fbDebugLog("FileBrowserView: scrollFrame->addView(listContainer)");
    m_scrollFrame->addView(m_listContainer);
    fbDebugLog("FileBrowserView: addView(scrollFrame)");
    addView(m_scrollFrame);
    fbDebugLog("FileBrowserView: scrollFrame added");

    // Action bar at bottom
    setupActionBar();
    addView(m_actionBar);
    fbDebugLog("FileBrowserView: actionBar added");

    // Populate file list
    fbDebugLog("FileBrowserView: calling refreshFileList");
    refreshFileList();
    fbDebugLog("FileBrowserView: refreshFileList done");

    // Register L/R bumpers for destination toggle
    registerAction("", brls::ControllerButton::BUTTON_LB, [this](brls::View* view) {
        toggleDestination();
        return true;
    }, true);
    registerAction("", brls::ControllerButton::BUTTON_RB, [this](brls::View* view) {
        toggleDestination();
        return true;
    }, true);

    // Register B to deselect all or exit
    registerAction("", brls::ControllerButton::BUTTON_B, [this](brls::View* view) {
        bool anySelected = false;
        for (bool s : m_selected)
        {
            if (s) { anySelected = true; break; }
        }
        if (anySelected)
        {
            std::fill(m_selected.begin(), m_selected.end(), false);
            buildFileList();
            updateInstallButton();
        }
        else
        {
            brls::Application::quit();
        }
        return true;
    });
    fbDebugLog("FileBrowserView: ctor complete");
}

void FileBrowserView::setupAppletBadge()
{
    // Badge is created inline in constructor if applet mode
}

void FileBrowserView::setupActionBar()
{
    m_actionBar = new brls::Box(brls::Axis::ROW);
    m_actionBar->setHeight(72.0f);
    m_actionBar->setWidth(brls::View::AUTO);
    m_actionBar->setShrink(0.0f);
    m_actionBar->setBackgroundColor(nvgRGB(0x2E, 0x2E, 0x2E));
    m_actionBar->setPadding(12.0f, 32.0f, 12.0f, 32.0f);
    m_actionBar->setJustifyContent(brls::JustifyContent::SPACE_BETWEEN);
    m_actionBar->setAlignItems(brls::AlignItems::CENTER);

    // Destination toggle container
    auto* destToggle = new brls::Box(brls::Axis::ROW);
    destToggle->setAlignItems(brls::AlignItems::CENTER);
    destToggle->setCornerRadius(6.0f);
    destToggle->setBackgroundColor(nvgRGB(0x3A, 0x3A, 0x3A));

    // SD Card option
    m_sdOption = new brls::Box();
    m_sdOption->setPadding(8.0f, 16.0f, 8.0f, 16.0f);
    m_sdOption->setCornerRadius(6.0f);
    m_sdOption->setBackgroundColor(nvgRGB(0x3D, 0x9C, 0xF0)); // Active by default
    m_sdOption->setAlignItems(brls::AlignItems::CENTER);
    m_sdOption->setJustifyContent(brls::JustifyContent::CENTER);

    m_sdLabel = new brls::Label();
    m_sdLabel->setText("SD Card");
    m_sdLabel->setFontSize(18.0f);
    m_sdLabel->setTextColor(nvgRGB(0xFF, 0xFF, 0xFF));
    m_sdOption->addView(m_sdLabel);

    // NAND option
    m_nandOption = new brls::Box();
    m_nandOption->setPadding(8.0f, 16.0f, 8.0f, 16.0f);
    m_nandOption->setCornerRadius(6.0f);
    m_nandOption->setAlignItems(brls::AlignItems::CENTER);
    m_nandOption->setJustifyContent(brls::JustifyContent::CENTER);

    m_nandLabel = new brls::Label();
    m_nandLabel->setText("NAND");
    m_nandLabel->setFontSize(18.0f);
    m_nandLabel->setTextColor(nvgRGB(0xAD, 0xAD, 0xAD));
    m_nandOption->addView(m_nandLabel);

    destToggle->addView(m_sdOption);
    destToggle->addView(m_nandOption);
    m_actionBar->addView(destToggle);

    // Install Selected button
    m_installButton = new brls::Box();
    m_installButton->setBackgroundColor(nvgRGB(0x3D, 0x9C, 0xF0));
    m_installButton->setCornerRadius(6.0f);
    m_installButton->setHeight(48.0f);
    m_installButton->setPadding(0.0f, 24.0f, 0.0f, 24.0f);
    m_installButton->setAlignItems(brls::AlignItems::CENTER);
    m_installButton->setJustifyContent(brls::JustifyContent::CENTER);
    m_installButton->setFocusable(true);

    m_installLabel = new brls::Label();
    m_installLabel->setText("Install Selected");
    m_installLabel->setFontSize(20.0f);
    m_installLabel->setTextColor(nvgRGB(0xFF, 0xFF, 0xFF));
    m_installButton->addView(m_installLabel);

    m_installButton->registerClickAction([this](brls::View* view) {
        onInstallPressed();
        return true;
    });

    m_actionBar->addView(m_installButton);

    updateInstallButton();
}

void FileBrowserView::toggleDestination()
{
    if (m_destination == install::InstallDestination::SdCard)
    {
        m_destination = install::InstallDestination::NAND;
        m_sdOption->setBackgroundColor(nvgRGBA(0, 0, 0, 0));
        m_sdLabel->setTextColor(nvgRGB(0xAD, 0xAD, 0xAD));
        m_nandOption->setBackgroundColor(nvgRGB(0x3D, 0x9C, 0xF0));
        m_nandLabel->setTextColor(nvgRGB(0xFF, 0xFF, 0xFF));
    }
    else
    {
        m_destination = install::InstallDestination::SdCard;
        m_sdOption->setBackgroundColor(nvgRGB(0x3D, 0x9C, 0xF0));
        m_sdLabel->setTextColor(nvgRGB(0xFF, 0xFF, 0xFF));
        m_nandOption->setBackgroundColor(nvgRGBA(0, 0, 0, 0));
        m_nandLabel->setTextColor(nvgRGB(0xAD, 0xAD, 0xAD));
    }
}

void FileBrowserView::refreshFileList()
{
    m_allFiles = switchpalace::util::scanForInstallableFiles();
    m_selected.resize(m_allFiles.size(), false);
    buildFileList();
}

void FileBrowserView::buildFileList()
{
    m_listContainer->clearViews();

    if (m_allFiles.empty())
    {
        setupEmptyState();
        return;
    }

    for (size_t i = 0; i < m_allFiles.size(); i++)
    {
        const auto& file = m_allFiles[i];
        bool isCompressed = switchpalace::util::isCompressedFormat(file.extension);
        bool isDisabled = m_isAppletMode && isCompressed;

        auto* row = new brls::Box(brls::Axis::ROW);
        row->setWidth(brls::View::AUTO);
        row->setHeight(56.0f);
        row->setPadding(16.0f, 16.0f, 16.0f, 16.0f);
        row->setAlignItems(brls::AlignItems::CENTER);

        if (m_selected[i])
        {
            row->setBackgroundColor(nvgRGBA(0x3D, 0x9C, 0xF0, 20)); // 8% opacity Royal Sapphire
            row->setBorderColor(nvgRGB(0x3D, 0x9C, 0xF0));
            row->setBorderThickness(3.0f);
        }

        // Checkbox indicator
        auto* checkbox = new brls::Box();
        checkbox->setWidth(20.0f);
        checkbox->setHeight(20.0f);
        checkbox->setShrink(0.0f);
        checkbox->setCornerRadius(4.0f);
        checkbox->setMargins(0.0f, 12.0f, 0.0f, 0.0f);

        if (m_selected[i])
            checkbox->setBackgroundColor(nvgRGB(0x3D, 0x9C, 0xF0));
        else
        {
            checkbox->setBorderColor(nvgRGB(0x3A, 0x3A, 0x3A));
            checkbox->setBorderThickness(2.0f);
        }

        row->addView(checkbox);

        // File info column
        auto* infoCol = new brls::Box(brls::Axis::COLUMN);
        infoCol->setGrow(1.0f);
        infoCol->setMargins(0.0f, 0.0f, 0.0f, 0.0f);

        // Filename
        auto* nameLabel = new brls::Label();
        nameLabel->setText(file.filename);
        nameLabel->setFontSize(20.0f);

        if (isDisabled)
            nameLabel->setTextColor(nvgRGB(0x5A, 0x5A, 0x5A));
        else
            nameLabel->setTextColor(nvgRGB(0xFF, 0xFF, 0xFF));

        infoCol->addView(nameLabel);

        // Size + format badge row
        auto* metaRow = new brls::Box(brls::Axis::ROW);
        metaRow->setAlignItems(brls::AlignItems::CENTER);

        auto* sizeLabel = new brls::Label();
        std::string ext = file.extension;
        std::transform(ext.begin(), ext.end(), ext.begin(), ::toupper);
        sizeLabel->setText(switchpalace::util::formatFileSize(file.fileSize) + " - " + ext);
        sizeLabel->setFontSize(18.0f);
        sizeLabel->setTextColor(nvgRGB(0xAD, 0xAD, 0xAD));
        metaRow->addView(sizeLabel);

        if (isDisabled)
        {
            auto* disabledLabel = new brls::Label();
            disabledLabel->setText(" - Requires app mode");
            disabledLabel->setFontSize(18.0f);
            disabledLabel->setTextColor(nvgRGB(0x5A, 0x5A, 0x5A));
            metaRow->addView(disabledLabel);
        }

        infoCol->addView(metaRow);
        row->addView(infoCol);

        if (!isDisabled)
        {
            row->setFocusable(true);
            size_t idx = i;
            row->registerClickAction([this, idx](brls::View* view) {
                m_selected[idx] = !m_selected[idx];
                buildFileList();
                updateInstallButton();
                return true;
            });
        }
        else
        {
            row->setFocusable(false);
        }

        m_listContainer->addView(row);
    }
}

void FileBrowserView::setupEmptyState()
{
    auto* emptyBox = new brls::Box(brls::Axis::COLUMN);
    emptyBox->setGrow(1.0f);
    emptyBox->setJustifyContent(brls::JustifyContent::CENTER);
    emptyBox->setAlignItems(brls::AlignItems::CENTER);
    emptyBox->setPadding(64.0f, 32.0f, 64.0f, 32.0f);

    auto* heading = new brls::Label();
    heading->setText("No installable files found");
    heading->setFontSize(22.0f);
    heading->setTextColor(nvgRGB(0xFF, 0xFF, 0xFF));
    heading->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    emptyBox->addView(heading);

    auto* body = new brls::Label();
    body->setText("Place .nsp, .xci, .nsz, or .xcz files on your SD card and reopen SwitchPalace.");
    body->setFontSize(20.0f);
    body->setTextColor(nvgRGB(0xAD, 0xAD, 0xAD));
    body->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    body->setMargins(8.0f, 0.0f, 0.0f, 0.0f);
    emptyBox->addView(body);

    m_listContainer->addView(emptyBox);
}

void FileBrowserView::updateInstallButton()
{
    bool anySelected = false;
    for (bool s : m_selected)
    {
        if (s) { anySelected = true; break; }
    }

    if (anySelected)
    {
        m_installButton->setBackgroundColor(nvgRGB(0x3D, 0x9C, 0xF0));
        m_installLabel->setTextColor(nvgRGB(0xFF, 0xFF, 0xFF));
    }
    else
    {
        m_installButton->setBackgroundColor(nvgRGB(0x3A, 0x3A, 0x3A));
        m_installLabel->setTextColor(nvgRGB(0x5A, 0x5A, 0x5A));
    }
}

std::vector<switchpalace::util::FileEntry> FileBrowserView::getSelectedFiles() const
{
    std::vector<switchpalace::util::FileEntry> result;
    for (size_t i = 0; i < m_allFiles.size(); i++)
    {
        if (m_selected[i])
            result.push_back(m_allFiles[i]);
    }
    return result;
}

install::InstallDestination FileBrowserView::getSelectedDestination() const
{
    return m_destination;
}

void FileBrowserView::onInstallPressed()
{
    auto selectedFiles = getSelectedFiles();
    if (selectedFiles.empty())
        return;

    // Check already-installed for each file
    for (const auto& file : selectedFiles)
    {
        auto format = install::detectFormat(file.filename);
        std::unique_ptr<install::Install> installer;

        if (format == install::FileFormat::NSP || format == install::FileFormat::NSZ)
            installer = std::make_unique<install::NSPInstall>(file.path);
        else if (format == install::FileFormat::XCI || format == install::FileFormat::XCZ)
            installer = std::make_unique<install::XCIInstall>(file.path);
        else
            continue;

        if (installer->prepare() && installer->checkAlreadyInstalled(m_destination))
        {
            showConflictDialog(file.filename);
            return;
        }
    }

    // Check free space
    switchpalace::nx::ContentManager cm;
    cm.openStorage(m_destination);
    int64_t freeSpace = cm.getFreeSpace();
    uint64_t totalNeeded = 0;

    for (const auto& file : selectedFiles)
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
            totalNeeded += installer->getTotalSize();
    }

    if (freeSpace >= 0 && static_cast<uint64_t>(freeSpace) < totalNeeded)
    {
        std::string destName = (m_destination == install::InstallDestination::SdCard) ? "SD Card" : "NAND";
        uint64_t deficit = totalNeeded - static_cast<uint64_t>(freeSpace);
        showInsufficientSpaceDialog(destName, switchpalace::util::formatFileSize(deficit));
        return;
    }

    // All checks passed: navigate to install progress view
    fbDebugLog("onInstallPressed: creating InstallProgressView");
    auto* installView = new InstallProgressView(selectedFiles, m_destination);
    fbDebugLog("onInstallPressed: pushActivity InstallProgressView");
    brls::Application::pushActivity(new brls::Activity(installView));
    fbDebugLog("onInstallPressed: pushActivity returned");
}

} // namespace switchpalace::ui
