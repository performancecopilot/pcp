/*
 * JSON web bridge for PMAPI.
 *
 * Copyright (c) 2011-2013 Red Hat Inc.
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
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include "pmwebapi.h"


/* ------------------------------------------------------------------------ */

static const char *guess_content_type (const char* filename)
{
    const char *extension = rindex (filename, '.');
    if (extension == NULL) return NULL;
    
    /* One could go all out and parse /etc/mime.types, or one can do this ... */
    if (0 == strcasecmp (extension, "html")) return "text/html";
    if (0 == strcasecmp (extension, "js")) return "text/javascript";
    if (0 == strcasecmp (extension, "json")) return "application/json";
    if (0 == strcasecmp (extension, "txt")) return "text/plain";
    if (0 == strcasecmp (extension, "xml")) return "text/xml";
    if (0 == strcasecmp (extension, "svg")) return "image/svg+xml";
    if (0 == strcasecmp (extension, "png")) return "image/png";
    if (0 == strcasecmp (extension, "jpg")) return "image/jpg";

    return NULL;
}


static const char *create_rfc822_date (time_t t)
{
    static char datebuf[80]; /* if-threaded: unstaticify */
    struct tm *now = gmtime (& t);
    size_t rc = strftime (datebuf, sizeof(datebuf), "%a, %d %b %Y %T %z", now);
    if (rc <= 0 || rc >= sizeof(datebuf)) return NULL;
    return datebuf;
}



/* Respond to a GET request, not under the pmwebapi URL prefix.  This
   is a mini fileserver, just for small standalone installations of
   pmwebapi-based web front-ends. */
int pmwebres_respond (void *cls, struct MHD_Connection *connection,
                      const char* url)
{
    int fd = -1;
    int rc;
    char filename [PATH_MAX];
    struct stat fds;
    unsigned int resp_code = MHD_HTTP_OK;
    struct MHD_Response *resp;
    const char *ctype = NULL;
    static const char error_page[] =
        "PMRESAPI error"; /* could also be an actual error page... */

    (void) cls;
    assert (resourcedir != NULL); /* facility is enabled at all */

    assert (url[0] == '/');
    rc = snprintf (filename, sizeof(filename), "%s%s", resourcedir, url);
    if (rc < 0 || rc >= (int)sizeof(filename))
        goto error_response;

    /* Reject some obvious ways of escaping resourcedir. */
    if (NULL != strstr (filename, "/../")) {
        pmweb_notify (LOG_ERR, connection, "pmwebres suspicious url %s\n", url);
        goto error_response;
    }

    fd = open (filename, O_RDONLY);
    if (fd < 0) {
        pmweb_notify (LOG_ERR, connection, "pmwebres open %s failed (%d)\n", filename, fd);
        resp_code = MHD_HTTP_NOT_FOUND;
        goto error_response;
    }

    rc = fstat (fd, &fds);
    if (rc < 0) {
        pmweb_notify (LOG_ERR, connection, "pmwebres stat %s failed (%d)\n", filename, rc);
        close (fd);
        goto error_response;
    }

    /* XXX: handle if-modified-since */

    if (! S_ISREG (fds.st_mode)) {
        pmweb_notify (LOG_ERR, connection, "pmwebres non-file %s attempted\n", filename);
        close (fd);

        /* XXX: list directory, or redirect to index.html instead? */
        resp_code = MHD_HTTP_FORBIDDEN;
        goto error_response;
    }

    if (verbosity > 2)
        pmweb_notify (LOG_INFO, connection, "pmwebres serving file %s.\n", filename);

    resp = MHD_create_response_from_fd_at_offset (fds.st_size, fd, 0);    /* auto-closes fd */
    if (resp == NULL) {
        pmweb_notify (LOG_ERR, connection, "MHD_create_response_from_callback failed\n");
        goto error_response;
    }

    /* Guess at a suitable MIME content-type. */
    ctype = guess_content_type (filename);
    if (ctype)
        (void) MHD_add_response_header (resp, "Content-Type", ctype);

    /* And since we're generous to a fault, supply a timestamp field to
       assist caching. */
    ctype = create_rfc822_date (fds.st_mtime);
    if (ctype)
        (void) MHD_add_response_header (resp, "Last-Modified", ctype);

    /* Add a 5-minute expiry. */
    ctype = create_rfc822_date (time(0) + 300); /* XXX: configure */
    if (ctype)
        (void) MHD_add_response_header (resp, "Expires", ctype);

    (void) MHD_add_response_header (resp, "Cache-Control", "public");

    rc = MHD_queue_response (connection, resp_code, resp);
    MHD_destroy_response (resp);
    return rc;

 error_response:
    resp = MHD_create_response_from_buffer (strlen(error_page),
                                            (char*)error_page, 
                                            MHD_RESPMEM_PERSISTENT);
    if (resp == NULL) {
        pmweb_notify (LOG_ERR, connection, "MHD_create_response_from_callback failed\n");
        return MHD_NO;
    }

    (void) MHD_add_response_header (resp, "Content-Type", "text/plain");

    rc = MHD_queue_response (connection, resp_code, resp);
    MHD_destroy_response (resp);
    if (rc != MHD_YES) {
        pmweb_notify (LOG_ERR, connection, "MHD_queue_response failed\n");
        return MHD_NO;
    }

    return rc;
}
