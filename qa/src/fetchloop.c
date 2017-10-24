/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2016 Ken McDonell.  All Rights Reserved.
 *
 * Hard pmFetch loop, sort of like the core of pmlogger.
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <sys/time.h>
#include <sys/times.h>
#include <unistd.h>

#define BUILD_STANDALONE

char	**namelist = NULL;
pmID	*pmidlist = NULL;
int	numpmid = 0;

static void
dometric(const char *name)
{
    numpmid++;

    namelist = (char **)realloc(namelist, numpmid*sizeof(char *));
    if (namelist == NULL) {
	__pmNoMem("dometric: namelist", numpmid*sizeof(char *), PM_FATAL_ERR);
	/* NOTREACHED */
    }
    pmidlist = (pmID *)realloc(pmidlist, numpmid*sizeof(pmID));
    if (pmidlist == NULL) {
	__pmNoMem("dometric: pmidlist", numpmid*sizeof(pmID), PM_FATAL_ERR);
	/* NOTREACHED */
    }
    namelist[numpmid-1] = strdup(name);
    if (namelist[numpmid-1] == NULL) {
	__pmNoMem("dometric: namelist[]", strlen(name)+1, PM_FATAL_ERR);
	/* NOTREACHED */
    }
}

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		errflag = 0;
    int		type = 0;
    char	*host = NULL;			/* pander to gcc */
    char 	*configfile = NULL;
    char	*pmnsfile = PM_NS_DEFAULT;
    int		samples = 1000;
    int		save_samples;
    char	*endnum;
    pmResult	*rp;
    struct tms	now, then;
    clock_t	ticks;
    long	hz = sysconf(_SC_CLK_TCK);

    /* trim cmd name of leading directory components */
    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "c:D:h:Ln:s:?")) != EOF) {
	switch (c) {

	case 'c':	/* configfile */
	    if (configfile != NULL) {
		fprintf(stderr, "%s: at most one -c option allowed\n", pmProgname);
		errflag++;
	    }
	    configfile = optarg;
	    break;	

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmProgname, optarg);
		errflag++;
	    }
	    break;

	case 'h':	/* contact PMCD on this hostname */
#ifdef BUILD_STANDALONE
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -h and -L allowed\n", pmProgname);
		errflag++;
	    }
#endif
	    host = optarg;
	    type = PM_CONTEXT_HOST;
	    break;

#ifdef BUILD_STANDALONE
	case 'L':	/* LOCAL, no PMCD */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -h and -L allowed\n", pmProgname);
		errflag++;
	    }
	    host = NULL;
	    type = PM_CONTEXT_LOCAL;
	    putenv("PMDA_LOCAL_PROC=");		/* if proc PMDA needed */
	    putenv("PMDA_LOCAL_SAMPLE=");	/* if sampledso PMDA needed */
	    break;
#endif

	case 'n':	/* alternative name space file */
	    pmnsfile = optarg;
	    break;

	case 's':	/* sample count */
	    samples = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || samples < 0) {
		fprintf(stderr, "%s: -s requires numeric argument\n", pmProgname);
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
"Usage: %s [options] [metrics ...]\n\
\n\
Options:\n\
  -c configfile  file to load configuration from\n\
  -h host        metrics source is PMCD on host\n"
#ifdef BUILD_STANDALONE
"  -L             use local context instead of PMCD\n"
#endif
"  -n pmnsfile    use an alternative PMNS\n\
  -s samples     terminate after this many samples [default 1000]\n",
                pmProgname);
        exit(1);
    }

    if (pmnsfile != PM_NS_DEFAULT && (sts = pmLoadASCIINameSpace(pmnsfile, 1)) < 0) {
	fprintf(stderr, "%s: Cannot load namespace from \"%s\": %s\n", pmProgname, 
	       pmnsfile, pmErrStr(sts));
	exit(1);
    }

    if (type == 0) {
	type = PM_CONTEXT_HOST;
	host = "unix:";
    }

    if ((sts = pmNewContext(type, host)) < 0) {
	if (type == PM_CONTEXT_HOST)
	    fprintf(stderr, "%s: Cannot connect to PMCD on host \"%s\": %s\n",
		pmProgname, host, pmErrStr(sts));
	else
	    fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		pmProgname, host, pmErrStr(sts));
	exit(1);
    }

    /* command line metrics are argv[optind] ... argv[argc-1] */
    while (optind < argc) {
	pmTraversePMNS(argv[optind], dometric);
	optind++;
    }

    if (configfile != NULL) {
	FILE	*f;
	char	buf[1024];
	char	*cp;
	/*
	 * metric names (terminal or non-terminal) are one per line in
	 * configfile
	 */

	if ((f = fopen(configfile, "r")) == NULL) {
	    fprintf(stderr, "fopen: %s: failed: %s\n", configfile, pmErrStr(-oserror()));
	    exit(1);
	}
	while (fgets(buf, sizeof(buf), f) != NULL) {
	    /* strip \n */
	    for (cp = buf; *cp && *cp != '\n'; cp++)
		;
	    *cp = '\0';
	    pmTraversePMNS(buf, dometric);
	}
	fclose(f);
    }

    sts = pmLookupName(numpmid, namelist, pmidlist);
    if (sts != numpmid) {
	int		i;
	if (sts < 0)
	    fprintf(stderr, "%s: pmLookupName: %s\n", pmProgname, pmErrStr(sts));
	else
	    fprintf(stderr, "%s: pmLookupName: returned %d, expected %d\n", pmProgname, sts, numpmid);
	for (i = 0; i < numpmid; i++) {
	    if (pmidlist[i] == PM_ID_NULL)
		fprintf(stderr, "   %s is bad\n", namelist[i]);
	}
	exit(1);
    }

    save_samples = samples;
    times(&then);
    while (samples == -1 || samples-- > 0) {
	sts = pmFetch(numpmid, pmidlist, &rp);
	if (sts < 0) {
	    fprintf(stderr, "%s: pmFetch: %s\n", pmProgname, pmErrStr(sts));
	    exit(1);
	}
	pmFreeResult(rp);
    }
    times(&now);
    ticks = now.tms_utime - then.tms_utime + now.tms_stime - then.tms_stime;
    printf("pmFetch time: %d iterations, CPU time %.3f sec (%.1f usec / fetch)\n", save_samples, ((double)(ticks))/hz, 1000000*((double)(ticks))/(hz*save_samples));

    return 0;
}
