/*
 * Copyright (c) 1995-2002 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include "libpcp.h"

/*
 * internal structs with field order reversed for the not network
 * byte order case
 */
typedef struct {
	unsigned int	serial : 22;		/* unique within PMD */
	unsigned int	domain : 9;		/* the administrative PMD */
	int		flag : 1;
} __pmInDom_rev;

typedef struct {
	unsigned int	item : 10;
	unsigned int	cluster : 12;
	unsigned int	domain : 9;
	int		flag : 1;
} __pmID_rev;

int
main(int argc, char **argv)
{
    char		buf[4096];
    char		*p;
    char		*q;
    char		*last;
    char		*t;
    char		c;
    unsigned long	val;
    unsigned long	nval;
    int			type;	/* 0 plain, 1 indom, 2 pmid */
    __pmInDom_int	*ip;
    __pmInDom_rev	*nip;
    __pmID_int		*pp;
    __pmID_rev		*npp;

    /* 0x01020304 */

    while (fgets(buf, sizeof(buf), stdin) != NULL) {
	p = buf;
	while ((q = strstr(p, "0x")) != NULL) {
	    q += 2;
	    c = *q;
	    *q = '\0';
	    fputs(p, stdout);
	    type = 0;
	    last = p;
	    if ((t = strstr(p, "InDom")) >= last) {
		last = t;
		type = 1;
	    }
	    if ((t = strstr(p, "indom:")) >= last) {
		last = t;
		type = 1;
	    }
	    if ((t = strstr(p, "indom=")) >= last) {
		last = t;
		type = 1;
	    }
	    if ((t = strstr(p, "instance id:")) >= last) {
		last = t;
		type = -1;	/* don't convert */
	    }
	    if ((t = strstr(p, "PMID")) >= last) {
		last = t;
		type = 2;
	    }
	    /* final check, no assignment needed */
	    if (strstr(p, "pmid:") >= last) {
		type = 2;
	    }
	    *q = c;
	    p = q;
	    val = strtoul(p, &q, 16);
	    if (q-p > 0 && val != (nval = htonl(val))) {
		if (type == -1) {
		    /* don't convert */
		    nval = val;
		}
		else if (type == 0) {
		    /* full-word byte swap */
		    ;
		}
		else if (type == 1) {
		    /* instance domain */
		    ip = (__pmInDom_int *)&val;
		    nip = (__pmInDom_rev *)&nval;
		    nip->flag = ip->flag;
		    nip->domain = ip->domain;
		    nip->serial = ip->serial;
		}
		else if (type == 2) {
		    /* metric identifier */
		    pp = (__pmID_int *)&val;
		    npp = (__pmID_rev *)&nval;
		    npp->flag = pp->flag;
		    npp->domain = pp->domain;
		    npp->cluster = pp->cluster;
		    npp->item = pp->item;
		}
		printf("%0*x", (int)(q-p), (unsigned)nval);
		p = q;
	    }
	}
	fputs(p, stdout);
    }
    return 0;
}
