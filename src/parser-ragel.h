#ifndef PARSER_RAGEL_
#define PARSER_RAGEL_

#include "parsers.h"

/**
 * Ragel parser entry point
 * Parsers given buffer and populates datagram with parsed data if they are valid
 * @arg str - Buffer to be parsed
 * @arg datagram - Placeholder for parsed data
 * @return 1 on success, 0 on fail 
 */
int ragel_parser_parse(char* str, statsd_datagram** datagram);

#endif
