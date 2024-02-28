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

/* get traffic light configuration from the central server */
int get_traffic_light_configuration(void)
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
    int rc = parse_response(response_data);
    // fprintf(stderr, "%s response parsing result: \n%d\n", EntityName, rc);

    // close the socket
    close(sockfd);

    return EXIT_SUCCESS;
}


/* Connector entity entry point. */
int main(int argc, const char *argv[]) {

    // fprintf(stderr, "%-13s [DEBUG] Entity initialized: state={\"mode\": 0x%08x, \"lights\": [\"%s\", \"%s\"]}\n",
    //                 EntityName, impl.mode, gpio1Color, gpio2Color);
    // fprintf(stderr, "%-13s [DEBUG] Hello, I'm about to start working\n", EntityName);

    bool is_network_available;
    is_network_available = wait_for_network();
    // fprintf(stderr, "%-13s [DEBUG] Network status: %s\n", EntityName, is_network_available ? "UP" : "DOWN");


    // Diagnostics
    EventLogProxy logProxy;
    EventLogProxy_Init(&logProxy);
    LogEvent(&logProxy, 0, EntityName, "\"Hello I'm Connector!\"");
    
    do {
    
        int rc = get_traffic_light_configuration();    
        fprintf(stderr, "%-13s Сonfiguration parsing status: %s\n", EntityName, rc == EXIT_SUCCESS ? "OK" : "FAILED");
    //     LogEvent(&logProxy, 0, EntityName, "\"Hello I'm Connector!\"");
    //     KosThreadSleep(5000);
    } while (true);

    return EXIT_SUCCESS;
}
