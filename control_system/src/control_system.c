#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

// Files required for transport initialization
#include <coresrv/nk/transport-kos.h>
#include <coresrv/sl/sl_api.h>
#include <kos/thread.h>

#define NK_USE_UNQUALIFIED_NAMES

// ICrossMode Server
#include <traffic_light/ICrossMode.idl.h>
#include <traffic_light/CrossMode.cdl.h>
#include <traffic_light/ControlSystem.edl.h>

// ICrossMode Client
#include "crossmode_proxy.h"
// IEventLog Client
#include "eventlog_proxy.h"

#include "color_defs.h"

static const char EntityName[] = "ControlSystem";
static const char CommunicationChannel[] = "communication_channel";


// ICrossMode implementing type
typedef struct IControlModeImpl {
    // Base interface of object
    struct ICrossMode base;
    // CrossChecker.CrossMode
    CrossModeProxy crossCheckerProxy;
    // Diagnostics
    EventLogProxy logProxy;
} IControlModeImpl;


// ICrossMod.SetCrossMode method implementation
static nk_err_t SetControlMode_impl(struct ICrossMode *self,
                                    const struct ICrossMode_SetCrossMode_req *req,
                                    const struct nk_arena *req_arena,
                                    struct ICrossMode_SetCrossMode_res *res,
                                    struct nk_arena *res_arena)
{
    //IControlModeImpl *impl = (IControlModeImpl *)self;

    const char *direction1Color = GetColorName(req->mode & 0xFF);
    const char *direction2Color = GetColorName((req->mode >> 8) & 0xFF);
    fprintf(stderr, "%-13s [DEBUG] Request SetCrossMode: req=[\"%s\", \"%s\"]\n",
                    EntityName, direction1Color, direction2Color);

    //nk_char_t result[ILightsMode_MaxLength];

    // Forward request to CrossChecker

    // char msgBuffer[IEventLog_MaxTextLength];
    // snprintf(msgBuffer, IEventLog_MaxTextLength, "{\"mode\": 0x%08x}", impl->mode);
    // LogEvent(&impl->logProxy, 2, EntityName, msgBuffer);

    // Set result
    // res->result = impl->mode;

    return NK_EOK;
}

// ICrossMode object constructor
static struct ICrossMode *CreateIControlModeImpl()
{
    // Table of implementations of ICrossMode interface methods
    static const struct ICrossMode_ops ops = {
        .SetCrossMode = SetControlMode_impl
    };

    // Interface implementing object
    static struct IControlModeImpl impl = {
        .base = {&ops}
    };

    CrossModeProxy_Init(&impl.crossCheckerProxy, "crossControl_channel", "crossChecker.crossMode");

    EventLogProxy_Init(&impl.logProxy);
    LogEvent(&impl.logProxy, 0, EntityName, "\"Hello I'm ControlSystem!\"");

    // const char *gpio1Color = GetColorName(impl.mode & 0xFF);
    // const char *gpio2Color = GetColorName((impl.mode >> 8) & 0xFF);
    // fprintf(stderr, "%-13s [DEBUG] Entity initialized: state={\"mode\": 0x%08x, \"lights\": [\"%s\", \"%s\"]}\n",
    //                 EntityName, impl.mode, gpio1Color, gpio2Color);

    return &impl.base;
}



#define MODES_NUM 27

int main(int argc, const char *argv[])
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
    CrossModeProxy_Init(&crossModeProxy, "crossControl_channel", "crossChecker.crossMode");

    KosThreadSleep(1000);

    fprintf(stderr, "%-13s [DEBUG] Normal mode testing!\n", "CrossModeProxy");

    /* Test loop. */
    for (i = 0; i < MODES_NUM; i++)
    {
        fprintf(stderr, "request = 0x%08x\n", tl_modes[i]);

        nk_err_t rcResult = CrossModeProxy_SetCrossMode(&crossModeProxy, tl_modes[i]);
        if (rcOk == rcResult) {
            fprintf(stdout, "result = 0x%08x\n", (int)rcResult);
        } else {
            fprintf(stderr, "Failed to call traffic_light.Mode.Mode()\n");
        }
    }

    // fprintf(stderr, "%-13s [DEBUG] Normal mode testing!\n", "CrossModeProxy");

    // for (i = 0; i < 10; ++i) {
    //     for (j = 0; j < 10; ++j) {
    //         unsigned int mode = tl_combinations[i] | (tl_combinations[j] << 8);

    //         switch(mode) {
    //             case 0x0203: continue; // ["blink-yellow", "blink-yellow"] -> ["red-yellow", "yellow"]
    //             case 0x0302: continue; // ["blink-yellow", "blink-yellow"] -> ["yellow", "red-yellow"]
    //             case 0x0A0A: continue; // ["blink-yellow", "blink-yellow"] -> ["blink-yellow", "blink-yellow"]
    //         }

    //         fprintf(stderr, "request = 0x%08x\n", mode);

    //         nk_err_t rcResult = CrossModeProxy_SetCrossMode(&crossModeProxy, mode);
    //         if (rcOk == rcResult) {
    //             fprintf(stdout, "result = 0x%08x\n", rcResult);
    //         } else {
    //             fprintf(stderr, "Failed to call traffic_light.Mode.Mode()\n");
    //         }
    //     }
    // }

    return EXIT_SUCCESS;
}






// ControlSystem entity entry point
int main_ipc(int argc, const char *argv[])
{
    ICrossMode* impl = CreateIControlModeImpl();

    // Request transport structures
    ControlSystem_entity_req req;
    char reqBuffer[ControlSystem_entity_req_arena_size];
    struct nk_arena reqArena = NK_ARENA_INITIALIZER(reqBuffer, reqBuffer + ControlSystem_entity_req_arena_size);

    // Response transport structures
    ControlSystem_entity_res res;
    char resBuffer[ControlSystem_entity_res_arena_size];
    struct nk_arena resArena = NK_ARENA_INITIALIZER(resBuffer, resBuffer + ControlSystem_entity_res_arena_size);

    // Component dispatcher
    ControlSystem_component component;
    ControlSystem_component_init(&component, impl);

    // Entity dispatcher
    ControlSystem_entity entity;
    ControlSystem_entity_init(&entity, &component);

    // Get IPC service handle
    ServiceId iid;
    Handle handle = ServiceLocatorRegister(CommunicationChannel, NULL, 0, &iid);
    assert(handle != INVALID_HANDLE);

    // Initialize transport
    NkKosTransport transport;
    NkKosTransport_Init(&transport, handle, NK_NULL, 0);

    // Dispatch loop
    do {
        // Flush request/response buffers
        nk_req_reset(&req);
        nk_arena_reset(&reqArena);
        nk_arena_reset(&resArena);

        nk_err_t err = NK_EOK;

        // Wait for request
        err = nk_transport_recv(&transport.base, &req.base_, &reqArena);
        if (NK_EOK != err) {
            fprintf(stderr, "%-13s [ERROR] nk_transport_recv: err=%d\n", EntityName, err);
        }

        // Dispatch request
        err = ControlSystem_entity_dispatch(&entity, &req.base_, &reqArena, &res.base_, &resArena);
        if (NK_EOK != err) {
            fprintf(stderr, "%-13s [ERROR] ControlSystem_entity_dispatch: err=%d\n", EntityName, err);
            //continue;
        }

        // Send response
        err = nk_transport_reply(&transport.base, &res.base_, &resArena);
        if (NK_EOK != err) {
            fprintf(stderr, "%-13s [ERROR] nk_transport_reply: err=%d\n", EntityName, err);
            //continue;
        }
    } while (true);

    return EXIT_SUCCESS;
}