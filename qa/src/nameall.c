/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

/*
 * nameall - exercise pmNameAll
 */

#include <ctype.h>
#include <pcp/pmapi.h>
#include "libpcp.h"

static int 	pmns_style = 1;
static int	vflag;
static char	*host = "localhost";
static char	*namespace = PM_NS_DEFAULT;
static int	dupok = 1;

static void
dometric(const char *name)
{
    pmID	pmid;
    int		i;
    int		n;
    char	**nameset;

    /* cast const away as pmLookUpName will not modify this string */
    n = pmLookupName(1, (char **)&name, &pmid);
    if (n < 0) {
	printf("pmLookupName(%s): %s\n", name, pmErrStr(n));
	return;
    }
    n = pmNameAll(pmid, &nameset);
    if (n < 0) {
	printf("pmNameAll(%s): %s\n", name, pmErrStr(n));
	return;
    }
    for (i = 0; i < n; i++) {
	if (strcmp(name, nameset[i]) != 0)
	    printf("%s alias %s and %s\n", pmIDStr(pmid), name, nameset[i]);
    }
    free(nameset);
}

void
parse_args(int argc, char **argv)
{
    int		errflag = 0;
    int		c;
    int		sts;
    static char	*usage = "[-D debugspec] [-h hostname] [-[N|n] namespace] [-v]";
    static char *style_str = "[-s 1|2]";
    char	*endnum;

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:h:N:n:s:v")) != EOF) {
	switch (c) {

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmGetProgname(), optarg);
		errflag++;
	    }
	    break;

	case 'h':	/* hostname for PMCD to contact */
	    host = optarg;
	    break;

	case 'N':
	    dupok=0;
	    /*FALLTHROUGH*/
	case 'n':	/* alternative name space file */
	    namespace = optarg;
	    break;

	case 'v':	/* verbose */
	    vflag++;
	    break;

	case 's':	/* pmns style */
	    pmns_style = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0') {
		printf("%s: -s requires numeric argument\n", pmGetProgname());
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
	printf("Usage: %s %s%s\n", pmGetProgname(), style_str, usage);
	exit(1);
    }
}

void
load_namespace(char *namespace)
{
    struct timeval	now, then;
    int sts;

    gettimeofday(&then, (struct timezone *)0);
    sts = pmLoadASCIINameSpace(namespace, dupok);
    if (sts < 0) {
	printf("%s: Cannot load namespace from \"%s\" (dupok=%d): %s\n", pmGetProgname(), namespace, dupok, pmErrStr(sts));
	exit(1);
    }
    gettimeofday(&now, (struct timezone *)0);
    printf("Name space load: %.2f msec\n", pmtimevalSub(&now, &then)*1000);
}

void 
test_nameall(int argc, char *argv[])
{
    int sts;

    if ((sts = pmNewContext(PM_CONTEXT_HOST, host)) < 0) {
	printf("%s: Cannot connect to PMCD on host \"%s\": %s\n", pmGetProgname(), host, pmErrStr(sts));
	exit(1);
    }


    if (vflag > 1)
	__pmDumpNameSpace(stdout, 1);

    for ( ; optind < argc; optind++)
	pmTraversePMNS(argv[optind], dometric);
}


int
main(int argc, char **argv)
{
    parse_args(argc, argv);

    if (pmns_style == 2) {
	/* test it the new way with distributed namespace */
	/* i.e. no client loaded namespace */
	test_nameall(argc, argv);
    }
    else {
	/* test it the old way with namespace file */
	load_namespace(namespace);
	test_nameall(argc, argv);
    }

   exit(0);
}
