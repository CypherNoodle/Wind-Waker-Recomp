// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>

#include <switch.h>

#include "gxruntime/event_clock.h"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    consoleInit(NULL);

    // Exercise a real GXRuntime subsystem in the linked executable. This is
    // intentionally not a game or renderer stub: milestone 1 proves that the
    // portable runtime compiles and links for AArch64 before platform backends
    // are introduced.
    DolEventClock clock;
    dol_event_clock_init(&clock);

    printf("Wind Waker Recomp - Nintendo Switch bring-up\n");
    printf("GXRuntime linked; event clock starts at %llu\n",
           (unsigned long long)dol_event_clock_now(&clock));
    printf("Press + to exit.\n");

    PadState pad;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);

    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus)
            break;
        consoleUpdate(NULL);
    }

    consoleExit(NULL);
    return 0;
}
