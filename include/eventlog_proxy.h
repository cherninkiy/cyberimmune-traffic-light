#ifndef __EVENTLOG_PROXY_H__
#define __EVENTLOG_PROXY_H__

// IEventLog Client
#include <nk/types.h>
#include <nk/errno.h>
#include <traffic_light/IEventLog.idl.h>


inline static const char* GetErrMessage(nk_err_t err) {
    switch(err) {
        case NK_EOK: return "no error";
        case NK_EPERM: return "permission denied";
        case NK_ENOENT: return "no such entry";
        case NK_EAGAIN: return "try again";
        case NK_ENOMEM: return "out of memory";
        case NK_EINVAL: return "invalid message";
        case NK_ENOTEMPTY: return "entry not empty";
        case NK_ENOMSG: return "no reply message";
        case NK_EBADMSG: return "bad message";
        case NK_ENOTCONN: return "not connected";
        default: return "unspecified error";
    }
}

// #define KNRM  "\x1B[0m"
// #define KRED  "\x1B[31m"
// #define KGRN  "\x1B[32m"
// #define KYEL  "\x1B[33m"
// #define KBLU  "\x1B[34m"
// #define KMAG  "\x1B[35m"
// #define KCYN  "\x1B[36m"
// #define KWHT  "\x1B[37m"


#define EVENTLOG_CHANNEL "diagnostics_channel"
#define EVENTLOG_ENDPOINT "eventLog.eventLog"

typedef struct EventLogProxy {
    // TODO: Exclude if proxy releases all resources
    NkKosTransport transport;
    // TODO: Exclude if proxy releases all resources
    Handle channel;
    // ILightsMode client proxy
    IEventLog_proxy proxy;
} EventLogProxy;

void EventLogProxy_Init(EventLogProxy *client);

void LogEvent(EventLogProxy *client, unsigned int code, const char *source, const char *text);

void EventLogProxy_Init(EventLogProxy *client)
{
    // Get the IPC handle of the connection
    client->channel = ServiceLocatorConnect(EVENTLOG_CHANNEL);
    nk_assert(client->channel != INVALID_HANDLE);

    // Initialize IPC transport for interaction with the entities
    NkKosTransport_Init(&client->transport, client->channel, NK_NULL, 0);

    // Get Runtime Interface ID (RIID) for interface traffic_light.Mode.mode.
    nk_iid_t riid = ServiceLocatorGetRiid(client->channel, EVENTLOG_ENDPOINT);
    nk_assert(riid != INVALID_RIID);

    // Initialize proxy object by specifying transport (&transport)
    // and lights gpio interface identifier (riid). Each method of the
    // proxy object will be implemented by sending a request to the lights gpio.
    IEventLog_proxy_init(&client->proxy, &client->transport.base, riid);
}

void LogEvent(EventLogProxy *client, unsigned int code, const char *source, const char *text)
{
    // Request's transport structures
    IEventLog_Collect_req req;
    char reqBuffer[IEventLog_Collect_req_arena_size];
    nk_arena reqArena = NK_ARENA_INITIALIZER(reqBuffer, reqBuffer + IEventLog_Collect_req_arena_size);

    // Request's transport structures
    IEventLog_Collect_res res;
    char resBuffer[IEventLog_Collect_res_arena_size];
    nk_arena resArena = NK_ARENA_INITIALIZER(resBuffer, resBuffer + IEventLog_Collect_res_arena_size);

    // // Set event code
    // req.event.code = code;
    // // Set event source
    // nk_err_t err1 = NkKosCopyStringToArena(&reqArena, &req.event.source, source);
    // // Set event text
    // nk_err_t err2 = NkKosCopyStringToArena(&reqArena, &req.event.text, text);

    nk_err_t err = NK_EOK;

    // Set event code
    req.event.code = code;

    // Set event source
    err = NkKosCopyStringToArena(&reqArena, &req.event.source, source);
    nk_assert(err == NK_EOK);

    // Set event text
    err = NkKosCopyStringToArena(&reqArena, &req.event.text, text);
    nk_assert(err == NK_EOK);

    // Send request to Diagnostics
    err = IEventLog_Collect(&client->proxy.base, &req, &reqArena, &res, &resArena);
    nk_assert(err == NK_EOK);
}

#endif // __EVENTLOG_PROXY_H__