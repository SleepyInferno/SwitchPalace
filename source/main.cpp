#ifdef __SWITCH__
#include <switch.h>
#endif

#include <borealis.hpp>
#include "nx/applet.hpp"
#include "nx/content_mgmt.hpp"
#include "ui/file_browser.hpp"
#include "ui/install_view.hpp"
#include "ui/summary_view.hpp"

#ifdef __SWITCH__
// 128MB heap: safe ceiling for both applet mode (hbloader limit ~128MB)
// and application mode. Sufficient for UI + NCA streaming buffers.
extern "C" {
    u32 __nx_applet_type = AppletType_Default;
    u32 __nx_heap_size = 0x8000000; // 128MB
}
#endif

namespace {
    bool g_isAppletMode = false;
}

bool switchpalace_isAppletMode()
{
    return g_isAppletMode;
}

int main(int argc, char* argv[])
{
    // Detect applet vs application mode
    // Call appletGetAppletType() to check execution context
#ifdef __SWITCH__
    AppletType appletType = appletGetAppletType();
    (void)appletType; // Used via isAppletMode() helper
#endif
    g_isAppletMode = switchpalace::nx::isAppletMode();

    brls::Logger::setLogLevel(brls::LogLevel::LOG_DEBUG);
    brls::Logger::debug("SwitchPalace starting in {} mode",
                        switchpalace::nx::getAppletModeString());

    // Initialize borealis
    // TRUST-03: No network service initialization at startup.
    // Network is only initialized on-demand when user triggers network install.
    // TRUST-03 verified: no network service init calls in this file.
    if (!brls::Application::init())
    {
        brls::Logger::error("Failed to initialize borealis application");
        return EXIT_FAILURE;
    }

    brls::Application::createWindow("SwitchPalace");

    // Clean up stale placeholders from prior crashed runs (INST-06 safety net)
    // Done after borealis init so NCM is accessed with display/services ready.
    {
        switchpalace::nx::ContentManager cm;
        cm.cleanupStalePlaceholders();
    }

    // Navigation flow:
    //   App starts -> FileBrowserView (root)
    //   User selects files + presses Install -> push InstallProgressView
    //   Install completes -> push SummaryView (replace InstallProgressView)
    //   User presses B on SummaryView -> pop back to FileBrowserView
    auto* fileBrowser = new switchpalace::ui::FileBrowserView();
    brls::Application::pushActivity(new brls::Activity(fileBrowser));

    // Main loop
    while (brls::Application::mainLoop())
        ;

    return EXIT_SUCCESS;
}
