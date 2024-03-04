#ifndef __LIGHTS_GPIO_PROXY_H__
#define __LIGHTS_GPIO_PROXY_H__

#include <coresrv/nk/transport-kos.h>
#include <coresrv/sl/sl_api.h>
#include <nk/types.h>
#include <nk/assert.h>


#define NK_USE_UNQUALIFIED_NAMES
// ILightsGpio client
#include <traffic_light/ILightsGpio.idl.h>

#include "color_defs.h"


static const nk_char_t LightsGpioChannel[] = "gpiolights_channel";
static const nk_char_t LightsGpioEndpoint[] = "lightsGpio.lightsGpio";


typedef struct LightsGpioProxy {
    // TODO: Exclude if proxy releases all resources
    NkKosTransport transport;
    // TODO: Exclude if proxy releases all resources
    Handle channel;
    // ILightsGpio client proxy
    ILightsGpio_proxy proxy;
} LightsGpioProxy;

void LightsGpioProxy_Init(LightsGpioProxy *client);

nk_err_t LightsGpioProxy_SetCrossLights(LightsGpioProxy *client, nk_uint32_t crossMode, nk_uint32_t *result);


void LightsGpioProxy_Init(LightsGpioProxy *client) {
    // Get the IPC handle of the connection
    client->channel = ServiceLocatorConnect(LightsGpioChannel);
    nk_assert(client->channel != INVALID_HANDLE);

    // Initialize IPC transport for interaction with the entities
    NkKosTransport_Init(&client->transport, client->channel, NK_NULL, 0);

    // Get Runtime Interface ID (RIID) for interface traffic_light.Mode.mode.
    nk_iid_t riid = ServiceLocatorGetRiid(client->channel, LightsGpioEndpoint);
    nk_assert(riid != INVALID_RIID);

    // Initialize proxy object by specifying transport (&transport)
    // and lights gpio interface identifier (riid). Each method of the
    // proxy object will be implemented by sending a request to the lights gpio.
    ILightsGpio_proxy_init(&client->proxy, &client->transport.base, riid);
};


nk_err_t LightsGpioProxy_SetCrossLights(LightsGpioProxy *client, nk_uint32_t crossMode, nk_uint32_t *result) {

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

    const nk_char_t *dir1Mode = GetColorName(crossMode);
    NkKosCopyStringToArena(&reqArena, &req.lights.dir1Mode, dir1Mode);

    const nk_char_t *dir2Mode = GetColorName((crossMode >> 8) & 0xFF);
    NkKosCopyStringToArena(&reqArena, &req.lights.dir2Mode, dir2Mode);

    // Send request to LigthsGPIO
    nk_err_t err = ILightsGpio_SetCrossLights(&client->proxy.base, &req, &reqArena, &res, &resArena);
    if (NK_EOK != err) {
        // Forward error
        return err;
    }

    *result = res.result;

    return NK_EOK;
};

#endif // __LIGHTS_GPIO_PROXY_H__