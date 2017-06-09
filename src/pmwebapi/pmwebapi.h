/*
 * JSON web bridge for PMAPI.
 *
 * Copyright (c) 2011-2017 Red Hat Inc.
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
#include <iostream>
#include <map>
#include <limits>

extern "C"
{
#include "pmapi.h"
#include "impl.h"
#include <assert.h>
#include <sys/stat.h>
#include <time.h>
#include <stdarg.h>
#include <microhttpd.h>
}

#if !defined(MHD_SOCKET_DEFINED)
typedef int MHD_socket;
#endif

// Some strings (metric names!) would be repeated over and over and
// over again on the heap, were it not for some interning machinery
// such as boost::flyweight.
#ifdef HAVE_BOOST_FLYWEIGHT_HPP
#undef uint_t
#include <boost/flyweight.hpp>
#include <boost/flyweight/no_tracking.hpp>
#include <boost/flyweight/hashed_factory.hpp>
typedef boost::flyweight<std::string,boost::flyweights::no_tracking,boost::flyweights::hashed_factory<> > flyweight_string;
#else
typedef std::string flyweight_string;
#endif




/* ------------------------------------------------------------------------ */

/* a subset of option flags that needs to be read by the other modules */
extern std::string uriprefix;			/* hard-coded */
extern std::string archivesdir;			/* set by -A option */
extern std::string resourcedir;			/* set by -R option */
extern int dumpstats;                           /* set by -d option */
extern unsigned verbosity;			/* set by -v option */
extern unsigned permissive;			/* set by -P option */
extern unsigned new_contexts_p;		/* cleared by -N option */
extern unsigned exit_p;			/* counted by SIG* handler */
extern unsigned maxtimeout;			/* set by -t option */
extern unsigned multithread;			/* set by -M option */
extern unsigned graphite_timestep;              /* set by -i option */
extern unsigned graphite_hostcache;             /* set by -J option */
extern unsigned graphite_encode;                /* set by -X option */

struct http_params: public std::multimap <std::string, std::string> {
    std::string operator [] (const std::string &) const;
    std::vector <std::string> find_all (const std::string &) const;
};


/* ------------------------------------------------------------------------ */

// main.cxx
extern int
mhd_notify_error (struct MHD_Connection *connection, int rc);

// pmwebapi.cxx
extern int
pmwebapi_bind_permanent (int webapi_ctx, int pcp_context);
extern int
pmwebapi_respond (struct MHD_Connection *connection, const http_params &,
                  const std::vector <std::string> &url);
extern unsigned
pmwebapi_gc (void);
extern void
pmwebapi_deallocate_all (void);

// pmresapi.cxx
extern int
pmwebres_respond (struct MHD_Connection *connectio, const http_params &,
                  const std::string & url);

#if defined(HAVE_GRAPHITE)
// pmgraphite.cxx
extern int
pmgraphite_respond (struct MHD_Connection *connection, const http_params &,
                    const std::vector <std::string> &url, const std::string& url0);
#else
#define pmgraphite_respond(conn,params,url,url0) mhd_notify_error(conn, -EOPNOTSUPP)
#endif

extern void ac_refresh_all(struct MHD_Connection *connection);


// util.cxx
extern std::ostream & timestamp (std::ostream & o);
extern std::string conninfo (MHD_Connection *, bool serv_p);
extern std::ostream & connstamp (std::ostream & o, MHD_Connection *);
extern std::string urlencode (const std::string &);
extern std::vector <std::string> split (const std::string & s, char sep);
extern std::vector <flyweight_string> fwsplit (const std::string & s, char sep);
extern int cursed_path_p (const std::string & blessed, const std::string & questionable);
extern void json_quote (std::ostream & o, const std::string & value);
extern struct MHD_Response *NOTMHD_compressible_response(struct MHD_Connection *connection,
                                                         const std::string& buf);
extern std::string escapeString(const std::string&);

// inlined right here

// http://stackoverflow.com/a/20446239/661150
inline bool has_suffix(const std::string &str, const std::string &suffix)
{
    return (str.size() >= suffix.size() &&
            str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0);
}

inline bool has_prefix(const std::string &str, const std::string &prefix)
{
    return (str.size() >= prefix.size() &&
            str.compare(0, prefix.size(), prefix) == 0);
}



/* A convenience function to print a vanilla-ascii key and an
   unknown ancestry value as a JSON pair.  Add given suffix,
   which is likely to be a comma or a \n. */
template <class Value> void static inline
json_key_value (std::ostream & o, const std::string & key, const Value & value,
                const char *suffix = " ")
{
    o << '"' << key << '"' << ':';
    if (std::numeric_limits<Value>::is_specialized) {
        // print more bits of precision for larger numeric types
        std::streamsize ss = o.precision();
        o.precision (std::numeric_limits<Value>::digits10 + 2 /* for rounding? */);
        o << value;
        o.precision (ss);
    } else {
        o << value;
    }
    o << suffix;
}

// prevent pointers (including char*) from coming this way
template <class Value> void json_key_value (std::ostream &, const std::string &,
        const Value *, const char *);	// link-time error

template <>			// <-- NB: important for proper overloading/specialization of the template
void inline
json_key_value (std::ostream & o, const std::string & key, const std::string & value,
                const char *suffix)
{
    o << '"' << key << '"' << ':';
    json_quote (o, value);
    o << suffix;
}
