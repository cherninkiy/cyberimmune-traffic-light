#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

// Files required for transport initialization
#include <coresrv/nk/transport-kos.h>
#include <coresrv/sl/sl_api.h>
#include <nk/arena.h>

#define NK_USE_UNQUALIFIED_NAMES

// IGpioLights Server
#include <traffic_light/IGpioLights.idl.h>
#include <traffic_light/GpioLights.cdl.h>
#include <traffic_light/LightsGPIO.edl.h>

// IEventLog Client
#include "eventlog_proxy.h"

static char EntityName[] = "LightsGPIO*";
static char ChannelName[] = "lightsGpio*_channel";
static unsigned int Direction;

#define DEFAULT_LIGHTS_MODE "blink-yellow"
#define MAX_DIRECTION_LENGTH 12
#define MAX_COLOR_LENGTH IGpioLights_MaxLength

// IGpioLights implementing type
typedef struct IGpioLightsImpl {
    // Base interface of object
    struct IGpioLights base;
    // Direction (i.e. 1, 2) if unspecified then 0
    char direction[MAX_DIRECTION_LENGTH];
    // Current GpioLights
    char mode[MAX_COLOR_LENGTH];
    // Diagnostics
    EventLogProxy logProxy;
} IGpioLightsImpl;

// IGpioLights.SetLightsMode method implementation
static nk_err_t SetLightsMode_impl(struct IGpioLights *self,
                                   const struct IGpioLights_SetLightsMode_req *req,
                                   const struct nk_arena *req_arena,
                                   struct IGpioLights_SetLightsMode_res *res,
                                   struct nk_arena *res_arena) {

    IGpioLightsImpl *impl = (IGpioLightsImpl *)self;

    // // Initial State
    // fprintf(stderr, "%-13s [DEBUG] state={direction=%d, mode=\"%s\"}\n",
    //                 EntityName, impl->direction, impl->mode);

    // Check direction
    nk_size_t size = 0;
    const nk_char_t *direction = nk_arena_get(nk_char_t, req_arena, &req->direction, &size);
    nk_assert(size > 0);
    nk_assert(nk_strcmp(direction, impl->direction) == 0);

    // Get requested mode
    const nk_char_t *color = nk_arena_get(nk_char_t, req_arena, &req->mode, &size);

    // Check value
    nk_assert(size > 0);

    // Turn On/Off GPIO
    fprintf(stderr, "%-13s [DEBUG] Request SetGpioLights: req={\"direction\": %s, \"color\"=\"%s\"} state={\"direction\": %s, \"value\"=\"%s\"}\n",
                    EntityName, direction, color, impl->direction, impl->mode);

    // Update current mode
    nk_memset(impl->mode, 0, IGpioLights_MaxLength);
    nk_size_t len = nk_strnlen(color, IGpioLights_MaxLength);
    nk_strncpy(impl->mode, color, len);

    // Write result
    nk_arena_reset(res_arena);
    nk_err_t err = NkKosCopyStringToArena(res_arena, &res->result, impl->mode);
    nk_assert(err == NK_EOK);

    char msgBuffer[IEventLog_MaxTextLength];
    snprintf(msgBuffer, IEventLog_MaxTextLength,
             "{\"direction\": %s, \"mode\": \"%s\"}", impl->direction, impl->mode);
    LogEvent(&impl->logProxy, 2, EntityName, msgBuffer);

    return NK_EOK;
}

// IGpioLights object constructor
static struct IGpioLights *CreateIGpioLightsImpl(unsigned int direction)
{
    // Table of implementations of IGpioLights interface methods
    static const struct IGpioLights_ops ops = {
        .SetLightsMode = SetLightsMode_impl
    };

    // Interface implementing object
    static struct IGpioLightsImpl impl = {
        .base = {&ops},
        .mode = DEFAULT_LIGHTS_MODE
    };

    // Object data initialization
    snprintf(impl.direction, MAX_DIRECTION_LENGTH, "%d", direction);

    EventLogProxy_Init(&impl.logProxy);

    fprintf(stderr, "%-13s [DEBUG] Entity initialized: state={\"direction\": %s, \"mode\": \"%s\"}\n",
                    EntityName, impl.direction, impl.mode);

    LogEvent(&impl.logProxy, 0, EntityName, "\"Hello I'm LightsGPIO!\"");

    return &impl.base;
}

// LightsGPIO entry point
int main(int argc, char** argv)
{
    assert(argc == 1);
    if (strcmp(argv[0], "LightsGPIO1") == 0) {
        EntityName[10] = '1';
        ChannelName[10] = '1';
        Direction = 1;
    } else if (strcmp(argv[0], "LightsGPIO2") == 0) {
        EntityName[10] = '2';
        ChannelName[10] = '2';
        Direction = 2;
    } else {
        fprintf(stderr, "%-13s [ERROR] Unknown EntityName: %s in %s at %s\n",
                EntityName, argv[0], __FILE__, __FUNCTION__);
        return -1;
    }

    // Request transport structures
    LightsGPIO_entity_req req;
    char reqBuffer[LightsGPIO_entity_req_arena_size];
    struct nk_arena reqArena = NK_ARENA_INITIALIZER(reqBuffer, reqBuffer + LightsGPIO_entity_req_arena_size);

    // Response transport structures
    LightsGPIO_entity_res res;
    char resBuffer[LightsGPIO_entity_res_arena_size];
    struct nk_arena resArena = NK_ARENA_INITIALIZER(resBuffer, resBuffer + LightsGPIO_entity_res_arena_size);

    // Component dispatcher
    GpioLights_component component;
    GpioLights_component_init(&component, CreateIGpioLightsImpl(Direction));

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
    do
    {
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
        // fprintf(stderr, "%-13s [ERROR] Response sent: err=%d\n", EntityName, err);

    } while (true);

    return EXIT_SUCCESS;
}