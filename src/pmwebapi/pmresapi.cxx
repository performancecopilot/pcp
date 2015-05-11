/*
 * JSON web bridge for PMAPI.
 *
 * Copyright (c) 2011-2014 Red Hat Inc.
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
#include "config.h"

#include <iostream>

using namespace std;


/* ------------------------------------------------------------------------ */

static const char *
guess_content_type (const char *filename)
{
    const char *extension = strrchr (filename, '.');
    if (extension == NULL) {
        return NULL;
    }

    /* One could go all out and parse /etc/mime.types, or one can do this ... */
    if (0 == strcasecmp (extension, ".html")) {
        return "text/html";
    }
    if (0 == strcasecmp (extension, ".js")) {
        return "text/javascript";
    }
    if (0 == strcasecmp (extension, ".json")) {
        return "application/json";
    }
    if (0 == strcasecmp (extension, ".txt")) {
        return "text/plain";
    }
    if (0 == strcasecmp (extension, ".xml")) {
        return "text/xml";
    }
    if (0 == strcasecmp (extension, ".svg")) {
        return "image/svg+xml";
    }
    if (0 == strcasecmp (extension, ".png")) {
        return "image/png";
    }
    if (0 == strcasecmp (extension, ".jpg")) {
        return "image/jpg";
    }

    return NULL;
}


static const char *
create_rfc822_date (time_t t)
{
    static char datebuf[80];	/* if-threaded: unstaticify */
    struct tm *now = gmtime (&t);
    size_t rc = strftime (datebuf, sizeof (datebuf), "%a, %d %b %Y %T %z", now);
    if (rc <= 0 || rc >= sizeof (datebuf)) {
        return NULL;
    }
    return datebuf;
}



/* Respond to a GET request, not under the pmwebapi URL prefix.  This
   is a mini fileserver, just for small standalone installations of
   pmwebapi-based web front-ends. */
int
pmwebres_respond (struct MHD_Connection *connection, const http_params& params, const string & url)
{
    int fd = -1;
    int rc;
    struct stat fds;
    unsigned int resp_code = MHD_HTTP_OK;
    struct MHD_Response *resp;
    const char *ctype = NULL;

    assert (resourcedir != "");	/* facility is enabled at all */
    /* NB: formerly, we asserted (url[0] == '/'), and this is proper for normal HTTP requests.
       But deliberately standards-violating HTTP requests could come with other guck, or
       even an empty string, which we need to tolerate  ... at least until we determine them
       to be cursed & reject them. */

    string resourcefile = resourcedir + url;	// pass through / path separators

    if (cursed_path_p (resourcedir, resourcefile)) {
        connstamp (cerr, connection) << "suspicious resource path " << resourcefile << endl;
        goto error_response;
    }

    fd = open (resourcefile.c_str (), O_RDONLY);
    if (fd < 0) {
        connstamp (cerr, connection) << "pmwebres failed to open " << resourcefile << endl;
        resp_code = MHD_HTTP_NOT_FOUND;
        goto error_response;
    }

    rc = fstat (fd, &fds);
    if (rc < 0) {
        connstamp (cerr, connection) << "pmwebres failed to stat " << resourcefile << endl;
        close (fd);
        goto error_response;
    }

    /* XXX: handle if-modified-since */

    if (S_ISDIR (fds.st_mode)) {

        string new_file = url;
        // NB: don't add a redundant / -- unlike in UNIX paths, // are not harmless in URLs
        if (url[url.size () - 1] != '/') {
            new_file += '/';
        }
        new_file += "index.html";

        // Propagate any incoming query-string parameters, since they might include
        // data that page javascript will look for.
        if (! params.empty ()) {
            for (http_params::const_iterator it = params.begin ();
                    it != params.end ();
                    it++) {
                if (it == params.begin ()) {
                    new_file += "?";
                } else {
                    new_file += "&";
                }

                new_file += it->first;
                new_file += "=";
                if (it->second != "") {
                    new_file += urlencode (it->second);
                }
            }
        }

        static char blank[] = "";
        resp = MHD_create_response_from_buffer (strlen (blank), blank, MHD_RESPMEM_PERSISTENT);
        if (resp) {
            rc = MHD_add_response_header (resp, "Location", new_file.c_str ());
            if (rc != MHD_YES) {
                connstamp (cerr, connection) << "MHD_add_response_header Location: failed" << endl;
            }

            /* Adding ACAO header */
            (void) MHD_add_response_header (resp, "Access-Control-Allow-Origin", "*");

            rc = MHD_queue_response (connection, MHD_HTTP_FOUND /* 302 */ , resp);
            if (rc != MHD_YES) {
                connstamp (cerr, connection) << "MHD_queue_response failed" << endl;
            }
            MHD_destroy_response (resp);
            return MHD_YES;
        }
        return MHD_NO;		// general 500 error
    }

    if (!S_ISREG (fds.st_mode)) { // some non-file non-directory
        connstamp (cerr, connection) << "pmwebres non-file " << resourcefile << endl;
        close (fd);

        resp_code = MHD_HTTP_FORBIDDEN;
        goto error_response;
    }

    if (verbosity > 2) {
        connstamp (clog, connection) << "pmwebres serving file " << resourcefile << endl;
    }

    resp = MHD_create_response_from_fd (fds.st_size, fd);	/* auto-closes fd */
    /* NB: Problems have been observed with *_from_fd_at_offset() on
       32-bit RHEL5, perhaps due to confusing off_t sizes.  */
    if (resp == NULL) {
        connstamp (cerr, connection) << "MHD_create_response_from_callbac failed" << endl;
        goto error_response;
    }

    /* Guess at a suitable MIME content-type. */
    ctype = guess_content_type (resourcefile.c_str ());
    if (ctype) {
        (void) MHD_add_response_header (resp, "Content-Type", ctype);
    }

    /* And since we're generous to a fault, supply a timestamp field to
       assist caching. */
    ctype = create_rfc822_date (fds.st_mtime);
    if (ctype) {
        (void) MHD_add_response_header (resp, "Last-Modified", ctype);
    }

#if 0
    /* Add a 5-minute expiry. */
    ctype = create_rfc822_date (time (0) + 300);	/* XXX: configure */
    if (ctype) {
        (void) MHD_add_response_header (resp, "Expires", ctype);
    }
#endif

    (void) MHD_add_response_header (resp, "Cache-Control", "public");

    /* Adding ACAO header */
    (void) MHD_add_response_header (resp, "Access-Control-Allow-Origin", "*");

    rc = MHD_queue_response (connection, resp_code, resp);
    MHD_destroy_response (resp);
    return rc;

error_response:
    return mhd_notify_error (connection, -EINVAL);
}
