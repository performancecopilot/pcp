/*
 * Copyright (c) 1998,2004 Silicon Graphics, Inc.  All Rights Reserved.
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
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "pmapi.h"
#include <rpc/rpc.h>

#define AUTOFSD_PROGRAM 100099UL
#define AUTOFSD_VERSION 1UL

/*
 * probe IRIX autofsd(1M)
 */
int
main(int argc, char **argv)
{
    struct timeval	tv = { 10, 0 };
    CLIENT		*clnt;
    enum clnt_stat	stat;
    int			c;
    char		*p;
    char		*host = "local:";
    int			errflag = 0;

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "h:t:?")) != EOF) {
	switch (c) {

	case 'h':	/* contact autofsd on this hostname */
	    host = optarg;
	    break;

	case 't':	/* change timeout interval */
	    if (pmParseInterval(optarg, &tv, &p) < 0) {
		fprintf(stderr, "%s: illegal -t argument\n", pmGetProgname());
		fputs(p, stderr);
		free(p);
		errflag++;
	    }
	    break;

	case '?':
	default:
	    fprintf(stderr, "Usage: %s [-h host] [-t timeout]\n", pmGetProgname());
	    errflag++;
	    break;
	}
    }

    if (errflag)
	exit(4);

    if ((clnt = clnt_create(host, AUTOFSD_PROGRAM, AUTOFSD_VERSION, "udp")) == NULL) {
	clnt_pcreateerror("clnt_create");
	exit(2);
    }

    /*
     * take control of the timeout algorithm
     */
    clnt_control(clnt, CLSET_TIMEOUT, (char *)&tv);
    clnt_control(clnt, CLSET_RETRY_TIMEOUT, (char *)&tv);

    stat = clnt_call(clnt, NULLPROC, (xdrproc_t)xdr_void, (char *)0,
					(xdrproc_t)xdr_void, (char *)0, tv);

    if (stat != RPC_SUCCESS) {
	clnt_perror(clnt, "clnt_call");
	exit(1);
    }

    exit(0);
}
