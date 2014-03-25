/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 1984 Ken McDonell.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/pmda.h>

int
main(int argc, char **argv)
{
    int			c;
    int			sts;
    int			errflag = 0;
    pmdaInterface	dispatch;

    /* trim cmd name of leading directory components */
    __pmSetProgname(argv[0]);
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    while ((c = getopt(argc, argv, "D:?")) != EOF) {
	switch (c) {

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

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag || optind < argc) {
	fprintf(stderr, "Usage: %s [-D debugflags]\n", pmProgname);
        exit(1);
    }

    printf("Valid transitions ...\n");
    printf("READY: ");
    if ((sts = pmdaControl(&dispatch, PMDA_CONTROL_READY)) == 0)
	printf("OK\n");
    else
	printf("%s\n", pmErrStr(sts));
    printf("READY: ");
    if ((sts = pmdaControl(&dispatch, PMDA_CONTROL_READY)) == 0)
	printf("OK\n");
    else
	printf("%s\n", pmErrStr(sts));

    printf("BUSY: ");
    if ((sts = pmdaControl(&dispatch, PMDA_CONTROL_BUSY)) == 0)
	printf("OK\n");
    else
	printf("%s\n", pmErrStr(sts));
    printf("BUSY: ");
    if ((sts = pmdaControl(&dispatch, PMDA_CONTROL_BUSY)) == 0)
	printf("OK\n");
    else
	printf("%s\n", pmErrStr(sts));
    printf("READY: ");
    if ((sts = pmdaControl(&dispatch, PMDA_CONTROL_READY)) == 0)
	printf("OK\n");
    else
	printf("%s\n", pmErrStr(sts));
    printf("BUSY: ");
    if ((sts = pmdaControl(&dispatch, PMDA_CONTROL_BUSY)) == 0)
	printf("OK\n");
    else
	printf("%s\n", pmErrStr(sts));
    printf("NOTREADY (should fail from BUSY): ");
    if ((sts = pmdaControl(&dispatch, PMDA_CONTROL_NOTREADY)) == 0)
	printf("OK\n");
    else
	printf("%s\n", pmErrStr(sts));

    return 0;
}
