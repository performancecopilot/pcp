/*
 * Copyright (c) 2016 Red Hat.
 * Check the pmhttp.h / libpcp_web client APIs
 */

#include <pcp/pmapi.h>
#include <pcp/pmhttp.h>

static struct http_client  *client;
static char sock[256];

int
call_http_get(char *url)
{
    static size_t       buflen, typelen;
    static char         *buf, *type;
    int                 c, code = 0;

    c = pmhttpClientGet(client, url, NULL,
		    &buf, &buflen, &type, &typelen);
    if (c < 0) {
	    fprintf(stderr, "Failed HTTP GET %s [%d]\n", url, c);
	    code = 1;
    } else if (buflen == 0) {
	    printf("Response with empty body\n");
    } else {
	    printf("%.*s", (int)buflen, buf);
    }
    return code;
}

int
call_unix_domain_get(char *url)
{
    static size_t       buflen, typelen;
    static char         *buf, *type;
    int                 c, code = 0;

    c = pmhttpClientGet(client, sock, url,
		    &buf, &buflen, &type, &typelen);
    if (c < 0) {
	    fprintf(stderr, "Failed HTTP GET %s [%d]\n", url, c);
	    code = 1;
    } else if (buflen == 0) {
	    printf("Response with empty body\n");
    } else {
	    printf("%.*s", (int)buflen, buf);
    }
    return code;
}


int
main(int argc, char *argv[])
{
    int code = 0;
    char url[256];
    char port[6];

    pmSetProgname(argv[0]);

    if (argc != 3) {
	    fprintf(stderr, "Usage: %s <socket_path> <port_number>\n", argv[0]);
	    return 1;
    }

    pmstrncpy(sock, sizeof(sock), argv[1]);
    pmstrncpy(port, sizeof(port), argv[2]);

    if ((client = pmhttpNewClient()) == NULL) {
	perror("pmhttpNewClient");
	exit(1);
    }

    pmsprintf(url, sizeof(url), "http://localhost:%s/series/metrics?match=kernel.all.load", port);
    code |= call_http_get(url);
    code |= call_http_get(url);
    pmsprintf(url, sizeof(url), "http://localhost:%s/series/metrics?match=kernel.all.intr", port);
    code |= call_http_get(url);

    code |= call_unix_domain_get("/series/metrics?match=kernel.all.load");
    code |= call_unix_domain_get("/series/metrics?match=kernel.all.load");
    code |= call_unix_domain_get("/series/metrics?match=kernel.all.intr");

    pmhttpFreeClient(client);
    exit(code);
}
