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

#include "pmapi.h"
#include "impl.h"
#include <assert.h>
#include <sys/stat.h>
#include <time.h>
#include <microhttpd.h>

/* ------------------------------------------------------------------------ */

extern char *pmnsfile;                /* set by -n option */
extern char *uriprefix;               /* overridden by -a option */
extern char *resourcedir;             /* set by -r option */
extern unsigned verbosity;            /* set by -v option */
extern unsigned maxtimeout;           /* set by -t option */
extern unsigned exit_p;               /* counted by SIG* handler */

/* ------------------------------------------------------------------------ */

extern int pmwebapi_bind_permanent (int webapi_ctx, int pcp_context);
extern int pmwebapi_respond (void *cls, struct MHD_Connection *connection,
                             const char* url, const char* method,
                             const char* upload_data, size_t *upload_data_size);
extern void pmwebapi_gc (void);
extern int pmwebres_respond (void *cls, struct MHD_Connection *connection,
                             const char* url);

extern void pmweb_notify (int, struct MHD_Connection*, const char *, ...) __PM_PRINTFLIKE(3,4);
