/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

/*
 * exercise meta-data services for an archive
 */

#include <ctype.h>
#include <pcp/pmapi.h>
#include "libpcp.h"

static int	vflag;
static int	numpmid;
static pmID	pmidlist[20];
static pmDesc	desclist[20];
static const char *namelist[20];

static void
grind(void)
{
    int		sts;
    int		i;
    int		*instlist;
    char	**inamelist;

    for (i = 0; i < numpmid; i++) {
	if (pmidlist[i] != PM_ID_NULL) {
	    printf("\npmid: 0x%x name: %s", pmidlist[i], namelist[i]);
	    printf(" indom: 0x%x", desclist[i].indom);
	    if (vflag) {
		const char	*u = pmUnitsStr(&desclist[i].units);
		printf("\ndesc: type=%d indom=0x%x sem=%d units=%s",
			desclist[i].type, desclist[i].indom, desclist[i].sem,
			*u == '\0' ? "none" : u);
	    }
	    if (desclist[i].indom == PM_INDOM_NULL) {
		printf("\n");
		continue;
	    }
	    if (vflag)
		putchar('\n');
	    if ((sts = pmGetInDomArchive(desclist[i].indom, &instlist, &inamelist)) < 0) {
		printf("pmGetInDomArchive: %s\n", pmErrStr(sts));
	    }
	    else {
		int	j;
		int	numinst = sts;
		char	*name;

		printf(" numinst: %d\n", numinst);
		for (j = 0; j < numinst; j++) {
		    if (vflag)
			printf("  instance id: 0x%x ", instlist[j]);
		    if ((sts = pmNameInDomArchive(desclist[i].indom, instlist[j], &name)) < 0) {
			printf("pmNameInDomArchive: %s\n", pmErrStr(sts));
		    }
		    else {
			if (vflag)
			    printf("%s (== %s?)", name, inamelist[j]);
			if ((sts = pmLookupInDomArchive(desclist[i].indom, name)) < 0) {
			    printf(" pmLookupInDomArchive: %s\n", pmErrStr(sts));
			}
			else {
			    if (sts != instlist[j]) {
				printf(" botch: pmLookupInDom returns 0x%x, expected 0x%x\n",
					sts, instlist[j]);
			    }
			    else if (vflag)
				putchar('\n');
			}
			free(name);
		    }
		}
		free(instlist);
		free(inamelist);
	    }
	}
    }
}

int
main(int argc, char **argv)
{
    int		c;
    int		i;
    int		sts;
    int		ctx[2];
    int		errflag = 0;
    char	*archive = "foo";
    char	*namespace = PM_NS_DEFAULT;
    static char	*usage = "[-D debugspec] [-a archive] [-n namespace] [-v]";
    struct timespec	delta = { 0, 5000000 };	/* 5 msec */
    pmLogLabel	loglabel;
    pmLogLabel	duplabel;

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "a:D:n:v")) != EOF) {
	switch (c) {

	case 'a':	/* archive */
	    archive = optarg;
	    break;

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmGetProgname(), optarg);
		errflag++;
	    }
	    break;

	case 'n':	/* alternative name space file */
	    namespace = optarg;
	    break;

	case 'v':	/* verbose output */
	    vflag++;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag) {
	printf("Usage: %s %s\n", pmGetProgname(), usage);
	exit(1);
    }

    if (namespace != PM_NS_DEFAULT && (sts = pmLoadASCIINameSpace(namespace, 1)) < 0) {
	printf("%s: Cannot load namespace from \"%s\": %s\n", pmGetProgname(), namespace, pmErrStr(sts));
	exit(1);
    }

    if ((ctx[0] = pmNewContext(PM_CONTEXT_ARCHIVE, archive)) < 0) {
	printf("%s: Cannot connect to archive \"%s\": %s\n", pmGetProgname(), archive, pmErrStr(ctx[0]));
	exit(1);
    }
    if ((sts = pmGetArchiveLabel(&loglabel)) < 0) {
	printf("%s: pmGetHighResArchiveLabel(%d): %s\n", pmGetProgname(), ctx[0], pmErrStr(sts));
	exit(1);
    }
    if ((sts = pmSetModeHighRes(PM_MODE_INTERP, &loglabel.start, &delta)) < 0) {
	printf("%s: pmSetModeHighRes: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(1);
    }
    if ((ctx[1] = pmDupContext()) < 0) {
	printf("%s: Cannot dup context: %s\n", pmGetProgname(), pmErrStr(ctx[1]));
	exit(1);
    }
    if ((sts = pmGetArchiveLabel(&duplabel)) < 0) {
	printf("%s: pmGetHighResArchiveLabel(%d): %s\n", pmGetProgname(), ctx[1], pmErrStr(sts));
	exit(1);
    }
    if (loglabel.magic != duplabel.magic ||
	loglabel.pid != duplabel.pid ||
 	loglabel.start.tv_sec != duplabel.start.tv_sec ||
	loglabel.start.tv_nsec != duplabel.start.tv_nsec ||
	strcmp(loglabel.hostname, duplabel.hostname) != 0 ||
	strcmp(loglabel.timezone, duplabel.timezone) != 0 ||
	strcmp(loglabel.zoneinfo, duplabel.zoneinfo) != 0) {
	printf("Error: pmHighResLogLabel mismatch\n");
	printf("First context: magic=0x%x pid=%" FMT_PID " start=%lld.%09ld\n",
		loglabel.magic, loglabel.pid,
		(long long)loglabel.start.tv_sec,
		(long)loglabel.start.tv_nsec);
	printf("hostname=%s timezone=%s zoneinfo=%s\n",
		loglabel.hostname, loglabel.timezone, loglabel.zoneinfo);
	printf("Error: pmHighResLogLabel mismatch\n");
	printf("Dup context: magic=0x%x pid=%" FMT_PID " start=%lld.%09ld\n",
		duplabel.magic, duplabel.pid,
		(long long)duplabel.start.tv_sec,
		(long)duplabel.start.tv_nsec);
	printf("hostname=%s timezone=%s zoneinfo=%s\n",
		duplabel.hostname, duplabel.timezone, duplabel.zoneinfo);
	exit(1);
    }

    /*
     * metrics biased towards log "foo"
     */
    i = 0;
    namelist[i++] = "sample.seconds";
    namelist[i++] = "sample.bin";
    namelist[i++] = "sample.colour";
    namelist[i++] = "sample.drift";
    namelist[i++] = "sample.lights";
    namelist[i++] = "sampledso.milliseconds";
    namelist[i++] = "sampledso.bin";
    numpmid = i;


    for (c = 0; c < 2; c++) {
	printf("\n=== Context %d ===\n", ctx[c]);
	pmUseContext(ctx[c]);
	__pmDumpContext(stdout, ctx[c], PM_INDOM_NULL);

	pmTrimNameSpace();

	sts = pmLookupName(numpmid, namelist, pmidlist);
	if (sts != numpmid) {
	    if (sts < 0)
		printf("pmLookupName: %s\n", pmErrStr(sts));
	    else
		printf("pmLookupName: Warning: some metrics unknown\n");

	    for (i = 0; i < numpmid; i++) {
		if (pmidlist[i] == PM_ID_NULL)
		    printf("	%s - not known\n", namelist[i]);
	    }
	}

	if ((sts = pmLookupDescs(numpmid, pmidlist, desclist)) < 0) {
	    printf("\npmLookupDesc: %s\n", pmErrStr(sts));
	} else {
	    grind();
	}
    }
    exit(0);
}
