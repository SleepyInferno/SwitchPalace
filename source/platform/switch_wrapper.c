/*
    SwitchPalace - Custom switch_wrapper.c

    This replaces the default borealis switch_wrapper.c to comply with
    TRUST-03: No hardcoded external network socket setup on app launch.

    The default borealis wrapper calls socketInitialize() at startup.
    We exclude that -- BSD socket infrastructure is only set up when the user
    explicitly requests a network install (Phase 3+).

    nifmInitialize() IS included here because borealis's SwitchPlatform directly
    calls nifm status-check functions (wireless/ethernet state) for the status bar.
    nifm only opens a read-only IPC handle; it does not create any sockets or
    make outbound connections, so it does not violate TRUST-03.
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
    /* nifmInitialize is required: borealis SwitchPlatform calls nifmIsWirelessCommunicationEnabled
       and related functions during status-bar rendering. Without this, the IPC call uses a zero
       (uninitialized) service handle which causes an abort (error 2168-0001).
       NOTE: nifmInitialize only opens a status-check handle -- no sockets, no connections.
       TRUST-03 compliance is maintained: socketInitialize() is still NOT called at startup. */
    nifmInitialize(NifmServiceType_User);
    lblInitialize();

    /* TRUST-03: socketInitialize() and all BSD socket setup remain excluded.
       Network sockets are only initialized on-demand when the user explicitly
       triggers a network install (Phase 3+). */
}

void userAppExit()
{
    lblExit();
    nifmExit();
    psmExit();
    setExit();
    setsysExit();
    plExit();
    romfsExit();

    appletUnlockExit();
}
