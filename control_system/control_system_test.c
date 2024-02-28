#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

// Files required for transport initialization
#include <coresrv/nk/transport-kos.h>
#include <coresrv/sl/sl_api.h>
#include <kos/thread.h>

#define NK_USE_UNQUALIFIED_NAMES

// ICrossMode Client
#include "crossmode_proxy.h"


#define MODES_NUM 27

int test(int argc, const char *argv[])
{
    int i, j;

    // INITIAL ["blink-yellow", "blink-yellow"]
    static const nk_uint32_t tl_modes[MODES_NUM] = {

        // 1. UNREGULATED ["blink-yellow", "blink-yellow"]
        ICrossMode_Direction1Blink | ICrossMode_Direction1Yellow | ICrossMode_Direction2Blink | ICrossMode_Direction2Yellow,

        // 3. Switch to REGULAR, priotiry to direction 1
        // ["blink-yellow", "blink-yellow"] -> ["red-yellow", "yellow"] -> ["green", "red"]
        ICrossMode_Direction1Red | ICrossMode_Direction1Yellow   | ICrossMode_Direction2Yellow,
        ICrossMode_Direction1Green                               | ICrossMode_Direction2Red,

        // 8. Full cycle on direction 1, direction 2 is "red"
        // "green" -> "blink-green" -> "yellow" -> "red-yellow" -> "green"
        ICrossMode_Direction1Blink | ICrossMode_Direction1Green  | ICrossMode_Direction2Red,
        ICrossMode_Direction1Yellow                              | ICrossMode_Direction2Red,
        ICrossMode_Direction1Red                                 | ICrossMode_Direction2Red,
        ICrossMode_Direction1Red | ICrossMode_Direction1Yellow   | ICrossMode_Direction2Red,
        ICrossMode_Direction1Green                               | ICrossMode_Direction2Red,

        // 11. Switch to UNREGULATED
        // ["blink-green", "red"] -> ["yellow", "red-yellow"] -> ["blink-yellow", "blink-yellow"]
        ICrossMode_Direction1Blink | ICrossMode_Direction1Green  | ICrossMode_Direction2Red,
        ICrossMode_Direction1Yellow                              | ICrossMode_Direction2Red | ICrossMode_Direction2Yellow,
        ICrossMode_Direction1Blink | ICrossMode_Direction1Yellow | ICrossMode_Direction2Blink | ICrossMode_Direction2Yellow,

        // 13. Switch to REGULAR, priotiry to direction 2
        // ["blink-yellow", "blink-yellow"] -> ["yellow", "red-yellow"] -> ["red", "green"]
        ICrossMode_Direction2Red | ICrossMode_Direction2Yellow   | ICrossMode_Direction1Yellow,
        ICrossMode_Direction2Green                               | ICrossMode_Direction1Red,

        // 18. Full cycle on direction 2, direction 1 is "red"
        // "green" -> "blink-green" -> "yellow" -> "red-yellow" -> "green"
        ICrossMode_Direction2Blink | ICrossMode_Direction2Green  | ICrossMode_Direction1Red,
        ICrossMode_Direction2Yellow                              | ICrossMode_Direction1Red,
        ICrossMode_Direction2Red                                 | ICrossMode_Direction1Red,
        ICrossMode_Direction2Red | ICrossMode_Direction2Yellow   | ICrossMode_Direction1Red,
        ICrossMode_Direction2Green                               | ICrossMode_Direction1Red,

        // 21. Switch to UNREGULATED
        // ["blink-green", "red"] -> ["yellow", "red-yellow"] -> ["blink-yellow", "blink-yellow"]
        ICrossMode_Direction2Blink | ICrossMode_Direction2Green  | ICrossMode_Direction1Red,
        ICrossMode_Direction2Yellow                              | ICrossMode_Direction1Red | ICrossMode_Direction1Yellow,
        ICrossMode_Direction2Blink | ICrossMode_Direction2Yellow | ICrossMode_Direction1Blink | ICrossMode_Direction1Yellow,

        // 27... Other
        ICrossMode_Direction1Red | ICrossMode_Direction2Red,
        ICrossMode_Direction1Red | ICrossMode_Direction2Red | ICrossMode_Direction2Yellow,
        ICrossMode_Direction1Red | ICrossMode_Direction2Green,
        ICrossMode_Direction1Red | ICrossMode_Direction2Blink | ICrossMode_Direction2Green,
        ICrossMode_Direction1Red | ICrossMode_Direction2Yellow,
        ICrossMode_Direction1Red | ICrossMode_Direction2Red,

        // ICrossMode_Direction1Red    + ICrossMode_Direction1Yellow + ICrossMode_Direction2Red,
        // ICrossMode_Direction1Green  + ICrossMode_Direction2Red,
        // ICrossMode_Direction1Yellow + ICrossMode_Direction2Red,
        // ICrossMode_Direction1Red    + ICrossMode_Direction2Yellow + ICrossMode_Direction2Red,
        // ICrossMode_Direction1Red    + ICrossMode_Direction2Green,
        // ICrossMode_Direction1Red    + ICrossMode_Direction2Yellow,
        // ICrossMode_Direction1Yellow + ICrossMode_Direction1Blink  + ICrossMode_Direction2Yellow + ICrossMode_Direction2Blink,
        // ICrossMode_Direction1Green  + ICrossMode_Direction2Green // <-- try to forbid this via security policies
    };

    static const nk_uint32_t tl_combinations[10] = {
    //    r     ry           ryg                rygb
         0x01, 0x01 | 0x02, 0x01 | 0x02 | 0x04, 0x01 | 0x02 | 0x04 | 0x08,
    //   y      yg           ygb                 g     gb           b
         0x02, 0x02 | 0x04, 0x02 | 0x04 | 0x08, 0x04, 0x04 | 0x08, 0x08 };

    CrossModeProxy crossModeProxy;
    CrossModeProxy_Init(&crossModeProxy, "crossControl_channel", "crossControl.crossMode");

    KosThreadSleep(1000);

    fprintf(stderr, "%-13s [DEBUG] Enter testing!\n", "CrossModeProxy");

    // /* Test loop. */
    // for (i = 0; i < MODES_NUM; i++)
    // {
    //     fprintf(stderr, "request = 0x%08x\n", tl_modes[i]);

    //     nk_err_t rcResult = CrossChecker_Client_RequestCrossMode(&client, tl_modes[i]);
    //     if (rcOk == rcResult) {
    //         fprintf(stdout, "result = 0x%08x\n", (int)client.res.result);
    //     } else {
    //         fprintf(stderr, "Failed to call traffic_light.Mode.Mode()\n");
    //     }
    // }

    for (i = 0; i < 10; ++i) {
        for (j = 0; j < 10; ++j) {
            unsigned int mode = tl_combinations[i] | (tl_combinations[j] << 8);

            switch(mode) {
                case 0x0203: continue; // ["blink-yellow", "blink-yellow"] -> ["red-yellow", "yellow"]
                case 0x0302: continue; // ["blink-yellow", "blink-yellow"] -> ["yellow", "red-yellow"]
                case 0x0A0A: continue; // ["blink-yellow", "blink-yellow"] -> ["blink-yellow", "blink-yellow"]
            }

            fprintf(stderr, "request = 0x%08x\n", mode);

            nk_err_t rcResult = CrossCheckerProxy_SetCrossMode(&client, mode);
            if (rcOk == rcResult) {
                fprintf(stdout, "result = 0x%08x\n", (int)client.res.result);
            } else {
                fprintf(stderr, "Failed to call traffic_light.Mode.Mode()\n");
            }
        }
    }

    return EXIT_SUCCESS;
}
