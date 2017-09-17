/*
 * Copyright (c) 2014,2016-2017 Red Hat.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 */

#include <ctype.h>
#include "pmapi.h"
#include "impl.h"
#include "pmhttp.h"
#include "http_client.h"
#include "http_parser.h"

#define DEFAULT_READ_TIMEOUT	1	/* seconds to wait before timing out */
#define DEFAULT_MAX_REDIRECT	3	/* number of HTTP redirects to follow */
#define HTTP_PORT		80	/* HTTP server port */

#define HTTP			"http"
#define UNIX			"unix"
#define LOCATION		"location"
#define CONTENT_TYPE		"content-type"

static int
http_client_connectunix(const char *path, struct timeval *timeout)
{
#if defined(HAVE_STRUCT_SOCKADDR_UN)
    __pmFdSet		wfds;
    __pmSockAddr	*myAddr;
    struct timeval	stv, *ptv;
    int			fdFlags = 0;
    int			fd = -1;
    int			sts;
    int			rc;

    /* Initialize the socket address. */
    if ((myAddr = __pmSockAddrAlloc()) == NULL) {
	if (pmDebugOptions.http)
	    fprintf(stderr, "HTTP connect unix(%s): out of memory\n", path);
	return -ENOMEM;
    }
    __pmSockAddrSetFamily(myAddr, AF_UNIX);
    __pmSockAddrSetPath(myAddr, path);

    if ((fd = __pmCreateUnixSocket()) < 0) {
	if (pmDebugOptions.http) {
	    char	errmsg[PM_MAXERRMSGLEN];
	    fprintf(stderr, "HTTP connect unix(%s) unable to create socket: %s\n",
		    path, osstrerror_r(errmsg, sizeof(errmsg)));
	}
	__pmSockAddrFree(myAddr);
	return fd;
    }

    /* Attempt to connect */
    fdFlags = __pmConnectTo(fd, myAddr, -1);
    __pmSockAddrFree(myAddr);
    if (fdFlags < 0)
	return -ECONNREFUSED;

    /* FNDELAY and we're in progress - wait on select */
    stv = *timeout;
    ptv = (stv.tv_sec || stv.tv_usec) ? &stv : NULL;
    __pmFD_ZERO(&wfds);
    __pmFD_SET(fd, &wfds);
    sts = 0;
    if ((rc = __pmSelectWrite(fd+1, &wfds, ptv)) == 1)
	sts = __pmConnectCheckError(fd);
    else if (rc == 0)
	sts = ETIMEDOUT;
    else
	sts = (rc < 0) ? neterror() : EINVAL;

    if (sts != 0) {
	/* Unsuccessful connection. */
	if (sts == ENOENT)
	    sts = ECONNREFUSED;
	__pmCloseSocket(fd);
	fd = -sts;
    }

    if (fd < 0)
	return fd;

    /*
     * If we're here, it means we have a valid connection; restore the
     * flags and make sure this file descriptor is closed if exec() is
     * called
     */
    return __pmConnectRestoreFlags(fd, fdFlags);

#else
    if (pmDebugOptions.http)
	__pmNotifyErr(LOG_ERR, "HTTP connect unix(%s) not supported\n", path);
    return -EOPNOTSUPP;
#endif
}

static int
http_client_connectto(const char *host, int port, struct timeval *timeout)
{
    struct timeval	stv, *ptv;
    __pmHostEnt		*servInfo;
    __pmSockAddr	*myAddr;
    __pmFdSet		readyFds, allFds;
    void		*enumIx;
    int			fdFlags[FD_SETSIZE];
    int			i, fd, sts, maxFd;

    if ((servInfo = __pmGetAddrInfo(host)) == NULL) {
	if (pmDebugOptions.http)
	    fprintf(stderr, "HTTP connect(%s, %d): hosterror=%d, ``%s''\n",
		    host, port, hosterror(), hoststrerror());
	return -EHOSTUNREACH;
    }
    /*
     * We want to respect the connect timeout that has been configured, but we
     * may have more than one address to try.  Do this by creating a socket for
     * each address and then using __pmSelectWrite() to wait for one of them to
     * respond.  That way, the timeout is applied to all of the addresses
     * simultaneously.  First, create the sockets, add them to the fd set and
     * try to establish connectections.
     */
    __pmFD_ZERO(&allFds);
    maxFd = -1;
    enumIx = NULL;
    for (myAddr = __pmHostEntGetSockAddr(servInfo, &enumIx);
	 myAddr != NULL;
	 myAddr = __pmHostEntGetSockAddr(servInfo, &enumIx)) {
	/* Create a socket */
	if (__pmSockAddrIsInet(myAddr))
	    fd = __pmCreateSocket();
	else if (__pmSockAddrIsIPv6(myAddr))
	    fd = __pmCreateIPv6Socket();
	else {
	    if (pmDebugOptions.http)
		fprintf(stderr, "HTTP connect(%s, %d): bad address family %d\n",
			host, port, __pmSockAddrGetFamily(myAddr));
	    fd = -EINVAL;
	}
	if (fd < 0) {
	    __pmSockAddrFree(myAddr);
	    continue; /* Try the next address */
	}

	/* Attempt to connect */
	fdFlags[fd] = __pmConnectTo(fd, myAddr, port);
	__pmSockAddrFree(myAddr);
	if (fdFlags[fd] < 0)
	    continue;

	/* Add it to the fd set. */
	__pmFD_SET(fd, &allFds);
	if (fd > maxFd)
	    maxFd = fd;
    }
    __pmHostEntFree(servInfo);

    /* If we were unable to open any sockets, then give up. */
    if (maxFd == -1)
	return -ECONNREFUSED;

    /* FNDELAY and we're in progress - wait on select */
    __pmFD_COPY(&readyFds, &allFds);
    stv = *timeout;
    ptv = (stv.tv_sec || stv.tv_usec) ? &stv : NULL;
    sts = __pmSelectWrite(maxFd+1, &readyFds, ptv);

    /* Figure out what happened. */
    if (sts == 0)
	fd = -ETIMEDOUT;
    else if (sts < 0)
	fd = -neterror();
    else {
	/* Scan fd set, find first successfully connected socket (if any). */
	fd = -EINVAL;
	for (i = 0; i <= maxFd; ++i) {
	    if (__pmFD_ISSET(i, &readyFds)) {
		/* Successful connection? */
		sts = __pmConnectCheckError(i);
		if (sts == 0) {
		    fd = i;
		    break;
		}
		fd = -sts;
	    }
	}
    }

    /* Clean up the unused fds. */
    for (i = 0; i <= maxFd; ++i) {
	if (i != fd && __pmFD_ISSET(i, &allFds))
	    __pmCloseSocket(i);
    }

    if (fd < 0)
	return fd;

    /*
     * If we're here, it means we have a valid connection; restore the
     * flags and make sure this file descriptor is closed if exec() is
     * called
     */
    return __pmConnectRestoreFlags(fd, fdFlags[fd]);
}
    
static void
http_client_disconnect(http_client *cp)
{
    if (cp->fd != -1)
	__pmCloseSocket(cp->fd);
    cp->fd = -1;
}

static int
http_client_connect(http_client *cp)
{
    http_parser_url	*up = &cp->parser_url;
    const char		*protocol, *url = cp->url;
    size_t		length;

    if (pmDebugOptions.http)
	fprintf(stderr, "http_client_connect fd=%d\n", cp->fd);

    if (cp->fd != -1)	/* already connected */
	return 0;

    protocol = url + up->field_data[UF_SCHEMA].off;
    length = up->field_data[UF_SCHEMA].len;

    if (length == sizeof(HTTP)-1 && strncmp(protocol, HTTP, length) == 0) {
	char	host[MAXHOSTNAMELEN];
	int	port;

	if (!up->field_data[UF_HOST].len) {
	    cp->error_code = -EINVAL;
	    return -1;
	}
	length = 1;	/* just the terminator here */
	length += up->field_data[UF_HOST].len;
	if (length > MAXHOSTNAMELEN) {
	    cp->error_code = -EINVAL;
	    return -1;
	}
	length = up->field_data[UF_HOST].len;
	strncpy(host, url + up->field_data[UF_HOST].off, length);
	host[length] = '\0';
	port = up->port ? up->port : HTTP_PORT;

	cp->fd = http_client_connectto(host, port, &cp->timeout);
	return cp->fd;
    }

    if (length == sizeof(UNIX)-1 && strncmp(protocol, UNIX, length) == 0) {
	char	path[MAXPATHLEN];

	if (!up->field_data[UF_HOST].len || !up->field_data[UF_PATH].len) {
	    cp->error_code = -EINVAL;
	    return -1;
	}
	length = 3;	/* 2 separators, terminator */
	length += up->field_data[UF_HOST].len;
	length += up->field_data[UF_PATH].len;
	if (length > MAXPATHLEN) {
	    cp->error_code = -EINVAL;
	    return -1;
	}
	pmsprintf(path, sizeof(path), "/%.*s/%.*s",
		up->field_data[UF_HOST].len, url + up->field_data[UF_HOST].off,
		up->field_data[UF_PATH].len, url + up->field_data[UF_PATH].off);
	cp->fd = http_client_connectunix(path, &cp->timeout);
	return cp->fd;
    }

    return -EPROTO;
}

static const char *
http_versionstr(http_protocol version)
{
    switch (version) {
    case PV_HTTP_1_0:
	return "1.0";
    case PV_HTTP_1_1:
	return "1.1";
    default:
	break;
    }
    return NULL;
}

static int
http_client_get(http_client *cp)
{
    char		buf[BUFSIZ];
    char		host[MAXHOSTNAMELEN];
    char		*bp = &buf[0], *url = cp->url;
    http_parser_url	*up = &cp->parser_url;
    const char		*path, *agent, *version, *protocol;
    size_t		hostlen, len = 0, length;
    int			sts;


    /* sanitize request parameters */
    if ((agent = cp->user_agent) == NULL)
	agent = pmProgname;
    if ((version = cp->agent_vers) == NULL)
	version = "1.0";
    if ((path = url + up->field_data[UF_PATH].off) == NULL ||
	up->field_data[UF_PATH].off == 0 ||
	strchr(path, '/') == NULL){
	path = "/";	/* assume root-level request */
    }
    hostlen = up->field_data[UF_HOST].len;
    strncpy(host, url + up->field_data[UF_HOST].off, hostlen);
    host[hostlen] = '\0';
    //    strncpy(host, "localhost", sizeof("localhost"));
    //    host[sizeof("localhost")] = '\0';
    //    strncpy(path, "/containers/8d70f8a47a6b6e515fb8e40d31da7928de70e883c235ba16b132e6a3b4f8267d/json", sizeof("/containers/8d70f8a47a6b6e515fb8e40d31da7928de70e883c235ba16b132e6a3b4f8267d/json"));
    //    __pmNotifyErr(LOG_DEBUG, "hit here: %s", cp->type_buffer);
    
    protocol = url + up->field_data[UF_SCHEMA].off;
    length = up->field_data[UF_SCHEMA].len;
    /* prepare and send a GET request */
    if (length == sizeof(HTTP)-1 && strncmp(protocol, UNIX, length) == 0) {
	len += pmsprintf(bp+len, sizeof(buf)-len, "GET %s HTTP/%s\r\n",
			cp->type_buffer, http_versionstr(cp->http_version));
	len += pmsprintf(bp+len, sizeof(buf)-len, "Host: %s\r\n", "localhost");
    }
    else {
	len += pmsprintf(bp+len, sizeof(buf)-len, "GET %s HTTP/%s\r\n",
			path, http_versionstr(cp->http_version));
	len += pmsprintf(bp+len, sizeof(buf)-len, "Host: %s\r\n", host);
    }
    len += pmsprintf(bp+len, sizeof(buf)-len, "User-Agent: %s/%s\r\n",
			agent, version);
    /* establish persistent connections (default in HTTP/1.1 onward) */
    if (cp->http_version < PV_HTTP_1_1)
	len += pmsprintf(bp+len, sizeof(buf)-len, "Connection: keep-alive\r\n");
    len += pmsprintf(bp+len, sizeof(buf)-len, "\r\n");
    buf[BUFSIZ-1] = '\0';

    if (pmDebugOptions.http && pmDebugOptions.desperate)
	fprintf(stderr, "Sending HTTP request:\n\n%s\n", buf);

    if ((sts = __pmSend(cp->fd, buf, len, 0)) < 0) {
	if (__pmSocketClosed()) {
	    sts = 1;
	} else {
	    cp->error_code = sts;
	    sts = -1;
	}
	http_client_disconnect(cp);
    } else {
	sts = 0;
    }

    if (pmDebugOptions.http)
	fprintf(stderr, "http_client_get sts=%d\n", sts);

    return sts;
}

static int
on_headers_complete(http_parser *pp)
{
    http_client 	*cp = (http_client *)pp->data;

    if (cp->flags & F_REDIRECTED)
	return 1;
    return 0;
}

static int
on_header_field(http_parser *pp, const char *offset, size_t length)
{
    http_client		*cp = (http_client *)pp->data;

    if (pmDebugOptions.http)
	fprintf(stderr, "Header field: %.*s\n", (int)length, offset);

    if (length == sizeof(LOCATION)-1 &&
	strncasecmp(offset, LOCATION, length) == 0)
	cp->flags |= F_LOCATION;
    else
	cp->flags &= ~F_LOCATION;

    if (length == sizeof(CONTENT_TYPE)-1 &&
	strncasecmp(offset, CONTENT_TYPE, length) == 0)
	cp->flags |= F_CONTENT_TYPE;
    else
	cp->flags &= ~F_CONTENT_TYPE;

    /* "Connection: close" is handled via parser flags */
    return 0;
}

/*
 * Handle absolute and relative URL locations (redirects).
 * If it is relative, we need to preserve the original URL
 * schema/host/port component.
 */
static int
reset_url_location(const char *tourl, size_t tolen, http_parser_url *top,
                   char **fromurl, http_parser_url *fromp)
{
    http_parser_url	*cp = fromp;
    char		*curl = *fromurl;
    const char		*suffix;
    char		*url, *str;
    int			size, bytes, length;
    int			flags = F_REDIRECTED;

    /* if relative location, stay connected and fast-track it */
    length = top->field_data[UF_SCHEMA].len + top->field_data[UF_HOST].len;
    if (length == 0) {
	/* build URL with host, schema, port from original */
	if ((bytes = cp->field_data[UF_PATH].off) == 0)
	    bytes = strlen(curl);
	length = (tolen - top->field_data[UF_PATH].off);
	suffix = (tourl + top->field_data[UF_PATH].off);
	size = bytes + length + 2;	/* separator + terminator */
	if ((url = malloc(size)) == NULL)
	    return -ENOMEM;
	strncpy(url, curl, bytes);
	str = url + bytes;
	if (*suffix != '/')
	    *str++ = '/';
	strncat(str, suffix, length);
	url[size - 1] = '\0';
	http_parser_parse_url(url, size, 0, fromp);

	if (pmDebugOptions.http)
	    fprintf(stderr, "Redirecting from '%s' to '%s'\n", curl, url);
	free(curl);
	*fromurl = url;
	return flags;
    }

    /*
     * Redirecting to absolute locations life is simpler.  However
     * if we have a new/different schema/host/port flag that so we
     * can avoid the teardown/reconnect to the same server later.
     */
    bytes = top->field_data[UF_HOST].len;
    if (cp->field_data[UF_HOST].len != bytes)
	flags |= F_DISCONNECT;
    else if (strncmp(tourl + top->field_data[UF_HOST].off,
		curl + cp->field_data[UF_HOST].off, bytes) != 0)
	flags |= F_DISCONNECT;
    if (top->port == 0)
	top->port = HTTP_PORT;
    if (top->port != cp->port)
	flags |= F_DISCONNECT;

    *fromurl = (char *)tourl;
    *fromp = *top;
    return flags;
}

static int
on_header_value(http_parser *pp, const char *offset, size_t length)
{
    http_client		*cp = (http_client *)pp->data;
    int			sts;

    if (pmDebugOptions.http)
	fprintf(stderr, "Header value: %.*s\n", (int)length, offset);

    if (cp->flags & F_LOCATION) {	/* redirect location */
	cp->flags &= ~F_LOCATION;

	if (pp->status_code >= 300 && pp->status_code < 400) {
	    http_parser_url	up;

	    http_parser_url_init(&up);
	    if ((sts = http_parser_parse_url(offset, length, 0, &up)) != 0)
		return sts;
	    if ((sts = reset_url_location(offset, length, &up,
					&cp->url, &cp->parser_url)) < 0) {
		cp->error_code = -ENOMEM;
		return 1;
	    }
	    cp->flags |= sts;
	}
    }

    if (cp->flags & F_CONTENT_TYPE) {	/* stash content-type */
	cp->flags &= ~F_CONTENT_TYPE;

	if (cp->type_length > 0) {
	    if (length + 1 > cp->type_length) {
		cp->error_code = -E2BIG;
		return 1;
	    }
	    strncpy(cp->type_buffer, offset, length);
	    cp->type_buffer[length] = '\0';
	}
    }

    return 0;
}

static int
on_body(http_parser *pp, const char *offset, size_t length)
{
    http_client		*cp = (http_client *)pp->data;

    if (pmDebugOptions.http)
	fprintf(stderr, "Body: %.*s\n", (int)length, offset);

    if (length > cp->body_length - cp->offset) {
	cp->error_code = -E2BIG;
	return 1;
    }
    strncpy(cp->body_buffer + cp->offset, offset, length);
    cp->offset += length;
    return 0;
}

static int
on_message_complete(http_parser *pp)
{
    http_client		*cp = (http_client *)pp->data;

    cp->flags |= F_MESSAGE_END;
    return 0;
}

static int
http_should_client_disconnect(http_client *cp)
{
    if (!(cp->flags & F_DISCONNECT))
	return 0;
    cp->flags &= ~F_DISCONNECT;
    return 1;
}

static int
http_should_client_redirect(http_client *cp)
{
    if (!(cp->flags & F_REDIRECTED))
	return 0;
    cp->flags &= ~F_REDIRECTED;
    return 1;
}

static int
http_client_response(http_client *cp)
{
    size_t		bytes;
    char		buffer[BUFSIZ];
    int			sts;
    static int		setup;
    static http_parser_settings	settings;

    if (!setup) {
	memset(&settings, 0, sizeof(settings));
	settings.on_header_field = on_header_field;
	settings.on_header_value = on_header_value;
	settings.on_headers_complete = on_headers_complete;
	settings.on_body = on_body;
	settings.on_message_complete = on_message_complete;
	setup = 1;
    }

    if (pmDebugOptions.http)
	fprintf(stderr, "http_client_response\n");

    http_parser_init(&cp->parser, HTTP_RESPONSE);
    cp->parser.data = (void *)cp;
    cp->error_code = 0;
    cp->offset = 0;

    do {
	if ((sts = __pmRecv(cp->fd, buffer, sizeof(buffer), 0)) <= 0) {
	    http_client_disconnect(cp);
	    return sts ? sts : -EAGAIN;
	}
	bytes = http_parser_execute(&cp->parser, &settings, buffer, sts);

    } while (bytes && !(cp->flags & F_MESSAGE_END));

    if (http_should_client_disconnect(cp))
	http_client_disconnect(cp);

    if (http_should_client_redirect(cp))
	return -EMLINK;

    if (http_should_keep_alive(&cp->parser) == 0)
	http_client_disconnect(cp);

    if (cp->error_code)
	return cp->error_code;

    return cp->offset;
}

http_client *
pmhttpNewClient(void)
{
    http_client	*cp;

    if ((cp = calloc(1, sizeof(http_client))) == NULL)
	return NULL;
    cp->timeout.tv_sec = DEFAULT_READ_TIMEOUT;
    cp->max_redirect = DEFAULT_MAX_REDIRECT;
    cp->http_version = PV_HTTP_1_1;
    cp->user_agent = pmProgname;
    cp->agent_vers = pmGetOptionalConfig("PCP_VERSION");
    cp->fd = -1;
    return cp;
}

void
pmhttpFreeClient(http_client *cp)
{
    http_client_disconnect(cp);
    free(cp);
}

int
pmhttpClientSetTimeout(http_client *cp, struct timeval *tv)
{
    cp->timeout = *tv;	/* struct copy */
    return 0;
}

int
pmhttpClientSetProtocol(http_client *cp, enum http_protocol version)
{
    if (version >= PV_MAX)
	return -ENOTSUP;
    cp->http_version = version;
    return 0;
}

int
pmhttpClientSetUserAgent(http_client *cp, const char *agent, const char *vers)
{
    cp->user_agent = agent;
    cp->agent_vers = vers;
    return 0;
}

static int
http_compare_source(http_parser_url *a, const char *urla,
                    http_parser_url *b, const char *urlb)
{
    const char	*protocol;
    int		length;

    if (a->field_data[UF_SCHEMA].len != b->field_data[UF_SCHEMA].len ||
	a->field_data[UF_HOST].len != b->field_data[UF_HOST].len)
	return 1;

    protocol = urla + a->field_data[UF_SCHEMA].off;
    length = a->field_data[UF_SCHEMA].len;

    if (length == sizeof(HTTP)-1 && strncmp(protocol, HTTP, length) == 0) {
	if (a->port != b->port)
	    return 1;
	if (strncmp(urla + a->field_data[UF_SCHEMA].off,
		    urlb + b->field_data[UF_SCHEMA].off, length) == 0)
	    return 1;
	return 0;
    }
    if (length == sizeof(UNIX)-1 && strncmp(protocol, UNIX, length) == 0) {
	if (a->field_data[UF_PATH].len != b->field_data[UF_PATH].len)
	    return 1;
	return 0;
    }
    return 0;
}

static int
http_client_prepare(http_client *cp, const char *url,
		char *body_buffer, size_t body_length,
		char *type_buffer, size_t type_length)
{
    http_parser_url	parser_url;
    char		*new_url;
    int			sts;

    cp->body_buffer = body_buffer;
    cp->body_length = body_length;
    cp->type_buffer = type_buffer;
    cp->type_length = type_length;

    /* extract individual fields from the given URL */
    http_parser_url_init(&parser_url);
    if ((sts = http_parser_parse_url(url, strlen(url), 0, &parser_url)) != 0) {
	cp->error_code = sts;
	return -1;
    }

    /* short-circuit if we are making a request from a connected server */
    if (http_compare_source(&parser_url, url, &cp->parser_url, cp->url) == 0)
	return 0;
    http_client_disconnect(cp);

    if ((new_url = strdup(url)) == NULL) {
	cp->error_code = -ENOMEM;
	return -1;
    }
    free(cp->url);
    cp->url = new_url;
    cp->parser_url = parser_url;
    return 0;
}

int
pmhttpClientFetch(http_client *cp, const char *url,
		char *body_buffer, size_t body_length,
		char *type_buffer, size_t type_length)
{
    int		sts = 0, redirected = 0;

    if (pmDebugOptions.http)
	fprintf(stderr, "pmhttpClientFetch: %s\n", url);

    if (http_client_prepare(cp, url, body_buffer, body_length,
			    type_buffer, type_length) != 0)
	return -1;

    while (redirected <= cp->max_redirect) {
	/* ensure we're connected to the server */
	if ((sts = http_client_connect(cp)) < 0)
	    return sts;

	/* make a GET request via the given URL */
	if ((sts = http_client_get(cp)) < 0)
	    return sts;
	if (sts == 1)	/* server connection is lost */
	    continue;

	/* parse, extract body, handle redirect */
	if ((sts = http_client_response(cp)) < 0) {
	    if (sts == -EAGAIN)		/* server closed */
		continue;
	    if (sts == -EMLINK) {	/* http redirect */
		redirected++;
		continue;
	    }
	    break;	/* propogate errors */
	}
	break;	/* successful exchange */
    }

    if (pmDebugOptions.http)
	fprintf(stderr, "pmhttpClientFetch sts=%d\n", sts);

    return sts;
}
