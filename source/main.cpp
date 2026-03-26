#ifdef __SWITCH__
#include <switch.h>
#endif

#include <borealis.hpp>
#include "nx/applet.hpp"

#ifdef __SWITCH__
// Heap size override for application mode: 1GB
// In applet mode the system grants a smaller heap automatically
extern "C" {
    u32 __nx_applet_type = AppletType_Default;
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
    if (!brls::Application::init("SwitchPalace"))
    {
        brls::Logger::error("Failed to initialize borealis application");
        return EXIT_FAILURE;
    }

    brls::Application::createWindow("SwitchPalace");

    // TODO (Plan 03): Create and push the file browser view
    // For now, push a placeholder activity
    brls::Application::pushActivity(new brls::Activity());

    // Main loop
    while (brls::Application::mainLoop())
        ;

    return EXIT_SUCCESS;
}
