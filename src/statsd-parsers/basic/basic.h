#ifndef BASIC_
#define BASIC_

void basic_parser_parse(char buffer[], ssize_t count, void (*callback)(statsd_datagram*));

#endif