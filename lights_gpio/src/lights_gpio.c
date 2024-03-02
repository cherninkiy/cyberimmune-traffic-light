#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

// Files required for transport initialization
#include <coresrv/nk/transport-kos.h>
#include <coresrv/sl/sl_api.h>
#include <nk/arena.h>
#include <nk/types.h>

#define NK_USE_UNQUALIFIED_NAMES

// ILightsGpio Server
#include <traffic_light/ILightsGpio.idl.h>
#include <traffic_light/LightsGpio.cdl.h>
#include <traffic_light/LightsGPIO.edl.h>

// IEventLog Client
#include "eventlog_proxy.h"

#include "color_defs.h"

static const nk_char_t EntityName[] = "LightsGPIO";
static const nk_char_t ChannelName[] = "gpiolights_channel";
static const nk_uint32_t DefaultLightsMode = ILightsGpio_Direction1Blink
                                     | ILightsGpio_Direction1Yellow
                                     | ILightsGpio_Direction2Blink
                                     | ILightsGpio_Direction2Yellow;

// ILightsGpio implementing type
typedef struct ILightsGpioImpl {
    // Base interface of object
    struct ILightsGpio base;
    // Cross lights binary mode
    nk_uint32_t mode;
    // Diagnostics
    EventLogProxy logProxy;
} ILightsGpioImpl;

// ILightsGpio.SetCrossLights method implementation
static nk_err_t SetCrossLights_impl(struct ILightsGpio *self,
                                    const struct ILightsGpio_SetCrossLights_req *req,
                                    const struct nk_arena *req_arena,
                                    struct ILightsGpio_SetCrossLights_res *res,
                                    struct nk_arena *res_arena) {

    ILightsGpioImpl *impl = (ILightsGpioImpl *)self;

    // Current mode
    nk_uint32_t curCrossMode = impl->mode;
    const nk_char_t *curWayMode1 = GetConsoleColor(curCrossMode & 0xFF);
    const nk_char_t *curWayMode2 = GetConsoleColor((curCrossMode >> 8) & 0xFF);

    // Requested mode
    nk_uint32_t reqCrossMode = req->lights.crossMode;
    const nk_char_t *reqWayMode1 = GetConsoleColor(reqCrossMode & 0xFF);
    const nk_char_t *reqWayMode2 = GetConsoleColor((reqCrossMode >> 8) & 0xFF);

    fprintf(stderr, "%-13s [DEBUG] Request SetCrossLights: \n"
                    "current={\"mode\": 0x%08x, \"lights\": [\"%s\", \"%s\"]}\n"
                    "request={\"mode\": 0x%08x, \"lights\": [\"%s\", \"%s\"]}\n",
                    EntityName, curCrossMode, curWayMode1, curWayMode2,
                                reqCrossMode, reqWayMode1, reqWayMode2);

    // Some GPIO-related code
    // ......................

    // Store new mode
    impl->mode = req->lights.crossMode;

    // Set result
    res->result = impl->mode;

    return NK_EOK;
}

// ILightsGpio object constructor
static struct ILightsGpio *CreateILightsGpioImpl() {

    // Table of implementations of ILightsGpio interface methods
    static const struct ILightsGpio_ops ops = {
        .SetCrossLights = SetCrossLights_impl
    };

    // Interface implementing object
    static struct ILightsGpioImpl impl = {
        .base = {&ops},
        .mode = DefaultLightsMode
    };

    EventLogProxy_Init(&impl.logProxy);

    const nk_char_t *wayMode1 = GetConsoleColor(impl.mode & 0xFF);
    const nk_char_t *wayMode2 = GetConsoleColor((impl.mode >> 8) & 0xFF);
    fprintf(stderr, "%-13s [DEBUG] Entity initialized: "
                    "state={\"mode\": 0x%08x, \"lights\": [\"%s\", \"%s\"]}\n",
                    EntityName, impl.mode, wayMode1, wayMode2);

    LogEvent(&impl.logProxy, 0, EntityName, "\"Hello I'm LightsGPIO!\"");

    return &impl.base;
}

// LightsGPIO entry point
int main(void) {

    // Request transport structures
    LightsGPIO_entity_req req;
    char reqBuffer[LightsGPIO_entity_req_arena_size];
    struct nk_arena reqArena = NK_ARENA_INITIALIZER(reqBuffer, reqBuffer + LightsGPIO_entity_req_arena_size);

    // Response transport structures
    LightsGPIO_entity_res res;
    char resBuffer[LightsGPIO_entity_res_arena_size];
    struct nk_arena resArena = NK_ARENA_INITIALIZER(resBuffer, resBuffer + LightsGPIO_entity_res_arena_size);

    // Component dispatcher
    LightsGpio_component component;
    LightsGpio_component_init(&component, CreateILightsGpioImpl());

    // Entity dispatcher
    LightsGPIO_entity entity;
    LightsGPIO_entity_init(&entity, &component);

    // Get IPC service handle
    ServiceId iid;
    Handle handle = ServiceLocatorRegister(ChannelName, NULL, 0, &iid);
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
            continue;
        }

        // Dispath request
        err = LightsGPIO_entity_dispatch(&entity, &req.base_, &reqArena, &res.base_, &resArena);
        if (NK_EOK != err) {
            fprintf(stderr, "%-13s [ERROR] LightsGPIO_entity_dispatch: err=%d\n", EntityName, err);
            continue;
        }

        // Send response
        err = nk_transport_reply(&transport.base, &res.base_, &resArena);
        if (NK_EOK != err) {
            fprintf(stderr, "%-13s [ERROR] nk_transport_reply: err=%d\n", EntityName, err);
            continue;
        }

    } while (true);

    return EXIT_SUCCESS;
}