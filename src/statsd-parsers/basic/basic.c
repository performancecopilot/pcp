#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include "../statsd-parsers.h"
#include "../../utils/utils.h"

void basic_parser_parse(char buffer[], ssize_t count, void (*callback)(statsd_datagram*)) {
    struct statsd_datagram* datagram = (struct statsd_datagram*) malloc(sizeof(struct statsd_datagram));
    int current_segment_length = 0;
    int i = 0;
    char previous_delimiter = ' ';
    char *segment = (char *) malloc(sizeof(char) * 549);
    if (segment == NULL) {
        die(__LINE__, "Unable to assign memory for StatsD datagram message parsing");
    }
    for (i = 0; i < count; i++) {
        segment[current_segment_length] = buffer[i];
        if (buffer[i] == '.' ||
            buffer[i] == ':' ||
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
                memcpy(datagram->data_namespace, attr, current_segment_length + 1);
                previous_delimiter = '.';
            } else if (buffer[i] == ':') {
                datagram->metric = (char *) malloc(current_segment_length + 1);
                if (datagram->metric == NULL) {
                    die(__LINE__, "Not enough memory to save metric attribute.");
                }
                memcpy(datagram->metric, attr, current_segment_length + 1);
            } else if (buffer[i] == '|') {
                double result;
                if (sscanf(attr, "%lf", &result) != 1) {
                    // not a double
                    die(__LINE__, "Unable to parse metric double value");
                } else {
                    // a double
                    datagram->val_float = result;
                }
                previous_delimiter = '|';
            } else if (buffer[i] == '@') {
                datagram->val_str = (char *) malloc(current_segment_length + 1);
                if (datagram->val_str == NULL) {
                    die(__LINE__, "Not enough memory to save val_str attribute.");
                }
                memcpy(datagram->val_str, attr, current_segment_length + 1);
                previous_delimiter = '@';
            } else if (buffer[i] == '\n') {
                if (previous_delimiter == '@') {
                    datagram->sampling = (char *) malloc(current_segment_length + 1);
                    if (datagram->sampling == NULL) {
                        die(__LINE__, "Not enough memory to save sampling attribute.");
                    }
                    memcpy(datagram->sampling, attr, current_segment_length + 1);
                } else {
                    datagram->val_str = (char *) malloc(current_segment_length + 1);
                    if (datagram->val_str == NULL) {
                        die(__LINE__, "Not enough memory to save val_str attribute.");
                    }
                    memcpy(datagram->val_str, attr, current_segment_length + 1);
                }
            }
            free(attr);
            memset(segment, '\0', 549);
            current_segment_length = 0;
            continue;
        }
        current_segment_length++;
    }
    callback(datagram);
    free(datagram);
    free(segment);
}