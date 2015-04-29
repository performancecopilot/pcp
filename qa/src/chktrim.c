/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

/*
 * Check new functionality of pmTrimNameSpace
 */

#include <ctype.h>
#include <string.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>

extern int	errno;

static char	tag;

static void
dometric(const char *name)
{
    printf("%c %s\n", tag, name);
}

int
main(argc, argv)
int argc;
char *argv[];
{
    int		c;
    int		sts;
    int		errflag = 0;
    char	*namespace = PM_NS_DEFAULT;
    int		ctx;		/* context for localhost */
    int		arch;		/* context for archive */
#ifdef PCP_DEBUG
    static char	*debug = "[-D N]";
#else
    static char	*debug = "";
#endif
    static char	*usage = "[-n namespace] archive";

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:n:")) != EOF) {
	switch (c) {

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

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag || optind != argc-1) {
	fprintf(stderr, "Usage: %s %s%s\n", pmProgname, debug, usage);
	exit(1);
    }

    if (namespace != PM_NS_DEFAULT && (sts = pmLoadASCIINameSpace(namespace, 1)) < 0) {
	printf("%s: Cannot load namespace from \"%s\": %s\n", pmProgname, namespace, pmErrStr(sts));
	exit(1);
    }

    if ((ctx = pmNewContext(PM_CONTEXT_HOST, "localhost")) < 0) {
	printf("%s: pmNewContext(localhost): %s\n", pmProgname, pmErrStr(ctx));
	exit(1);
    }

    printf("0 NameSpace for host context ...\n");
    tag = '0';
    pmTraversePMNS("sample", dometric);

    if ((arch = pmNewContext(PM_CONTEXT_ARCHIVE, argv[optind])) < 0) {
	printf("%s: pmNewContext(%s): %s\n", pmProgname, argv[optind], pmErrStr(arch));
	exit(1);
    }

    printf("1\n1 Trimmed NameSpace for archive context ...\n");
    pmTrimNameSpace();
    tag = '1';
    pmTraversePMNS("", dometric);

    printf("2\n2 Trimmed NameSpace for host context ...\n");
    pmUseContext(ctx);
    pmTrimNameSpace();
    tag = '2';
    pmTraversePMNS("sample", dometric);

    exit(0);
}
