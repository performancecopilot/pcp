/*
 * Verifies that the information we see from PMDA.pm (perl) matches
 * the local PCP installation (PMAPI) - used by test.pl: `make test`
 *
 * Copyright (c) 2004 Silicon Graphics, Inc.  All Rights Reserved.
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
#include <stdio.h>
#include <stdlib.h>
#include <values.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/pmda.h>

void
ids(void)
{
    struct {
	int	cluster;
	int	item;
    } data[] = {
	{ 0, 0 },
	{ 1, 1 },
	{ 37, 13 },
	{ MAXINT, 0 },
	{ 0, MAXINT },
	{ MAXINT, MAXINT }
    };
    int		i;

    for (i = 0; i < sizeof(data)/sizeof(data[0]); i++) {
	if (PMDA_PMID(data[i].cluster,data[i].item) != pmid_build(0,data[i].cluster,data[i].item)) {
	    fprintf(stderr, "botch: PMDA_PMID(%d,%d) -> %x", data[i].cluster, data[i].item, PMDA_PMID(data[i].cluster,data[i].item));
	    fprintf(stderr, " pmid_build(0,%d,%d) -> %x\n", data[i].cluster, data[i].item, pmid_build(0,data[i].cluster,data[i].item));
	}
	printf("PMDA_PMID: %d,%d = %d\n", data[i].cluster, data[i].item, PMDA_PMID(data[i].cluster,data[i].item));
    }
}

void
units(void)
{
    pmUnits forw;
    pmUnits back;

    forw.pad = back.pad = 0;
    forw.dimSpace	= back.scaleCount	= 1;
    forw.dimTime	= back.scaleTime	= 2;
    forw.dimCount	= back.scaleSpace	= 3;
    forw.scaleSpace	= back.dimCount		= 4;
    forw.scaleTime	= back.dimTime		= 5;
    forw.scaleCount	= back.dimSpace		= 6;
    printf("pmUnits: 1,2,3,4,5,6 = %d\n", *(unsigned int *)&forw);
    printf("pmUnits: 6,5,4,3,2,1 = %d\n", *(unsigned int *)&back);
}

void
defines(void)
{
    printf("PM_ID_NULL=%u\n", PM_ID_NULL);
    printf("PM_INDOM_NULL=%u\n", PM_INDOM_NULL);
    printf("PM_IN_NULL=%u\n", PM_IN_NULL);

    printf("PM_SPACE_BYTE=%u\n", PM_SPACE_BYTE);
    printf("PM_SPACE_KBYTE=%u\n", PM_SPACE_KBYTE);
    printf("PM_SPACE_MBYTE=%u\n", PM_SPACE_MBYTE);
    printf("PM_SPACE_GBYTE=%u\n", PM_SPACE_GBYTE);
    printf("PM_SPACE_TBYTE=%u\n", PM_SPACE_TBYTE);

    printf("PM_TIME_NSEC=%u\n", PM_TIME_NSEC);
    printf("PM_TIME_USEC=%u\n", PM_TIME_USEC);
    printf("PM_TIME_MSEC=%u\n", PM_TIME_MSEC);
    printf("PM_TIME_SEC=%u\n", PM_TIME_SEC);
    printf("PM_TIME_MIN=%u\n", PM_TIME_MIN);
    printf("PM_TIME_HOUR=%u\n", PM_TIME_HOUR);

    printf("PM_TYPE_NOSUPPORT=%u\n", PM_TYPE_NOSUPPORT);
    printf("PM_TYPE_32=%u\n", PM_TYPE_32);
    printf("PM_TYPE_U32=%u\n", PM_TYPE_U32);
    printf("PM_TYPE_64=%u\n", PM_TYPE_64);
    printf("PM_TYPE_U64=%u\n", PM_TYPE_U64);
    printf("PM_TYPE_FLOAT=%u\n", PM_TYPE_FLOAT);
    printf("PM_TYPE_DOUBLE=%u\n", PM_TYPE_DOUBLE);
    printf("PM_TYPE_STRING=%u\n", PM_TYPE_STRING);

    printf("PM_SEM_COUNTER=%u\n", PM_SEM_COUNTER);
    printf("PM_SEM_INSTANT=%u\n", PM_SEM_INSTANT);
    printf("PM_SEM_DISCRETE=%u\n", PM_SEM_DISCRETE);

    /*
     * for ease of maintenance make the order of the error codes
     * here the same as the output from pmerr -l
     */
    printf("PM_ERR_GENERIC=%d\n", PM_ERR_GENERIC);
    printf("PM_ERR_PMNS=%d\n", PM_ERR_PMNS);
    printf("PM_ERR_NOPMNS=%d\n", PM_ERR_NOPMNS);
    printf("PM_ERR_DUPPMNS=%d\n", PM_ERR_DUPPMNS);
    printf("PM_ERR_TEXT=%d\n", PM_ERR_TEXT);
    printf("PM_ERR_APPVERSION=%d\n", PM_ERR_APPVERSION);
    printf("PM_ERR_VALUE=%d\n", PM_ERR_VALUE);
    printf("PM_ERR_TIMEOUT=%d\n", PM_ERR_TIMEOUT);
    printf("PM_ERR_NODATA=%d\n", PM_ERR_NODATA);
    printf("PM_ERR_RESET=%d\n", PM_ERR_RESET);
    printf("PM_ERR_NAME=%d\n", PM_ERR_NAME);
    printf("PM_ERR_PMID=%d\n", PM_ERR_PMID);
    printf("PM_ERR_INDOM=%d\n", PM_ERR_INDOM);
    printf("PM_ERR_INST=%d\n", PM_ERR_INST);
    printf("PM_ERR_TYPE=%d\n", PM_ERR_TYPE);
    printf("PM_ERR_UNIT=%d\n", PM_ERR_UNIT);
    printf("PM_ERR_CONV=%d\n", PM_ERR_CONV);
    printf("PM_ERR_TRUNC=%d\n", PM_ERR_TRUNC);
    printf("PM_ERR_SIGN=%d\n", PM_ERR_SIGN);
    printf("PM_ERR_PROFILE=%d\n", PM_ERR_PROFILE);
    printf("PM_ERR_IPC=%d\n", PM_ERR_IPC);
    printf("PM_ERR_EOF=%d\n", PM_ERR_EOF);
    printf("PM_ERR_NOTHOST=%d\n", PM_ERR_NOTHOST);
    printf("PM_ERR_EOL=%d\n", PM_ERR_EOL);
    printf("PM_ERR_MODE=%d\n", PM_ERR_MODE);
    printf("PM_ERR_LABEL=%d\n", PM_ERR_LABEL);
    printf("PM_ERR_LOGREC=%d\n", PM_ERR_LOGREC);
    printf("PM_ERR_LOGFILE=%d\n", PM_ERR_LOGFILE);
    printf("PM_ERR_NOTARCHIVE=%d\n", PM_ERR_NOTARCHIVE);
    printf("PM_ERR_NOCONTEXT=%d\n", PM_ERR_NOCONTEXT);
    printf("PM_ERR_PROFILESPEC=%d\n", PM_ERR_PROFILESPEC);
    printf("PM_ERR_PMID_LOG=%d\n", PM_ERR_PMID_LOG);
    printf("PM_ERR_INDOM_LOG=%d\n", PM_ERR_INDOM_LOG);
    printf("PM_ERR_INST_LOG=%d\n", PM_ERR_INST_LOG);
    printf("PM_ERR_NOPROFILE=%d\n", PM_ERR_NOPROFILE);
    printf("PM_ERR_NOAGENT=%d\n", PM_ERR_NOAGENT);
    printf("PM_ERR_PERMISSION=%d\n", PM_ERR_PERMISSION);
    printf("PM_ERR_CONNLIMIT=%d\n", PM_ERR_CONNLIMIT);
    printf("PM_ERR_AGAIN=%d\n", PM_ERR_AGAIN);
    printf("PM_ERR_ISCONN=%d\n", PM_ERR_ISCONN);
    printf("PM_ERR_NOTCONN=%d\n", PM_ERR_NOTCONN);
    printf("PM_ERR_NEEDPORT=%d\n", PM_ERR_NEEDPORT);
    printf("PM_ERR_NONLEAF=%d\n", PM_ERR_NONLEAF);
    printf("PM_ERR_PMDAREADY=%d\n", PM_ERR_PMDAREADY);
    printf("PM_ERR_PMDANOTREADY=%d\n", PM_ERR_PMDANOTREADY);
    printf("PM_ERR_TOOSMALL=%d\n", PM_ERR_TOOSMALL);
    printf("PM_ERR_TOOBIG=%d\n", PM_ERR_TOOBIG);
    printf("PM_ERR_FAULT=%d\n", PM_ERR_FAULT);
    printf("PM_ERR_THREAD=%d\n", PM_ERR_THREAD);
    printf("PM_ERR_NOCONTAINER=%d\n", PM_ERR_NOCONTAINER);
    printf("PM_ERR_BADSTORE=%d\n", PM_ERR_BADSTORE);
    printf("PM_ERR_NYI=%d\n", PM_ERR_NYI);
}

int
main(int argc, char **argv)
{
    char *use = "Error: must provide one argument - either 'd', 'i' or 'u'\n";
    if (argc != 2) {
	fputs(use, stderr);
	return 1;
    }
    else if (argv[1][0] == 'd')
	defines();
    else if (argv[1][0] == 'i')
	ids();
    else if (argv[1][0] == 'u')
	units();
    else {
	fputs(use, stderr);
	fprintf(stderr, "(ouch!!!!  that really hurt!  who throws a '%s' anyway? --Austin)\n", argv[1]);
	return 1;
    }
    return 0;
}
