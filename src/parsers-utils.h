#ifndef PARSERS_UTILS_
#define PARSERS_UTILS_

#include "parsers.h"

#define JSON_BUFFER_SIZE 4096

typedef struct tag {
    char* key;
    char* value;
} tag;

typedef struct tag_collection {
    tag** values;
    long int length;
} tag_collection;

/**
 * Converts tag_collection* struct to JSON string that is sorted by keys
 */
char* tag_collection_to_json(tag_collection* tags);

int assert_statsd_datagram_eq(statsd_datagram** datagram, char* metric, char* tags, char* instance, char* value, char* type, char* sampling);

#endif
