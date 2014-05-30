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

#define _XOPEN_SOURCE 500

#include "pmapi.h"
#include "impl.h"

#include <iostream>
#include <sstream>
#include <vector>

extern "C" {
#include <sys/stat.h>
#include <sys/syscall.h>
#include <time.h>
#include <stdarg.h>
#include <stdlib.h>
#include <netdb.h>
#include <microhttpd.h>
} using namespace std;


// Print a string to cout/cerr progress reports, similar to the
// stuff produced by __pmNotifyErr
ostream & timestamp (ostream & o)
{
    time_t now;
    time (&now);
    char *now2 = ctime (&now);
    if (now2)
	now2[19] = '\0';	// overwrite \n

    return o << "[" << (now2 ? now2 : "") << "] " << pmProgname << "(" << getpid () <<
	"): ";
    // NB: we're single-threaded; no point printing out a thread-id too
}


// Print connection-specific string
ostream & connstamp (ostream & o, struct MHD_Connection * conn)
{
    char hostname[128];
    char servname[128];
    int sts = -1;

    /* Look up client address data. */
    const union MHD_ConnectionInfo *u =
	MHD_get_connection_info (conn, MHD_CONNECTION_INFO_CLIENT_ADDRESS);
    struct sockaddr *so = u ? u->client_addr : 0;

    if (so && so->sa_family == AF_INET)
	sts = getnameinfo (so, sizeof (struct sockaddr_in),
			   hostname, sizeof (hostname),
			   servname, sizeof (servname), NI_NUMERICHOST | NI_NUMERICSERV);
    else if (so && so->sa_family == AF_INET6)
	sts = getnameinfo (so, sizeof (struct sockaddr_in6),
			   hostname, sizeof (hostname),
			   servname, sizeof (servname), NI_NUMERICHOST | NI_NUMERICSERV);
    if (sts != 0)
	hostname[0] = servname[0] = '\0';

    timestamp (o) << "[" << hostname << ":" << servname << "] ";

    return o;
}


// based on http://stackoverflow.com/a/236803/661150
vector < string > split (const std::string & s, char delim)
{
    vector < string > elems;
    stringstream ss (s);
    string item;
    while (getline (ss, item, delim)) {
	elems.push_back (item);
    }
    return elems;
}


// Take two names of files/directories.  Resolve them both with
// realpath(3), and check whether the latter is beneath the first.
// This is intended to catch naughty people passing /../ in URLs.
// Default to rejection (on errors).
bool cursed_path_p (const string & blessed, const string & questionable)
{
    char *rp1c = realpath (blessed.c_str (), NULL);
    if (rp1c == NULL)
	return true;
    string rp1 = string (rp1c);
    free (rp1c);

    char *rp2c = realpath (questionable.c_str (), NULL);
    if (rp2c == NULL) {
	return true;
    }
    string rp2 = string (rp2c);
    free (rp2c);

    if (rp1 == rp2)		// identical: OK
	return false;

    if (rp2.size () < rp1.size () + 1)	// rp2 cannot be nested within rp1
	return true;

    if (rp2.substr (0, rp1.size () + 1) == (rp1 + '/'))	// rp2 nested beneath rp1/
	return false;

    return true;
}
