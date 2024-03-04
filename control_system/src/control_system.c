#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

// Files required for transport initialization
#include <coresrv/nk/transport-kos.h>
#include <coresrv/sl/sl_api.h>
#include <kos/thread.h>
#include <nk/arena.h>
#include <nk/types.h>
#include <nk/errno.h>

#define NK_USE_UNQUALIFIED_NAMES

// ITrafficMode Client
#include <traffic_light/ITrafficMode.idl.h>
#include "response-parser.h"

// ICrossControl Client
#include <traffic_light/ICrossControl.idl.h>

// IEventLog Client
#include "eventlog_proxy.h"

#include "color_defs.h"
#include "response-parser.h"

static const char EntityName[] = "ControlSystem";

typedef struct TrafficModeProxy {
    // TODO: Exclude if proxy releases all resources
    NkKosTransport transport;
    // TODO: Exclude if proxy releases all resources
    Handle channel;

    ITrafficMode_proxy proxy;
} TrafficModeProxy;

typedef struct TrafficModeData {
    // 'unregulated' | 'regulated' | 'manual'
    nk_char_t mode[ITrafficMode_MaxModeLength];
    // 'green' duration for direction1 in 'regulated' mode
    nk_uint32_t duration1;
    // color for direction1 in 'manual' mode
    nk_char_t color1[ITrafficMode_MaxColorLength];
    // 'green' duration for direction2 in 'regulated' mode
    nk_uint32_t duration2;
    // color for direction2 in 'manual' mode
    nk_char_t color2[ITrafficMode_MaxColorLength];
} TrafficModeData;

int TrafficModeProxy_Init(TrafficModeProxy *client, const char *channelName, const char *interfaceName);

nk_err_t TrafficModeProxy_GetTrafficMode(TrafficModeProxy *client, TrafficModeData *trafficMode);

nk_err_t TrafficModeProxy_SendDiagnostics(TrafficModeProxy *client, sys_health_data *sysHealthData);


typedef struct CrossControlProxy {
    // TODO: Exclude if proxy releases all resources
    NkKosTransport transport;
    // TODO: Exclude if proxy releases all resources
    Handle channel;

    

    ICrossControl_proxy proxy;
} CrossControlProxy;


int CrossControlProxy_Init(CrossControlProxy *client, const char *channelName, const char *interfaceName);

nk_err_t CrossControlProxy_SetCrossSchedule(CrossControlProxy *client, TrafficModeData *schedule);

void SelfDiagnostic(EventLogProxy *client, sys_health_data *sysHealthData);

int main(void) {

    TrafficModeProxy trafficModeProxy;
    TrafficModeProxy_Init(&trafficModeProxy, "trafficmode_channel", "trafficMode.trafficMode");

    CrossControlProxy crossModeProxy;
    CrossControlProxy_Init(&crossModeProxy, "crosscontrol_channel", "crossController.crossControl");

    EventLogProxy eventLogProxy;
    EventLogProxy_Init(&eventLogProxy);
    LogEvent(&eventLogProxy, 0, EntityName, "\"Hello I'm ControlSystem!\"");

    TrafficModeData trafficMode;
    sys_health_data sysHealthData;
    do {
        SelfDiagnostic(&eventLogProxy, &sysHealthData);
        fprintf(stderr, "%-16s [DEBUG] Self Diagnostic Data: %d\n", EntityName, sysHealthData.controlSystem);

        TrafficModeProxy_GetTrafficMode(&trafficModeProxy, &trafficMode);
        // fprintf(stderr, "%-16s [DEBUG] Traffic Mode Data: "
        //                 "response={\"mode\": \"%s\", \"direction_1_duration\": \"%d\", \"direction_1_color\": \"%s\"\n"
        //                 "                            \"direction_2_duration\": \"%d\", \"direction_2_color\": \"%s\"}\n",
        //                 EntityName, trafficMode.mode, trafficMode.duration1, trafficMode.color1,
        //                 trafficMode.duration2, trafficMode.color2);

        nk_err_t err = NK_EOK;
        if (nk_strcmp(trafficMode.mode, "unregulated") == 0) {
            err = CrossControlProxy_SetCrossSchedule(&crossModeProxy, &trafficMode);
        }
        
        if (nk_strcmp(trafficMode.mode, "regulated") == 0) {
            err = CrossControlProxy_SetCrossSchedule(&crossModeProxy, &trafficMode);
        }

        if (nk_strcmp(trafficMode.mode, "manual") == 0) {
            err = CrossControlProxy_SetCrossSchedule(&crossModeProxy, &trafficMode);
        }

        if (err != NK_EOK) {
            fprintf(stderr, "\x1B[31m%-16s [ERROR] SetCrossController failed: req={\"mode\": \"%s\", \"color1\": \"%s\", \"color2\"=\"%s\"}, err={code: %d, \"message\": \"%s\"}\x1B[0m\n",
                            EntityName, trafficMode.mode, trafficMode.color1, trafficMode.color2, err, GetErrMessage(err));
        }

        //for (int i = 0; i < duration; ++i) {
        //    KosThreadSleep(1000);
        //}
        KosThreadSleep(10000);
    } while (true);

    return EXIT_SUCCESS;
}

int TrafficModeProxy_Init(TrafficModeProxy *client, const char *channelName, const char *interfaceName) {
    /**
     * Get the IPC handle of the connection named
     * "lights_gpio_connection".
     */
    client->channel = ServiceLocatorConnect(channelName);
    assert(client->channel != INVALID_HANDLE);

    /* Initialize IPC transport for interaction with the entities. */
    NkKosTransport_Init(&client->transport, client->channel, NK_NULL, 0);

    /**
     * Get Runtime Interface ID (RIID) for interface traffic_light.Mode.mode.
     * Here mode is the name of the traffic_light.Mode component instance,
     * traffic_light.Mode.mode is the name of the Mode interface implementation.
     */
    nk_iid_t riid = ServiceLocatorGetRiid(client->channel, interfaceName);
    assert(riid != INVALID_RIID);

    /**
     * Initialize proxy object by specifying transport (&transport)
     * and lights gpio interface identifier (riid). Each method of the
     * proxy object will be implemented by sending a request to the lights gpio.
     */
    ITrafficMode_proxy_init(&client->proxy, &client->transport.base, riid);

    return 0;
}

nk_err_t TrafficModeProxy_GetTrafficMode(TrafficModeProxy *client, TrafficModeData *trafficMode) {
    // Request's transport structures
    ITrafficMode_GetTrafficMode_req req;
    char reqBuffer[ITrafficMode_req_arena_size];
    nk_arena reqArena = NK_ARENA_INITIALIZER(reqBuffer, reqBuffer + ITrafficMode_req_arena_size);

    // Response's transport structures
    ITrafficMode_GetTrafficMode_res res;
    char resBuffer[ITrafficMode_res_arena_size];
    nk_arena resArena = NK_ARENA_INITIALIZER(resBuffer, resBuffer + ITrafficMode_res_arena_size);


    nk_err_t rcResult = ITrafficMode_GetTrafficMode(&client->proxy.base, &req, &reqArena, &res, &resArena);

    if (NK_EOK != rcResult) {
        return rcResult;
    }

    nk_size_t size = 0;
    const nk_char_t *value = nk_arena_get(nk_char_t, &resArena, &res.mode.mode, &size);
    nk_assert(size > 0);

    const nk_char_t *color1 = nk_arena_get(nk_char_t, &resArena, &res.mode.color1, &size);
    nk_assert(size > 0);

    const nk_char_t *color2 = nk_arena_get(nk_char_t, &resArena, &res.mode.color2, &size);
    nk_assert(size > 0);

    nk_strncpy(trafficMode->mode, value, ITrafficMode_MaxModeLength);
    trafficMode->duration1 = res.mode.duration1;
    nk_strncpy(trafficMode->color1, color1, ITrafficMode_MaxColorLength);
    trafficMode->duration2 = res.mode.duration2;
    nk_strncpy(trafficMode->color2, color2, ITrafficMode_MaxColorLength);

    return NK_EOK;
}

void SelfDiagnostic(EventLogProxy *client, sys_health_data *sysHealthData) {
    // Request's transport structures
    IEventLog_Collect_req req;
    char reqBuffer[IEventLog_Collect_req_arena_size];
    nk_arena reqArena = NK_ARENA_INITIALIZER(reqBuffer, reqBuffer + IEventLog_Collect_req_arena_size);

    // Request's transport structures
    IEventLog_Collect_res res;
    char resBuffer[IEventLog_Collect_res_arena_size];
    nk_arena resArena = NK_ARENA_INITIALIZER(resBuffer, resBuffer + IEventLog_Collect_res_arena_size);

    nk_err_t err = NK_EOK;

    // Set event code
    req.event.code = 1;

    // Set event source
    err = NkKosCopyStringToArena(&reqArena, &req.event.source, EntityName);
    nk_assert(err == NK_EOK);

    // Set event text
    err = NkKosCopyStringToArena(&reqArena, &req.event.text, "SelfDiagnostic");
    nk_assert(err == NK_EOK);

    // Send request to Diagnostics
    err = IEventLog_Collect(&client->proxy.base, &req, &reqArena, &res, &resArena);
    nk_assert(err == NK_EOK);

    sysHealthData->controlSystem = res.state.controlSystem;
    sysHealthData->connector = res.state.connector;
    sysHealthData->crossChecker = res.state.crossController;
    sysHealthData->lightsGPIO = res.state.lightsGpio;
    sysHealthData->diagnostics = err == (NK_EOK ? 0 : 1);
}


int CrossControlProxy_Init(CrossControlProxy *client, const char *channelName, const char *interfaceName) {
    /**
     * Get the IPC handle of the connection named
     * "lights_gpio_connection".
     */
    client->channel = ServiceLocatorConnect(channelName);
    assert(client->channel != INVALID_HANDLE);

    /* Initialize IPC transport for interaction with the entities. */
    NkKosTransport_Init(&client->transport, client->channel, NK_NULL, 0);

    /**
     * Get Runtime Interface ID (RIID) for interface traffic_light.Mode.mode.
     * Here mode is the name of the traffic_light.Mode component instance,
     * traffic_light.Mode.mode is the name of the Mode interface implementation.
     */
    nk_iid_t riid = ServiceLocatorGetRiid(client->channel, interfaceName);
    assert(riid != INVALID_RIID);

    /**
     * Initialize proxy object by specifying transport (&transport)
     * and lights gpio interface identifier (riid). Each method of the
     * proxy object will be implemented by sending a request to the lights gpio.
     */
    ICrossControl_proxy_init(&client->proxy, &client->transport.base, riid);

    return 0;
}

nk_err_t CrossControlProxy_SetCrossSchedule(CrossControlProxy *client, TrafficModeData *schedule) {
    // Request's transport structures
    ICrossControl_SetCrossSchedule_req req;
    char reqBuffer[ICrossControl_req_arena_size];
    nk_arena reqArena = NK_ARENA_INITIALIZER(reqBuffer, reqBuffer + ICrossControl_req_arena_size);

    // Request's transport structures
    ICrossControl_SetCrossSchedule_res res;

    // Fill request data
    if (nk_strcmp(schedule->mode, "regulated") == 0) {
        req.schedule.behavior = ICrossControl_BehaviorRegulated;
    } else if (nk_strcmp(schedule->mode, "unregulated") == 0) {
        req.schedule.behavior = ICrossControl_BehaviorUnregulated;
    } else if (nk_strcmp(schedule->mode, "manual") == 0) {
        req.schedule.behavior = ICrossControl_BehaviorManual;
    }
    req.schedule.crossMode = (GetColorMode(schedule->color2) << 8) | GetColorMode(schedule->color1);
    req.schedule.dir1Duration = schedule->duration1;
    req.schedule.dir2Duration = schedule->duration2;

    // Call dispatcher interface
    ICrossControl_SetCrossSchedule(&client->proxy.base, &req, &reqArena, &res, NULL);

    return NK_EOK;
}
