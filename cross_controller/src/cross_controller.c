#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

// Files required for transport initialization
#include <coresrv/thread/thread_api.h>
#include <coresrv/nk/transport-kos.h>
#include <coresrv/sl/sl_api.h>
#include <rtl/retcode.h>
#include <kos/thread.h>
#include <kos/mutex.h>
#include <nk/arena.h>
#include <nk/types.h>

#define NK_USE_UNQUALIFIED_NAMES
// ICrossController Server
#include <traffic_light/ICrossControl.idl.h>
#include <traffic_light/CrossControl.cdl.h>
#include <traffic_light/CrossController.edl.h>

// ILightsGpio Client
#include "lights_gpio_proxy.h"

// IEventLog Client
#include "eventlog_proxy.h"

#include "color_defs.h"

static const char EntityName[] = "CrossController";
static const char ChannelName[] = "crosscontrol_channel";

#define DEFAILT_CROSS_MODE     0x00000A0A
#define DEFAULT_DURATION_FREQ  3000
#define DEFAULT_DURATION       0
#define BLINK_GREEN_DURATION   3
#define RED_YELLOW_DURATION    3


// ICrossControl implementing type
typedef struct ICrossControlImpl {
    // Base interface of object
    struct ICrossControl base;

    Tid timerTid;
    bool timerStop;
    KosMutex timerMutex;
    nk_uint32_t timerFreq;

    // Current behavior
    nk_uint32_t curBehavior;
    // Current mode
    nk_uint32_t curCrossMode;
    // Current duration
    nk_uint32_t curDuration;

    // Current behavior
    nk_uint32_t reqBehavior;
    // Requested mode
    nk_uint32_t reqCrossMode;
    // Requested dir1 'green' duration
    nk_uint32_t dir1Duration;
    // Requested dir2 'green' duration
    nk_uint32_t dir2Duration;

    // ["blink-green", "red"] duration (constant)
    nk_uint32_t blinkGreenDuration;
    // ["red-yellow", "yellow"] duration (constant)
    nk_uint32_t redYellowDuration;

    // LightsGPIO client
    LightsGpioProxy gpioProxy;

    // Diagnostics
    EventLogProxy logProxy;

} ICrossControlImpl;


nk_uint32_t GetNextRegulatedMode(nk_uint32_t curMode);
nk_uint32_t GetNextDuration(nk_uint32_t curMode, ICrossControlImpl *impl);
int RegulatedBehaviorRoutine(void *context);


// Regulated mode FSM
nk_uint32_t GetNextRegulatedMode(nk_uint32_t curMode) {
    switch (curMode) {
    case ILightsGpio_Regulated1Green2Red:
        return ILightsGpio_Regulated1BlinkGreen2Red;
    case ILightsGpio_Regulated1BlinkGreen2Red:
        return ILightsGpio_Regulated1Yellow2RedYellow;
    case ILightsGpio_Regulated1Yellow2RedYellow:
        return ILightsGpio_Regulated1Red2Green;
    case ILightsGpio_Regulated1Red2Green:
        return ILightsGpio_Regulated1Red2BlinkGreen;
    case ILightsGpio_Regulated1Red2BlinkGreen:
        return ILightsGpio_Regulated1RedYellow2Yellow;
    case ILightsGpio_Regulated1RedYellow2Yellow:
        return ILightsGpio_Regulated1Green2Red;
    case ILightsGpio_Unregulated1BlinkYellow2BlinkYellow:
        return ILightsGpio_Regulated1Green2Red;
    default:
        return 0;
    }
}

nk_uint32_t GetNextDuration(nk_uint32_t curMode, ICrossControlImpl *impl) {
    switch (curMode) {
    case ILightsGpio_Regulated1Green2Red:
        return impl->dir1Duration;
    case ILightsGpio_Regulated1BlinkGreen2Red:
        return impl->blinkGreenDuration;
    case ILightsGpio_Regulated1Yellow2RedYellow:
        return impl->redYellowDuration;
    case ILightsGpio_Regulated1Red2Green:
        return impl->dir2Duration;
    case ILightsGpio_Regulated1Red2BlinkGreen:
        return impl->blinkGreenDuration;
    case ILightsGpio_Regulated1RedYellow2Yellow:
        return impl->redYellowDuration;
    default:
        return DEFAULT_DURATION;
    }
}

// Regulated mode routine
int RegulatedBehaviorRoutine(void *context) {
    ICrossControlImpl *impl = (ICrossControlImpl *)context;

    nk_bool_t timerStop = true;
    nk_uint32_t timerFreq = DEFAULT_DURATION_FREQ;

    // Timer Loop
    do {
        if (rcOk == KosMutexTryLock(&impl->timerMutex)) {

            const nk_char_t *curDir1Mode = GetColorName(impl->curCrossMode);
            const nk_char_t *curDir2Mode = GetColorName((impl->curCrossMode >> 8) & 0xFF);

            nk_err_t err = NK_EOK;
            nk_uint32_t resultMode = 0;
            switch(impl->reqBehavior) {
                case ICrossControl_BehaviorManual: {
                    nk_uint32_t reqCrossMode = impl->reqCrossMode;
                    const nk_char_t *reqDir1Mode = GetColorName(reqCrossMode);
                    const nk_char_t *reqDir2Mode = GetColorName((reqCrossMode >> 8) & 0xFF);

                    fprintf(stderr, "%-16s [DEBUG] Manual behavior: [\"%s\", \"%s\"] -> [\"%s\", \"%s\"]\n",
                                    EntityName, curDir1Mode, curDir2Mode, reqDir1Mode, reqDir2Mode);

                    err = LightsGpioProxy_SetCrossLights(&impl->gpioProxy, reqCrossMode, &resultMode);

                    if (NK_EOK == err) {
                        impl->curBehavior = impl->reqBehavior;
                        impl->curCrossMode = resultMode;
                        impl->curDuration = 0;
                    } else {
                        fprintf(stderr, "\x1B[31m%-16s [ERROR] SetCrossLights failed:\n"
                                        "req={\"mode\": \"0x%08d\", \"color1\": \"%s\", \"color2\"=\"%s\"},\n"
                                        "err={\"code\": %d, \"message\": \"%s\"}\x1B[0m\n",
                                        EntityName, reqCrossMode, reqDir1Mode, reqDir2Mode, err, GetErrMessage(err));

                        impl->reqBehavior = impl->curBehavior;
                        // impl->reqCrossMode = impl->curCrossMode;
                    }
                    break;
                }
                case ICrossControl_BehaviorUnregulated: {
                    nk_uint32_t reqCrossMode = ILightsGpio_Unregulated1BlinkYellow2BlinkYellow;
                    const nk_char_t *reqDir1Mode = GetColorName(reqCrossMode);
                    const nk_char_t *reqDir2Mode = GetColorName((reqCrossMode >> 8) & 0xFF);

                    fprintf(stderr, "%-16s [DEBUG] Unregulated behavior: [\"%s\", \"%s\"] -> [\"%s\", \"%s\"]\n",
                                    EntityName, curDir1Mode, curDir2Mode, reqDir1Mode, reqDir2Mode);

                    err = LightsGpioProxy_SetCrossLights(&impl->gpioProxy, reqCrossMode, &resultMode);

                    if (NK_EOK == err) {
                        impl->curBehavior = impl->reqBehavior;
                        impl->curCrossMode = resultMode;
                        impl->curDuration = 0;
                    } else {
                        fprintf(stderr, "\x1B[31m%-16s [ERROR] SetCrossLights failed:\n"
                                        "req={\"mode\": \"0x%08d\", \"color1\": \"%s\", \"color2\"=\"%s\"},\n"
                                        "err={\"code\": %d, \"message\": \"%s\"}\x1B[0m\n",
                                        EntityName, reqCrossMode, reqDir1Mode, reqDir2Mode, err, GetErrMessage(err));

                        impl->reqBehavior = impl->curBehavior;
                        // impl->reqCrossMode = impl->curCrossMode;
                    }
                    break;
                }
                case ICrossControl_BehaviorRegulated: {
                    if (impl->curDuration > 0) {
                        // Decrement current duration
                        --impl->curDuration;
                        fprintf(stderr, "%-16s [DEBUG] Regulated Behavior: duration=%d [\"%s\", \"%s\"]\n",
                                        EntityName, impl->curDuration, curDir1Mode, curDir2Mode);
                    } else {
                        // Try to switch to next mode
                        nk_uint32_t nextCrossMode = GetNextRegulatedMode(impl->curCrossMode);

                        const nk_char_t *nextDir1Mode = GetColorName(nextCrossMode);
                        const nk_char_t *nextDir2Mode = GetColorName((nextCrossMode >> 8) & 0xFF);

                        fprintf(stderr, "%-16s [DEBUG] Regulated Behavior: [\"%s\", \"%s\"] -> [\"%s\", \"%s\"]\n",
                                        EntityName, curDir1Mode, curDir2Mode, nextDir1Mode, nextDir2Mode);

                        err = LightsGpioProxy_SetCrossLights(&impl->gpioProxy, nextCrossMode, &resultMode);
                        if (NK_EOK == err) {
                            impl->curBehavior = impl->reqBehavior;
                            impl->curCrossMode = resultMode;
                            impl->curDuration = GetNextDuration(resultMode, impl);
                        } else {
                        fprintf(stderr, "\x1B[31m%-16s [ERROR] SetCrossLights failed:\n"
                                        "req={\"mode\": \"0x%08d\", \"color1\": \"%s\", \"color2\"=\"%s\"},\n"
                                        "err={\"code\": %d, \"message\": \"%s\"}\x1B[0m\n",
                                        EntityName, nextCrossMode, nextDir1Mode, nextDir2Mode, err, GetErrMessage(err));

                            impl->reqBehavior = impl->curBehavior;
                            // impl->reqCrossMode = impl->curCrossMode;
                        }
                    }
                }
            }

            // Synchronize timer params
            timerStop = impl->timerStop;
            timerFreq = impl->timerFreq;

            KosMutexUnlock(&impl->timerMutex);
        }

        // Sleep thread
        KosThreadSleep(timerFreq);

    } while (!timerStop);
    return NK_EOK;
}

// ICrossControl.SetCrossSchedule method implementation
static nk_err_t SetCrossSchedule_impl(struct ICrossControl *self,
                                      const struct ICrossControl_SetCrossSchedule_req *req,
                                      const struct nk_arena *req_arena,
                                      struct ICrossControl_SetCrossSchedule_res *res,
                                      struct nk_arena *res_arena) {

    ICrossControlImpl *impl = (ICrossControlImpl *)self;

    nk_uint32_t reqBehavior = req->schedule.behavior;
    nk_uint32_t reqCrossMode = req->schedule.crossMode;
    nk_uint32_t reqDir1Duration = req->schedule.dir1Duration;
    nk_uint32_t reqDir2Duration = req->schedule.dir2Duration;

    KosMutexLock(&impl->timerMutex);

    impl->reqBehavior = reqBehavior;
    impl->reqCrossMode = reqCrossMode;
    impl->dir1Duration = reqDir1Duration;
    impl->dir2Duration = reqDir2Duration;

    nk_uint32_t curBehavior = impl->curBehavior;
    nk_uint32_t curCrossMode = impl->curCrossMode;

    KosMutexUnlock(&impl->timerMutex);

    char msgBuffer[IEventLog_MaxTextLength];
    snprintf(msgBuffer, IEventLog_MaxTextLength, "{\"behavior\": %d, \"mode\": \"0x%08x\"}", curBehavior, curCrossMode);
    LogEvent(&impl->logProxy, 2, EntityName, msgBuffer);

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
        .timerStop = false,
        .timerFreq = DEFAULT_DURATION_FREQ,
        .curBehavior = ICrossControl_BehaviorUnregulated,
        .curCrossMode = DEFAILT_CROSS_MODE,
        .curDuration = DEFAULT_DURATION,
        .reqBehavior = ICrossControl_BehaviorUnregulated,
        .reqCrossMode = DEFAILT_CROSS_MODE,
        .dir1Duration = DEFAULT_DURATION,
        .dir2Duration = DEFAULT_DURATION,
        .blinkGreenDuration = BLINK_GREEN_DURATION,
        .redYellowDuration = RED_YELLOW_DURATION
    };

    // Duration timer mutex
    KosMutexInit(&impl.timerMutex);
    // Duration timer thread
    KosThreadCreate(&impl.timerTid, ThreadPriorityNormal, ThreadStackSizeDefault,
                    RegulatedBehaviorRoutine, &impl, 0);

    LightsGpioProxy_Init(&impl.gpioProxy);

    EventLogProxy_Init(&impl.logProxy);

    const char *dir1Mode = GetColorName(impl.curCrossMode & 0xFF);
    const char *dir2Mode = GetColorName((impl.curCrossMode >> 8) & 0xFF);
    fprintf(stderr, "%-16s [DEBUG] Entity initialized: state={\"mode\": 0x%08x, \"lights\": [\"%s\", \"%s\"]}\n",
                    EntityName, impl.curCrossMode, dir1Mode, dir2Mode);

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
