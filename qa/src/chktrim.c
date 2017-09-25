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
    int		ctx = -1;	/* context for host */
    static char	*usage = "[-D debugspec] [-n namespace] [-a archive] [-h host] [-L]";

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "a:D:h:Ln:")) != EOF) {
	switch (c) {

	case 'a':
	    if ((sts = pmNewContext(PM_CONTEXT_ARCHIVE, optarg)) < 0) {
		printf("%s: pmNewContext(archive %s): %s\n", pmProgname, optarg, pmErrStr(sts));
		exit(1);
	    }
	    break;

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmProgname, optarg);
		errflag++;
	    }
	    break;

	case 'h':
	    if ((ctx = pmNewContext(PM_CONTEXT_HOST, optarg)) < 0) {
		printf("%s: pmNewContext(host %s): %s\n", pmProgname, optarg, pmErrStr(ctx));
		exit(1);
	    }
	    break;

	case 'L':
	    putenv("PMDA_LOCAL_SAMPLE=");	/* sampledso needed */
	    if ((sts = pmNewContext(PM_CONTEXT_LOCAL, NULL)) < 0) {
		printf("%s: pmNewContext(host %s): %s\n", pmProgname, optarg, pmErrStr(sts));
		exit(1);
	    }
	    break;

	case 'n':	/* alternative name space file */
	    namespace = optarg;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag || optind != argc) {
	fprintf(stderr, "Usage: %s %s\n", pmProgname, usage);
	exit(1);
    }

    if (namespace != PM_NS_DEFAULT && (sts = pmLoadASCIINameSpace(namespace, 1)) < 0) {
	printf("%s: Cannot load namespace from \"%s\": %s\n", pmProgname, namespace, pmErrStr(sts));
	exit(1);
    }

    printf("0 Initial NameSpace below sampledso ...\n");
    tag = '0';
    sts = pmTraversePMNS("sampledso", dometric);
    if (sts < 0) {
	printf("Error: pmTraversePMNS: %s\n", pmErrStr(sts));
    }

    printf("1\n1 Trimmed NameSpace below sampledso for current context ...\n");
    sts = pmTrimNameSpace();
    if (sts == 0) {
	tag = '1';
	sts = pmTraversePMNS("sampledso", dometric);
	if (sts < 0) {
	    printf("Error: pmTraversePMNS: %s\n", pmErrStr(sts));
	}
    }
    else {
	printf("Error: pmTrimeNameSpace: %s\n", pmErrStr(sts));
    }

    if (ctx == -1) {
	if ((ctx = pmNewContext(PM_CONTEXT_HOST, "local:")) < 0) {
	    printf("%s: pmNewContext(local:): %s\n", pmProgname, pmErrStr(ctx));
	    exit(1);
	}
    }

    printf("2\n2 Trimmed NameSpace below sampledso for host context ...\n");
    pmUseContext(ctx);
    sts = pmTrimNameSpace();
    if (sts == 0) {
	tag = '2';
	sts = pmTraversePMNS("sampledso", dometric);
	if (sts < 0) {
	    printf("Error: pmTraversePMNS: %s\n", pmErrStr(sts));
	}
    }
    else {
	printf("Error: pmTrimeNameSpace: %s\n", pmErrStr(sts));
    }

    exit(0);
}
