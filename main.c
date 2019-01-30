#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdarg.h>

static int verbose = 0;
static int debug = 0;
static int trace = 0;
static int show_version = 0;
static char *port = (char *) "8125"; // Statsd default port
static char *tcp_address = (char *) "0.0.0.0";
static uint64_t max_udp_packet_size = 1472; // 1472 - Ethernet MTU
const int32_t MAX_UNPROCESSED_PACKETS = 2048;
const int32_t TCP_READ_SIZE = 4096; // MTU

struct StatsDDatagram
{
    char *Modifier;
    char *Metric;
    char *ValStr;
    char *Namespace;
    double ValFlt;
    float Sampling;
};

/* https://www.lemoda.net/c/die/ */
static void die (int line_number, const char * format, ...)
{
    va_list vargs;
    va_start(vargs, format);
    fprintf(stderr, "%d: ", line_number);
    vfprintf(stderr, format, vargs);
    printf("\n");
    fprintf(stderr, ".\n");
    va_end(vargs);
    exit (1);
}

void parse_arguments(int argc, char **argv) {
    int c;
    while(1) {
        static struct option long_options[] = {
            { "verbose", no_argument, &verbose, 1 },
            { "debug", no_argument, &debug, 1 },
            { "version", no_argument, &show_version, 1 },
            { "trace", no_argument, &trace, 1 },
            { "max-udp", required_argument, 0, 'u' },
            { "tcpaddr", required_argument, 0, 't' },
            { "port", required_argument, 0, 'a' },
            { 0, 0, 0, 0 }
        };
        int option_index = 0;
        c = getopt_long_only(argc, argv, "u::d::t::a::v::t::", long_options, &option_index);
        if (c == -1) break;
        switch (c) {
            case 0:
                /* If this option set a flag, do nothing else now. */
                if (long_options[option_index].flag != 0)
                    break;
                printf("option %s", long_options[option_index].name);
                if (optarg)
                    printf(" with arg %s", optarg);
                printf("\n");
                break;
            case 'u':
                max_udp_packet_size = strtoll(optarg, NULL, 10);
                break;
            case 't':
                tcp_address = optarg;
                break;
            case 'a':
                port = optarg;
                break;
        }
    }
}

void handle_datagram(char buffer[], ssize_t count) {
    struct StatsDDatagram* datagram = (struct StatsDDatagram *) malloc(sizeof(struct StatsDDatagram));
    char delimiters[5] = "._:|";
    int delimiter_index = 0;
    int current_segment_length = 0;
    int i;
    for (i = 0; i < count; i++, current_segment_length++) {
        char current_delimiter = delimiters[delimiter_index];
        if (buffer[i] == current_delimiter) {
            if (current_delimiter == '.' || current_delimiter == '_' || current_delimiter == '|' ||
                current_delimiter == ':') {
                char *attr = (char *) malloc(sizeof(char) * current_segment_length);
                if (attr == NULL) {
                    die(__LINE__, "Not enough memory to parse StatsD datagram");
                }
                // TODO: parse char val
            } else {
                // TODO: parse float val
            }       
            current_segment_length = 0;
        }
    }
}

void udpListen() {
    /* wildcard */
    const char* hostname = 0; 
    const char* portname = port;
    struct addrinfo hints;
    memset(&hints,0,sizeof(hints));
    /* AD_UNSPEC; dont specify IP version */
    hints.ai_family = AF_UNSPEC;
    /* SOCK_DGRAM; allows UDP, excludes TCP */
    hints.ai_socktype = SOCK_DGRAM;
    /* only meaningful in context of specific address family */
    hints.ai_protocol = 0;
    /* 
    The AI_PASSIVE flag has been set because the address is intended for use by a server.
    It causes the IP address to default to the wildcard address as opposed to the loopback address.
    The AI_ADDRCONFIG flag has been set so that IPv6 results will only be returned if the server has an IPv6 address, and similarly for IPv4.
    */
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;
    /* getaddrinfo; construct socket address */
    struct addrinfo* res = 0;
    int err = getaddrinfo(hostname, portname, &hints, &res);
    if (err != 0) {
        die(__LINE__, "failed to resolve local socket address (err=%s)", gai_strerror(err));
    }
    /* create socket */
    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd == -1) {
        die(__LINE__, "failed creating socket (err=%s)", strerror(errno));
    }
    /* bind local address to socket, so it can listen to inbound datagrams */
    if (bind(fd, res->ai_addr, res->ai_addrlen) == -1) {
        die(__LINE__, "failed binding socket (err=%s)", strerror(errno));
    }
    printf("Socket enstablished. \n");
    printf("Waiting for datagrams. \n");
    /* socket successfully opened, no need for addressinfo anymore */
    freeaddrinfo(res);
    /* TODO what buffer size?, 548 is DHCP maximum */
    char buffer[549];
    struct sockaddr_storage src_addr;
    socklen_t src_addr_len = sizeof(src_addr);
    while(1) {
        ssize_t count = recvfrom(fd, buffer, sizeof(buffer), 0, (struct sockaddr*)&src_addr, &src_addr_len);
        if (count == -1) {
            die(__LINE__, "%s", strerror(errno));
        } else if (count == sizeof(buffer)) {
            die(__LINE__, "datagram too large for buffer: truncated");
        } else {
            handle_datagram(buffer, count);
        }
        memset(buffer, 0, 549);
    }
}

void print_arguments() {
    if (verbose)
        puts("verbose flag is set");
    if (debug)
        puts("debug flag is set");
    if (show_version)
        puts("version flag is set");
    printf("maxudp: %lu \n", max_udp_packet_size);
    printf("tcpaddr: %s \n", tcp_address);
    printf("port: %s \n", port);
}

int main(int argc, char **argv)
{
    /* save into scoped variables parsed program arguments */
    parse_arguments(argc, argv);
    /* print those arguments */
    print_arguments();
    /* start listening on udp for pcp datagrams */    
    udpListen();
    return 1;
}