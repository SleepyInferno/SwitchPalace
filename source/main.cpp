#ifdef __SWITCH__
#include <switch.h>
#endif

#include <stdexcept>
#include <borealis.hpp>
#include "nx/applet.hpp"
#include "nx/content_mgmt.hpp"
#include "ui/file_browser.hpp"
#include "ui/install_view.hpp"
#include "ui/summary_view.hpp"

#ifdef __SWITCH__
// 64MB heap: safe for both applet mode (hbloader provides ~128MB but 64MB leaves
// headroom for hbloader overhead + system reserves) and application mode.
// NCA streaming is chunk-based so we do not need the full 128MB ceiling.
extern "C" {
    u32 __nx_applet_type = AppletType_Default;
    u32 __nx_heap_size = 0x4000000; // 64MB
}
#endif

namespace {
    bool g_isAppletMode = false;
}

bool switchpalace_isAppletMode()
{
    return g_isAppletMode;
}

#ifdef __SWITCH__
// Write a log line to sdmc:/switchpalace_debug.log.
// Called before borealis init so crashes during GPU/service init leave a trace.
static void debugLog(const char* msg)
{
    FILE* f = fopen("sdmc:/switchpalace_debug.log", "a");
    if (f) { fprintf(f, "%s\n", msg); fclose(f); }
}
#else
static void debugLog(const char*) {}
#endif

int main(int argc, char* argv[])
{
    // Detect applet vs application mode
    // Call appletGetAppletType() to check execution context
#ifdef __SWITCH__
    AppletType appletType = appletGetAppletType();
    (void)appletType; // Used via isAppletMode() helper
#endif
    g_isAppletMode = switchpalace::nx::isAppletMode();

    debugLog("main: entered");
    debugLog(g_isAppletMode ? "mode: APPLET" : "mode: APPLICATION");

    brls::Logger::setLogLevel(brls::LogLevel::LOG_DEBUG);
    brls::Logger::debug("SwitchPalace starting in {} mode",
                        switchpalace::nx::getAppletModeString());

    // Initialize borealis
    // TRUST-03: No network service initialization at startup.
    // Network is only initialized on-demand when user triggers network install.
    // TRUST-03 verified: no network service init calls in this file.
    // Wrap in try/catch: borealis calls fatal() which throws std::logic_error on
    // init failure (e.g. GPU context failure). Without this, the exception is
    // unhandled, causing std::terminate() -> abort() -> error 2168-0001.
    debugLog("borealis: calling Application::init");
    try
    {
        if (!brls::Application::init())
        {
            debugLog("borealis: Application::init returned false");
            brls::Logger::error("Failed to initialize borealis application");
            return EXIT_FAILURE;
        }
        debugLog("borealis: Application::init OK, calling createWindow");
        brls::Application::createWindow("SwitchPalace");
        debugLog("borealis: createWindow OK");
    }
    catch (const std::exception& e)
    {
        debugLog("borealis: caught exception during init");
        brls::Logger::error("Borealis init exception: {}", e.what());
        return EXIT_FAILURE;
    }

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
