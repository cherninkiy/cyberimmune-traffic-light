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
// ICrossController Server
#include <traffic_light/ICrossControl.idl.h>
#include <traffic_light/CrossControl.cdl.h>
#include <traffic_light/CrossController.edl.h>
// ILightsGpio Client
#include <traffic_light/ILightsGpio.idl.h>
// IEventLog Client
#include "eventlog_proxy.h"

#include "color_defs.h"

static const char EntityName[] = "CrossController";
static const char ChannelName[] = "crosscontrol_channel";

#define DEFAILT_CROSS_MODE    0x00000A0A
#define BLINK_YELLOW_DURATION 1
#define BLINK_GREEN_DURATION  3
#define RED_YELLOW_DURATION   3

typedef struct LightsGpioProxy {
    // TODO: Exclude if proxy releases all resources
    NkKosTransport transport;
    // TODO: Exclude if proxy releases all resources
    Handle channel;
    // ILightsGpio client proxy
    ILightsGpio_proxy proxy;
} LightsGpioProxy;

void LightsGpioProxy_Init(LightsGpioProxy *client,
                          const char *channelName,
                          const char *endpointName);

nk_err_t LightsGpioProxy_SetCrossLights(LightsGpioProxy *client,
                                        nk_uint32_t crossMode,
                                        const nk_char_t *wayMode1,
                                        const nk_char_t *wayMode2,
                                        nk_uint32_t *result
                                        );

// ICrossControl implementing type
typedef struct ICrossControlImpl {
    // Base interface of object
    struct ICrossControl base;
    // Current mode
    nk_uint32_t mode;
    // 'green' duration for way1 if 'regulated' behavior
    nk_uint32_t way1Duration;
    // 'green' duration for way2 if 'regulated' behavior
    nk_uint32_t way2Duration;
    // LightsGPIO
    LightsGpioProxy gpioProxy;
    // Diagnostics
    EventLogProxy logProxy;

    nk_uint32_t blinkYellowDuration;
    nk_uint32_t blinkGreenDuration;
    nk_uint32_t redYellowDuration;
} ICrossControlImpl;

nk_uint32_t GetNextRegulatedMode(ICrossControlImpl *impl, nk_uint32_t *duration) {
    switch (impl->mode) {
    case ILightsGpio_Regulated1Green2Red:
        *duration = impl->blinkGreenDuration;
        return ILightsGpio_Regulated1BlinkGreen2Red;
    case ILightsGpio_Regulated1BlinkGreen2Red:
        *duration = impl->redYellowDuration;
        return ILightsGpio_Regulated1Yellow2RedYellow;
    case ILightsGpio_Regulated1Yellow2RedYellow:
        *duration = impl->way2Duration;
        return ILightsGpio_Regulated1Red2Green;
    case ILightsGpio_Regulated1Red2Green:
        *duration = impl->blinkGreenDuration;
        return ILightsGpio_Regulated1Red2BlinkGreen;
    case ILightsGpio_Regulated1Red2BlinkGreen:
        *duration = impl->redYellowDuration;
        return ILightsGpio_Regulated1RedYellow2Yellow;
    case ILightsGpio_Regulated1RedYellow2Yellow:
        *duration = impl->way1Duration;
        return ILightsGpio_Regulated1Green2Red;
    case ILightsGpio_Unregulated1BlinkYellow2BlinkYellow:
        *duration = impl->way1Duration;
        return ILightsGpio_Regulated1Green2Red;
    default:
        *duration = impl->blinkYellowDuration;
        return ILightsGpio_Unregulated1BlinkYellow2BlinkYellow;
    }
}

// ICrossControl.SetCrossSchedule method implementation
static nk_err_t SetCrossSchedule_impl(struct ICrossControl *self,
                                      const struct ICrossControl_SetCrossSchedule_req *req,
                                      const struct nk_arena *req_arena,
                                      struct ICrossControl_SetCrossSchedule_res *res,
                                      struct nk_arena *res_arena) {

    ICrossControlImpl *impl = (ICrossControlImpl *)self;

    nk_size_t size = 0;

    // Get behavior
    const nk_char_t *behavior = nk_arena_get(nk_char_t, req_arena, &req->schedule.behavior, &size);
    nk_assert(size > 0);

    // Get way1 mode
    const nk_char_t *way1Mode = nk_arena_get(nk_char_t, req_arena, &req->schedule.way1Mode, &size);
    nk_assert(size > 0);

    // Get way2 mode
    const nk_char_t *way2Mode = nk_arena_get(nk_char_t, req_arena, &req->schedule.way2Mode, &size);
    nk_assert(size > 0);

    nk_uint32_t way1Duration = req->schedule.way1Duration;
    nk_uint32_t way2Duration = req->schedule.way2Duration;

    nk_uint32_t result = 0;
    nk_uint32_t duration = 0;

    nk_err_t err = NK_EOK;
    if (nk_strcmp(behavior, "unregulated") == 0) {
        duration = impl->blinkYellowDuration;
        err = LightsGpioProxy_SetCrossLights(&impl->gpioProxy, ILightsGpio_Unregulated1BlinkYellow2BlinkYellow,
                                             "blink-yellow", "blink-yellow", &result);
    }
    
    if (nk_strcmp(behavior, "regulated") == 0) {
        impl->way1Duration = way1Duration;
        impl->way2Duration = way2Duration;

        nk_uint32_t nextCrossMode = GetNextRegulatedMode(impl->mode, &duration);

        const nk_char_t *nextWay1Mode = GetColorName(nextCrossMode);
        const nk_char_t *nextWay2Mode = GetColorName((nextCrossMode >> 8) & 0xFF);
        err = LightsGpioProxy_SetCrossLights(&impl->gpioProxy, nextCrossMode,
                                             nextWay1Mode, nextWay2Mode, &result);
    }

    if (nk_strcmp(behavior, "manual") == 0) {
        nk_uint32_t crossMode = (GetColorMode(way2Mode) << 8) | GetColorMode(way1Mode);
        err = LightsGpioProxy_SetCrossLights(&impl->gpioProxy, crossMode, way1Mode, way2Mode, &result);
    }

    if (err != NK_EOK) {
        fprintf(stderr, "\x1B[31m%-16s [ERROR] SetCrossLights failed: \n"
                        "request={\"mode\": \"%s\", \"color1\": \"%s\", \"color2\": \"%s\"}\n"
                        "  error={code: %d, \"message\": \"%s\"}\x1B[0m\n",
                        EntityName, behavior, way1Mode, way2Mode, err, GetErrMessage(err));
    }

    impl->mode = result;

    res->result = duration;


    // const char *curMode1 = GetColorName(impl->mode & 0xFF);
    // const char *curMode2 = GetColorName((impl->mode >> 8) & 0xFF);

    // nk_uint32_t crossMode = req->mode;
    // const char *wayMode1 = GetColorName(crossMode & 0xFF);
    // const char *wayMode2 = GetColorName((crossMode >> 8) & 0xFF);
    // // fprintf(stderr, "%-16s [DEBUG] Request SetCrossController: req={\"mode\": 0x%08x, \"lights\": [\"%s\", \"%s\"]} state={\"mode\": 0x%08x, \"lights\": [\"%s\", \"%s\"]}\n",
    // //                 EntityName, crossMode, wayMode1, wayMode2, impl->mode, curMode1, curMode2);
    // fprintf(stderr, "%-16s [DEBUG] Request SetCrossController: "
    //                 "req=[\"%s\", \"%s\"] state=[\"%s\", \"%s\"]\n",
    //                 EntityName, wayMode1, wayMode2, curMode1, curMode2);

    // nk_uint32_t result;
    // nk_err_t err = LightsGpioProxy_SetCrossLights(&impl->gpioProxy, crossMode, wayMode1, wayMode2, &result);
    // if (NK_EOK != err) {
    //     fprintf(stderr, "\x1B[31m%-16s [ERROR] SetCrossLights failed: "
    //                     "req={\"wayMode1\": \"%s\", \"wayMode2\"=\"%s\"}, "
    //                     "err={code: %d, \"message\": \"%s\"}\x1B[0m\n",
    //                     EntityName, wayMode1, wayMode2, err, GetErrMessage(err));
    // } else {
    //     impl->mode = result;
    // }

    // // char msgBuffer[IEventLog_MaxTextLength];
    // // snprintf(msgBuffer, IEventLog_MaxTextLength, "{\"mode\": 0x%08x}", impl->mode);
    // // LogEvent(&impl->logProxy, 2, EntityName, msgBuffer);

    // // // Set result
    // // res->result = impl->mode;

    return NK_EOK;
}

// ICrossControl object constructor
static struct ICrossControl *CreateICrossControlImpl() {

    // Table of implementations of ICrossController interface methods
    static const struct ICrossControl_ops ops = {
        .SetCrossSchedule = SetCrossSchedule_impl
    };

    // Interface implementing object
    static struct ICrossControlImpl impl = {
        .base = {&ops},
        .mode = DEFAILT_CROSS_MODE,
        .blinkYellowDuration = BLINK_YELLOW_DURATION,
        .blinkGreenDuration = BLINK_GREEN_DURATION,
        .redYellowDuration = RED_YELLOW_DURATION
    };

    LightsGpioProxy_Init(&impl.gpioProxy, "gpiolights_channel", "lightsGpio.lightsGpio");

    EventLogProxy_Init(&impl.logProxy);

    const char *wayMode1 = GetColorName(impl.mode & 0xFF);
    const char *wayMode2 = GetColorName((impl.mode >> 8) & 0xFF);
    fprintf(stderr, "%-16s [DEBUG] Entity initialized: state={\"mode\": 0x%08x, \"lights\": [\"%s\", \"%s\"]}\n",
                    EntityName, impl.mode, wayMode1, wayMode2);

    LogEvent(&impl.logProxy, 0, EntityName, "\"Hello I'm CrossController!\"");

    return &impl.base;
}

// CrossController entity entry point
int main(void) {

    ICrossControl* impl = CreateICrossControlImpl();

    // Request transport structures
    CrossController_entity_req req;
    char reqBuffer[CrossController_entity_req_arena_size];
    struct nk_arena reqArena = NK_ARENA_INITIALIZER(reqBuffer, reqBuffer + CrossController_entity_req_arena_size);

    // Response transport structures
    CrossController_entity_res res;
    char resBuffer[CrossController_entity_res_arena_size];
    struct nk_arena resArena = NK_ARENA_INITIALIZER(resBuffer, resBuffer + CrossController_entity_res_arena_size);

    // Component dispatcher
    CrossControl_component component;
    CrossControl_component_init(&component, impl);

    // Entity dispatcher
    CrossController_entity entity;
    CrossController_entity_init(&entity, &component);

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
            fprintf(stderr, "%-16s [ERROR] nk_transport_recv: err=%d\n", EntityName, err);
        }

        // Dispatch request
        err = CrossController_entity_dispatch(&entity, &req.base_, &reqArena, &res.base_, &resArena);
        if (NK_EOK != err) {
            fprintf(stderr, "%-16s [ERROR] CrossController_entity_dispatch: err=%d\n", EntityName, err);
            //continue;
        }

        // Send response
        err = nk_transport_reply(&transport.base, &res.base_, &resArena);
        if (NK_EOK != err) {
            fprintf(stderr, "%-16s [ERROR] nk_transport_reply: err=%d\n", EntityName, err);
            //continue;
        }
    } while (true);

    return EXIT_SUCCESS;
}

void LightsGpioProxy_Init(LightsGpioProxy *client,
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
    ILightsGpio_proxy_init(&client->proxy, &client->transport.base, riid);
}

nk_err_t LightsGpioProxy_SetCrossLights(LightsGpioProxy *client,
                                        nk_uint32_t crossMode,
                                        const nk_char_t *wayMode1,
                                        const nk_char_t *wayMode2,
                                        nk_uint32_t *result
                                        ) {
    // Request's transport structures
    ILightsGpio_SetCrossLights_req req;
    char reqBuffer[ILightsGpio_req_arena_size];
    nk_arena reqArena = NK_ARENA_INITIALIZER(reqBuffer, reqBuffer + ILightsGpio_req_arena_size);

    // Response's transport structures
    ILightsGpio_SetCrossLights_res res;
    char resBuffer[ILightsGpio_res_arena_size];
    nk_arena resArena = NK_ARENA_INITIALIZER(resBuffer, resBuffer + ILightsGpio_res_arena_size);

    // Set requested mode
    req.lights.crossMode = crossMode;
    NkKosCopyStringToArena(&reqArena, &req.lights.wayMode1, wayMode1);
    NkKosCopyStringToArena(&reqArena, &req.lights.wayMode2, wayMode2);

    // Send request to LigthsGPIO
    nk_err_t err = ILightsGpio_SetCrossLights(&client->proxy.base, &req, &reqArena, &res, &resArena);
    if (NK_EOK != err) {
        // Forward error
        return err;
    }

    *result = res.result;

    

    // // Translate response
    // nk_size_t size = 0;
    // const nk_char_t *resColor = nk_arena_get(nk_char_t, &resArena, &res.result, &size);

    // // Validate response
    // nk_assert(size > 0);                    // Result value is not empty
    // nk_assert(nk_strcmp(color, resColor) == 0); // Result value equals requested mode

    // // Write result
    // nk_size_t len = nk_strnlen(resColor, ILightsGpio_MaxLength);
    // nk_strncpy(result, resColor, len);

    return NK_EOK;
}