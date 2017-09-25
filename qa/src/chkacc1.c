/*
 * Copyright (c) 2012-2014 Red Hat.
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include "localconfig.h"

int
main(int argc, char **argv)
{
    int			s, sts, op, host;
    unsigned int	i;
    char		name[4*8 + 7 + 1]; /* handles full IPv6 address, if supported */
    int			ipv4 = -1;
    int			ipv6 = -1;
    int			errflag = 0;
    int			c;
    __pmSockAddr	*inaddr;

    /* trim cmd name of leading directory components */
    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "46D:?")) != EOF) {
	switch (c) {

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
		    pmProgname, optarg);
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
                pmProgname);
        return 1;
    }

    /* defaults */
    if (ipv4 == -1) ipv4 = 1;
    if (ipv6 == -1) ipv6 = 0;

    sts = 0;
    for (op = 0; op < WORD_BIT; op++) {
	if ((s = __pmAccAddOp(1 << op)) < 0) {
	    printf("Bad op %d: %s\n", op, strerror(errno));
	    sts = s;
	}
	if ((s = __pmAccAddOp(1 << op)) >= 0) {
	    printf("duplicate op test failed for op %d\n", op);
	    sts = -EINVAL;
	}
    }
    if (sts < 0)
	return 1;

    for (host = 0; host < WORD_BIT; host++) {
	if (ipv4) {
	    pmsprintf(name, sizeof(name), "155.%d.%d.%d", host * 3, 17+host, host);
	    if ((s = __pmAccAddHost(name, 1 << host, 1 << host, 0)) < 0) {
		printf("cannot add inet host for op%d: %s\n", host, strerror(s));
		sts = s;
	    }
	}
	if (ipv6) {
	    pmsprintf(name, sizeof(name), "fec0::%x:%x:%x:%x:%x:%x",
		    host * 3, 17+host, host,
		    host * 3, 17+host, host);
	    if ((s = __pmAccAddHost(name, 1 << host, 1 << host, 0)) < 0) {
		printf("cannot add IPv6 host for op%d: %s\n", host, strerror(s));
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

    if (ipv4) {
	for (host = 0; host < WORD_BIT; host++) {
	    char	buf[20];

	    pmsprintf(buf, sizeof(buf), "%d.%d.%d.%d", 155, host * 3, 17+host, host);
	    if ((inaddr =__pmStringToSockAddr(buf)) == NULL) {
		printf("insufficient memory\n");
		continue;
	    }
	    sts = __pmAccAddClient(inaddr, &i);
	    __pmSockAddrFree(inaddr);

	    if (sts < 0) {
		printf("add inet client from host %d: %s\n", host, pmErrStr(sts));
		continue;
	    }
	    else if (i != (1 << host))
		printf("inet host %d: __pmAccAddClient returns denyOpsResult 0x%x (expected 0x%x)\n",
		       host, i, 1 << host);
	}
    }

    if (ipv6) {
	for (host = 0; host < WORD_BIT; host++) {
	    char	buf[4*8 + 7 + 1]; /* handles full IPv6 address */

	    pmsprintf(buf, sizeof(buf), "fec0::%x:%x:%x:%x:%x:%x",
		    host * 3, 17+host, host,
		    host * 3, 17+host, host);
	    if ((inaddr =__pmStringToSockAddr(buf)) == NULL) {
		printf("insufficient memory\n");
		continue;
	    }
	    sts = __pmAccAddClient(inaddr, &i);
	    __pmSockAddrFree(inaddr);

	    if (sts < 0) {
		printf("add IPv6 client from host %d: %s\n", host, pmErrStr(sts));
		continue;
	    }
	    else if (i != (1 << host))
		printf("IPv6 host %d: __pmAccAddClient returns denyOpsResult 0x%x (expected 0x%x)\n",
		       host, i, 1 << host);
	}
    }
    
    putc('\n', stderr);

    putc('\n', stderr);
    __pmAccDumpHosts(stderr);

    putc('\n', stderr);

    return 0;
}
