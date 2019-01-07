/*
 * Copyright (c) 2018 Red Hat.
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */
#include "server.h"

#define PMPROXY_SERVER	"pmproxy-server 1\n"
#define PMPROXY_CLIENT	"pmproxy-client 1\n"
#define HEADER_LENGTH	(sizeof(PMPROXY_CLIENT)-1)
#define PDU_MAXLENGTH	(MAXHOSTNAMELEN + HEADER_LENGTH + sizeof("65536")-1)

static void
on_server_close(uv_handle_t *handle)
{
    struct client	*client = (struct client *)handle;

    if (pmDebugOptions.pdu)
	fprintf(stderr, "client %p pmcd connection closed\n", client);
}

static void
on_server_write(uv_write_t *writer, int status)
{
    struct client	*client = (struct client *)writer->handle;
    stream_write_baton	*request = (stream_write_baton *)writer;

    sdsfree(request->buffer[0].base);
    free(request);

    if (status != 0)
	uv_close((uv_handle_t *)&client->stream, on_client_close);
}

static void
server_write(struct client *client, sds buffer)
{
    stream_write_baton	*request = calloc(1, sizeof(stream_write_baton));

    if (request) {
	if (pmDebugOptions.pdu)
	    fprintf(stderr, "%s: %ld bytes from client %p to pmcd\n",
			"server_write", (long)sdslen(buffer), client);
	request->buffer[0] = uv_buf_init(buffer, sdslen(buffer));
	uv_write(&request->writer, (uv_stream_t *)&client->u.pcp.socket,
		 request->buffer, 1, on_server_write);
    } else {
	uv_close((uv_handle_t *)&client->stream, on_client_close);
    }
}

static void
on_server_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
{
    uv_handle_t		*handle = (uv_handle_t *)stream;
    struct client	*client = (struct client *)handle->data;
    sds			buffer;

    if (pmDebugOptions.pdu)
	fprintf(stderr, "%s: client %p read %ld bytes from pmcd\n",
			"on_server_read", client, (long)nread);

    /* proxy data through to the client */
    buffer = sdsnewlen(buf->base, nread);
    client_write(client, buffer, NULL);
}

void
on_pcp_client_close(struct client *client)
{
    if (client->u.pcp.connected)
	uv_close((uv_handle_t *)&client->u.pcp.socket, on_server_close);
    if (client->buffer)
	sdsfree(client->buffer);
    memset(&client->u.pcp, 0, sizeof(client->u.pcp));
}

static void
on_pcp_client_connect(uv_connect_t *connected, int status)
{
    uv_handle_t		*handle = (uv_handle_t *)connected;
    struct client	*client = (struct client *)handle->data;
    sds			buffer;

    if (pmDebugOptions.pdu)
	fprintf(stderr, "%s: client %p connected to pmcd (status=%d)\n",
			"on_pcp_client_connect", client, status);

    if (status != 0) {
	uv_close((uv_handle_t *)&client->stream, on_client_close);
	return;
    }

    /* socket connection to pmcd successfully established */
    client->u.pcp.state = PCP_PROXY_SETUP;

    /* if we have already received PDUs, send them on now */
    if ((buffer = client->buffer) != NULL) {
	client->buffer = NULL;	/* free buffer on completion */
	server_write(client, buffer);
    }

    status = uv_read_start((uv_stream_t *)&client->u.pcp.socket,
			    on_buffer_alloc, on_server_read);
    if (status != 0) {
	fprintf(stderr, "%s: server read start failed: %s\n",
			"on_pcp_client_connect", uv_strerror(status));
	uv_close((uv_handle_t *)&client->u.pcp.socket, on_client_close);
    }
}

static void
pcp_client_connect_pmcd(struct client *client)
{
    struct proxy	*proxy = client->proxy;
    struct sockaddr_in	pmcd;
    uv_handle_t		*handle;

    if (pmDebugOptions.context | pmDebugOptions.pdu)
	fprintf(stderr, "%s: connecting to pmcd on host %s, port %u\n",
			"pcp_client_connect_pmcd",
			client->u.pcp.hostname, client->u.pcp.port);

    handle = (uv_handle_t *)&client->u.pcp.pmcd;
    handle->data = (void *)client;
    handle = (uv_handle_t *)&client->u.pcp.socket;
    handle->data = (void *)client;

    uv_tcp_init(proxy->events, &client->u.pcp.socket);
    uv_ip4_addr(client->u.pcp.hostname, client->u.pcp.port, &pmcd);
    uv_tcp_connect(&client->u.pcp.pmcd, &client->u.pcp.socket,
		    (struct sockaddr *)&pmcd, on_pcp_client_connect);
}

static ssize_t
pcp_consume_bytes(struct client *client, const char *base, ssize_t nread)
{
    sds		buffer;

    if (pmDebugOptions.pdu)
	fprintf(stderr, "%s: client %p consuming %ld PDU bytes\n",
			"pcp_consume_bytes", client, (long)nread);

    if ((buffer = client->buffer) == NULL)
	buffer = sdsnewlen(base, nread);
    else
	buffer = sdscatlen(buffer, base, nread);
    client->buffer = buffer;
    return sdslen(buffer);
}

static int
pcp_consume_client_hostspec(struct client *client, char *buffer, ssize_t buflen)
{
    char	*host = buffer, *port = NULL, *endnum, *bp;

    for (bp = buffer; bp - buffer < buflen; bp++) {
	if (*bp == '\n') {
	    *bp = '\0';
	    break;
	}
	if (*bp == ' ') {
	    if (port)
		return -EINVAL;	/* maximum of one port number */
	    port = bp + 1;
	    *bp = '\0';
	}
    }
    if (bp - buffer == buflen)
	return -EINVAL;		/* end of hostspec not found */
    if (port == NULL)
	return -EINVAL;		/* port number was not found */
    client->u.pcp.port = strtoul(port, &endnum, 10);
    if (*endnum != '\0')
	return -EINVAL;		/* invalid port number given */
    client->u.pcp.hostname = sdsnew(host);
    client->u.pcp.state = PCP_PROXY_CONNECT;

    /* some PDU bytes have already arrived? - buffer them up */
    if (bp - buffer < buflen - 1) {
	if (client->buffer) {
	    sdsfree(client->buffer);
	    client->buffer = NULL;
	}
	pcp_consume_bytes(client, bp + 1, buflen - (bp - buffer));
    }

    /* initiate the connection to pmcd */
    pcp_client_connect_pmcd(client);
    return 0;
}

static int
pcp_consume_client_header(struct proxy *proxy, struct client *client,
			  char *buffer, ssize_t buflen)
{
    if (strncmp(buffer, PMPROXY_CLIENT, HEADER_LENGTH) == 0) {
	/* next state is to consume pmcd hostname/port (target) */
	client->u.pcp.state = PCP_PROXY_HOSTSPEC;
	/* negotiate proxy server protocol version with client */
	client_write(client, sdsnew(PMPROXY_SERVER), NULL);
	/* has part/all of the pmcd target line also arrived? */
	if (buflen > HEADER_LENGTH) {
	    buflen -= HEADER_LENGTH;
	    buffer += HEADER_LENGTH;
	    return pcp_consume_client_hostspec(client, buffer, buflen);
	}
	return 0;
    }
    return -EINVAL;
}

void
on_pcp_client_read(struct proxy *proxy, struct client *client,
		ssize_t nread, const uv_buf_t *buf)
{
    sds		part;
    size_t	bytes;

    if (pmDebugOptions.pdu)
	fprintf(stderr, "%s: read %ld bytes from PCP client %p (state=%x)\n",
		"on_pcp_client_read", (long)nread, client, client->u.pcp.state);

    if (nread <= 0)
	return;

    switch (client->u.pcp.state) {
    case PCP_PROXY_UNKNOWN:
    case PCP_PROXY_HEADER:
	/*
	 * Consume the 'pmproxy-client 1\n' string and change state.
	 * Fast path - all data arrives at once, so no need to stash
	 * any bytes for later.
	 */
	if (client->buffer == NULL && nread >= HEADER_LENGTH) {
	    if (pcp_consume_client_header(proxy, client, buf->base, nread) < 0)
		uv_close((uv_handle_t *)&client->stream, on_client_close);
	} else if (nread < PDU_MAXLENGTH) {
	    bytes = pcp_consume_bytes(client, buf->base, nread);
	    if (bytes >= HEADER_LENGTH) {
		part = client->buffer;
		if (pcp_consume_client_header(proxy, client, part, bytes) < 0)
		    uv_close((uv_handle_t *)&client->stream, on_client_close);
	    }
	} else {
	    /* PDU is too large, tear down the client connection */
	    uv_close((uv_handle_t *)&client->stream, on_client_close);
	}
	break;

    case PCP_PROXY_HOSTSPEC:
	/* consume the host name and port for connecting to pmcd */
	pcp_consume_client_hostspec(client, buf->base, nread);
	break;

    case PCP_PROXY_CONNECT:
	/* not connected yet but a PDU has arrived - buffer until connected */
	pcp_consume_bytes(client, buf->base, nread);
	break;

    case PCP_PROXY_SETUP:
	/* initial setup is now complete - direct proxying from here onward */
	sdssetlen(buf->base, nread);
	server_write(client, buf->base);
	break;
    }
}

void
setup_pcp_modules(struct proxy *proxy)
{
    /* no PCP protocol modules */
}
