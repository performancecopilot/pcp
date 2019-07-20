#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <chan/chan.h>
#include <pthread.h>

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
void*
network_listener_exec(void* args) {
    pthread_setname_np(pthread_self(), "Net. Listener");
    struct agent_config* config = ((struct network_listener_args*)args)->config;
    chan_t* network_listener_to_parser = ((struct network_listener_args*)args)->network_listener_to_parser;
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
            struct unprocessed_statsd_datagram* datagram = (struct unprocessed_statsd_datagram*) malloc(sizeof(struct unprocessed_statsd_datagram));
            ALLOC_CHECK("Unable to assign memory for struct representing unprocessed datagrams.");
            datagram->value = (char*) malloc(sizeof(char) * count);
            ALLOC_CHECK("Unable to assign memory for datagram value.");
            strncpy(datagram->value, buffer, count);
            datagram->value[count] = '\0';
            chan_send(network_listener_to_parser, datagram);
        }
        memset(buffer, 0, max_udp_packet_size);
    }
    free(buffer);
    pthread_exit(NULL);
}

/**
 * Creates arguments for network listener thread
 * @arg config - Application config
 * @arg unprocessed_channel - Network listener -> Parser
 * @arg stats_sink - Channel for sending stats about PMDA itself
 * @return network_listener_args
 */
struct network_listener_args*
create_listener_args(struct agent_config* config, chan_t* network_listener_to_parser) {
    struct network_listener_args* listener_args = (struct network_listener_args*) malloc(sizeof(struct network_listener_args));
    ALLOC_CHECK("Unable to assign memory for listener arguments.");
    listener_args->config = config;
    listener_args->network_listener_to_parser = network_listener_to_parser;
    return listener_args;
}
