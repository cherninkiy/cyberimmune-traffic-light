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

int parse_response(char *response);

#define MAX_MODE_LEN 128

static struct {
    char mode_id[MAX_MODE_LEN];
    int direction;
    int red_duration;
    int yellow_duration;
    int green_duration;
    int disabled;
} traffic_light_mode;


#endif // RESPONSE_PARSER