#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <rtl/stdbool.h>
#include <kos_net.h>
#include <unistd.h> // read(), write(), close()
#include <strings.h> // bzero()
#include <assert.h>

/* Files required for transport initialization. */
#include <coresrv/nk/transport-kos.h>
#include <coresrv/sl/sl_api.h>
#include <kos/thread.h>


/* Description of the server interface used by the `client` entity. */
#define NK_USE_UNQUALIFIED_NAMES

// ITrafficLight Server
#include <traffic_light/ITrafficMode.idl.h>
#include <traffic_light/TrafficMode.cdl.h>
#include <traffic_light/Connector.edl.h>

// IEventLog Client
#include "eventlog_proxy.h"

#include "response-parser.h"
#include <json.h>


#define HOST_IP                 "10.0.2.2"
#define HOST_PORT               8081
#define NUM_RETRIES             10
#define MSG_BUF_SIZE            1024
#define MSG_CHUNK_BUF_SIZE      256
#define SA struct sockaddr


static const char EntityName[] = "Connector";
static char ChannelName[] = "trafficmode_channel";

/* get traffic light configuration from the central server */
int get_traffic_light_configuration(traffic_light_mode *mode)
{
    int sockfd, connfd;
    struct sockaddr_in servaddr, cli;

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        fprintf(stderr, "%-13s [ERROR] Socket creation failed!\n", EntityName);
        return EXIT_FAILURE;
    }
    else {
        // fprintf(stderr, "%-13s [DEBUG] Socket successfully created\n", EntityName);
    }
    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(HOST_IP);

    if ( servaddr.sin_addr.s_addr == INADDR_NONE ) {
        fprintf(stderr, "%-13s [ERROR] Bad network address\n", EntityName);
    }

    servaddr.sin_port = htons(HOST_PORT);

    int res = -1;
    for (int i = 0; res == -1 && i < NUM_RETRIES; i++)
    {
        sleep(1); // Wait some time for server be ready.
        res = connect(sockfd, (SA*)&servaddr, sizeof(servaddr));
    }

    // connect the client socket to server socket
    if (res != 0) {
        fprintf(stderr, "%-13s [ERROR] Connection not established: error=%d\n", EntityName, res);
    }
    else {
        // fprintf(stderr, "%-13s [DEBUG] Connection established\n", EntityName);

    }

    // printf("preparing request..\n");

    char request_data[MSG_BUF_SIZE];
    char response_data[MSG_BUF_SIZE];
    char response_data_chunk[MSG_BUF_SIZE];
    int  request_len = 0;    
    size_t n;

    snprintf(request_data, MSG_CHUNK_BUF_SIZE,
        "GET /mode/112233 HTTP/1.1\r\n"
        "Host: 172.20.172.221:5765\r\n\r\n"
        // "Host-Agent: KOS\r\n"
        // "Accept: */*\r\n"
    );

    request_len = strlen(request_data);
    response_data[0] = 0;
    response_data_chunk[0] = 0;
    // fprintf(stderr, "%s, sending request: %s\n len: %d\n", EntityName, request_data, request_len);

    /// Write the request
    if (write(sockfd, request_data, request_len) >= 0)
    {
        // fprintf(stderr, "%s request sent, reading response..\n", EntityName);
        /// Read the response
        while ((n = read(sockfd, response_data_chunk, MSG_BUF_SIZE)) > 0)
        {
            strcat(response_data, response_data_chunk);
            // fprintf(stderr, "%s response data: \n%s\n", EntityName, response_data);
        }
    }
    // fprintf(stderr, "%s read data: %s..\n", EntityName, response_data);
    int rc = parse_response(response_data, mode);
    // fprintf(stderr, "%s response parsing result: \n%d\n", EntityName, rc);

    // close the socket
    close(sockfd);

    return EXIT_SUCCESS;
}


// ITrafficMode implementing type
typedef struct ITrafficModeImpl {
    // Base interface of object
    struct ITrafficMode base;
    // Diagnostics
    EventLogProxy logProxy;
} ITrafficModeImpl;

// ITrafficMode.GetTrafficMode method implementation
static nk_err_t GetTrafficMode_impl(struct ITrafficMode *self,
                                   const struct ITrafficMode_GetTrafficMode_req *req,
                                   const struct nk_arena *req_arena,
                                   struct ITrafficMode_GetTrafficMode_res *res,
                                   struct nk_arena *res_arena)
{
    ITrafficModeImpl *impl = (ITrafficModeImpl *)self;

    traffic_light_mode mode;
    int rc = get_traffic_light_configuration(&mode);
    fprintf(stderr, "%-13s [DEBUG] Сonfiguration parsing status: %s\n", EntityName, rc == EXIT_SUCCESS ? "OK" : "FAILED");

    NkKosCopyStringToArena(res_arena, &res->mode.mode, mode.mode);
    res->mode.duration1 = mode.direction_1_duration;
    NkKosCopyStringToArena(res_arena, &res->mode.color1, mode.direction_1_color);
    res->mode.duration2 = mode.direction_2_duration;
    NkKosCopyStringToArena(res_arena, &res->mode.color2, mode.direction_2_color);

    char msgBuffer[IEventLog_MaxTextLength];
    snprintf(msgBuffer, IEventLog_MaxTextLength,
             "{\"mode\": \"%s\", \"color1\": \"%s\", \"color2\": \"%s\"}",
             mode.mode, mode.direction_1_color, mode.direction_2_color);
    LogEvent(&impl->logProxy, 2, EntityName, msgBuffer);

    return NK_EOK;
}

// ITrafficMode object constructor
static struct ITrafficMode *CreateITrafficModeImpl()
{
    // Table of implementations of ITrafficMode interface methods
    static const struct ITrafficMode_ops ops = {
        .GetTrafficMode = GetTrafficMode_impl
    };

    // Interface implementing object
    static struct ITrafficModeImpl impl = {
        .base = {&ops}
    };

    EventLogProxy_Init(&impl.logProxy);

    // fprintf(stderr, "%-13s [DEBUG] Entity initialized: state={\"direction\": %s, \"mode\": \"%s\"}\n",
    //                 EntityName, impl.direction, impl.mode);

    LogEvent(&impl.logProxy, 0, EntityName, "\"Hello I'm Connector!\"");

    return &impl.base;
}

// Connector entry point
int main(int argc, char** argv)
{
    // Request transport structures
    Connector_entity_req req;
    char reqBuffer[Connector_entity_req_arena_size];
    struct nk_arena reqArena = NK_ARENA_INITIALIZER(reqBuffer, reqBuffer + Connector_entity_req_arena_size);

    // Response transport structures
    Connector_entity_res res;
    char resBuffer[Connector_entity_res_arena_size];
    struct nk_arena resArena = NK_ARENA_INITIALIZER(resBuffer, resBuffer + Connector_entity_res_arena_size);

    bool is_network_available;
    is_network_available = wait_for_network();

    ITrafficMode* impl = CreateITrafficModeImpl();

    // Component dispatcher
    Connector_component component;
    Connector_component_init(&component, impl);

    // Entity dispatcher
    Connector_entity entity;
    Connector_entity_init(&entity, &component);

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
            // continue;
        }

        // Dispath request
        err = Connector_entity_dispatch(&entity, &req.base_, &reqArena, &res.base_, &resArena);
        if (NK_EOK != err) {
            fprintf(stderr, "%-13s [ERROR] LightsGPIO_entity_dispatch: err=%d\n", EntityName, err);
            // continue;
        }

        // Send response
        err = nk_transport_reply(&transport.base, &res.base_, &resArena);
        if (NK_EOK != err) {
            fprintf(stderr, "%-13s [ERROR] nk_transport_reply: err=%d\n", EntityName, err);
            // continue;
        }
    } while (true);

    return EXIT_SUCCESS;
}

/* Connector entity entry point. */
int main_test(int argc, const char *argv[]) {

    // fprintf(stderr, "%-13s [DEBUG] Entity initialized: state={\"mode\": 0x%08x, \"lights\": [\"%s\", \"%s\"]}\n",
    //                 EntityName, impl.mode, gpio1Color, gpio2Color);
    // fprintf(stderr, "%-13s [DEBUG] Hello, I'm about to start working\n", EntityName);

    // bool is_network_available;
    // is_network_available = wait_for_network();
    // fprintf(stderr, "%-13s [DEBUG] Network status: %s\n", EntityName, is_network_available ? "UP" : "DOWN");


    // // Diagnostics
    // EventLogProxy logProxy;
    // EventLogProxy_Init(&logProxy);
    // LogEvent(&logProxy, 0, EntityName, "\"Hello I'm Connector!\"");
    
    // do {
    
    //     int rc = get_traffic_light_configuration();    
    //     fprintf(stderr, "%-13s Сonfiguration parsing status: %s\n", EntityName, rc == EXIT_SUCCESS ? "OK" : "FAILED");
    // //     LogEvent(&logProxy, 0, EntityName, "\"Hello I'm Connector!\"");
    //     KosThreadSleep(1000);
    // } while (true);

    return EXIT_SUCCESS;
}
