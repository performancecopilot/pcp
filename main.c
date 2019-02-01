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
#include <unistd.h>

static int verbose = 0;
static int debug = 0;
static int trace = 0;
static int show_version = 0;
static char *port = (char *) "8125"; // Statsd default port
static char *tcp_address = (char *) "0.0.0.0";
static uint64_t max_udp_packet_size = 1472; // 1472 - Ethernet MTU
const int32_t MAX_UNPROCESSED_PACKETS = 2048;
const int32_t TCP_READ_SIZE = 4096; // MTU


// <Namespace> . <ValStr> _ <Modifier> : <ValFlt> | <Metric>
struct StatsDDatagram
{
    char *Namespace;
    char *ValStr;
    char *Modifier;
    double ValFlt;
    char *Metric;
    char *Sampling; // ? FLOAT
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

void parse_file_arguments() {
    char line_buffer[256];
    int line_index = 0;
    if (access("config", F_OK) == -1) {
        printf("No config file found. Command line arguments only. \n");
        return;
    } else {
        printf("Config file found. Command line arguments take precedence. \n");
    }
    FILE *config = fopen("config", "r");
    char* option = (char *) malloc(256);
    if (option == NULL) {
        die(__LINE__, "Unable to assign memory for parsing options file");
    }
    char* value = (char *) malloc(256);
    if (value == NULL) {
        die(__LINE__, "Unable to assign memory for parsing options file.");
    }
    const char MAX_UDP_PACKET_SIZE_OPTION[] = "max_udp_packet_size";
    const char PORT_OPTION[] = "port";
    const char TCP_ADDRESS_OPTION[] = "tcp_address";
    const char VERSION_OPTION[] = "version";
    const char VERBOSE_OPTION[] = "verbose";
    const char TRACE_OPTION[] = "trace";
    const char DEBUG_OPTION[] = "debug";
    while (fgets(line_buffer, 256, config) != NULL) {
        if (sscanf(line_buffer, "%s %s", option, value) != 2) {
            die(__LINE__, "Syntax error in config file on line %d", line_index + 1);
        }
        if (strcmp(option, MAX_UDP_PACKET_SIZE_OPTION) == 0) {
            max_udp_packet_size = strtoull(value, NULL, 10);
        } else if (strcmp(option, PORT_OPTION) == 0) {
            port = (char *) malloc(strlen(value));
            if (port == NULL) {
                die(__LINE__, "Unable to assign memory for port number.");
            }
            strncat(port, value, strlen(value));
        } else if (strcmp(option, TCP_ADDRESS_OPTION) == 0) {
            tcp_address = (char *) malloc(strlen(value));
            if (tcp_address == NULL) {
                die(__LINE__, "Unable to assign memory for tcp address.");
            }
            strncat(tcp_address, value, strlen(value));
        } else if (strcmp(option, VERBOSE_OPTION) == 0) {
            verbose = atoi(value);
        } else if (strcmp(option, DEBUG_OPTION) == 0) {
            debug = atoi(value);
        } else if (strcmp(option, VERSION_OPTION) == 0) {
            show_version = atoi(value);
        } else if (strcmp(option, TRACE_OPTION) == 0) {
            trace = atoi(value);
        }
        line_index++;
        memset(option, '\0', 256);
        memset(value, '\0', 256);
    }
    free(value);
    free(option);
    fclose(config);
}

void parse_cmd_arguments(int argc, char **argv) {
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
    int current_segment_length = 0;
    int i;
    char previous_delimiter = ' ';
    char *segment = (char *) malloc(sizeof(char) * 549);
    if (segment == NULL) {
        die(__LINE__, "Unable to assign memory for StatsD datagram message parsing");
    }
    for (i = 0; i < count; i++) {
        segment[current_segment_length] = buffer[i];
        if (buffer[i] == '.' ||
            buffer[i] == '_' ||
            buffer[i] == ':' ||
            buffer[i] == '|' ||
            buffer[i] == '\n') 
        {
            char* attr = (char *) malloc(current_segment_length + 1);
            strncpy(attr, segment, current_segment_length);
            attr[current_segment_length] = '\0';
            if (attr == NULL) {
                die(__LINE__, "Not enough memory to parse StatsD datagram segment");
            }
            if (buffer[i] == '.') {
                datagram->Namespace = (char *) malloc(current_segment_length + 1);
                if (datagram->Namespace == NULL) {
                    die(__LINE__, "Not enough memory to save Namespace attribute");
                }
                memcpy(datagram->Namespace, attr, current_segment_length + 1);
                previous_delimiter = '.';
            } else if (buffer[i] == '_') {
                datagram->ValStr = (char *) malloc(current_segment_length + 1);
                if (datagram->ValStr == NULL) {
                    die(__LINE__, "Not enough memory to save ValStr attribute.");
                }
                memcpy(datagram->ValStr, attr, current_segment_length + 1);
                previous_delimiter = '_';
            } else if (buffer[i] == ':') {
                if (previous_delimiter == '_') {
                    datagram->Modifier = (char *) malloc(current_segment_length + 1);
                    if (datagram->Modifier == NULL) {
                        die(__LINE__, "Not enough memory to save Modifier attribute.");
                    }
                    memcpy(datagram->Modifier, attr, current_segment_length + 1);
                } else {
                    datagram->ValStr = (char *) malloc(current_segment_length + 1);
                    if (datagram->ValStr == NULL) {
                        die(__LINE__, "Not enough memory to save ValStr attribute.");
                    }
                    memcpy(datagram->ValStr, attr, current_segment_length + 1);
                }
                previous_delimiter = ':';
            } else if (buffer[i] == '|') {
                double result;
                if (sscanf(attr, "%lf", &result) != 1) {
                    // not a double
                    die(__LINE__, "Unable to parse metric double value");
                } else {
                    // a double
                    datagram->ValFlt = result;
                }
                previous_delimiter = '|';
            } else if (buffer[i] == '\n') {
                datagram->Metric = (char *) malloc(current_segment_length + 1);
                if (datagram->Metric == NULL) {
                    die(__LINE__, "Not enough memory to save Metric attribute.");
                }
                memcpy(datagram->Metric, attr, current_segment_length + 1);
            }
            free(attr);
            memset(segment, '\0', 549);
            current_segment_length = 0;
            continue;
        }
        current_segment_length++;
    }
    printf("Namespace: %s \n", datagram->Namespace);
    printf("ValStr: %s \n", datagram->ValStr);
    printf("Modifier: %s \n", datagram->Modifier);
    printf("ValFlt: %f \n", datagram->ValFlt);
    printf("Metric: %s \n", datagram->Metric);
    printf("------------------------------ \n");
    free(datagram);
    free(segment);
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
    parse_cmd_arguments(argc, argv);
    parse_file_arguments();
    
    /* print those arguments */
    print_arguments();
    
    /* start listening on udp for pcp datagrams */    
    udpListen();
    return 1;
}