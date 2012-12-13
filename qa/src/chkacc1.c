/*
 * Copyright (c) 2012 Red Hat.
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>

int
main()
{
    int			s, sts, op, host;
    unsigned int	i;
    char		name[20];
    struct __pmSockAddr	*inaddr;

    sts = 0;
    for (op = 0; op < WORD_BIT; op++) {
	if ((s = __pmAccAddOp(1 << op)) < 0) {
	    printf("Bad op %d: %s\n", op, strerror(errno));
	    sts = s;
	}
	if ((s = __pmAccAddOp(1 << op)) >= 0) {
	    printf("duplicate op test failed for op %d\n", op);
	    sts = s;
	}
    }
    if (sts < 0)
	exit(1);

    for (host = 0; host < WORD_BIT; host++) {
	sprintf(name, "155.%d.%d.%d", host * 3, 17+host, host);
	if ((s = __pmAccAddHost(name, 1 << host, 1 << host, 0)) < 0) {
	    printf("cannot add host for op%d: %s\n", host, strerror(s));
	    sts = s;
	}
    }
    if (sts < 0)
	exit(1);

    if ((inaddr = __pmAllocSockAddr()) == NULL) {
	printf("insufficient memory\n");
	exit(2);
    }
    putc('\n', stderr);
    __pmAccDumpHosts(stderr);

    for (host = 0; host < WORD_BIT; host++) {
	char	buf[20];

	sprintf(buf, "%d.%d.%d.%d", 155, host * 3, 17+host, host);
	__pmStringToSockAddr(buf, inaddr);
	sts = __pmAccAddClient(inaddr, &i);
	if (sts < 0) {
	    printf("add client from host %d: %s\n", host, pmErrStr(sts));
	    continue;
	}
	else if (i != (1 << host))
	    printf("host %d: __pmAccAddClient returns denyOpsResult 0x%x (expected 0x%x)\n",
		   host, i, 1 << host);
	    
    }
    
    putc('\n', stderr);
    __pmAccDumpHosts(stderr);
    __pmFreeSockAddr(inaddr);

    exit(0);
}
