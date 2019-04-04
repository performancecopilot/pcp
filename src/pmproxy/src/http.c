/*
 * Copyright (c) 2019 Red Hat.
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
#include <assert.h>
#include "server.h"
#include "dict.h"
#include "util.h"

static int chunked_transfer_size; /* pmproxy.chunksize, pagesize by default */
static int smallest_buffer_size = 128;

/*
 * Simple helpers to manage the cumlative addition of JSON
 * (arrays and/or objects) to a buffer.
 */
sds
json_push_suffix(sds suffix, json_flags type)
{
    size_t	length;

    if (type != JSON_FLAG_ARRAY && type != JSON_FLAG_OBJECT)
	return suffix;

    if (suffix == NULL) {
	if (type == JSON_FLAG_ARRAY)
	    return sdsnewlen("]\r\n", 3);
	return sdsnewlen("}\r\n", 3);
    }

    /* prepend to existing string */
    length = sdslen(suffix);
    suffix = sdsgrowzero(suffix, length + 1);
    memmove(suffix+1, suffix, length);
    suffix[0] = (type == JSON_FLAG_ARRAY)? ']' : '}';
    return suffix;
}

sds
json_pop_suffix(sds suffix)
{
    size_t	length;

    /* chop first character - no resize, pad with null terminators */
    if (suffix) {
	length = sdslen(suffix);
	memmove(suffix, suffix+1, length-1);
    }
    return suffix;
}

static inline int
ishex(int x)
{
    return (x >= '0' && x <= '9') ||
	   (x >= 'a' && x <= 'f') ||
	   (x >= 'A' && x <= 'F');
}

int
http_decode(const char *url, size_t urllen, sds buf)
{
    const char		*end = url + urllen;
    char		*out;
    int			c;

    for (out = buf; url < end; out++) {
	c = *url++;
	if (c == '+')
	    c = ' ';
	if (c == '%' &&
	    (!ishex(*url++) || !ishex(*url++) || !sscanf(url - 2, "%2x", &c)))
	    return -EINVAL;
	if (out - buf > sdslen(buf))
	    return -E2BIG;
	*out = c;
    }
    *out = '\0';
    c = out - buf;
    assert(c <= sdslen(buf));
    sdssetlen(buf, c);
    return c;
}

const char *
http_content_type(http_flags flags)
{
    if (flags & HTTP_FLAG_JSON)
	return "application/json";
    if (flags & HTTP_FLAG_HTML)
	return "text/html";
    if (flags & HTTP_FLAG_TEXT)
	return "text/plain";
    if (flags & HTTP_FLAG_JS)
	return "text/javascript";
    if (flags & HTTP_FLAG_CSS)
	return "text/css";
    if (flags & HTTP_FLAG_ICO)
	return "image/x-icon";
    if (flags & HTTP_FLAG_JPG)
	return "image/jpeg";
    if (flags & HTTP_FLAG_PNG)
	return "image/png";
    if (flags & HTTP_FLAG_GIF)
	return "image/gif";
    return "application/octet-stream";
}

http_flags
http_suffix_type(const char *suffix)
{
    if (strcmp(suffix, "js") == 0)
	return HTTP_FLAG_JS;
    if (strcmp(suffix, "ico") == 0)
	return HTTP_FLAG_ICO;
    if (strcmp(suffix, "css") == 0)
	return HTTP_FLAG_CSS;
    if (strcmp(suffix, "png") == 0)
	return HTTP_FLAG_PNG;
    if (strcmp(suffix, "gif") == 0)
	return HTTP_FLAG_GIF;
    if (strcmp(suffix, "jpg") == 0)
	return HTTP_FLAG_JPG;
    if (strcmp(suffix, "jpeg") == 0)
	return HTTP_FLAG_JPG;
    if (strcmp(suffix, "html") == 0)
	return HTTP_FLAG_HTML;
    if (strcmp(suffix, "txt") == 0)
	return HTTP_FLAG_TEXT;
    return 0;
}

static const char * const
http_content_encoding(http_flags flags)
{
    if (flags & HTTP_FLAG_UTF8)
	return "; charset=UTF-8\r\n";
    if (flags & HTTP_FLAG_UTF16)
	return "; charset=UTF-16\r\n";
    return "";
}

static int
http_status_index(http_code code)
{
    static const int	codes[] = {
#define XX(num, name, string) num,
	HTTP_STATUS_MAP(XX)
#undef XX
    };
    int			i;

    for (i = 0; i < sizeof(codes) / sizeof(int); i++) {
	if (codes[i] == code)
	    return i;
    }
    return HTTP_STATUS_OK;
}

const char *
http_status_mapping(http_code code)
{
    static const char  *strings[] = {
#define XX(num, name, string) #string,
	HTTP_STATUS_MAP(XX)
#undef XX
    };
    return strings[http_status_index(code)];
}

char *
http_date_string(time_t stamp, char *buffer, size_t buflen)
{
    static const char	epoch[] = \
	"Thu, 01 Jan 1970 00:00:00 GMT";
    static const char *const   days[] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
    };
    static const char *const   months[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    struct tm		tm;

    if (gmtime_r(&stamp, &tm) == NULL)
	return (char *)epoch;
    pmsprintf(buffer, buflen,
		"%3s, %02u %3s %04u %02u:%02u:%02u GMT",
		days[tm.tm_wday % 7],
		(unsigned int)tm.tm_mday,
		months[tm.tm_mon % 12],
		(unsigned int)(1900 + tm.tm_year),
		(unsigned int)tm.tm_hour,
		(unsigned int)tm.tm_min,
		(unsigned int)tm.tm_sec);
    return buffer;
}

sds
http_get_buffer(struct client *client)
{
    sds		buffer = client->buffer;

    client->buffer = NULL;
    if (buffer == NULL) {
	buffer = sdsnewlen(SDS_NOINIT, smallest_buffer_size);
	sdsclear(buffer);
    }
    return buffer;
}

void
http_set_buffer(struct client *client, sds buffer, http_flags flags)
{
    assert(client->buffer == NULL);
    client->u.http.flags |= flags;
    client->buffer = buffer;
}

static sds
http_response_header(struct client *client, unsigned int length, http_code sts, http_flags flags)
{
    struct http_parser	*parser = &client->u.http.parser;
    char		date[64];
    sds			header;

    if (parser->http_major == 0)
	parser->http_major = parser->http_minor = 1;

    header = sdscatfmt(sdsempty(),
		"HTTP/%u.%u %u %s\r\n"
		"Connection: Keep-Alive\r\n"
		"Access-Control-Allow-Origin: *\r\n",
		parser->http_major, parser->http_minor,
		sts, http_status_mapping(sts));

    if ((flags & HTTP_FLAG_STREAMING))
	header = sdscatfmt(header, "Transfer-encoding: %s\r\n", "chunked");

    if (!(flags & HTTP_FLAG_STREAMING))
	header = sdscatfmt(header, "Content-Length: %u\r\n", length);

    header = sdscatfmt(header,
		"Content-Type: %s%s\r\n"
		"Date: %s\r\n\r\n",
		http_content_type(flags), http_content_encoding(flags),
		http_date_string(time(NULL), date, sizeof(date)));

    if (pmDebugOptions.http && pmDebugOptions.desperate) {
	fprintf(stderr, "reply headers for response to client %p\n", client);
	fputs(header, stderr);
    }
    return header;
}

void
http_close(struct client *client)
{
    uv_close((uv_handle_t *)&client->stream, on_client_close);
}

void
http_reply(struct client *client, sds message, http_code sts, http_flags type)
{
    http_flags		flags = client->u.http.flags;
    char		length[32]; /* hex length */
    sds			buffer, suffix;

    if (flags & HTTP_FLAG_STREAMING) {
	buffer = sdsempty();
	if (client->buffer == NULL) {
	    pmsprintf(length, sizeof(length), "%lX", (unsigned long)sdslen(message));
	    buffer = sdscatfmt(buffer, "%s\r\n%S\r\n", length, message);
	} else {
	    pmsprintf(length, sizeof(length), "%lX",
				(unsigned long)sdslen(client->buffer) + sdslen(message));
	    buffer = sdscatfmt(buffer, "%s\r\n%S%S\r\n",
				length, client->buffer, message);
	    client->buffer = NULL;
	}
	sdsfree(message);
	suffix = sdsnewlen("0\r\n\r\n", 5);		/* chunked suffix */
    } else {	/* regular non-chunked response - headers + response body */
	if (client->buffer == NULL) {
	    suffix = message;
	} else {
	    suffix = sdscatsds(client->buffer, message);
	    sdsfree(message);
	    client->buffer = NULL;
	}
	buffer = http_response_header(client, sdslen(suffix), sts, type);
    }

    if (pmDebugOptions.http) {
	fprintf(stderr, "HTTP response (client=%p)\n%s%s",
			client, buffer, suffix);
    }
    client_write(client, buffer, suffix);
}

void
http_error(struct client *client, http_code status, const char *errstr)
{
    const char		*mapping = http_status_mapping(status);
    struct servlet	*servlet = client->u.http.servlet;
    sds			message;

    /* on error, we must first discard any accumulated partial result */
    if (client->buffer != NULL) {
	sdsfree(client->buffer);
	client->buffer = NULL;
    }

    message = sdscatfmt(sdsempty(),
		"<html>\r\n"
		"<head><title>%u %s</title></head>\r\n"
		"<body>\r\n"
		    "<h1>%u %s</h1>\r\n"
		    "<p><b>%s servlet</b>: %s</p><hr>\r\n"
		    "<p><small><i>%s/%s</i></small></p>\r\n"
		"</body>\r\n"
		"</html>\r\n",
		status, mapping, status, mapping,
		servlet ? servlet->name : "unknown", errstr,
		pmGetProgname(), PCP_VERSION);

    if (pmDebugOptions.http || pmDebugOptions.desperate) {
	fprintf(stderr, "sending error %s (%d) to client %p\n",
			mapping, status, client);
	if (pmDebugOptions.desperate)
	    fputs(message, stderr);
    }
    http_reply(client, message, status, HTTP_FLAG_HTML);
}

void
http_transfer(struct client *client)
{
    struct http_parser	*parser = &client->u.http.parser;
    http_flags		flags = client->u.http.flags;
    sds			buffer, suffix;

    /* If the client buffer length is now beyond a set maximum size,
     * send it using chunked transfer encoding.  Once buffer pointer
     * is copied into the uv_buf_t, clear it in the client, and then
     * return control to caller.
     */
    if (sdslen(client->buffer) >= chunked_transfer_size) {
	if (parser->http_major == 1 && parser->http_minor > 0) {
	    if (!(flags & HTTP_FLAG_STREAMING)) {
		/* send headers (no content length) and initial content */
		flags |= HTTP_FLAG_STREAMING;
		buffer = http_response_header(client, 0, HTTP_STATUS_OK, flags);
		client->u.http.flags = flags;
	    } else {
		/* headers already sent, send the next chunk of content */
		buffer = sdsempty();
	    }
	    /* prepend a chunked transfer encoding message length (hex) */
	    buffer = sdscatprintf(buffer, "%lX\r\n", (unsigned long)sdslen(client->buffer));
	    suffix = sdscatfmt(client->buffer, "\r\n");
	    /* reset for next call, original released on I/O completion */
	    client->buffer = NULL;

	    if (pmDebugOptions.http) {
		fprintf(stderr, "HTTP chunked buffer (client %p)\n%s"
				"HTTP chunked suffix (client %p)\n%s",
				client, buffer, client, suffix);
	    }
	    client_write(client, buffer, suffix);

	} else if (parser->http_major <= 1) {
	    buffer = sdsnew("HTTP 1.0 request result exceeds server limits");
	    http_error(client, HTTP_STATUS_PAYLOAD_TOO_LARGE, buffer);
	}
    }
}

static int
http_add_parameter(dict *parameters,
	const char *name, int namelen, const char *value, int valuelen)
{
    sds			pvalue, pname = sdsnewlen(SDS_NOINIT, namelen);
    int			sts;

    if ((sts = http_decode(name, namelen, pname)) < 0) {
	sdsfree(pname);
	return sts;
    }
    if (valuelen > 0) {
	pvalue = sdsnewlen(SDS_NOINIT, valuelen);
	if ((sts = http_decode(value, valuelen, pvalue)) < 0) {
	    sdsfree(pvalue);
	    sdsfree(pname);
	    return sts;
	}
    } else {
	pvalue = NULL;
    }

    if (pmDebugOptions.http) {
	if (pvalue)
	    fprintf(stderr, "URL parameter %s=%s\n", pname, pvalue);
	else
	    fprintf(stderr, "URL parameter %s\n", pname);
    }

    dictAdd(parameters, pname, pvalue);
    return 0;
}

static int
http_parameters(const char *url, size_t length, dict **parameters)
{
    const char		*end = url + length;
    const char		*p, *name, *value = NULL;
    int			sts = 0, namelen = 0, valuelen = 0;

    *parameters = dictCreate(&sdsDictCallBacks, NULL);
    for (p = name = url; p < end; p++) {
	if (*p == '=') {
	    namelen = p - name;
	    value = p + 1;
	}
	else if (*p == '&') {
	    valuelen = p - value;
	    sts = http_add_parameter(*parameters, name, namelen, value, valuelen);
	    if (sts < 0)
		break;
	    value = NULL;
	    name = p + 1;
	}
    }
    if (p == end && p != name) {
	if (p == end && value == NULL)
	    namelen = p - name;
	else
	    valuelen = p - value;
	sts = http_add_parameter(*parameters, name, namelen, value, valuelen);
    }
    return sts;
}

/*
 * Return parameters dictionary and 'base' URL (preceding any '?' character)
 */
static sds
http_url_decode(const char *url, size_t length, dict **output)
{
    const char		*p, *end = url + length;
    size_t		psize, urlsize = length;
    sds			result;

    if (length == 0)
	return NULL;
    for (p = url; p < end; p++) {
	if (*p == '\0')
	    break;
	if (*p != '?')
	    continue;
	urlsize = p - url;
	p++;	/* skip over the '?' marker */
	break;
    }

    /* extract decoded parameters */
    psize = length - (p - url);
    if (psize > 0 && http_parameters(p, psize, output) < 0)
	return NULL;

    /* extract decoded base URL */
    result = sdsnewlen(SDS_NOINIT, urlsize);
    if (http_decode(url, urlsize, result) < 0) {
	sdsfree(result);
	return NULL;
    }
    return result;
}

static servlet *
servlet_lookup(struct client *client, const char *offset, size_t length)
{
    struct proxy	*proxy = (struct proxy *)client->proxy;
    struct servlet	*servlet;
    sds			url;

    url = http_url_decode(offset, length, &client->u.http.parameters);
    for (servlet = proxy->servlets; servlet != NULL; servlet = servlet->next) {
	if (servlet->on_url(client, url, client->u.http.parameters) != 0)
	    break;
    }
    if (servlet && pmDebugOptions.http)
	fprintf(stderr, "%s servlet accepts URL %s from client %p\n",
			servlet->name, url, client);
    sdsfree(url);
    return servlet;
}

static int
on_url(http_parser *request, const char *offset, size_t length)
{
    struct client	*client = (struct client *)request->data;
    struct servlet	*servlet;
    sds			result;
    int			sts;

    if ((servlet = servlet_lookup(client, offset, length)) != NULL) {
	client->u.http.servlet = servlet;
	if ((sts = client->u.http.parser.status_code) == 0) {
	    client->u.http.headers = dictCreate(&sdsDictCallBacks, NULL);
	    return 0;
	}
	result = sdsnew("failed to process URL");
    } else {
	sts = client->u.http.parser.status_code = HTTP_STATUS_NOT_FOUND;
	result = sdsnew("no handler for URL");
    }
    http_error(client, sts, result);
    return 0;
}

static int
on_body(http_parser *request, const char *offset, size_t length)
{
    struct client	*client = (struct client *)request->data;
    struct servlet	*servlet = client->u.http.servlet;

    if (pmDebugOptions.http && pmDebugOptions.desperate)
	printf("Body: %.*s\n(client=%p)\n", (int)length, offset, client);

    if (servlet->on_body)
	return servlet->on_body(client, offset, length);
    return 0;
}

static int
on_header_field(http_parser *request, const char *offset, size_t length)
{
    struct client	*client = (struct client *)request->data;
    sds			field = sdsnewlen(offset, length);

    if (pmDebugOptions.http)
	fprintf(stderr, "Header field: %s (client=%p)\n", field, client);

    if (client->u.http.parser.status_code || !client->u.http.headers)
	return 0;	/* already in process of failing connection */

    /*
     * Insert this header into the dictionary (name only so far);
     * track this header for associating the value to it (below).
     */
    client->u.http.privdata = dictAddRaw(client->u.http.headers, field, NULL);
    return 0;
}

static int
on_header_value(http_parser *request, const char *offset, size_t length)
{
    struct client	*client = (struct client *)request->data;
    sds			value = sdsnewlen(offset, length);

    if (pmDebugOptions.http)
	fprintf(stderr, "Header value: %s (client=%p)\n", value, client);
    if (client->u.http.parser.status_code || !client->u.http.headers)
	return 0;	/* already in process of failing connection */

    dictSetVal(client->u.http.headers, (dictEntry *)client->u.http.privdata, value);
    return 0;
}

static int
on_headers_complete(http_parser *request)
{
    struct client	*client = (struct client *)request->data;
    struct servlet	*servlet = client->u.http.servlet;

    if (pmDebugOptions.http)
	fprintf(stderr, "HTTP headers complete (client=%p)\n", client);
    if (client->u.http.parser.status_code || !client->u.http.headers)
	return 0;	/* already in process of failing connection */

    client->u.http.privdata = NULL;
    if (servlet->on_headers)
	return servlet->on_headers(client, client->u.http.headers);
    return 0;
}

static int
on_message_begin(http_parser *request)
{
    struct client	*client = (struct client *)request->data;

    if (pmDebugOptions.http)
	fprintf(stderr, "HTTP message begin (client=%p)\n", client);
    return 0;
}

static int
on_message_complete(http_parser *request)
{
    struct client	*client = (struct client *)request->data;
    struct servlet	*servlet = client->u.http.servlet;
    int			sts = 0;

    if (pmDebugOptions.http)
	fprintf(stderr, "HTTP message complete (client=%p)\n", client);

    if (servlet && servlet->on_done)
	sts = servlet->on_done(client);

    return sts;
}

void
on_http_client_close(struct client *client)
{
    if (client->u.http.headers)
	dictRelease(client->u.http.headers);
    if (client->u.http.parameters)
	dictRelease(client->u.http.parameters);
    memset(&client->u.http, 0, sizeof(client->u.http));
}

static const http_parser_settings settings = {
    .on_url			= on_url,
    .on_body			= on_body,
    .on_header_field		= on_header_field,
    .on_header_value		= on_header_value,
    .on_headers_complete	= on_headers_complete,
    .on_message_begin		= on_message_begin,
    .on_message_complete	= on_message_complete,
};

void
on_http_client_read(struct proxy *proxy, struct client *client,
		ssize_t nread, const uv_buf_t *buf)
{
    http_parser		*parser = &client->u.http.parser;
    size_t		bytes;

    if (pmDebugOptions.http) {
	fprintf(stderr, "read %ld bytes from HTTP client %p\n", (long)nread, client);
	fprintf(stderr, "%.*s", (int)nread, buf->base);
    }

    if (nread <= 0)
	return;

    /* first time setup for this request */
    if (parser->data == NULL) {
	parser->data = client;
	http_parser_init(parser, HTTP_REQUEST);
    }

    bytes = http_parser_execute(parser, &settings, buf->base, nread);
    if (pmDebugOptions.http && bytes != nread) {
	fprintf(stderr, "Error: %s (%s)\n",
		http_errno_description(HTTP_PARSER_ERRNO(parser)),
		http_errno_name(HTTP_PARSER_ERRNO(parser)));
    }
}

static void
register_servlet(struct proxy *proxy, struct servlet *servlet)
{
    struct servlet	*tail;

    if (proxy->servlets) {
	/* move tail pointer to the end of the list */
	for (tail = proxy->servlets; tail->next; tail = tail->next) {
	    /* bail if found to be already inserted */
	    if (tail == servlet)
		goto setup;
	}
	/* append the new servlet onto the list end */
	tail->next = servlet;
    } else {
	proxy->servlets = servlet;
    }

setup:
    servlet->setup(proxy);
}

void
setup_http_modules(struct proxy *proxy)
{
    extern struct servlet pmseries_servlet;
    extern struct servlet grafana_servlet;
    sds			option;

    if ((option = pmIniFileLookup(config, "pmproxy", "chunksize")) != NULL)
	chunked_transfer_size = atoi(option);
    else
	chunked_transfer_size = getpagesize();

    register_servlet(proxy, &pmseries_servlet);
    register_servlet(proxy, &grafana_servlet);
}
