
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/* Files required for transport initialization. */
#include <coresrv/nk/transport-kos.h>
#include <coresrv/sl/sl_api.h>

/* EDL description of the Diagnostics entity. */
#include <traffic_light/Diagnostics.edl.h>

#include <assert.h>

/* Type of interface implementing object. */
typedef struct IStateImpl {
    struct traffic_light_IState base;     /* Base interface of object */
    rtl_uint32_t state;                   /* Extra parameters */
} IStateImpl;

/* State method implementation. */
static nk_err_t FState_impl(struct traffic_light_IState *self,
                          const struct traffic_light_IState_FState_req *req,
                          const struct nk_arena *req_arena,
                          traffic_light_IState_FState_res *res,
                          struct nk_arena *res_arena)
{
    IStateImpl *impl = (IStateImpl *)self;
    /**
     * Increment value in control system request by
     * one step and include into result argument that will be
     * sent to the control system in the lights gpio response.
     */
    res->result = req->value + impl->state;
    return NK_EOK;
}

/**
 * IState object constructor.
 * state is the number by which the input value is increased.
 */
static struct traffic_light_IState *CreateIStateImpl(rtl_uint32_t state)
{
    /* Table of implementations of IState interface methods. */
    static const struct traffic_light_IState_ops ops = {
        .FState = FState_impl
    };

    /* Interface implementing object. */
    static struct IStateImpl impl = {
        .base = {&ops}
    };

    impl.state = state;

    return &impl.base;
}

/* Diagnostics entry point. */
int main(void)
{
    NkKosTransport transport;
    ServiceId iid;

    /* Get lights gpio IPC handle of "diagnostics_connection". */
    Handle handle = ServiceLocatorRegister("diagnostics_connection", NULL, 0, &iid);
    assert(handle != INVALID_HANDLE);

    /* Initialize transport to lights_gpio. */
    NkKosTransport_Init(&transport, handle, NK_NULL, 0);

    /**
     * Prepare the structures of the request to the lights gpio entity: constant
     * part and arena. Because none of the methods of the lights gpio entity has
     * sequence type arguments, only constant parts of the
     * request and response are used. Arenas are effectively unused. However,
     * valid arenas of the request and response must be passed to
     * lights gpio transport methods (nk_transport_recv, nk_transport_reply) and
     * to the lights gpio method.
     */
    traffic_light_Diagnostics_entity_req req;
    char req_buffer[traffic_light_Diagnostics_entity_req_arena_size];
    struct nk_arena req_arena = NK_ARENA_INITIALIZER(req_buffer,
                                        req_buffer + sizeof(req_buffer));

    /* Prepare response structures: constant part and arena. */
    traffic_light_Diagnostics_entity_res res;
    char res_buffer[traffic_light_Diagnostics_entity_res_arena_size];
    struct nk_arena res_arena = NK_ARENA_INITIALIZER(res_buffer,
                                        res_buffer + sizeof(res_buffer));

    /**
     * Initialize mode component dispatcher. 3 is the value of the step,
     * which is the number by which the input value is increased.
     */
    traffic_light_CState_component component;
    traffic_light_CState_component_init(&component, CreateIStateImpl(0x1000000));

    /* Initialize lights gpio entity dispatcher. */
    traffic_light_Diagnostics_entity entity;
    traffic_light_Diagnostics_entity_init(&entity, &component);

    fprintf(stderr, "Hello I'm Diagnostics\n");

    /* Dispatch loop implementation. */
    do
    {
        /* Flush request/response buffers. */
        nk_req_reset(&req);
        nk_arena_reset(&req_arena);
        nk_arena_reset(&res_arena);

        /* Wait for request to lights gpio entity. */
        if (nk_transport_recv(&transport.base,
                              &req.base_,
                              &req_arena) != NK_EOK) {
            fprintf(stderr, "nk_transport_recv error\n");
        } else {
            /**
             * Handle received request by calling implementation Mode_impl
             * of the requested Mode interface method.
             */
            traffic_light_Diagnostics_entity_dispatch(&entity, &req.base_, &req_arena,
                                        &res.base_, &res_arena);
        }

        /* Send response. */
        if (nk_transport_reply(&transport.base,
                               &res.base_,
                               &res_arena) != NK_EOK) {
            fprintf(stderr, "nk_transport_reply error\n");
        }
    }
    while (true);

    return EXIT_SUCCESS;
}
