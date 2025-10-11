/*
 * Copyright (c) 2016 Red Hat.
 * Check the pmhttp.h / libpcp_web client APIs
 */

#include <pcp/pmapi.h>
#include <pcp/pmhttp.h>

static struct http_client  *client;
static char *sock = "unix://tmp/httpcache.sock";

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

    pmSetProgname(argv[0]);

    if ((client = pmhttpNewClient()) == NULL) {
	perror("pmhttpNewClient");
	exit(1);
    }

    code |= call_http_get("http://localhost:44323/series/metrics?match=kernel.all.load");
    code |= call_http_get("http://localhost:44323/series/metrics?match=kernel.all.load");
    code |= call_http_get("http://localhost:44323/series/metrics?match=kernel.all.intr");

    code |= call_unix_domain_get("/series/metrics?match=kernel.all.load");
    code |= call_unix_domain_get("/series/metrics?match=kernel.all.load");
    code |= call_unix_domain_get("/series/metrics?match=kernel.all.intr");

    pmhttpFreeClient(client);
    exit(code);
}
