#ifndef PARSER_BASIC_
#define PARSER_BASIC_

#include "parsers.h"

/**
 * Basic parser entry point
 * Parsers given buffer and populates datagram with parsed data if they are valid
 * @arg buffer - Buffer to be parsed
 * @arg datagram - Placeholder for parsed data
 * @return 1 on success, 0 on fail 
 */
int
basic_parser_parse(char *buffer, struct statsd_datagram** datagram);

#endif
