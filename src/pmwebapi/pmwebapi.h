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

#include <string>
#include <ios>
#include <vector>

extern "C" {
#include "pmapi.h"
#include "impl.h"
#include <assert.h>
#include <sys/stat.h>
#include <time.h>
#include <stdarg.h>
#include <microhttpd.h>
}
																																																																																	    /* ------------------------------------------------------------------------ *//* a subset of option flags that needs to be read by the other modules */
    extern std::string uriprefix;
																							/* hard-coded */
extern std::string archivesdir;	/* set by -A option */
extern std::string resourcedir;	/* set by -R option */
extern unsigned verbosity;	/* set by -v option */
extern unsigned new_contexts_p;	/* cleared by -N option */
extern unsigned exit_p;		/* counted by SIG* handler */
extern unsigned maxtimeout;	/* set by -t option */

/* ------------------------------------------------------------------------ */

// main.cxx
extern int mhd_notify_error (struct MHD_Connection *connection, int rc);

// pmwebapi.cxx
extern int pmwebapi_bind_permanent (int webapi_ctx, int pcp_context);
extern int pmwebapi_respond (struct MHD_Connection *connection,
			     const std::vector < std::string > &url);
extern unsigned pmwebapi_gc (void);
extern void pmwebapi_deallocate_all (void);

// pmresapi.cxx
extern int pmwebres_respond (struct MHD_Connection *connection, const std::string & url);

// pmgraphite.cxx
extern int pmgraphite_respond (struct MHD_Connection *connection,
			       const std::vector < std::string > &url);

// util.cxx
extern std::ostream & timestamp (std::ostream & o);
extern std::ostream & connstamp (std::ostream & o, MHD_Connection *);
extern std::vector < std::string > split (const std::string & s, char delim);
extern bool cursed_path_p (const std::string & blessed, const std::string & questionable);
