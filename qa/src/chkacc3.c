/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2012-2014 Red Hat.
 */

/* Check access control wildcarding, bad ops etc. */

#include <pcp/pmapi.h>
#include "libpcp.h"
#include "localconfig.h"

int a[4] = {0, 37, 235, 126};
int b[4] = {0, 201, 77, 127};
int c[4] = {0, 15, 191, 64};
int d[4] = {0, 1, 127, 254};

int
main(int argc, char **argv)
{
    int			s, sts, op, i, ai, bi, ci, di;
    unsigned int	perm;
    int			ipv4 = -1;
    int			ipv6 = -1;
    int			errflag = 0;
    int			copt;
    char		name[4*8 + 7 + 1]; /* handles full IPv6 address, if supported */
    char		*wnames[4] = { ".*", "38.*", "38.202.*", "38.202.16.*" };
    char		*wnames6[4] = { ":*", "26:*", "26:ca:*", "26:ca:10:*" };
    __pmSockAddr	*inaddr;

    /* trim cmd name of leading directory components */
    pmSetProgname(argv[0]);

    while ((copt = getopt(argc, argv, "46D:?")) != EOF) {
	switch (copt) {

	case '4':	/* ipv4 (default) */
	    ipv4 = 1;
	    break;

	case '6':	/* ipv6 */
	    ipv6 = 1;
	    break;

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmGetProgname(), optarg);
		errflag++;
	    }
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag) {
	fprintf(stderr,
"Usage: %s [options]\n\
\n\
Options:\n\
  -4             do IPv4 (default)\n\
  -6		 do IPv6\n",
                pmGetProgname());
        return 1;
    }

    /* defaults */
    if (ipv4 == -1) ipv4 = 1;
    if (ipv6 == -1) ipv6 = 0;


    /* there are 10 ops numbered from 0 to 9 */
    sts = 0;
    for (op = 0; op < 10; op++)
	if ((s = __pmAccAddOp(1 << op)) < 0) {
	    fprintf(stderr, "Bad op %d: %s\n", op, strerror(errno));
	    sts = s;
	}

    if (sts < 0)
	return 1;

    /* every address except address 0.* and 0:* leaves ops 8 and 9 unspecified */
    op = 0;
    for (ai = 0; ai < 4; ai++)
	for (bi = 0; bi < 4; bi++)
	    for (ci = 0; ci < 4; ci++)
		for (di = 0; di < 4; di++) {
		    perm = ++op;
		    if (ipv4) {
			pmsprintf(name, sizeof(name), "%d.%d.%d.%d", a[ai], b[bi], c[ci], d[di]);
			if (perm >= 1 << 8) {
			    fprintf(stderr, "expect error, perm=%d (>255):\n", perm);
			    perm = 1 << 10;
			}
			if (ai == 0) {
			    /* 0.0.* gets y,y; 0.201.* gets n,y; 0.77.* gets y,n;
			     * 0.127.* gets n,n
			     */
			    perm |= (bi << 8);
			    s = __pmAccAddHost(name, 0x3ff, perm, 0);
			}
			else
			    s = __pmAccAddHost(name, 0xff, perm, 0);
			if (s < 0) {
			    fprintf(stderr, "add host for host %s error: %s\n",
				    name, pmErrStr(s));
			    continue;
			}
			fprintf(stderr, "set %03x for host %d.%d.%d.%d\n",
				perm, a[ai], b[bi], c[ci], d[di]);
		    }
		    if (ipv6) {
			pmsprintf(name, sizeof(name), "%x:%x:%x:%x:%x:%x:%x:%x",
				a[ai], b[bi], c[ci], d[di],
				a[ai], b[bi], c[ci], d[di]);
			perm = op;
			if (perm >= 1 << 8) {
			    fprintf(stderr, "expect error, perm=%d (>255):\n", perm);
			    perm = 1 << 10;
			}
			if (ai == 0) {
			    /* 0.0.* gets y,y; 0.201.* gets n,y; 0.77.* gets y,n;
			     * 0.127.* gets n,n
			     */
			    perm |= (bi << 8);
			    s = __pmAccAddHost(name, 0x3ff, perm, 0);
			}
			else
			    s = __pmAccAddHost(name, 0xff, perm, 0);
			if (s < 0) {
			    fprintf(stderr, "add host for host %s error: %s\n",
				    name, pmErrStr(s));
			    continue;
			}
			fprintf(stderr, "set %03x for host %x:%x:%x:%x:%x:%x:%x:%x\n",
				perm,
				a[ai], b[bi], c[ci], d[di],
				a[ai], b[bi], c[ci], d[di]);
		    }
		}

    /* ops 8 and 9 are for wildcard testing:
     * hosts matching	op8	op9
     *	*		y	y
     *	38.*		y	n
     *  38.202.*	n	y
     *	38.202.16	n	n
     *  26:*		y	n
     *  26:ca:*		n	y
     *  26:ca:10:*	n	n
     */
    sts = 0;
    for (i = 0; i < 4; i++) {
	if (ipv4) {
	    if ((s = __pmAccAddHost(wnames[i], 0x300, (i << 8), 0)) < 0) {
		fprintf(stderr, "cannot add inet host for op%d: %s\n", i, strerror(s));
		sts = s;
	    }
	}
	if (ipv6) {
	    if ((s = __pmAccAddHost(wnames6[i], 0x300, (i << 8), 0)) < 0) {
		fprintf(stderr, "cannot add IPv6 host for op%d: %s\n", i, strerror(s));
		sts = s;
	    }
	}
    }
    if (sts < 0)
	return 1;

    putc('\n', stderr);

    putc('\n', stderr);
    __pmAccDumpHosts(stderr);

    putc('\n', stderr);

    putc('\n', stderr);
    if (ipv4) {
	for (i = 0; i < 2; i++)
	    for (ai = 0; ai < 4; ai++)
		for (bi = 0; bi < 4; bi++)
		    for (ci = 0; ci < 4; ci++)
			for (di = 0; di < 4; di++) {
			  char	buf[20];
			    char   *host;
			    pmsprintf(buf, sizeof(buf), "%d.%d.%d.%d", a[ai]+i, b[bi]+i, c[ci]+i, d[di]+i);
			    if ((inaddr =__pmStringToSockAddr(buf)) == NULL) {
			      printf("insufficient memory\n");
			      continue;
			    }
			    s = __pmAccAddClient(inaddr, &perm);
			    host = __pmSockAddrToString(inaddr);
			    __pmSockAddrFree(inaddr);
			    if (s < 0) {
				fprintf(stderr, "from %s error: %s\n", host, pmErrStr(s));
				free(host);
				continue;
			    }
			    fprintf(stderr, "got %03x for host %s\n", perm, host);
			    free(host);
			}
    }
    if (ipv6) {
	for (i = 0; i < 2; i++)
	    for (ai = 0; ai < 4; ai++)
		for (bi = 0; bi < 4; bi++)
		    for (ci = 0; ci < 4; ci++)
			for (di = 0; di < 4; di++) {
			    char	buf[4*8 + 7 + 1]; /* handles full IPv6 address */
			    char   *host;
			    pmsprintf(buf, sizeof(buf), "%x:%x:%x:%x:%x:%x:%x:%x",
				    a[ai]+i, b[bi]+i, c[ci]+i, d[di]+i,
				    a[ai]+i, b[bi]+i, c[ci]+i, d[di]+i);
			    if ((inaddr =__pmStringToSockAddr(buf)) == NULL) {
			      printf("insufficient memory\n");
			      continue;
			    }
			    s = __pmAccAddClient(inaddr, &perm);
			    host = __pmSockAddrToString(inaddr);
			    __pmSockAddrFree(inaddr);
			    if (s < 0) {
				fprintf(stderr, "from %s error: %s\n", host, pmErrStr(s));
				free(host);
				continue;
			    }
			    fprintf(stderr, "got %03x for host %s\n", perm, host);
			    free(host);
			}
    }
    
    return 0;
}
