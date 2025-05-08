/*
 * Copyright (c) 2016 Red Hat.
 * Check the pmhttp.h / libpcp_web client APIs
 */

#include <pcp/pmapi.h>
#include <pcp/pmhttp.h>

int
main(int argc, char *argv[])
{
    int			c, code = 0;
    int			verbose = 0;
    int			version = 0;
    int			errflag = 0;
    char		*agent = NULL;
    char		*http_version = NULL;
    char		*agent_version = NULL;
    char		*unix_location = NULL;
    struct timeval	timeout = { 0 };
    struct http_client	*client;
    static const char	*usage = "[-aAptV] url";
    static size_t	buflen, typelen;
    static char		*buf, *type;

    pmSetProgname(argv[0]);
    while ((c = getopt(argc, argv, "a:A:D:s:t:vV:?")) != EOF) {
	switch (c) {

	case 'a':	/* user-agent string */
	    agent = optarg;
	    break;

	case 'A':	/* user-agent version */
	    agent_version = optarg;
	    break;

	case 'D':
	    if ((c = pmSetDebug(optarg)) < 0) {
		fprintf(stderr, "Unrecognized debug options - %s\n", optarg);
		errflag++;
		break;
	    }
	    break;

	case 's':	/* Unix domain socket location */
	    unix_location = optarg;
	    break;

	case 't':	/* request timeout (sec) */
	    timeout.tv_sec = atoi(optarg);
	    break;

	case 'v':	/* increased verbosity */
	    verbose++;
	    break;

	case 'V':	/* HTTP protocol version */
	    http_version = optarg;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (agent && !agent_version) {
	fprintf(stderr, "User-Agent specified but not version (-A)\n");
	errflag++;
    }

    if (!agent && agent_version) {
	fprintf(stderr, "User-Agent version specified but not name (-a)\n");
	errflag++;
    }

    if (http_version) {
	if (strcmp(http_version, "1.0") == 0)
	    version = PV_HTTP_1_0;
	if (strcmp(http_version, "1.1") == 0)
	    version = PV_HTTP_1_1;
	else {
	    fprintf(stderr, "Unsupported HTTP protocol version\n");
	    errflag++;
	}
    }

    if (errflag || optind >= argc) {
	fprintf(stderr, "Usage: %s %s\n", pmGetProgname(), usage);
	exit(1);
    }

    if ((client = pmhttpNewClient()) == NULL) {
	perror("pmhttpNewClient");
	exit(1);
    }

    if (agent && agent_version)
	pmhttpClientSetUserAgent(client, agent, agent_version);

    if (http_version)
	pmhttpClientSetProtocol(client, version);

    if (timeout.tv_sec)
	pmhttpClientSetTimeout(client, &timeout);

    while (optind < argc) {
	if (verbose)
	    printf("<-- GET %s -->\n", argv[optind]);

	c = pmhttpClientGet(client, argv[optind], unix_location,
			&buf, &buflen, &type, &typelen);
	if (c < 0) {
	    fprintf(stderr, "Failed HTTP GET %s [%d]\n", argv[optind], c);
	    code = 1;
	} else if (buflen == 0) {
	    printf("Response with empty body\n");
	} else {
	    if (verbose) {
		printf("URL: %s\n", argv[optind]);
		printf("Bytes: %zu\n", buflen);
		printf("Content-type: %s\n", type);
		printf("Body:\n");
	    }
	    printf("%.*s", (int)buflen, buf);
	}
	optind++;
    }

    pmhttpFreeClient(client);
    exit(code);
}
