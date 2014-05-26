/*
 * PMWEBD graphite-api emulation
 *
 * Copyright (c) 2014 Red Hat Inc.
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
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <ctype.h>
#include <fts.h>
#ifdef HAVE_CAIRO
#include <cairo/cairo.h>
#endif


/* ------------------------------------------------------------------------ */

/* Search for archives under given base directory.  */


/* ------------------------------------------------------------------------ */


int pmgraphite_respond (void *cls, struct MHD_Connection *connection,
                        const char *url)
{
    int rc;


    /* XXX: emit CORS header, e.g.
       https://developer.mozilla.org/en-US/docs/HTTP/Access_control_CORS */

    /* -------------------------------------------------------------------- */
    /* context creation */
    /*
    if (0 == strcmp (url, "render") &&
	(0 == strcmp (method, "POST") || 0 == strcmp (method, "GET")))
	return pmgraphite_render (connection);
    */

    rc = -EINVAL;

  out:
    return mhd_notify_error (connection, rc);
}
