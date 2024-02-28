#ifndef __CROSSMODE_PROXY_H__
#define __CROSSMODE_PROXY_H__

// ICrossMode Client
#include <nk/types.h>
#include <nk/errno.h>
#include <traffic_light/ICrossMode.idl.h>


typedef struct CrossModeProxy {
    // TODO: Exclude if proxy releases all resources
    NkKosTransport transport;
    // TODO: Exclude if proxy releases all resources
    Handle channel;

    ICrossMode_proxy proxy;
} CrossModeProxy;


int CrossModeProxy_Init(CrossModeProxy *client, const char *channelName, const char *interfaceName)
{
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
    ICrossMode_proxy_init(&client->proxy, &client->transport.base, riid);

    return 0;
}

nk_err_t CrossModeProxy_SetCrossMode(CrossModeProxy *client, nk_uint32_t mode)
{
    // Request's transport structures
    ICrossMode_SetCrossMode_req req;
    // char reqBuffer[ICrossMode_req_arena_size];
    // nk_arena reqArena = NK_ARENA_INITIALIZER(reqBuffer, reqBuffer + ICrossMode_req_arena_size);

    // Response's transport structures
    ICrossMode_SetCrossMode_res res;
    // char resBuffer[ICrossMode_res_arena_size];
    // nk_arena resArena = NK_ARENA_INITIALIZER(resBuffer, resBuffer + ICrossMode_res_arena_size);

    req.mode = mode;

    /**
     * Call Mode interface method.
     * Lights GPIO will be sent a request for calling Mode interface method
     * mode_comp.mode_impl with the value argument. Calling thread is locked
     * until a response is received from the lights gpio.
     */
    nk_err_t rcResult = ICrossMode_SetCrossMode(&client->proxy.base, &req, NULL, &res, NULL);
    return rcResult;
}

#endif // __CROSSMODE_PROXY_H__