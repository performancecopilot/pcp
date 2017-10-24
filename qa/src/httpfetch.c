/*
 * Copyright (c) 2016 Red Hat.
 * Check the pmhttp.h / libpcp_web client APIs
 */

#include <pcp/pmapi.h>
#include <pcp/pmhttp.h>
#include <pcp/impl.h>

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
    struct timeval	timeout = { 0 };
    static const char	*usage = "[-aAptV] url";
    struct http_client	*client;

    __pmSetProgname(argv[0]);
    while ((c = getopt(argc, argv, "a:A:D:t:vV:?")) != EOF) {
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
	fprintf(stderr, "Usage: %s %s\n", pmProgname, usage);
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
	char buf[BUFSIZ];
	char type[64] = {0};

	if (verbose)
	    printf("<-- GET %s -->\n", argv[optind]);

	c = pmhttpClientFetch(client, argv[optind], buf, sizeof(buf),
				type, sizeof(type));
	if (c < 0) {
	    fprintf(stderr, "Failed to fetch %s [%d]\n", argv[optind], c);
	    code = 1;
	} else if (c == 0) {
	    printf("Response with empty body\n");
	} else {
	    if (verbose) {
		printf("URL: %s\n", argv[optind]);
		printf("Bytes: %d\n", c);
		printf("Content-type: %s\n", type);
		printf("Body:\n");
	    }
	    printf("%.*s", c, buf);
	}
	optind++;
    }

    pmhttpFreeClient(client);
    exit(code);
}
