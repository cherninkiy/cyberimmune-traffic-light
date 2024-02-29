#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

// Files required for transport initialization
#include <coresrv/nk/transport-kos.h>
#include <coresrv/sl/sl_api.h>
#include <kos/thread.h>
#include <nk/arena.h>

#define NK_USE_UNQUALIFIED_NAMES

// ITrafficMode Client
#include <traffic_light/ITrafficMode.idl.h>
#include "response-parser.h"

// ICrossMode Client
#include "color_defs.h"
#include "crossmode_proxy.h"

// IEventLog Client
#include "eventlog_proxy.h"

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

void SelfDiagnostic(EventLogProxy *client, sys_health_data *sysHealthData);


int main(int argc, char** argv) {
    TrafficModeProxy trafficModeProxy;
    TrafficModeProxy_Init(&trafficModeProxy, "trafficmode_channel", "trafficMode.trafficMode");

    CrossModeProxy crossModeProxy;
    CrossModeProxy_Init(&crossModeProxy, "crossmode_channel", "crossMode.crossMode");

    EventLogProxy eventLogProxy;
    EventLogProxy_Init(&eventLogProxy);
    LogEvent(&eventLogProxy, 0, EntityName, "\"Hello I'm ControlSystem!\"");

    TrafficModeData trafficMode;
    sys_health_data sysHealthData;
    do {
        SelfDiagnostic(&eventLogProxy, &sysHealthData);
        fprintf(stderr, "%-13s [DEBUG] Self Diagnostic Data: %d\n", EntityName, sysHealthData.stateControlSystem);

        TrafficModeProxy_GetTrafficMode(&trafficModeProxy, &trafficMode);

        if (nk_strcmp(trafficMode.mode, "unregulated") == 0) {
            nk_uint32_t unregulatedMode = ICrossMode_Direction1Yellow 
                                        | ICrossMode_Direction1Blink
                                        | ICrossMode_Direction2Yellow
                                        | ICrossMode_Direction2Blink;
            CrossModeProxy_SetCrossMode(&crossModeProxy, unregulatedMode);
        }
        
        if (nk_strcmp(trafficMode.mode, "regulated") == 0) {
            CrossModeProxy_SetCrossMode(&crossModeProxy, 0x0C0C);
        }

        if (nk_strcmp(trafficMode.mode, "manual") == 0) {
            nk_uint32_t direction1 = GetColorMode(trafficMode.color1);
            nk_uint32_t direction2 = GetColorMode(trafficMode.color2);
            CrossModeProxy_SetCrossMode(&crossModeProxy, (direction1 << 8) | direction2);
        }

        KosThreadSleep(1000);
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

    sysHealthData->stateControlSystem = res.state.controlSystem;
    sysHealthData->stateConnector = res.state.connector;
    sysHealthData->stateCrossChecker = res.state.crossChecker;
    sysHealthData->stateLightsGPIO1 = res.state.lightsGpio1;
    sysHealthData->stateLightsGPIO2 = res.state.lightsGpio2;
    sysHealthData->stateDiagnostics = err;
}
