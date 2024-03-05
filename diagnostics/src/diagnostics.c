#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

// Files required for transport initialization
#include <coresrv/nk/transport-kos.h>
#include <coresrv/sl/sl_api.h>
#include <nk/arena.h>

#define NK_USE_UNQUALIFIED_NAMES

// EDL description of the Diagnostics entity
#include <traffic_light/Diagnostics.edl.h>
#include "eventlog_proxy.h"

static const char EntityName[] = "Diagnostics";

// Type of interface implementing object
typedef struct IEventLogImpl {
    struct IEventLog base;     /* Base interface of object */
    rtl_uint32_t stateControlSystem;     /* Extra parameters */
    rtl_uint32_t stateConnector;         /* Extra parameters */
    rtl_uint32_t stateCrossController;   /* Extra parameters */
    rtl_uint32_t stateLightsGPIO;        /* Extra parameters */

    // ISysHealth_proxy sysHealthProxy;
} IEventLogImpl;

// State method implementation
static nk_err_t Collect_impl(struct IEventLog *self,
                          const struct IEventLog_Collect_req *req,
                          const struct nk_arena *req_arena,
                          struct IEventLog_Collect_res *res,
                          struct nk_arena *res_arena)
{
    IEventLogImpl *impl = (IEventLogImpl *)self;

    nk_size_t size = 0;

    // Get event source
    const nk_char_t *source = nk_arena_get(nk_char_t, req_arena, &req->event.source, &size);
    nk_assert(size > 0);

    // Get event text
    const nk_char_t *text = nk_arena_get(nk_char_t, req_arena, &req->event.text, &size);
    nk_assert(size > 0);

    // Print event
    fprintf(stderr, "\x1B[32m%-16s [INFO ] Diagnostic event: req={\"code\": %d, \"source\": \"%s\", \"state\": %s}\x1B[0m\n",
                    EntityName, req->event.code, source, text);

    if (nk_strcmp(source, "ControlSystem") == 0) {
        impl->stateControlSystem = req->event.code;
    }
    if (nk_strcmp(source, "Connector") == 0) {
        impl->stateConnector = req->event.code;
    }
    if (nk_strcmp(source, "CrossController") == 0) {
        impl->stateCrossController = req->event.code;
    }
    if (nk_strcmp(source, "LightsGPIO") == 0) {
        impl->stateLightsGPIO = req->event.code;
    }

    res->state.controlSystem = impl->stateControlSystem;
    res->state.connector = impl->stateConnector;
    res->state.crossController = impl->stateCrossController;
    res->state.lightsGpio = impl->stateLightsGPIO;
    res->state.diagnostics = 1;

    return NK_EOK;
}

// IEventLog object constructor
static struct IEventLog* CreateIEventLogImpl()
{
    // Table of implementations of IEventLog interface methods
    static const struct IEventLog_ops ops = {
        .Collect = Collect_impl
    };

    // Interface implementing object
    static struct IEventLogImpl impl = {
        .base = {&ops},
        .stateControlSystem = (rtl_uint32_t)-1,
        .stateConnector     = (rtl_uint32_t)-1,
        .stateCrossController  = (rtl_uint32_t)-1,
        .stateLightsGPIO    = (rtl_uint32_t)-1
    };

    // SysHealthProxy_Init(&impl.sysHealthProxy);

    return &impl.base;
}

// Diagnostics entry point
int main(void) {
    // IEventLog implementation instance;
    IEventLog* impl = CreateIEventLogImpl();

    // Request transport structures
    Diagnostics_entity_req req;
    char reqBuffer[Diagnostics_entity_req_arena_size];
    struct nk_arena reqArena = NK_ARENA_INITIALIZER(reqBuffer, reqBuffer + Diagnostics_entity_req_arena_size);

    // Response transport structures
    Diagnostics_entity_res res;
    char resBuffer[Diagnostics_entity_res_arena_size];
    struct nk_arena resArena = NK_ARENA_INITIALIZER(resBuffer, resBuffer + Diagnostics_entity_res_arena_size);

    // Component dispatcher
    EventLog_component component;
    EventLog_component_init(&component, impl);

    // Entity dispatcher
    Diagnostics_entity entity;
    Diagnostics_entity_init(&entity, &component);

    // Get IPC service handle
    ServiceId iid;
    Handle handle = ServiceLocatorRegister(EVENTLOG_CHANNEL, NULL, 0, &iid);
    assert(handle != INVALID_HANDLE);

    // Initialize transport
    NkKosTransport transport;
    NkKosTransport_Init(&transport, handle, NK_NULL, 0);

    fprintf(stderr, "%-16s [DEBUG] Hello I'm Diagnostics!\n", EntityName);
    IEventLogImpl* selfDiagnostic = (IEventLogImpl*)impl;

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
            continue;
        }

        // Dispatch request
        err = Diagnostics_entity_dispatch(&entity, &req.base_, &reqArena, &res.base_, &resArena);
        if (NK_EOK != err) {
            fprintf(stderr, "%-16s [ERROR] Diagnostics_entity_dispatch: err=%d\n", EntityName, err);
            continue;
        }

        // Send response
        err = nk_transport_reply(&transport.base, &res.base_, &resArena);
        if (NK_EOK != err) {
            fprintf(stderr, "%-16s [ERROR] nk_transport_reply: err=%d\n", EntityName, err);
            continue;
        }
    }
    while (true);

    return EXIT_SUCCESS;
}
