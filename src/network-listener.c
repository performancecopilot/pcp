#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <chan/chan.h>

#include "network-listener.h"
#include "parser-basic.h"
#include "parser-ragel.h"
#include "utils.h"
#include "config-reader.h"

/**
 * Thread entrypoint - listens on address and port specified in config 
 * for UDP/TCP containing StatsD payload and then sends it over to parser thread for parsing
 * @arg args - network_listener_args
 */
void* network_listener_exec(void* args) {
    agent_config* config = ((network_listener_args*)args)->config;
    chan_t* unprocessed_datagrams = ((network_listener_args*)args)->unprocessed_datagrams;
    const char* hostname = 0;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = 0;
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;
    struct addrinfo* res = 0;
    int err = getaddrinfo(hostname, config->port, &hints, &res);
    if (err != 0) {
        DIE("failed to resolve local socket address (err=%s)", gai_strerror(err));
    }
    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd == -1) {
        DIE("failed creating socket (err=%s)", strerror(errno));
    }
    if (bind(fd, res->ai_addr, res->ai_addrlen) == -1) {
        DIE("failed binding socket (err=%s)", strerror(errno));
    }
    verbose_log("Socket enstablished.");
    verbose_log("Waiting for datagrams.");
    freeaddrinfo(res);
    int max_udp_packet_size = config->max_udp_packet_size;
    char *buffer = (char *) malloc(max_udp_packet_size);
    struct sockaddr_storage src_addr;
    socklen_t src_addr_len = sizeof(src_addr);
    while(1) {
        ssize_t count = recvfrom(fd, buffer, max_udp_packet_size, 0, (struct sockaddr*)&src_addr, &src_addr_len);
        if (count == -1) {
            DIE("%s", strerror(errno));
        } 
        // since we checked for -1
        else if ((signed int)count == max_udp_packet_size) { 
            warn(__FILE__, __LINE__, "Datagram too large for buffer: truncated and skipped");
        } else {
            unprocessed_statsd_datagram* datagram = (unprocessed_statsd_datagram*) malloc(sizeof(unprocessed_statsd_datagram));
            ALLOC_CHECK("Unable to assign memory for struct representing unprocessed datagrams.");
            datagram->value = (char*) malloc(sizeof(char) * (count + 1));
            ALLOC_CHECK("Unable to assign memory for datagram value.");
            strncpy(datagram->value, buffer, count);
            datagram->value[count + 1] = '\0';
            chan_send(unprocessed_datagrams, datagram);
        }
        memset(buffer, 0, max_udp_packet_size);
    }
    free(buffer);
    return NULL;
}

/**
 * Packs up its arguments into struct so that we can pass it via single reference to the network listener thread
 */
network_listener_args* create_listener_args(agent_config* config, chan_t* unprocessed_channel) {
    struct network_listener_args* listener_args = (struct network_listener_args*) malloc(sizeof(struct network_listener_args));
    ALLOC_CHECK("Unable to assign memory for listener arguments.");
    listener_args->config = (agent_config*) malloc(sizeof(agent_config));
    ALLOC_CHECK("Unable to assign memory for listener config.");
    listener_args->config = config;
    listener_args->unprocessed_datagrams = unprocessed_channel;
    return listener_args;
}
