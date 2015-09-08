/*
 * Copyright (c) 2013-2014 Red Hat, Inc.  All Rights Reserved.
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

#include <iostream>
#include <sstream>
#include <vector>

extern "C"
{
#include <sys/stat.h>
#include <stdarg.h>
#include <microhttpd.h>
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
vector <string> split (const std::string & str, char sep)
{
    vector <string> elems;

    string::size_type lastPos = 0;

    while (1)
        {
            string::size_type pos = str.find_first_of(sep, lastPos); // may be ::npos
            elems.push_back(str.substr(lastPos, pos - lastPos));
            if (pos == string::npos) break;
            lastPos = pos + 1;
        }

    return elems;
}


// Take two names of files/directories.  Resolve them both with
// realpath(3), and check whether the latter is beneath the first.
// This is intended to catch naughty people passing /../ in URLs.
// Default to rejection (on errors).
bool
cursed_path_p (const string & blessed, const string & questionable)
{
    char *rp1c = realpath (blessed.c_str (), NULL);
    if (rp1c == NULL) {
        return true;
    }
    string rp1 = string (rp1c);
    free (rp1c);

    char *rp2c = realpath (questionable.c_str (), NULL);
    if (rp2c == NULL) {
        return true;
    }
    string rp2 = string (rp2c);
    free (rp2c);

    if (rp1 == rp2) {
        // identical: OK
        return false;
    }

    if (rp2.size () < rp1.size () + 1) {
        // rp2 cannot be nested within rp1
        return true;
    }

    if (rp2.substr (0, rp1.size () + 1) == (rp1 + '/')) {
        // rp2 nested beneath rp1/
        return false;
    }

    return true;
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
