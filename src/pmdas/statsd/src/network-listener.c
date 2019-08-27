/*
 * Copyright (c) 2019 Miroslav Foltýn.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <chan/chan.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>

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
    static char* end_message = "PMDASTATSD_EXIT"; 
    struct agent_config* config = ((struct network_listener_args*)args)->config;
    chan_t* network_listener_to_parser = ((struct network_listener_args*)args)->network_listener_to_parser;
    const char* hostname = 0;
    struct addrinfo hints;
    fd_set readfds;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = 0;
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;
    struct addrinfo* res = 0;
    char port_buffer[6];
    pmsprintf(port_buffer, 6, "%d", config->port);
    int err = getaddrinfo(hostname, port_buffer, &hints, &res);
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
    VERBOSE_LOG(0, "Socket enstablished.");
    VERBOSE_LOG(0, "Waiting for datagrams.");
    fcntl(fd, F_SETFL, O_NONBLOCK);
    struct timeval tv;
    freeaddrinfo(res);
    int max_udp_packet_size = config->max_udp_packet_size;
    char *buffer = (char *) malloc(max_udp_packet_size * sizeof(char*));
    struct sockaddr_storage src_addr;
    socklen_t src_addr_len = sizeof(src_addr);
    int rv;
    while(1) {
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        rv = select(fd + 1, &readfds, NULL, NULL, &tv);
        if (rv == 1) {
            ssize_t count = recvfrom(fd, buffer, max_udp_packet_size, 0, (struct sockaddr*)&src_addr, &src_addr_len);
            if (count == -1) {
                DIE("%s", strerror(errno));
            } 
            // since we checked for -1
            else if ((signed int)count == max_udp_packet_size) { 
                VERBOSE_LOG(2, "Datagram too large for buffer: truncated and skipped");
            } else {
                struct unprocessed_statsd_datagram* datagram = (struct unprocessed_statsd_datagram*) malloc(sizeof(struct unprocessed_statsd_datagram));
                ALLOC_CHECK("Unable to assign memory for struct representing unprocessed datagrams.");
                datagram->value = (char*) malloc(sizeof(char) * (count + 1));
                ALLOC_CHECK("Unable to assign memory for datagram value.");
                memcpy(datagram->value, buffer, count);
                datagram->value[count] = '\0';
                if (strcmp(end_message, datagram->value) == 0) {
                    free_unprocessed_datagram(datagram);
                    kill(getpid(), SIGINT);
                    break;
                }
                chan_send(network_listener_to_parser, datagram);
            }
            memset(buffer, 0, max_udp_packet_size);
            rv = 0;
        } else {
            int exit_flag = check_exit_flag();
            if (exit_flag) {        
                break;
            }
        }
    }
    VERBOSE_LOG(2, "Network listener thread exiting.");
    struct unprocessed_statsd_datagram* datagram = (struct unprocessed_statsd_datagram*) malloc(sizeof(struct unprocessed_statsd_datagram));
    ALLOC_CHECK("Unable to assign memory for struct representing unprocessed datagrams.");
    size_t length = strlen(end_message) + 1;
    datagram->value = (char*) malloc(sizeof(char) * length);
    memcpy(datagram->value, end_message, length);
    chan_send(network_listener_to_parser, datagram);
    free(buffer);
    pthread_exit(NULL);
}

/**
 * Frees unprocessed datagram
 * @arg datagram
 */
void
free_unprocessed_datagram(struct unprocessed_statsd_datagram* datagram) {
    if (datagram != NULL) {
        if (datagram->value != NULL) {
            free(datagram->value);
        }
        free(datagram);
    }
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
