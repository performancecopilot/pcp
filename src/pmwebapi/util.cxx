/*
 * Copyright (c) 2013-2017 Red Hat, Inc.  All Rights Reserved.
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

#define _XOPEN_SOURCE 600

#include "pmapi.h"
#include "impl.h"
#include "pmwebapi.h"

#include <iostream>
#include <sstream>
#include <vector>

extern "C"
{
#include <sys/stat.h>
#include <stdarg.h>
#include <microhttpd.h>
#ifdef HAVE_ZLIB
#include <zlib.h>
#endif
}
using namespace std;


// Print a string to cout/cerr progress reports, similar to the
// stuff produced by __pmNotifyErr
ostream & timestamp (ostream & o)
{
    time_t now;
    time (&now);
    char *now2 = ctime (&now);
    if (now2) {
        now2[19] = '\0';		// overwrite \n
    }

    return o << "[" << (now2 ? now2 : "") << "] " << pmProgname << "(" << getpid () << "): ";
    // NB: we're single-threaded; no point printing out a thread-id too
}



string conninfo (struct MHD_Connection * conn, bool serv_p)
{
    char hostname[128];
    char servname[128];
    int sts = -1;

    if (conn == 0)
        return "internal";
    
    /* Look up client address data. */
    const union MHD_ConnectionInfo *u = MHD_get_connection_info (conn,
                                                MHD_CONNECTION_INFO_CLIENT_ADDRESS);
    struct sockaddr *so = u ? u->client_addr : 0;

    if (so && so->sa_family == AF_INET) {
        sts = getnameinfo (so, sizeof (struct sockaddr_in), hostname, sizeof (hostname), servname,
                           sizeof (servname), NI_NUMERICHOST | NI_NUMERICSERV);
    } else if (so && so->sa_family == AF_INET6) {
        sts = getnameinfo (so, sizeof (struct sockaddr_in6), hostname, sizeof (hostname),
                           servname, sizeof (servname), NI_NUMERICHOST | NI_NUMERICSERV);
    }
    if (sts != 0) {
        hostname[0] = servname[0] = '\0';
    }

    if (serv_p) {
        return string (hostname) + string (":") + string (servname);
    } else {
        return string (hostname);
    }
}


// Print connection-specific string
ostream & connstamp (ostream & o, struct MHD_Connection * conn)
{
    timestamp (o) << "[" << conninfo (conn, true) << "] ";
    return o;
}


// originally based on http://stackoverflow.com/a/236803/661150
// but we'd like a separator rather than terminator semantics,
// in order to separate the   foo.bar -vs- foo.bar.  cases.
//
template <class Stringy>
vector <Stringy> split_tpl (const std::string & str, char sep)
{
    vector <Stringy> elems;
    string::size_type lastPos = 0;
    while (1)
        {
            string::size_type pos = str.find_first_of(sep, lastPos); // may be ::npos
            elems.push_back(Stringy(str.substr(lastPos, pos - lastPos)));
            if (pos == string::npos) break;
            lastPos = pos + 1;
        }
    return elems;
}

vector<string> split (const std::string & str, char sep)
{
    return split_tpl<std::string>(str, sep);
}

vector<flyweight_string> fwsplit (const std::string & str, char sep)
{
    return split_tpl<flyweight_string>(str, sep);
}




// Take two names of files/directories.  Resolve them both with
// realpath(3), and check whether the latter is beneath the first.
// This is intended to catch naughty people passing /../ in URLs.
// Default to rejection (on errors).  Return 0 on OK, some -errno
// on problem.
int
cursed_path_p (const string & blessed, const string & questionable)
{
    char *rp1c = realpath (blessed.c_str (), NULL);
    if (rp1c == NULL) {
        return -errno;
    }
    string rp1 = string (rp1c);
    free (rp1c);

    char *rp2c = realpath (questionable.c_str (), NULL);
    if (rp2c == NULL) {
        return -errno;
    }
    string rp2 = string (rp2c);
    free (rp2c);

    if (rp1 == rp2) {
        // identical: OK
        return 0;
    }

    if (rp2.size () < rp1.size () + 1) {
        // rp2 cannot be nested within rp1
        return -EACCES;
    }

    if (rp2.substr (0, rp1.size () + 1) == (rp1 + '/')) {
        // rp2 nested beneath rp1/
        return 0;
    }

    return -EACCES;
}



/* Print a string with JSON quoting.  Replace non-ASCII characters
   with \uFFFD "REPLACEMENT CHARACTER". */
void
json_quote (ostream & o, const string & value)
{
    const char hex[] = "0123456789ABCDEF";
    o << '"';
    for (unsigned i = 0; i < value.length (); i++) {
        char c = value[i];
        if (!isascii (c)) {
            o << "\\uFFFD";
        } else if (isalnum (c)) {
            o << c;
        } else if (ispunct (c) && !iscntrl (c) && (c != '\\' && c != '\"')) {
            o << c;
        } else if (c == ' ') {
            o << c;
        } else {
            o << "\\u00" << hex[ (c >> 4) & 0xf] << hex[ (c >> 0) & 0xf];
        }
    }
    o << '"';
}



std::string urlencode (const std::string &foo)
{
    stringstream output;
    static const char hex[] = "0123456789ABCDEF";

    for (unsigned i = 0; i < foo.size (); i++) {
        char c = foo[i];
        if (isalnum (c) || c == '-' || c == '_' || c == '.' || c == '~') {
            output << c;
        } else if (c == ' ') {
            output << '+';
        } else {
            output << "%" << hex[ (c >> 4) & 15] << hex[ (c >> 0) & 15];
        }
    }
    return output.str ();
}

std::string escapeString(const std::string& input) {
    std::ostringstream ss;
    for (std::string::const_iterator iter = input.begin(); iter != input.end(); iter++) {
        switch (*iter) {
            case '\\': ss << "\\\\"; break;
            case '"': ss << "\\\""; break;
         /* case '/': ss << "\\/"; break;  no need to escape / */
            case '\b': ss << "\\b"; break;
            case '\f': ss << "\\f"; break;
            case '\n': ss << "\\n"; break;
            case '\r': ss << "\\r"; break;
            case '\t': ss << "\\t"; break;
            default: ss << *iter; break;
        }
    }
    return ss.str();
}

/* Compress given input string via libz into a new malloc()-allocated buffer.
   Return compressed data length via *output_length.  Return NULL in case
   of compression or other failure.

   NB: Use http-compatible gzip encoding instead of deflate, for
   better browser compatibility.  This means we can't use the zlib
   default compress() wrapper.
*/
static void *compress_string(const std::string& input, size_t& output_length)
{
#if HAVE_ZLIB
    const int gzip_header_size_delta = 32; /* overestimate from eyeballing RFC1952 and zlib */
    uLong buf_needed = compressBound (input.length ()) + gzip_header_size_delta;
    void *buf = malloc (buf_needed);
    if (buf == NULL)
        return NULL;

    /* from zlib compress2 */
    z_stream stream;
    stream.next_in = (Bytef*) input.c_str();
    stream.avail_in = (uLong) input.length();
    stream.next_out = (Bytef*) buf;
    stream.avail_out = (uLong) buf_needed;
    stream.zalloc = (alloc_func) 0;
    stream.zfree = (free_func) 0;
    stream.opaque = (voidpf) 0;

    int rc = deflateInit2 (&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                           MAX_WBITS | 16 /*gzip*/,
                           8 /*DEF_MEM_LEVEL*/, Z_DEFAULT_STRATEGY);
    if (rc != Z_OK)
        goto out;
    rc = deflate (&stream, Z_FINISH);
    if (rc != Z_STREAM_END)
        goto out2;
    output_length = (size_t) stream.total_out;

    rc = deflateEnd (&stream);
    if (rc != Z_OK)
        goto out;

    return buf;

 out2:
    deflateEnd (& stream);
 out:
    free (buf);
    return NULL;
#else
    (void) input;
    (void) output_length;
    return NULL;
#endif
}



/* Create and return MHD_Request with the given string buffer content.
   Compress it if requested & possible.  Return NULL on error.  */
struct MHD_Response *NOTMHD_compressible_response(struct MHD_Connection *connection,
                                                  const std::string& buf)
{
    struct MHD_Response* resp = NULL;

    /* Did the client request compression? */
    const char *encodings = MHD_lookup_connection_value (connection,
                                                         MHD_HEADER_KIND,
                                                         MHD_HTTP_HEADER_ACCEPT_ENCODING);
    const char *useragent = MHD_lookup_connection_value (connection,
                                                         MHD_HEADER_KIND,
                                                         MHD_HTTP_HEADER_USER_AGENT);

    if (encodings == NULL) encodings = "";

    (void) useragent;
    /* if (strstr (useragent, "Trident/") != NULL) encodings = ""; */ /* ?? disable on MSIE */

    if (strstr (encodings, "gzip") != NULL) {
        size_t heap_buf_len;
        void *heap_buf = compress_string (buf, heap_buf_len);
        if (heap_buf != NULL) {
            resp = MHD_create_response_from_buffer (heap_buf_len, heap_buf,
                                                    MHD_RESPMEM_MUST_FREE);
            if (resp == NULL)
                free (heap_buf); /* throw away zlib output */

            int rc = MHD_add_response_header(resp, "Content-Encoding", "gzip");
            if (rc != MHD_YES) {
                /* without that header, the client can't make sense of the data;
                   we must try again without compression */
                MHD_destroy_response (resp);
                resp = NULL;
            }
        }
    }

    if (resp == NULL) { /* e.g., if compression failed or not requested. */
        resp = MHD_create_response_from_buffer (buf.length (), (void *) buf.c_str (),
                                                MHD_RESPMEM_MUST_COPY);
    }

    return resp;
}
