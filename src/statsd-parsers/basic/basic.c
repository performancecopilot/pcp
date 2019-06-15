#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pcp/pmapi.h>
#include <sys/types.h>
#include "../../utils/utils.h"
#include "../statsd-parsers.h"
#include "basic.h"

#define JSON_BUFFER_SIZE 4096

int basic_parser_parse(char* buffer, statsd_datagram** datagram) {
    *datagram = (struct statsd_datagram*) malloc(sizeof(struct statsd_datagram));
    *(*datagram) = (struct statsd_datagram) {0};
    if (parse(buffer, datagram)) {
        buffer[strcspn(buffer, "\n")] = 0;
        verbose_log("Parsed: %s", buffer);
        return 1;
    }
    free_datagram(*datagram);
    buffer[strcspn(buffer, "\n")] = 0;
    verbose_log("Throwing away datagram. REASON: unable to parse: %s", buffer);
    return 0;
};

int parse(char* buffer, statsd_datagram** datagram) {
    int current_segment_length = 0;
    int i = 0;
    char previous_delimiter = ' ';
    int count = strlen(buffer);
    char* segment = (char *) malloc(count); // cannot overflow since whole segment is count anyway
    ALLOC_CHECK("Unable to assign memory for StatsD datagram message parsing.");
    const char INSTANCE_TAG_IDENTIFIER[] = "instance";
    tag_collection* tags;
    char* tag_key = NULL;
    char* tag_value = NULL;
    char* attr;
    int any_tags = 0;
    /* Flag field used to determine which memory to free in stats_datagram should data parsing fail, bits in this order
     * tags
     * metric
     * type
     * value
     * instance
     * sampling
     * */
    char field_allocated_flags = 0b00000000;
    /* Flag field for required datagram fields */
    char required_fields_flags = 0b00001110;
    char tag_allocated_flags = 0b00000000;
    for (i = 0; i < count; i++) {
        segment[current_segment_length] = buffer[i];
        if (buffer[i] == ':' ||
            buffer[i] == ',' ||
            buffer[i] == '=' ||
            buffer[i] == '|' ||
            buffer[i] == '@' ||
            buffer[i] == '\n')
        {
            attr = (char *) malloc(current_segment_length + 1);
            strncpy(attr, segment, current_segment_length);
            attr[current_segment_length] = '\0';
            ALLOC_CHECK("Not enough memory to parse StatsD datagram segment.");
            if (buffer[i] == ':' && previous_delimiter == ' ') {
                if (!sanitize_string(attr)) {
                    goto error_clean_up;
                }
                (*datagram)->metric = (char *) malloc(current_segment_length + 1);
                ALLOC_CHECK("Not enough memory to save metric attribute.");
                field_allocated_flags = field_allocated_flags | 1 << 1;
                memcpy((*datagram)->metric, attr, current_segment_length + 1);
                previous_delimiter = ':';
            } else if ((buffer[i] == ',' || buffer[i] == ':') && previous_delimiter == '=') {
                tag_value = (char *) realloc(tag_value, current_segment_length + 1);
                ALLOC_CHECK("Not enough memory for tag value buffer.");
                tag_allocated_flags = tag_allocated_flags | 1 << 1;
                memcpy(tag_value, attr, current_segment_length + 1);
                if (strcmp(tag_key, INSTANCE_TAG_IDENTIFIER) == 0) {
                    if (!sanitize_string(attr)) {
                        goto error_clean_up;
                    }
                    (*datagram)->instance = (char *) malloc(current_segment_length + 1);
                    ALLOC_CHECK("Not enough memory for instance identifiers.");
                    field_allocated_flags = field_allocated_flags | 1 << 4;
                    memcpy((*datagram)->instance, attr, current_segment_length + 1);
                } else {                    
                    if (!sanitize_string(tag_key) ||
                        !sanitize_string(tag_value)) {
                        goto error_clean_up;
                    }
                    int key_len = strlen(tag_key);
                    int value_len = strlen(tag_value);
                    if (key_len > 0 && value_len > 0) {
                        tag* t = (tag*) malloc(sizeof(tag));
                        ALLOC_CHECK("Unable to allocate memory for tag.");
                        t->key = (char*) malloc(key_len);
                        ALLOC_CHECK("Unable to allocate memory for tag key.");
                        t->value = (char*) malloc(value_len);
                        ALLOC_CHECK("Unable to allocate memory for tag value.");
                        memcpy(t->key, tag_key, key_len);
                        memcpy(t->value, tag_value, value_len);
                        if (any_tags == 0) {
                            tags = (tag_collection*) malloc(sizeof(tag_collection));
                            ALLOC_CHECK("Unable to allocate memory for tag collection.");
                            field_allocated_flags = field_allocated_flags | 1 << 0;
                            *tags = (tag_collection) { 0 };
                            any_tags = 1;
                        }
                        tags->values = (tag**) realloc(tags->values, sizeof(tag*) * (tags->length + 1));
                        tags->values[tags->length] = t;
                        tags->length++;
                    }
                }
                free(tag_key);
                free(tag_value);
                tag_allocated_flags = 0;
                tag_key = NULL;
                tag_value = NULL;
                if (buffer[i] == ',') {
                    previous_delimiter = ',';
                } else {
                    previous_delimiter = ':';
                }
            } else if (buffer[i] == '=' && previous_delimiter == ',') {
                tag_key = (char *) realloc(tag_key, current_segment_length + 1);
                ALLOC_CHECK("Not enough memory for tag key buffer.");
                tag_allocated_flags = tag_allocated_flags | 1 << 2;
                memcpy(tag_key, attr, current_segment_length + 1);
                previous_delimiter = '=';
            } else if (buffer[i] == ',') {
                if (!sanitize_string(attr)) {
                    goto error_clean_up;
                }
                (*datagram)->metric = (char *) malloc(current_segment_length + 1);
                ALLOC_CHECK("Not enough memory to save metric attribute.");
                field_allocated_flags = field_allocated_flags | 1 << 1;
                memcpy((*datagram)->metric, attr, current_segment_length + 1);
                previous_delimiter = ',';
            } else if (buffer[i] == '|') {
                if (!sanitize_metric_val_string(attr)) {
                    goto error_clean_up;
                }
                (*datagram)->value = (char *) malloc(current_segment_length + 1);
                ALLOC_CHECK("Not enough memory to save value attribute.");
                field_allocated_flags = field_allocated_flags | 1 << 3;
                memcpy((*datagram)->value, attr, current_segment_length + 1);
                previous_delimiter = '|';
            } else if (buffer[i] == '@') {
                if (!sanitize_sampling_val_string(attr)) {
                    goto error_clean_up;
                }
                (*datagram)->type = (char *) malloc(current_segment_length + 1);
                ALLOC_CHECK("Not enough memory to save type attribute.");
                field_allocated_flags = field_allocated_flags | 1 << 2;
                memcpy((*datagram)->type, attr, current_segment_length + 1);
                previous_delimiter = '@';
            } else if (buffer[i] == '\n') {
                if (previous_delimiter == '@') {
                    if (!sanitize_sampling_val_string(attr)) {
                        goto error_clean_up;
                    }
                    (*datagram)->sampling = (char *) malloc(current_segment_length + 1);
                    ALLOC_CHECK("Not enough memory for datagram sampling.");
                    field_allocated_flags = field_allocated_flags | 1 << 4;
                    memcpy((*datagram)->sampling, attr, current_segment_length + 1);
                } else {
                    if (!sanitize_type_val_string(attr)) {
                        goto error_clean_up;
                    }
                    (*datagram)->type = (char *) malloc(current_segment_length + 1);
                    ALLOC_CHECK("Not enough memory to save type attribute.");
                    field_allocated_flags = field_allocated_flags | 1 << 2;
                    memcpy((*datagram)->type, attr, current_segment_length + 1);
                }
            }
            free(attr);
            current_segment_length = 0;
            continue;
        }
        current_segment_length++;
    }
    if (any_tags == 1) {
        char* json = tag_collection_to_json(tags);
        if (json != NULL) {
            (*datagram)->tags = malloc(strlen(json) + 1);
            (*datagram)->tags = json;
        }
        free(tags);
        if (tag_allocated_flags & 1) {
            free(tag_key);
        }
        if (tag_allocated_flags & 2) {
            free(tag_value);
        }
    }
    free(segment);    
    if ((required_fields_flags & field_allocated_flags) != required_fields_flags)  {
        goto error_clean_up_end;
    }
    return 1;

    error_clean_up:
    free(attr);
    free(segment);
    if (tag_allocated_flags & 1) {
        free(tag_key);
    }
    if (tag_allocated_flags & 2) {
        free(tag_value);
    }
    error_clean_up_end:
    return 0;
}

static int tag_comparator(const void* x, const void* y) {
    int res = strcmp((*(tag**)x)->key, (*(tag**)y)->key);
    return res;
}

/**
 * Converts tag_collection* struct to JSON string that is sorted by keys
 */
char* tag_collection_to_json(tag_collection* tags) {
    char buffer[JSON_BUFFER_SIZE];
    qsort(tags->values, tags->length, sizeof(tag*), tag_comparator);
    buffer[0] = '{';
    int i;
    int current_size = 1;
    for (i = 0; i < tags->length; i++) {
        if (i == 0) {
            current_size += pmsprintf(buffer + current_size, JSON_BUFFER_SIZE - current_size, "\"%s\":\"%s\"",
                tags->values[i]->key, tags->values[i]->value);
        } else {
            current_size += pmsprintf(buffer + current_size, JSON_BUFFER_SIZE - current_size, ",\"%s\":\"%s\"",
                tags->values[i]->key, tags->values[i]->value);
        }
    }
    if (current_size >= JSON_BUFFER_SIZE - 2) {
        return NULL;
    }
    buffer[current_size] = '}';
    buffer[current_size + 1] = '\0';
    char* result = malloc(sizeof(char) * (current_size + 2));
    ALLOC_CHECK("Unable to allocate memory for tags json.");
    memcpy(result, buffer, current_size + 2);
    return result;
}