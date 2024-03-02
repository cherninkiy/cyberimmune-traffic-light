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
// ICrossMode Server
#include <traffic_light/ICrossMode.idl.h>
#include <traffic_light/CrossMode.cdl.h>
#include <traffic_light/CrossChecker.edl.h>
// IGpioLights Client
#include <traffic_light/IGpioLights.idl.h>
// IEventLog Client
#include "eventlog_proxy.h"

#include "color_defs.h"

static const char EntityName[] = "CrossChecker";
static const char ChannelName[] = "crossmode_channel";

#define DEFAILT_CROSS_MODE 0x00000A0A

typedef struct GpioLightsProxy {
    // TODO: Exclude if proxy releases all resources
    NkKosTransport transport;
    // TODO: Exclude if proxy releases all resources
    Handle channel;
    // IGpioLights client proxy
    IGpioLights_proxy proxy;
} GpioLightsProxy;

void GpioLightsProxy_Init(GpioLightsProxy *client,
                          const char *channelName,
                          const char *endpointName);

nk_err_t GpioLightsProxy_SetCrossLights(GpioLightsProxy *client,
                                        nk_uint32_t crossMode,
                                        const nk_char_t *wayMode1,
                                        const nk_char_t *wayMode2,
                                        nk_uint32_t *result
                                        );

// ICrossMode implementing type
typedef struct ICrossModeImpl {
    // Base interface of object
    struct ICrossMode base;
    // Current mode
    rtl_uint32_t mode;
    // LightsGPIO
    GpioLightsProxy gpioProxy;
    // Diagnostics
    EventLogProxy logProxy;
} ICrossModeImpl;

// ICrossMod.SetCrossMode method implementation
static nk_err_t SetCrossMode_impl(struct ICrossMode *self,
                                  const struct ICrossMode_SetCrossMode_req *req,
                                  const struct nk_arena *req_arena,
                                  struct ICrossMode_SetCrossMode_res *res,
                                  struct nk_arena *res_arena) {

    ICrossModeImpl *impl = (ICrossModeImpl *)self;

    const char *curMode1 = GetColorName(impl->mode & 0xFF);
    const char *curMode2 = GetColorName((impl->mode >> 8) & 0xFF);

    nk_uint32_t crossMode = req->mode;
    const char *wayMode1 = GetColorName(crossMode & 0xFF);
    const char *wayMode2 = GetColorName((crossMode >> 8) & 0xFF);
    // fprintf(stderr, "%-13s [DEBUG] Request SetCrossMode: req={\"mode\": 0x%08x, \"lights\": [\"%s\", \"%s\"]} state={\"mode\": 0x%08x, \"lights\": [\"%s\", \"%s\"]}\n",
    //                 EntityName, crossMode, wayMode1, wayMode2, impl->mode, curMode1, curMode2);
    fprintf(stderr, "%-13s [DEBUG] Request SetCrossMode: "
                    "req=[\"%s\", \"%s\"] state=[\"%s\", \"%s\"]\n",
                    EntityName, wayMode1, wayMode2, curMode1, curMode2);

    nk_uint32_t result;
    nk_err_t err = GpioLightsProxy_SetCrossLights(&impl->gpioProxy, crossMode, wayMode1, wayMode2, &result);
    if (NK_EOK != err) {
        fprintf(stderr, "\x1B[31m%-13s [ERROR] SetCrossLights failed: "
                        "req={\"wayMode1\": \"%s\", \"wayMode2\"=\"%s\"}, "
                        "err={code: %d, \"message\": \"%s\"}\x1B[0m\n",
                        EntityName, wayMode1, wayMode2, err, GetErrMessage(err));
    } else {
        impl->mode = result;
    }

    // char msgBuffer[IEventLog_MaxTextLength];
    // snprintf(msgBuffer, IEventLog_MaxTextLength, "{\"mode\": 0x%08x}", impl->mode);
    // LogEvent(&impl->logProxy, 2, EntityName, msgBuffer);

    // // Set result
    // res->result = impl->mode;

    return NK_EOK;
}

// ICrossMode object constructor
static struct ICrossMode *CreateICrossModeImpl(rtl_uint32_t mode)
{
    // Table of implementations of ICrossMode interface methods
    static const struct ICrossMode_ops ops = {
        .SetCrossMode = SetCrossMode_impl
    };

    // Interface implementing object
    static struct ICrossModeImpl impl = {
        .base = {&ops}
    };

    impl.mode = mode;

    GpioLightsProxy_Init(&impl.gpioProxy, "gpiolights_channel", "lightsGpio.gpioLights");

    EventLogProxy_Init(&impl.logProxy);

    const char *wayMode1 = GetColorName(impl.mode & 0xFF);
    const char *wayMode2 = GetColorName((impl.mode >> 8) & 0xFF);
    fprintf(stderr, "%-13s [DEBUG] Entity initialized: state={\"mode\": 0x%08x, \"lights\": [\"%s\", \"%s\"]}\n",
                    EntityName, impl.mode, wayMode1, wayMode2);

    LogEvent(&impl.logProxy, 0, EntityName, "\"Hello I'm CrossChecker!\"");

    return &impl.base;
}

// CrossChecker entity entry point
int main(void)
{
    ICrossMode* impl = CreateICrossModeImpl(DEFAILT_CROSS_MODE);

    // Request transport structures
    CrossChecker_entity_req req;
    char reqBuffer[CrossChecker_entity_req_arena_size];
    struct nk_arena reqArena = NK_ARENA_INITIALIZER(reqBuffer, reqBuffer + CrossChecker_entity_req_arena_size);

    // Response transport structures
    CrossChecker_entity_res res;
    char resBuffer[CrossChecker_entity_res_arena_size];
    struct nk_arena resArena = NK_ARENA_INITIALIZER(resBuffer, resBuffer + CrossChecker_entity_res_arena_size);

    // Component dispatcher
    CrossMode_component component;
    CrossMode_component_init(&component, impl);

    // Entity dispatcher
    CrossChecker_entity entity;
    CrossChecker_entity_init(&entity, &component);

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
        }

        // Dispatch request
        err = CrossChecker_entity_dispatch(&entity, &req.base_, &reqArena, &res.base_, &resArena);
        if (NK_EOK != err) {
            fprintf(stderr, "%-13s [ERROR] CrossChecker_entity_dispatch: err=%d\n", EntityName, err);
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

void GpioLightsProxy_Init(GpioLightsProxy *client,
                          const char *channelName,
                          const char *endpointName)
{
    // Get the IPC handle of the connection
    client->channel = ServiceLocatorConnect(channelName);
    nk_assert(client->channel != INVALID_HANDLE);

    // Initialize IPC transport for interaction with the entities
    NkKosTransport_Init(&client->transport, client->channel, NK_NULL, 0);

    // Get Runtime Interface ID (RIID) for interface traffic_light.Mode.mode.
    nk_iid_t riid = ServiceLocatorGetRiid(client->channel, endpointName);
    nk_assert(riid != INVALID_RIID);

    // Initialize proxy object by specifying transport (&transport)
    // and lights gpio interface identifier (riid). Each method of the
    // proxy object will be implemented by sending a request to the lights gpio.
    IGpioLights_proxy_init(&client->proxy, &client->transport.base, riid);
}

nk_err_t GpioLightsProxy_SetCrossLights(GpioLightsProxy *client,
                                        nk_uint32_t crossMode,
                                        const nk_char_t *wayMode1,
                                        const nk_char_t *wayMode2,
                                        nk_uint32_t *result
                                        ) {
    // Request's transport structures
    IGpioLights_SetCrossLights_req req;
    char reqBuffer[IGpioLights_req_arena_size];
    nk_arena reqArena = NK_ARENA_INITIALIZER(reqBuffer, reqBuffer + IGpioLights_req_arena_size);

    // Response's transport structures
    IGpioLights_SetCrossLights_res res;
    char resBuffer[IGpioLights_res_arena_size];
    nk_arena resArena = NK_ARENA_INITIALIZER(resBuffer, resBuffer + IGpioLights_res_arena_size);

    // Set requested mode
    req.lights.crossMode = crossMode;
    NkKosCopyStringToArena(&reqArena, &req.lights.wayMode1, wayMode1);
    NkKosCopyStringToArena(&reqArena, &req.lights.wayMode2, wayMode2);

    // Send request to LigthsGPIO
    nk_err_t err = IGpioLights_SetCrossLights(&client->proxy.base, &req, &reqArena, &res, &resArena);
    if (NK_EOK != err) {
        // Forward error
        return err;
    }

    // // Translate response
    // nk_size_t size = 0;
    // const nk_char_t *resColor = nk_arena_get(nk_char_t, &resArena, &res.result, &size);

    // // Validate response
    // nk_assert(size > 0);                    // Result value is not empty
    // nk_assert(nk_strcmp(color, resColor) == 0); // Result value equals requested mode

    // // Write result
    // nk_size_t len = nk_strnlen(resColor, IGpioLights_MaxLength);
    // nk_strncpy(result, resColor, len);

    return NK_EOK;
}