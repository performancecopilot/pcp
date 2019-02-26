#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include "statsd-parsers.h"
#include "basic/basic.h"
#include "ragel/ragel.h"
#include "../utils/utils.h"

const int PARSER_TRIVIAL = 0;
const int PARSER_RAGEL = 1;

void statsd_parser_listen(agent_config* config, int parser_type, void (*callback)(statsd_datagram*)) {
    if (!(parser_type == PARSER_TRIVIAL || parser_type == PARSER_RAGEL)) {
        die(__LINE__, "Incorrect parser type given.");
    }
    void (*handle_datagram)(char[], ssize_t, void (statsd_datagram*));
    if (parser_type == PARSER_TRIVIAL) {
        handle_datagram = &basic_parser_parse;
    } else {
        // handle_datagram = &ragel_parses_parse;
    }
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
        die(__LINE__, "failed to resolve local socket address (err=%s)", gai_strerror(err));
    }
    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd == -1) {
        die(__LINE__, "failed creating socket (err=%s)", strerror(errno));
    }
    if (bind(fd, res->ai_addr, res->ai_addrlen) == -1) {
        die(__LINE__, "failed binding socket (err=%s)", strerror(errno));
    }
    if (config->verbose == 1) {
        printf("Socket enstablished. \n");
        printf("Waiting for datagrams. \n");
    }
    freeaddrinfo(res);
    int max_udp_packet_size = config->max_udp_packet_size;
    char *buffer = (char *) malloc(max_udp_packet_size);
    struct sockaddr_storage src_addr;
    socklen_t src_addr_len = sizeof(src_addr);
    while(1) {
        ssize_t count = recvfrom(fd, buffer, max_udp_packet_size, 0, (struct sockaddr*)&src_addr, &src_addr_len);
        if (count == -1) {
            die(__LINE__, "%s", strerror(errno));
        } 
        // since we checked for -1
        else if ((signed int)count == max_udp_packet_size) { 
            warn(__LINE__, "datagram too large for buffer: truncated and skipped");
        } else {
            handle_datagram(buffer, count, callback);
        }
        memset(buffer, 0, max_udp_packet_size);
    }
    free(buffer);
}

void print_out_datagram(statsd_datagram* datagram) {
    printf("DATAGRAM: \n");
    printf("data_namespace: %s \n", datagram->data_namespace);
    printf("metric: %s \n", datagram->metric);
    printf("instance: %s \n", datagram->instance);
    printf("tags: %s \n", datagram->tags);
    printf("value: %f \n", datagram->value);
    printf("type: %s \n", datagram->type);
    printf("sampling: %s \n", datagram->sampling);
    printf("------------------------------ \n");
}

void free_datagram(statsd_datagram* datagram) {
    free(datagram->data_namespace);
    free(datagram->metric);
    free(datagram->instance);
    free(datagram->tags);
    free(datagram->type);
    free(datagram->sampling);
    free(datagram);
}