#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "response-parser.h"

static traffic_light_mode traffic_light_resp;

static void print_depth_shift(int depth) {
    int j;
    for (j = 0; j < depth; j++) {
        printf(" ");
    }
}

static void process_value(json_value *value, int depth);

union {
    struct {
        unsigned int is_mode : 1;
        unsigned int is_direction_1 : 1;
        unsigned int is_direction_2 : 1;
    };
    unsigned int flags;
} update_modes;

void update_modes_field(char *field) {
    update_modes.flags = 0;
    if (strcmp("mode", field) == 0) {
        update_modes.is_mode = 1;
    } else if (strcmp("direction_1", field) == 0) {
        update_modes.is_direction_1 = 1;
        // traffic_light_resp.is_direction_1 = 1;
        fprintf(stderr, "updating configuration for direction 1\n");
    } else if (strcmp("direction_2", field) == 0) {
        update_modes.is_direction_2 = 1;
        // traffic_light_resp.is_direction_2 = 2;
        fprintf(stderr, "updating configuration for direction 2\n");
    }
}

void update_field_values_string(char *value) {
    if (update_modes.is_mode) {
        strncpy(traffic_light_resp.mode, value, MAX_MODE_LEN);
        fprintf(stderr, "updated mode: %s\n", traffic_light_resp.mode);
    } else if (update_modes.is_direction_1) {
        strncpy(traffic_light_resp.direction_1_color, value, MAX_MODE_LEN);
        fprintf(stderr, "updated direction_1_color: %s\n", traffic_light_resp.direction_1_color);
    } else if (update_modes.is_direction_2) {
        strncpy(traffic_light_resp.direction_2_color, value, MAX_MODE_LEN);
        fprintf(stderr, "updated direction_2_color: %s\n", traffic_light_resp.direction_2_color);
    }
}

void update_field_values_int(int value) {
    if (update_modes.is_direction_1) {
        traffic_light_resp.direction_1_duration = value;
        fprintf(stderr, "updated direction_1_duration: %s\n", traffic_light_resp.direction_1_duration);
    } else if (update_modes.is_direction_2) {
        traffic_light_resp.direction_2_duration = value;
        fprintf(stderr, "updated direction_2_duration: %s\n", traffic_light_resp.direction_2_duration);
    }
}

static void process_object(json_value *value, int depth)
{
    int length, x;
    if (value == NULL) {
        return;
    }
    length = value->u.object.length;
    for (x = 0; x < length; x++) {
        D(print_depth_shift(depth);)
        D(printf("object[%d].name = %s\n", x, value->u.object.values[x].name);)
        update_modes_field(value->u.object.values[x].name);
        process_value(value->u.object.values[x].value, depth + 1);
    }
}

static void process_array(json_value *value, int depth) {
    int length, x;
    if (value == NULL) {
        return;
    }
    length = value->u.array.length;
    D(printf("array\n");)
    for (x = 0; x < length; x++) {
        process_value(value->u.array.values[x], depth);
    }
}

static void process_value(json_value *value, int depth) {
    if (value == NULL) {
        return;
    }
    if (value->type != json_object) {
        print_depth_shift(depth);
    }
    switch (value->type) {
    case json_none:
        D(printf("none\n");)
        break;
    case json_null:
        D(printf("null\n");)
        break;
    case json_object:
        process_object(value, depth + 1);
        break;
    case json_array:
        process_array(value, depth + 1);
        break;
    case json_integer:
        D(printf("int: %10ld\n", (long)value->u.integer);)
        update_field_values_int((int)value->u.integer);
        break;
    case json_double:
        D(printf("double: %f\n", value->u.dbl);)
        break;
    case json_string:
        D(printf("string: %s\n", value->u.string.ptr);)
        update_field_values_string((char *)value->u.string.ptr);
        break;
    case json_boolean:
        D(printf("bool: %d\n", value->u.boolean);)
        break;
    }
}

int parse_response(char *response, traffic_light_mode *mode) {

    memset(traffic_light_resp.mode, 0, MAX_MODE_LEN);
    memset(traffic_light_resp.direction_1_color, 0, MAX_COLOR_LEN);
    memset(traffic_light_resp.direction_2_color, 0, MAX_COLOR_LEN);

    traffic_light_resp.direction_1_duration = 0;
    traffic_light_resp.direction_2_duration = 0;

    int data_size;
    json_char *json;
    json_value *value;

    // fprintf(stderr, "data for parsing:\n%s\n", response);

    // strip everything before the first opening bracket "{"
    data_size = strlen(response);
    for (int i = 0; i < data_size; i++) {
        if (response[i] == "{"[0]) {
            int json_length = 0;
            for (int j = 0; j < data_size - i; j++, json_length++) {
                response[j] = response[i + j];
            }
            response[json_length] = 0;
            break;
        }
    }

    // fprintf(stderr, "--------------------------------\n\n");
    // fprintf(stderr, "stripped data for parsing:\n%s\n", response);

    json = (json_char *)response;

    value = json_parse(json, data_size);

    if (value == NULL) {
        fprintf(stderr, "Unable to parse data\n%s\n", response);
        free(response);
        return 1;
    } else {
        process_value(value, 0);
        json_value_free(value);
    }

    memset(mode->mode, 0, MAX_MODE_LEN);
    strncpy(mode->mode, traffic_light_resp.mode, MAX_MODE_LEN);

    mode->direction_1_duration = traffic_light_resp.direction_1_duration;

    memset(mode->direction_1_color, 0, MAX_COLOR_LEN);
    strncpy(mode->direction_1_color, traffic_light_resp.direction_1_color, MAX_COLOR_LEN);

    mode->direction_2_duration = traffic_light_resp.direction_2_duration;

    memset(mode->direction_2_color, 0, MAX_COLOR_LEN);
    strncpy(mode->direction_2_color, traffic_light_resp.direction_2_color, MAX_COLOR_LEN);

    return 0;
}
