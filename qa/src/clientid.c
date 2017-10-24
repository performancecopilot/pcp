/*
 * Exercise __pmSetClientId()
 *
 * Copyright (c) 2009 Ken McDonell.  All Rights Reserved.
 */

#include <ctype.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>

#define TAG "QA-clientid "

int
main(int argc, char *argv[])
{
    int		ctx;
    int		sts;
    int		c;
    int		a;
    int		lflag = 0;
    int		errflag = 0;
    static char	*usage = "[-l] [-D debugopts]";

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:l")) != EOF) {
	switch (c) {

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmProgname, optarg);
		errflag++;
	    }
	    break;

	case 'l':	/* linger when done */
	    lflag = 1;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag) {
	printf("Usage: %s %s\n", pmProgname, usage);
	exit(1);
    }

    fprintf(stderr, "Error expected ...\n");
    if ((sts = __pmSetClientId("no context yet, bozo")) < 0) {
	fprintf(stderr, "__pmSetClientId(...): %s\n",
		pmErrStr(sts));
    }

    if ((ctx = pmNewContext(PM_CONTEXT_HOST, "localhost")) < 0) {
	fprintf(stderr, "pmNewContext(..., \"localhost\"): %s\n",
		pmErrStr(ctx));
	exit(1);
    }

    for (a = optind; a < argc; a++) {
	char	*cp;
	cp = (char *)malloc(strlen(argv[a])+strlen(TAG)+1);
	strcpy(cp, TAG);
	strcat(cp, argv[a]);
	if ((sts = __pmSetClientId(cp)) < 0) {
	    fprintf(stderr, "__pmSetClientId(%s): %s\n",
		    cp, pmErrStr(sts));
	}
	else {
	    sts = system("pminfo -f pmcd.client.whoami");
	    if (sts != 0)
		fprintf(stderr, "Warning: pminfo command: exit status %d\n", sts);
	}
	free(cp);
    }

    if (lflag)
	pause();

    exit(0);
}
