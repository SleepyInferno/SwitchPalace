/*
    SwitchPalace - Custom switch_wrapper.c

    This replaces the default borealis switch_wrapper.c to comply with
    TRUST-03: No hardcoded external network calls on app launch.

    The default borealis wrapper calls socketInitialize() and nifmInitialize()
    at startup. We skip those entirely -- network services are only initialized
    when the user explicitly requests a network operation (Phase 3+).
*/

#include <stdio.h>
#include <switch.h>

void userAppInit()
{
    appletLockExit();

    romfsInit();
    plInitialize(PlServiceType_User);
    setsysInitialize();
    setInitialize();
    psmInitialize();
    lblInitialize();

    /* TRUST-03: No socketInitialize(), no nifmInitialize() at startup.
       Network services will be initialized on-demand when the user
       explicitly triggers a network install (Phase 3+). */
}

void userAppExit()
{
    lblExit();
    psmExit();
    setExit();
    setsysExit();
    plExit();
    romfsExit();

    appletUnlockExit();
}
