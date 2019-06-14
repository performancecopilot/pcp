#include "../statsd-parsers.h"

#ifndef BASIC_
#define BASIC_

int basic_parser_parse(char *buffer, statsd_datagram** datagram);

int parse(char* buffer, statsd_datagram** datagram);

char* tag_collection_to_json(tag_collection* tags);

#endif