#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include "../statsd-parsers.h"
#include "../../utils/utils.h"

void basic_parser_parse(char buffer[], ssize_t count, void (*callback)(statsd_datagram*)) {
    struct statsd_datagram* datagram = (struct statsd_datagram*) malloc(sizeof(struct statsd_datagram));
    // memset 0 doesnt work here for some reason
    *datagram = (struct statsd_datagram) {0};
    int current_segment_length = 0;
    int i = 0;
    char previous_delimiter = ' ';
    char *segment = (char *) malloc(sizeof(char) * 1472);
    if (segment == NULL) {
        die(__LINE__, "Unable to assign memory for StatsD datagram message parsing");
    }
    const char INSTANCE_TAG_IDENTIFIER[] = "instance";
    char *json_key = NULL;
    char *json_value = NULL;
    char *tags_json_buffer = NULL;
    int any_tags = 0;
    int current_json_buffer_position = 0;
    for (i = 0; i < count; i++) {
        segment[current_segment_length] = buffer[i];
        if (buffer[i] == '.' ||
            buffer[i] == ':' ||
            buffer[i] == ',' ||
            buffer[i] == '=' ||
            buffer[i] == '|' ||
            buffer[i] == '@' ||
            buffer[i] == '\n')
        {
            char* attr = (char *) malloc(current_segment_length + 1);
            strncpy(attr, segment, current_segment_length);
            attr[current_segment_length] = '\0';
            if (attr == NULL) {
                die(__LINE__, "Not enough memory to parse StatsD datagram segment");
            }
            if (buffer[i] == '.') {
                datagram->data_namespace = (char *) malloc(current_segment_length + 1);
                if (datagram->data_namespace == NULL) {
                    die(__LINE__, "Not enough memory to save data_namespace attribute");
                }
                sanitize_string(attr);
                memcpy(datagram->data_namespace, attr, current_segment_length + 1);
                previous_delimiter = '.';
            } else if (buffer[i] == ':' && (previous_delimiter == ' ' || previous_delimiter == '.')) {
                datagram->metric = (char *) malloc(current_segment_length + 1);
                if (datagram->metric == NULL) {
                    die(__LINE__, "Not enough memory to save metric attribute.");
                }
                sanitize_string(attr);
                memcpy(datagram->metric, attr, current_segment_length + 1);
            } else if ((buffer[i] == ',' || buffer[i] == ':') && previous_delimiter == '=') {
                json_value = (char *) realloc(json_value, current_segment_length + 1);
                if (json_value == NULL) {
                    die(__LINE__, "Not enough memory for tag value buffer.");
                }
                memcpy(json_value, attr, current_segment_length + 1);
                if (strcmp(json_key, INSTANCE_TAG_IDENTIFIER) == 0) {
                    datagram->instance = (char *) malloc(current_segment_length + 1);
                    if (datagram->instance == NULL) {
                        die(__LINE__, "Not enough memory for instance identifier");
                    }
                    memcpy(datagram->instance, attr, current_segment_length + 1);
                } else {
                    if (any_tags == 0) {
                        // 4kb should be enough
                        tags_json_buffer = (char *) malloc(4096); 
                        if (tags_json_buffer == NULL) {
                            die(__LINE__, "Not enough memory for tag json buffer.");
                        }
                        memset(tags_json_buffer, '\0', 4096);
                        tags_json_buffer[0] = '{';
                        current_json_buffer_position++;
                        any_tags = 1;
                    } 
                    // save key
                    tags_json_buffer[current_json_buffer_position++] = '"';
                    int key_len = strlen(json_key);
                    memcpy(tags_json_buffer + current_json_buffer_position, json_key, key_len);
                    current_json_buffer_position += key_len;
                    tags_json_buffer[current_json_buffer_position++] = '"';
                    tags_json_buffer[current_json_buffer_position++] = ':';
                    // save value
                    tags_json_buffer[current_json_buffer_position++] = '"';
                    int value_len = strlen(json_value);
                    memcpy(tags_json_buffer + current_json_buffer_position, json_value, value_len);
                    current_json_buffer_position += value_len;
                    tags_json_buffer[current_json_buffer_position++] = '"';
                    tags_json_buffer[current_json_buffer_position++] = ',';
                }
                free(json_key);
                json_key = NULL;
                free(json_value);
                json_value = NULL;
                if (buffer[i] == ',') {
                    previous_delimiter = ',';
                } else {
                    previous_delimiter = ':';
                }
            } else if (buffer[i] == '=' && previous_delimiter == ',') {
                json_key = (char *) realloc(json_key, current_segment_length + 1);
                if (json_key == NULL) {
                    die(__LINE__, "Not enough memory for tag key buffer.");
                }
                memcpy(json_key, attr, current_segment_length + 1);
                previous_delimiter = '=';
            } else if (buffer[i] == ',') {
                datagram->metric = (char *) malloc(current_segment_length + 1);
                if (datagram->metric == NULL) {
                    die(__LINE__, "Not enough memory to save metric attribute.");
                }
                sanitize_string(attr);
                memcpy(datagram->metric, attr, current_segment_length + 1);
                previous_delimiter = ',';
            } else if (buffer[i] == '|') {
                double result;
                if (sscanf(attr, "%lf", &result) != 1) {
                    // not a double
                    die(__LINE__, "Unable to parse metric double value");
                } else {
                    // a double
                    datagram->value = result;
                }
                previous_delimiter = '|';
            } else if (buffer[i] == '@') {
                datagram->type = (char *) malloc(current_segment_length + 1);
                if (datagram->type == NULL) {
                    die(__LINE__, "Not enough memory to save type attribute.");
                }
                memcpy(datagram->type, attr, current_segment_length + 1);
                previous_delimiter = '@';
            } else if (buffer[i] == '\n') {
                if (previous_delimiter == '@') {
                    datagram->sampling = (char *) malloc(current_segment_length + 1);
                    if (datagram->sampling == NULL) {
                        die(__LINE__, "Not enough memory to save sampling attribute.");
                    }
                    memcpy(datagram->sampling, attr, current_segment_length + 1);
                } else {
                    datagram->type = (char *) malloc(current_segment_length + 1);
                    if (datagram->type == NULL) {
                        die(__LINE__, "Not enough memory to save type attribute.");
                    }
                    memcpy(datagram->type, attr, current_segment_length + 1);
                }
            }
            free(attr);
            memset(segment, '\0', 1472);
            current_segment_length = 0;
            continue;
        }
        current_segment_length++;
    }
    if (any_tags == 1) {
        int json_len = strlen(tags_json_buffer);
        tags_json_buffer[json_len - 1] = '}';
        datagram->tags = (char *) malloc(json_len + 1);
        if (datagram->tags == NULL) {
            die(__LINE__, "Not enough memory to save tags JSON");
        }
        memcpy(datagram->tags, tags_json_buffer, json_len);
        datagram->tags[json_len] = '\0';
        free(tags_json_buffer);
    }
    callback(datagram);
    free(datagram);
    free(segment);
}