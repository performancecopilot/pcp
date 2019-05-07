#include <stdio.h>
#include <string.h>
#include <chan.h>

#include "../statsd-parsers.h"
#include "../../utils/utils.h"

void* consume_datagram(void* args) {
    chan_t* parsed = ((consumer_args*)args)->parsed_datagrams;
    statsd_datagram* datagram = (statsd_datagram*) malloc(sizeof(statsd_datagram));
    while(1) {
        *datagram = (statsd_datagram) { 0 };
        chan_recv(parsed, (void *)&datagram);
        print_out_datagram(datagram);
    }
}
