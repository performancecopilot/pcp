/*
 * JSON web bridge for PMAPI.
 *
 * Copyright (c) 2011 Red Hat Inc.
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

#define _XOPEN_SOURCE 600
#define _FILE_OFFSET_BITS 64

#include <microhttpd.h>

#include "pmapi.h"
#include "impl.h"

#include <sys/types.h>
#include <sys/time.h>
#include <syslog.h>
#include <sys/socket.h>
#include <netdb.h>
#include <limits.h>
#include <assert.h>
#include <sys/stat.h>
#include <unistd.h>

/* ------------------------------------------------------------------------ */

extern char *pmnsfile;                /* set by -n option */
extern char *uriprefix;               /* overridden by -a option */
extern char *resourcedir;             /* set by -r option */
extern unsigned verbosity;            /* set by -v option */
extern unsigned maxtimeout;           /* set by -t option */
extern unsigned exit_p;               /* counted by SIG* handler */

/* ------------------------------------------------------------------------ */

extern int pmwebapi_respond (void *cls, struct MHD_Connection *connection,
                             const char* url, const char* method,
                             const char* upload_data, size_t *upload_data_size);
extern int pmwebres_respond (void *cls, struct MHD_Connection *connection, const char* url);
