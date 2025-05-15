/*
 * Copyright (c) 2019-2020,2023 Red Hat.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */
#include <ctype.h>
#include <assert.h>
#include "server.h"
#include "encoding.h"
#include "dict.h"
#include "util.h"
#include "sds.h"
#include <pcp/mmv_stats.h>
#ifdef HAVE_ZLIB
#include "zlib.h"
#endif

static int chunked_transfer_size; /* http.chunksize, pagesize by default */
static int smallest_buffer_size = 128;
static int max_age_value = 86400; /* 24h */
static sds allowed_headers;

/* https://tools.ietf.org/html/rfc7230#section-3.1.1 */
#define MAX_URL_SIZE	8192
#define MAX_PARAMS_SIZE 8000
#define MAX_HEADERS_SIZE 128

static sds HEADER_ACCESS_CONTROL_REQUEST_HEADERS,
	   HEADER_ACCESS_CONTROL_REQUEST_METHOD,
	   HEADER_ACCESS_CONTROL_ALLOW_METHODS,
	   HEADER_ACCESS_CONTROL_ALLOW_HEADERS,
	   HEADER_ACCESS_CONTROL_ALLOW_ORIGIN,
	   HEADER_ACCESS_CONTROL_MAX_AGE,
	   HEADER_CONNECTION, HEADER_CONTENT_LENGTH,
	   HEADER_ORIGIN, HEADER_WWW_AUTHENTICATE;

static const char *
compress_format(http_flags_t flags)
{
    if (flags & HTTP_FLAG_GZIP)
	return "gzip";
    if (flags & HTTP_FLAG_DEFLATE)
	return "deflate";
    return "no";
}

/*
 * Helper function to manage client buffer compression.
 * The compressed client buffer is returned and will be
 * a different buffer to the one supplied, on success.
 */
static sds
compress_buffer(struct client *client, sds input_buffer, int done)
{
#ifdef HAVE_ZLIB
    z_stream		*stream = &client->u.http.strm;
    size_t		output_length;
    sds			final_buffer = sdsnewlen(NULL, chunked_transfer_size);
    int			sts, flush = done ? Z_FINISH : Z_PARTIAL_FLUSH;

    if (input_buffer) {
	stream->next_in = (Bytef *)input_buffer;
	stream->avail_in = (uInt)sdslen(input_buffer);
    }
    stream->next_out = (Bytef *)final_buffer;
    stream->avail_out = (uInt)chunked_transfer_size;

    sts = deflate(stream, flush);
    assert(sts != Z_STREAM_ERROR);

    if (stream->avail_out == 0) {
	if (done) {
	    if (sts == Z_STREAM_END)
		client->u.http.flags &= ~HTTP_FLAG_FLUSHING;
	    else /* subsequent calls continue compression */
		client->u.http.flags |= HTTP_FLAG_FLUSHING;
	} else {
	    /* input completely consumed and no output */
	    sdsfree(input_buffer);
	    sdsfree(final_buffer);
	    return NULL;
	}
    }

    sdsfree(input_buffer);
    output_length = chunked_transfer_size - stream->avail_out;
    if (output_length > 0) {
	sdssetlen(final_buffer, output_length);
    } else {
	client->u.http.flags &= ~HTTP_FLAG_FLUSHING;
	sdsfree(final_buffer);
	final_buffer = NULL;
    }
    return final_buffer;

#else
    (void) client;
    return input_buffer;
#endif
}

static sds
prepare_buffer(struct client *client, sds buffer, int flags, int flush)
{
    struct proxy 	*proxy = client->proxy;
    pmAtomValue		av, **values = proxy->values;
    void 		*map = proxy->map;

    if (flags & (HTTP_FLAG_GZIP | HTTP_FLAG_DEFLATE)) {
	/* input buffer may be null here if we're doing final flushes */
	if (pmDebugOptions.http)
	    fprintf(stderr, "Length before %s compression: %llu\n",
		    compress_format(flags),
		    buffer ? (unsigned long long)sdslen(buffer) : 0);
	if ((buffer = compress_buffer(client, buffer, flush)) == NULL)
	    return buffer;
	av.ull = sdslen(buffer);
	if (pmDebugOptions.http)
	    fprintf(stderr, "Length after %s compression: %llu flush=%d\n",
		    compress_format(flags), (unsigned long long)av.ull, flush);
	if (map && av.ull > 0) {
	    mmv_inc_atomvalue(map, values[VALUE_HTTP_COMPRESSED_BYTES], &av);
	    mmv_inc(map, values[VALUE_HTTP_COMPRESSED_COUNT]);
	}
    } else if (map) {
	assert(buffer != NULL);
	av.ull = sdslen(buffer);
	mmv_inc_atomvalue(map, values[VALUE_HTTP_UNCOMPRESSED_BYTES], &av);
	mmv_inc(map, values[VALUE_HTTP_UNCOMPRESSED_COUNT]);
    }
    return buffer;
}

/*
 * Simple helpers to manage the cumulative addition of JSON
 * (arrays and/or objects) to a buffer.
 */
sds
json_push_suffix(sds suffix, json_flags_t type)
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
	/* also copy the NUL byte at the end of the c string, therefore use
	 * length instead of length-1 */
	memmove(suffix, suffix+1, length);
	sdssetlen(suffix, length-1); /* update sds string length accordingly */
    }
    return suffix;
}

sds
json_string(const sds original)
{
    return unicode_encode(original, sdslen(original));
}

const char *
http_content_type(http_flags_t flags)
{
    if (flags & HTTP_FLAG_JSON)
	return "application/json";
    if (flags & HTTP_FLAG_HTML)
	return "text/html";
    if (flags & HTTP_FLAG_TEXT)
	return "text/plain";
    return "application/octet-stream";
}

static const char * const
http_content_encoding(http_flags_t flags)
{
    if (flags & HTTP_FLAG_UTF8)
	return "; charset=UTF-8\r\n";
    if (flags & HTTP_FLAG_UTF16)
	return "; charset=UTF-16\r\n";
    return "";
}

static int
http_status_index(http_code_t code)
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
http_status_mapping(http_code_t code)
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
	buffer = sdsnewlen(NULL, smallest_buffer_size);
	sdsclear(buffer);
    }
    return buffer;
}

void
http_set_buffer(struct client *client, sds buffer, http_flags_t flags)
{
    assert(client->buffer == NULL);
    client->u.http.flags |= flags;
    client->buffer = buffer;
}

static sds
http_response_header(struct client *client, unsigned int length, http_code_t sts, http_flags_t flags)
{
    struct http_parser	*parser = &client->u.http.parser;
    char		date[64];
    sds			header;

    if (parser->http_major == 0)
	parser->http_major = parser->http_minor = 1;

    header = sdscatfmt(sdsempty(),
		"HTTP/%u.%u %u %s\r\n"
		"%S: Keep-Alive\r\n",
		parser->http_major, parser->http_minor,
		sts, http_status_mapping(sts), HEADER_CONNECTION);
    header = sdscatfmt(header,
		"%S: *\r\n"
		"%S: %S\r\n"
		"%S: %u\r\n",
		HEADER_ACCESS_CONTROL_ALLOW_ORIGIN,
		HEADER_ACCESS_CONTROL_ALLOW_HEADERS,
		allowed_headers,
		HEADER_ACCESS_CONTROL_MAX_AGE,
		max_age_value);

    if (sts == HTTP_STATUS_UNAUTHORIZED && client->u.http.realm)
	header = sdscatfmt(header, "%S: Basic realm=\"%S\"\r\n",
				HEADER_WWW_AUTHENTICATE, client->u.http.realm);

    if ((flags & (HTTP_FLAG_STREAMING | HTTP_FLAG_NO_BODY)))
	header = sdscatfmt(header, "Transfer-encoding: chunked\r\n");
    else
	header = sdscatfmt(header, "%S: %u\r\n", HEADER_CONTENT_LENGTH, length);

    header = sdscatfmt(header, "Content-Type: %s%s\r\n",
		http_content_type(flags), http_content_encoding(flags));

    if (flags & HTTP_FLAG_GZIP)
	header = sdscatfmt(header, "Content-Encoding: gzip\r\n");
    else if (flags & HTTP_FLAG_DEFLATE)
	header = sdscatfmt(header, "Content-Encoding: deflate\r\n");

    header = sdscatfmt(header, "Date: %s\r\n\r\n",
		http_date_string(time(NULL), date, sizeof(date)));

    if (pmDebugOptions.http && pmDebugOptions.desperate) {
	fprintf(stderr, "reply headers for response to client %p\n", client);
	fputs(header, stderr);
    }
    return header;
}

static sds
http_header_value(struct client *client, sds header)
{
    if (client->u.http.headers == NULL)
	return NULL;
    return (sds)dictFetchValue(client->u.http.headers, header);
}

static sds
http_headers_allowed(sds headers)
{
    (void)headers;
    return sdsdup(allowed_headers);
}

/* check whether the (preflight) method being proposed is acceptable */
static int
http_method_allowed(sds value, http_options_t options)
{
    if (strcmp(value, "GET") == 0 && (options & HTTP_OPT_GET))
	return 1;
    if (strcmp(value, "PUT") == 0 && (options & HTTP_OPT_PUT))
	return 1;
    if (strcmp(value, "POST") == 0 && (options & HTTP_OPT_POST))
	return 1;
    if (strcmp(value, "HEAD") == 0 && (options & HTTP_OPT_HEAD))
	return 1;
    if (strcmp(value, "TRACE") == 0 && (options & HTTP_OPT_TRACE))
	return 1;
    return 0;
}

static char *
http_methods_string(char *buffer, size_t length, http_options_t options)
{
    char		*p = buffer;

    /* ensure room for all options, spaces and comma separation */
    if (!options || length < 48)
	return NULL;

    memset(buffer, 0, length);
    if (options & HTTP_OPT_GET)
	strcat(p, ", GET");
    if (options & HTTP_OPT_PUT)
	strcat(p, ", PUT");
    if (options & HTTP_OPT_HEAD)
	strcat(p, ", HEAD");
    if (options & HTTP_OPT_POST)
	strcat(p, ", POST");
    if (options & HTTP_OPT_TRACE)
	strcat(p, ", TRACE");
    if (options & HTTP_OPT_OPTIONS)
	strcat(p, ", OPTIONS");
    return p + 2; /* skip leading comma+space */
}

static sds
http_response_trace(struct client *client, int sts)
{
    struct http_parser	*parser = &client->u.http.parser;
    dictIterator	*iterator;
    dictEntry		*entry;
    char		buffer[64];
    sds			header;

    parser->http_major = parser->http_minor = 1;

    header = sdscatfmt(sdsempty(),
		"HTTP/%u.%u %u %s\r\n"
		"%S: Keep-Alive\r\n",
		parser->http_major, parser->http_minor,
		sts, http_status_mapping(sts), HEADER_CONNECTION);
    header = sdscatfmt(header, "%S: %u\r\n", HEADER_CONTENT_LENGTH, 0);

    iterator = dictGetSafeIterator(client->u.http.headers);
    while ((entry = dictNext(iterator)) != NULL)
	header = sdscatfmt(header, "%S: %S\r\n", dictGetKey(entry), dictGetVal(entry));
    dictReleaseIterator(iterator);

    header = sdscatfmt(header, "Date: %s\r\n\r\n",
		http_date_string(time(NULL), buffer, sizeof(buffer)));

    if (pmDebugOptions.http && pmDebugOptions.desperate) {
	fprintf(stderr, "trace response to client %p\n", client);
	fputs(header, stderr);
    }
    return header;
}

static sds
http_response_access(struct client *client, http_code_t sts, http_options_t options)
{
    struct http_parser	*parser = &client->u.http.parser;
    char		buffer[64];
    sds			header, value, result;

    value = http_header_value(client, HEADER_ACCESS_CONTROL_REQUEST_METHOD);
    if (value && http_method_allowed(value, options) == 0)
	sts = HTTP_STATUS_METHOD_NOT_ALLOWED;

    parser->http_major = parser->http_minor = 1;

    header = sdscatfmt(sdsempty(),
		"HTTP/%u.%u %u %s\r\n"
		"%S: Keep-Alive\r\n",
		parser->http_major, parser->http_minor,
		sts, http_status_mapping(sts), HEADER_CONNECTION);
    header = sdscatfmt(header, "%S: %u\r\n", HEADER_CONTENT_LENGTH, 0);

    if (sts >= HTTP_STATUS_OK && sts < HTTP_STATUS_BAD_REQUEST) {
	if ((value = http_header_value(client, HEADER_ORIGIN)))
	    header = sdscatfmt(header, "%S: %S\r\n",
			        HEADER_ACCESS_CONTROL_ALLOW_ORIGIN, value);

	header = sdscatfmt(header,
			    "%S: %s\r\n"
			    "%S: %u\r\n",
			    HEADER_ACCESS_CONTROL_ALLOW_METHODS,
			    http_methods_string(buffer, sizeof(buffer), options),
			    HEADER_ACCESS_CONTROL_MAX_AGE, max_age_value);

	value = http_header_value(client, HEADER_ACCESS_CONTROL_REQUEST_HEADERS);
	if (value && (result = http_headers_allowed(value)) != NULL) {
	    header = sdscatfmt(header, "%S: %S\r\n",
				HEADER_ACCESS_CONTROL_ALLOW_HEADERS, result);
	    sdsfree(result);
	}
    }
    if (sts == HTTP_STATUS_UNAUTHORIZED && client->u.http.realm)
	header = sdscatfmt(header, "%S: Basic realm=\"%S\"\r\n",
			    HEADER_WWW_AUTHENTICATE, client->u.http.realm);

    header = sdscatfmt(header, "Date: %s\r\n\r\n",
		http_date_string(time(NULL), buffer, sizeof(buffer)));

    if (pmDebugOptions.http && pmDebugOptions.desperate) {
	fprintf(stderr, "access response to client %p\n", client);
	fputs(header, stderr);
    }
    return header;
}

void
http_reply(struct client *client, sds message,
		http_code_t sts, http_flags_t type, http_options_t options)
{
    enum http_flags	flags = client->u.http.flags;
    char		length[32]; /* hex in sdscatfmt (not sdscatprintf) */
    sds			buffer, suffix;

    if (flags & HTTP_FLAG_STREAMING) {

	if (pmDebugOptions.http)
	    fprintf(stderr,
		    "Final streaming HTTP %s response len=%lu (client=%p)\n",
		    http_method_str(client->u.http.parser.method),
		    client->buffer ? (long unsigned)sdslen(client->buffer) : 0, client);

	buffer = sdsempty();
	suffix = client->buffer;
	if (suffix == NULL) {	/* error or no data currently accumulated */
	    suffix = prepare_buffer(client, message, flags, 1);
	} else if (message != NULL) {
	    suffix = sdscatsds(suffix, message);
	    suffix = prepare_buffer(client, suffix, flags, 1);
	    sdsfree(message);
	}
	message = NULL;

	pmsprintf(length, sizeof(length), "%lX",
			(unsigned long)sdslen(suffix));
	buffer = sdscatfmt(buffer, "%s\r\n%S\r\n", length, suffix);
	sdsfree(suffix);
	suffix = NULL;

	client->buffer = NULL;

	if (!(client->u.http.flags & HTTP_FLAG_FLUSHING)) {
	    client->u.http.flags &= ~HTTP_FLAG_STREAMING; /* end of stream! */
	    suffix = sdsnewlen("0\r\n\r\n", 5);		/* chunked suffix */
	}

    } else if (flags & HTTP_FLAG_NO_BODY) {
	if (client->u.http.parser.method == HTTP_OPTIONS)
	    buffer = http_response_access(client, sts, options);
	else if (client->u.http.parser.method == HTTP_TRACE)
	    buffer = http_response_trace(client, sts);
	else	/* HTTP_HEAD */
	    buffer = http_response_header(client, 0, sts, type);
	suffix = NULL;
    } else {	/* regular non-chunked response - headers + response body */
	if (client->buffer == NULL) {
	    suffix = message;
	} else if (message != NULL) {
	    suffix = sdscatsds(client->buffer, message);
	    sdsfree(message);
	    client->buffer = NULL;
	} else {
	    suffix = sdsempty();
	}
	suffix = prepare_buffer(client, suffix, flags, 1);
	buffer = http_response_header(client, sdslen(suffix), sts, type);
    }

    if (pmDebugOptions.http) {
	if (flags & (HTTP_FLAG_GZIP | HTTP_FLAG_DEFLATE))
	    fprintf(stderr, "HTTP %s compressed response (client=%p) "
			    "len(buffer)=%lu len(suffix)=%lu\n",
			http_method_str(client->u.http.parser.method), client,
			(long unsigned)sdslen(buffer), suffix ? (long unsigned)sdslen(suffix) : 0);
	else
	    fprintf(stderr, "HTTP %s response (client=%p)\nbuffer=%ssuffix=%s",
			http_method_str(client->u.http.parser.method), client,
			buffer, suffix ? suffix : "");
    }

    client_write(client, buffer, suffix);
}

static int
http_flush(struct client *client)
{
    enum http_flags	flags = client->u.http.flags;
    char		length[32]; /* hex in sdscatfmt (not sdscatprintf) */
    sds			buffer, suffix = NULL;

    /* compression flushing, send any remaining buffer(s) */
    assert(flags & HTTP_FLAG_FLUSHING);
    assert(flags & (HTTP_FLAG_GZIP | HTTP_FLAG_DEFLATE));

    buffer = prepare_buffer(client, NULL, flags, 1);

    if (pmDebugOptions.http)
	fprintf(stderr, "HTTP %s compressed flush len=%lu (client=%p)\n",
		http_method_str(client->u.http.parser.method),
		buffer ? (long unsigned)sdslen(buffer) : 0, client);

    /* flags will now reflect if this is the final buffer to transfer */
    flags = client->u.http.flags;

    /* if streaming append a chunked transfer encoding length sequence */
    if (flags & HTTP_FLAG_STREAMING) {
	if (buffer) {
	    pmsprintf(length, sizeof(length), "%lX\r\n",
			(unsigned long)sdslen(buffer));
	    suffix = buffer;
	    buffer = sdscatfmt(sdsempty(), "%s\r\n%S\r\n", length, suffix);
	    sdsfree(suffix);

	    /* if finished, add chunked transfer termination sequence also */
	    if (!(flags & HTTP_FLAG_FLUSHING)) {
	        suffix = sdsnewlen("0\r\n\r\n", 5);
		client->u.http.flags &= ~HTTP_FLAG_STREAMING; /* stream end */
	    } else {
		suffix = NULL;
	    }
	} else {
	    if (!(flags & HTTP_FLAG_FLUSHING)) {
	        buffer = sdsnewlen("0\r\n\r\n", 5);
		client->u.http.flags &= ~HTTP_FLAG_STREAMING; /* stream end */
	    }
	}
    }

    if (buffer == NULL)
	return 0;

    client_write(client, buffer, suffix);
    return 1;
}

void
http_error(struct client *client, http_code_t status, const char *errstr)
{
    const char		*mapping = http_status_mapping(status);
    struct servlet	*servlet = client->u.http.servlet;
    sds			message;

    /* on error, we must first discard any accumulated partial result */
    sdsfree(client->buffer);
    client->buffer = NULL;

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
    http_reply(client, message, status, HTTP_FLAG_HTML, 0);
}

void
http_transfer(struct client *client)
{
    struct http_parser	*parser = &client->u.http.parser;
    enum http_flags	flags = client->u.http.flags;
    char		length[32]; /* hex in sdscatfmt (not sdscatprintf) */
    const char		*method;
    sds			buffer, suffix = NULL;

    /*
     * If the client buffer length is now beyond a set maximum size,
     * send it using chunked transfer encoding.  Once buffer pointer
     * is copied into the uv_buf_t, clear it in the client, and then
     * return control to caller.
     */

    if (sdslen(client->buffer) >= chunked_transfer_size) {
	if (parser->http_major == 1 && parser->http_minor > 0) {
	    if (pmDebugOptions.http)
		fprintf(stderr, "Chunked HTTP %s transfer [%lu] (client=%p)\n",
			http_method_str(client->u.http.parser.method),
			(unsigned long)sdslen(client->buffer), client);

	    if (!(flags & HTTP_FLAG_STREAMING)) {
		/* send headers (no content length) and initial content */
		flags |= HTTP_FLAG_STREAMING;
		buffer = http_response_header(client, 0, HTTP_STATUS_OK, flags);
		client->u.http.flags = flags;
	    } else {
		buffer = NULL;
	    }

	    client->buffer = prepare_buffer(client, client->buffer, flags, 0);
	    if (!client->buffer && !buffer)
		return; /* streaming + compressing, nothing to send yet */

	    if (client->buffer && sdslen(client->buffer) > 0) {
		/* headers already sent? send the next chunk of content */
		if (buffer == NULL)
		    buffer = sdsempty();
		/* prepend a chunked transfer encoding message length (hex) */
		pmsprintf(length, sizeof(length), "%lX",
			    (unsigned long)sdslen(client->buffer));
		buffer = sdscatfmt(buffer, "%s\r\n%S\r\n", length, client->buffer);
		/* reset for next call - original released on I/O completion */
		client->buffer = NULL;	/* safe, as now held in 'buffer' */
	    } else if (!buffer) {
		return; /* streaming + compressing, nothing to send yet */
	    }

	    if (pmDebugOptions.http) {
		method = http_method_str(client->u.http.parser.method);
		if (flags & (HTTP_FLAG_GZIP | HTTP_FLAG_DEFLATE))
		    fprintf(stderr,
			"HTTP %s compressed buffer (client %p, len=%lu)\n",
			method, client, (unsigned long)sdslen(buffer));
		else
		    fprintf(stderr,
			"HTTP %s chunk buffer (client %p, len=%lu)\n%s",
			method, client, (unsigned long)sdslen(buffer), buffer);
	    }
	    client_write(client, buffer, suffix);

	} else if (parser->http_major <= 1) {
	    http_error(client, HTTP_STATUS_PAYLOAD_TOO_LARGE,
			"HTTP 1.0 request result exceeds server limits");
	    sdsfree(client->buffer);
	    client->buffer = NULL;
	}
    }
}

static void
http_client_release(struct client *client)
{
    struct servlet	*servlet = client->u.http.servlet;

    if (servlet && servlet->on_release)
	servlet->on_release(client);
    client->u.http.privdata = NULL;
    client->u.http.servlet = NULL;
#ifdef HAVE_ZLIB
    if (client->u.http.flags & (HTTP_FLAG_GZIP | HTTP_FLAG_DEFLATE))
	deflateEnd(&client->u.http.strm);
#endif
    client->u.http.flags = 0;

    if (client->u.http.headers) {
	dictRelease(client->u.http.headers);
	client->u.http.headers = NULL;
    }
    if (client->u.http.parameters) {
	dictRelease(client->u.http.parameters);
	client->u.http.parameters = NULL;
    }
    if (client->u.http.username) {
	sdsfree(client->u.http.username);
	client->u.http.username = NULL;
    }
    if (client->u.http.password) {
	sdsfree(client->u.http.password);
	client->u.http.password = NULL;
    }
    if (client->u.http.realm) {
	sdsfree(client->u.http.realm);
	client->u.http.realm = NULL;
    }

}

static int
http_add_parameter(dict *parameters,
	const char *name, int namelen, const char *value, int valuelen)
{
    char		*pname, *pvalue;
    int			sts;

    if (namelen == 0)
	return 0;

    if ((sts = __pmUrlDecode(name, namelen, &pname)) < 0) {
	return sts;
    }
    if (valuelen > 0) {
	if ((sts = __pmUrlDecode(value, valuelen, &pvalue)) < 0) {
	    free(pname);
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

    dictAdd(parameters, sdsnew(pname), sdsnew(pvalue));
    free(pname);
    free(pvalue);
    return 0;
}

int
http_parameters(const char *url, size_t length, dict **parameters)
{
    const char		*end = url + length;
    const char		*p, *name, *value = NULL;
    int			sts = 0, namelen = 0, valuelen = 0;

    *parameters = dictCreate(&sdsOwnDictCallBacks, NULL);
    for (p = name = url; p < end; p++) {
	if (*p == '=') {
	    namelen = p - name;
	    value = p + 1;
	}
	else if (*p == '&') {
	    if (namelen == 0)
		namelen = p - name;
	    valuelen = value ? p - value : 0;
	    sts = http_add_parameter(*parameters, name, namelen, value, valuelen);
	    if (sts < 0)
		break;
	    value = NULL;
	    name = p + 1;
	    namelen = valuelen = 0;
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
    char		*cresult;
    sds			result;

    if (length == 0)
	return NULL;
    if (length > MAX_PARAMS_SIZE)
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
    if (__pmUrlDecode(url, urlsize, &cresult) < 0) {
	return NULL;
    }
    result = sdsnew(cresult);
    free(cresult);
    return result;
}

static struct servlet *
servlet_lookup(struct client *client, const char *offset, size_t length)
{
    struct proxy	*proxy = (struct proxy *)client->proxy;
    struct servlet	*servlet;
    sds			url;

    if (pmDebugOptions.http || pmDebugOptions.appl2)
	fprintf(stderr, "HTTP %s %.*s\n",
			http_method_str(client->u.http.parser.method),
			(int)length, offset);

    if (!(url = http_url_decode(offset, length, &client->u.http.parameters)))
	return NULL;
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
    int			sts;

    http_client_release(client);	/* new URL, clean slate */

    if (length >= MAX_URL_SIZE) {
	sts = client->u.http.parser.status_code = HTTP_STATUS_URI_TOO_LONG;
	http_error(client, sts, "request URL too long");
    }
    /* pass to servlets handling each of our internal request endpoints */
    else if ((servlet = servlet_lookup(client, offset, length)) != NULL) {
	client->u.http.servlet = servlet;
	if ((sts = client->u.http.parser.status_code) != 0)
	    http_error(client, sts, "failed to process URL");
	else {
	    if (client->u.http.parser.method == HTTP_OPTIONS ||
		client->u.http.parser.method == HTTP_TRACE ||
		client->u.http.parser.method == HTTP_HEAD)
		client->u.http.flags |= HTTP_FLAG_NO_BODY;
	    client->u.http.headers = dictCreate(&sdsOwnDictCallBacks, NULL);
	}
    }
    /* server options - https://tools.ietf.org/html/rfc7231#section-4.3.7 */
    else if (client->u.http.parser.method == HTTP_OPTIONS) {
	if (length == 1 && *offset == '*') {
	    client->u.http.flags |= HTTP_FLAG_NO_BODY;
	    client->u.http.headers = dictCreate(&sdsOwnDictCallBacks, NULL);
	} else {
	    sts = client->u.http.parser.status_code = HTTP_STATUS_BAD_REQUEST;
	    http_error(client, sts, "no handler for OPTIONS");
	}
    }
    /* server trace - https://tools.ietf.org/html/rfc7231#section-4.3.8 */
    else if (client->u.http.parser.method == HTTP_TRACE) {
	client->u.http.flags |= HTTP_FLAG_NO_BODY;
	client->u.http.headers = dictCreate(&sdsOwnDictCallBacks, NULL);
    }
    /* nothing available to respond to this request - inform the client */
    else {
	sts = client->u.http.parser.status_code = HTTP_STATUS_BAD_REQUEST;
	http_error(client, sts, "no handler for URL");
    }
    return 0;
}

static int
on_body(http_parser *request, const char *offset, size_t length)
{
    struct client	*client = (struct client *)request->data;
    struct servlet	*servlet = client->u.http.servlet;

    if (pmDebugOptions.http && pmDebugOptions.desperate)
	printf("Body: %.*s\n(client=%p)\n", (int)length, offset, client);

    if (servlet && servlet->on_body)
	return servlet->on_body(client, offset, length);
    return 0;
}

static int
on_header_field(http_parser *request, const char *offset, size_t length)
{
    struct client	*client = (struct client *)request->data;
    sds			field;

    if (client->u.http.parser.status_code || !client->u.http.headers)
	return 0;	/* already in process of failing connection */
    if (dictSize(client->u.http.headers) >= MAX_HEADERS_SIZE) {
	client->u.http.parser.status_code =
		HTTP_STATUS_REQUEST_HEADER_FIELDS_TOO_LARGE;
	return 0;
    }

    field = sdsnewlen(offset, length);
    if (pmDebugOptions.http)
	fprintf(stderr, "Header field: %s (client=%p)\n", field, client);
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
    dictEntry		*entry;
    char		*colon;
    sds			field, value, decoded, *values;
    int 		i, nvalues = 0;

    if (client->u.http.parser.status_code || !client->u.http.headers)
	return 0;	/* already in process of failing connection */

    value = sdsnewlen(offset, length);
    if (pmDebugOptions.http)
	fprintf(stderr, "Header value: %s (client=%p)\n", value, client);
    entry = (dictEntry *)client->u.http.privdata;
    dictSetVal(client->u.http.headers, entry, value);
    field = (sds)dictGetKey(entry);

    /* HTTP Basic Auth for all servlets */
    if (strncmp(field, "Authorization", 14) == 0 &&
	strncmp(value, "Basic ", 6) == 0) {
	decoded = base64_decode(value + 6, sdslen(value) - 6);
	if (decoded) {
	    /* extract username:password details */
	    if ((colon = strchr(decoded, ':')) != NULL) {
		length = colon - decoded;
		client->u.http.username = sdsnewlen(decoded, length);
		length = sdslen(decoded) - length - 1;
		client->u.http.password = sdsnewlen(colon + 1, length);
	    } else {
		client->u.http.parser.status_code = HTTP_STATUS_UNAUTHORIZED;
	    }
	    sdsfree(decoded);
	} else {
	    client->u.http.parser.status_code = HTTP_STATUS_UNAUTHORIZED;
	}
    }

    if (strncmp(field, "Accept-Encoding", 15) == 0) {
	values = sdssplitlen(value, sdslen(value), ", ", 2, &nvalues);
	for (i = 0; values && i < nvalues; i++) {
#ifdef HAVE_ZLIB
	    if (strcmp(values[i], "gzip") == 0) {
		if (!(client->u.http.flags & HTTP_FLAG_GZIP)) {
		    if (deflateInit2(&client->u.http.strm,
				Z_DEFAULT_COMPRESSION, Z_DEFLATED,
				15 | 16, 8, Z_DEFAULT_STRATEGY) == Z_OK)
		        client->u.http.flags |= HTTP_FLAG_GZIP;
		    else
		        client->u.http.parser.status_code =
				HTTP_STATUS_INTERNAL_SERVER_ERROR;
		} else if (deflateReset(&client->u.http.strm) != Z_OK) {
		    client->u.http.parser.status_code =
			    HTTP_STATUS_INTERNAL_SERVER_ERROR;
		}
		break;
	    }
	    if (strcmp(values[i], "deflate") == 0) {
		if (!(client->u.http.flags & HTTP_FLAG_DEFLATE)) {
		    if (deflateInit2(&client->u.http.strm,
				Z_DEFAULT_COMPRESSION, Z_DEFLATED,
				15, 8, Z_DEFAULT_STRATEGY) == Z_OK)
		        client->u.http.flags |= HTTP_FLAG_DEFLATE;
		    else
		        client->u.http.parser.status_code =
				HTTP_STATUS_INTERNAL_SERVER_ERROR;
		} else if (deflateReset(&client->u.http.strm) != Z_OK) {
		    client->u.http.parser.status_code =
			    HTTP_STATUS_INTERNAL_SERVER_ERROR;
		}
		break;
#endif
	    }
	}
	sdsfreesplitres(values, nvalues);
    }
    return 0;
}

static int
on_headers_complete(http_parser *request)
{
    struct client	*client = (struct client *)request->data;
    struct servlet	*servlet = client->u.http.servlet;
    int			sts = 0;

    if (pmDebugOptions.http)
	fprintf(stderr, "HTTP headers complete (client=%p)\n", client);
    if (client->u.http.parser.status_code || !client->u.http.headers)
	return 0;	/* already in process of failing connection */

    if (client->u.http.username) {
	if (!client->u.http.parameters)
	    client->u.http.parameters = dictCreate(&sdsOwnDictCallBacks, NULL);
	http_add_parameter(client->u.http.parameters, "auth.username", 13,
		client->u.http.username, sdslen(client->u.http.username));
	if (client->u.http.password)
	    http_add_parameter(client->u.http.parameters, "auth.password", 13,
		    client->u.http.password, sdslen(client->u.http.password));
    }

    client->u.http.privdata = NULL;
    if (servlet && servlet->on_headers)
	sts = servlet->on_headers(client, client->u.http.headers);

    /* HTTP Basic Auth for all servlets */
    if (__pmServerHasFeature(PM_SERVER_FEATURE_CREDS_REQD)) {
	if (!client->u.http.username || !client->u.http.password) {
	    /* request should be resubmitted with authentication */
	    client->u.http.parser.status_code = HTTP_STATUS_FORBIDDEN;
	}
    }

    return sts;
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
    sds			buffer;
    int			sts;

    if (pmDebugOptions.http)
	fprintf(stderr, "HTTP message complete (client=%p)\n", client);

    if (servlet) {
	if (servlet->on_done)
	    return servlet->on_done(client);
	return 0;
    }

    sts = HTTP_STATUS_OK;
    if (client->u.http.parser.method == HTTP_OPTIONS) {
	buffer = http_response_access(client, sts, HTTP_SERVER_OPTIONS);
	client_write(client, buffer, NULL);
	return 0;
    }
    if (client->u.http.parser.method == HTTP_TRACE) {
	buffer = http_response_trace(client, sts);
	client_write(client, buffer, NULL);
	return 0;
    }

    return 1;
}

void
on_http_client_close(struct client *client)
{
    if (pmDebugOptions.http)
	fprintf(stderr, "HTTP client close (client=%p)\n", client);

    http_client_release(client);
    memset(&client->u.http, 0, sizeof(client->u.http));
}

void
on_http_client_write(struct client *client)
{
    if (pmDebugOptions.http)
	fprintf(stderr, "%s: client %p\n", "on_http_client_write", client);

    /*
     * If further compressed writes to process, continue doing so
     * until none remain (each write ends up back here until then).
     */
    if (client->u.http.flags & HTTP_FLAG_FLUSHING)
	if (http_flush(client))
	    return;

    /*
     * Once all or none flushed, close the connection if required.
     */
    if (http_should_keep_alive(&client->u.http.parser) == 0)
	client_close(client);
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

    if (pmDebugOptions.http || pmDebugOptions.query)
	fprintf(stderr, "%s: %lld bytes from HTTP client %p\n%.*s",
		"on_http_client_read", (long long)nread, client,
		(int)nread, buf->base);

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
setup_http_module(struct proxy *proxy)
{
    sds			option;
    void		*map;
    pmAtomValue 	**values = proxy->values;
    mmv_registry_t 	*registry = proxymetrics(proxy, METRICS_HTTP);
    const pmUnits	units_count = MMV_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE);
    const pmUnits	units_bytes = MMV_UNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0);

    if (proxy == NULL || registry == NULL)
	return; /* no metric registry has been set up*/

    mmv_stats_add_metric(registry, "compressed.count", 1,
    MMV_TYPE_U64, MMV_SEM_COUNTER, units_count, MMV_INDOM_NULL,
    "Count of compressed transfers", "Number of compressed HTTP transfers");
    mmv_stats_add_metric(registry, "uncompressed.count", 2,
    MMV_TYPE_U64, MMV_SEM_COUNTER, units_count, MMV_INDOM_NULL,
    "Count of uncompresed transfers", "Number of uncompressed HTTP transfers");
    mmv_stats_add_metric(registry, "uncompressed.bytes", 3,
    MMV_TYPE_U64, MMV_SEM_COUNTER, units_bytes, MMV_INDOM_NULL,
    "Count of uncompressed bytes sent", "Total number of uncompressed bytes sent");
    mmv_stats_add_metric(registry, "compressed.bytes", 4,
    MMV_TYPE_U64, MMV_SEM_COUNTER, units_bytes, MMV_INDOM_NULL,
    "Count of compressed bytes sent", "Total number of compressed bytes sent");
    proxy->map = map = mmv_stats_start(registry);

    values[VALUE_HTTP_COMPRESSED_COUNT] = mmv_lookup_value_desc(map,"compressed.count", NULL);
    values[VALUE_HTTP_UNCOMPRESSED_COUNT] = mmv_lookup_value_desc(map,"uncompressed.count", NULL);
    values[VALUE_HTTP_COMPRESSED_BYTES] = mmv_lookup_value_desc(map,"compressed.bytes", NULL);
    values[VALUE_HTTP_UNCOMPRESSED_BYTES] = mmv_lookup_value_desc(map,"uncompressed.bytes", NULL);

    if ((option = pmIniFileLookup(config, "http", "chunksize")) != NULL ||
	(option = pmIniFileLookup(config, "pmproxy", "chunksize")) != NULL)
	chunked_transfer_size = atoi(option);
    else
	chunked_transfer_size = getpagesize();
    if (chunked_transfer_size < smallest_buffer_size)
	chunked_transfer_size = smallest_buffer_size;

    allowed_headers = sdsnew("Accept, Accept-Language, Content-Language, Content-Type");
    if ((option = pmIniFileLookup(config, "http", "Access-Control-Allow-Headers")))
	allowed_headers = sdscatfmt(allowed_headers, ", %S", option);

    if ((option = pmIniFileLookup(config, "http", "Access-Control-Max-Age")))
	max_age_value = atoi(option);

    HEADER_ACCESS_CONTROL_REQUEST_HEADERS = sdsnew("Access-Control-Request-Headers");
    HEADER_ACCESS_CONTROL_REQUEST_METHOD = sdsnew("Access-Control-Request-Method");
    HEADER_ACCESS_CONTROL_ALLOW_METHODS = sdsnew("Access-Control-Allow-Methods");
    HEADER_ACCESS_CONTROL_ALLOW_HEADERS = sdsnew("Access-Control-Allow-Headers");
    HEADER_ACCESS_CONTROL_ALLOW_ORIGIN = sdsnew("Access-Control-Allow-Origin");
    HEADER_ACCESS_CONTROL_MAX_AGE = sdsnew("Access-Control-Max-Age");
    HEADER_CONNECTION = sdsnew("Connection");
    HEADER_CONTENT_LENGTH = sdsnew("Content-Length");
    HEADER_ORIGIN = sdsnew("Origin");
    HEADER_WWW_AUTHENTICATE = sdsnew("WWW-Authenticate");

    register_servlet(proxy, &pmsearch_servlet);
    register_servlet(proxy, &pmseries_servlet);
    register_servlet(proxy, &pmwebapi_servlet);
}

void
close_http_module(struct proxy *proxy)
{
    struct servlet	*servlet;

    for (servlet = proxy->servlets; servlet != NULL; servlet = servlet->next)
	servlet->close(proxy);

    proxymetrics_close(proxy, METRICS_HTTP);

    sdsfree(HEADER_ACCESS_CONTROL_REQUEST_HEADERS);
    sdsfree(HEADER_ACCESS_CONTROL_REQUEST_METHOD);
    sdsfree(HEADER_ACCESS_CONTROL_ALLOW_METHODS);
    sdsfree(HEADER_ACCESS_CONTROL_ALLOW_HEADERS);
    sdsfree(HEADER_ACCESS_CONTROL_ALLOW_ORIGIN);
    sdsfree(HEADER_ACCESS_CONTROL_MAX_AGE);
    sdsfree(HEADER_CONNECTION);
    sdsfree(HEADER_CONTENT_LENGTH);
    sdsfree(HEADER_ORIGIN);
    sdsfree(HEADER_WWW_AUTHENTICATE);
}
