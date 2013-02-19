/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2012-2013 Red Hat.
 */

/* Check access control wildcarding, bad ops etc. */

#include <pcp/pmapi.h>
#include <pcp/impl.h>

int a[4] = {0, 37, 235, 126};
int b[4] = {0, 201, 77, 127};
int c[4] = {0, 15, 191, 64};
int d[4] = {0, 1, 127, 254};

int
main()
{
    int			s, sts, op, i, ai, bi, ci, di;
    unsigned int	perm;
    char		name[20];
    char		*wnames[4] = { "*", "38.*", "38.202.*", "38.202.16.*" };
    __pmSockAddr	*inaddr;

    /* there are 10 ops numbered from 0 to 9 */
    sts = 0;
    for (op = 0; op < 10; op++)
	if ((s = __pmAccAddOp(1 << op)) < 0) {
	    fprintf(stderr, "Bad op %d: %s\n", op, strerror(errno));
	    sts = s;
	}

    if (sts < 0)
	exit(1);

    /* every address except address 0.* leaves ops 8 and 9 unspecified */
    op = 0;
    for (ai = 0; ai < 4; ai++)
	for (bi = 0; bi < 4; bi++)
	    for (ci = 0; ci < 4; ci++)
		for (di = 0; di < 4; di++) {
		    sprintf(name, "%d.%d.%d.%d", a[ai], b[bi], c[ci], d[di]);
		    perm = ++op;
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

    /* ops 8 and 9 are for wildcard testing:
     * hosts matching	op8	op9
     *	*		y	y
     *	38.*		y	n
     *  38.202.*	n	y
     *	38.202.16	n	n
     */
    for (i = 0; i < 4; i++) {
	if ((s = __pmAccAddHost(wnames[i], 0x300, (i << 8), 0)) < 0) {
	    fprintf(stderr, "cannot add host for op%d: %s\n", i, strerror(s));
	    sts = s;
	}
    }
    if (sts < 0)
	exit(1);

    putc('\n', stderr);
    __pmAccDumpHosts(stderr);

    putc('\n', stderr);
    for (i = 0; i < 2; i++)
	for (ai = 0; ai < 4; ai++)
	    for (bi = 0; bi < 4; bi++)
		for (ci = 0; ci < 4; ci++)
		    for (di = 0; di < 4; di++) {
			char	buf[20];
			char   *host;
			sprintf(buf, "%d.%d.%d.%d", a[ai]+i, b[bi]+i, c[ci]+i, d[di]+i);
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
    
    exit(0);
}
