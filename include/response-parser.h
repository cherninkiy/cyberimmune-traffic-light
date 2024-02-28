#ifndef RESPONSE_PARSER
#define RESPONSE_PARSER

#include <string.h>  // bzero(), strlen(), strcmp()
#include "json.h"

#define DEBUG_LEVEL 1
#if DEBUG_LEVEL > 1
#define D(A) A
#else
#define D(A) 
#endif

#define MAX_MODE_LEN 16
#define MAX_COLOR_LEN 16

typedef struct traffic_light_mode {
    char mode[MAX_MODE_LEN];
    int direction_1_duration;
    char direction_1_color[MAX_COLOR_LEN];
    int direction_2_duration;
    char direction_2_color[MAX_COLOR_LEN];
} traffic_light_mode;

int parse_response(char *response, traffic_light_mode *mode);

#endif // RESPONSE_PARSER