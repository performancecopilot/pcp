#include "../statsd-parsers.h"

#ifndef BASIC_
#define BASIC_

statsd_datagram* basic_parser_parse(char *buffer);

void sanitize_datagram_segments(char **unsanitized_segment_string);

#endif