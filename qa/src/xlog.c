/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

/*
 * exercise meta-Data services for an archive
 */

#include <ctype.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>

static int	vflag;
static int	numpmid;
static pmID	pmidlist[20];
static char	*namelist[20];

static void
grind(void)
{
    int		sts;
    int		i;
    int		*instlist;
    char	**inamelist;
    pmDesc	desc;

    for (i = 0; i < numpmid; i++) {
	if (pmidlist[i] != PM_ID_NULL) {
	    printf("\npmid: 0x%x name: %s", pmidlist[i], namelist[i]);
	    if ((sts = pmLookupDesc(pmidlist[i], &desc)) < 0) {
		printf("\npmLookupDesc: %s\n", pmErrStr(sts));
	    }
	    else {
		printf(" indom: 0x%x", desc.indom);
		if (vflag) {
		    const char	*u = pmUnitsStr(&desc.units);
		    printf("\ndesc: type=%d indom=0x%x sem=%d units=%s",
			desc.type, desc.indom, desc.sem,
			*u == '\0' ? "none" : u);
		}
		if (desc.indom == PM_INDOM_NULL) {
		    printf("\n");
		    continue;
		}
		if (vflag)
		    putchar('\n');
		if ((sts = pmGetInDomArchive(desc.indom, &instlist, &inamelist)) < 0) {
		    printf("pmGetInDomArchive: %s\n", pmErrStr(sts));
		}
		else {
		    int		j;
		    int		numinst = sts;
		    char	*name;
		    printf(" numinst: %d\n", numinst);
		    for (j = 0; j < numinst; j++) {
			if (vflag)
			    printf("  instance id: 0x%x ", instlist[j]);
			if ((sts = pmNameInDomArchive(desc.indom, instlist[j], &name)) < 0) {
			    printf("pmNameInDomArchive: %s\n", pmErrStr(sts));
			}
			else {
			    if (vflag)
				printf("%s (== %s?)", name, inamelist[j]);
			    if ((sts = pmLookupInDomArchive(desc.indom, name)) < 0) {
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
}

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		ctx[2];
    int		errflag = 0;
    char	*archive = "foo";
    char	*namespace = PM_NS_DEFAULT;
#ifdef PCP_DEBUG
    static char	*debug = "[-D N]";
#else
    static char	*debug = "";
#endif
    static char	*usage = "[-D N] [-a archive] [-n namespace] [-v]";
    int		i;
    pmLogLabel	loglabel;
    pmLogLabel	duplabel;

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "a:D:n:v")) != EOF) {
	switch (c) {

	case 'a':	/* archive */
	    archive = optarg;
	    break;

#ifdef PCP_DEBUG
	case 'D':	/* debug flag */
	    sts = __pmParseDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug flag specification (%s)\n",
		    pmProgname, optarg);
		errflag++;
	    }
	    else
		pmDebug |= sts;
	    break;
#endif

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
	printf("Usage: %s %s%s\n", pmProgname, debug, usage);
	exit(1);
    }

    if (namespace != PM_NS_DEFAULT && (sts = pmLoadASCIINameSpace(namespace, 1)) < 0) {
	printf("%s: Cannot load namespace from \"%s\": %s\n", pmProgname, namespace, pmErrStr(sts));
	exit(1);
    }

    if ((ctx[0] = pmNewContext(PM_CONTEXT_ARCHIVE, archive)) < 0) {
	printf("%s: Cannot connect to archive \"%s\": %s\n", pmProgname, archive, pmErrStr(ctx[0]));
	exit(1);
    }
    if ((sts = pmGetArchiveLabel(&loglabel)) < 0) {
	printf("%s: pmGetArchiveLabel(%d): %s\n", pmProgname, ctx[0], pmErrStr(sts));
	exit(1);
    }
    if ((sts = pmSetMode(PM_MODE_INTERP, &loglabel.ll_start, 5)) < 0) {
	printf("%s: pmSetMode: %s\n", pmProgname, pmErrStr(sts));
	exit(1);
    }
    if ((ctx[1] = pmDupContext()) < 0) {
	printf("%s: Cannot dup context: %s\n", pmProgname, pmErrStr(ctx[1]));
	exit(1);
    }
    if ((sts = pmGetArchiveLabel(&duplabel)) < 0) {
	printf("%s: pmGetArchiveLabel(%d): %s\n", pmProgname, ctx[1], pmErrStr(sts));
	exit(1);
    }
    if (loglabel.ll_magic != duplabel.ll_magic ||
	loglabel.ll_pid != duplabel.ll_pid ||
 	loglabel.ll_start.tv_sec != duplabel.ll_start.tv_sec ||
	loglabel.ll_start.tv_usec != duplabel.ll_start.tv_usec ||
	strcmp(loglabel.ll_hostname, duplabel.ll_hostname) != 0 ||
	strcmp(loglabel.ll_tz, duplabel.ll_tz) != 0) {
	printf("Error: pmLogLabel mismatch\n");
	printf("First context: magic=0x%x pid=%" FMT_PID " start=%ld.%06ld\n",
		loglabel.ll_magic, loglabel.ll_pid,
		(long)loglabel.ll_start.tv_sec,
		(long)loglabel.ll_start.tv_usec);
	printf("host=%s TZ=%s\n", loglabel.ll_hostname, loglabel.ll_tz);
	printf("Error: pmLogLabel mismatch\n");
	printf("Dup context: magic=0x%x pid=%" FMT_PID " start=%ld.%06ld\n",
		duplabel.ll_magic, duplabel.ll_pid,
		(long)duplabel.ll_start.tv_sec,
		(long)duplabel.ll_start.tv_usec);
	printf("host=%s TZ=%s\n", duplabel.ll_hostname, duplabel.ll_tz);
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

	grind();
    }
    exit(0);
}
