/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2014 Ken McDonell.  All Rights Reserved.
 *
 * exercise pmGetContextHostName
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		ctx1;
    int		ctx2;
    int		errflag = 0;
    int		type = 0;
    char	*host = NULL;			/* pander to gcc */
    char	local[MAXHOSTNAMELEN];
    char	buf[MAXHOSTNAMELEN];

    /* trim cmd name of leading directory components */
    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "a:D:h:L?")) != EOF) {
	switch (c) {

	case 'a':	/* archive name */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a, -h and -L allowed\n", pmProgname);
		errflag++;
	    }
	    type = PM_CONTEXT_ARCHIVE;
	    host = optarg;
	    break;

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

	case 'h':	/* contact PMCD on this hostname */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a, -h and -L allowed\n", pmProgname);
		errflag++;
	    }
	    host = optarg;
	    type = PM_CONTEXT_HOST;
	    break;

	case 'L':	/* LOCAL, no PMCD */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a, -h, -L and -U allowed\n", pmProgname);
		errflag++;
	    }
	    host = NULL;
	    type = PM_CONTEXT_LOCAL;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag || optind < argc) {
	fprintf(stderr,
"Usage: %s [options]\n\
\n\
Options:\n\
  -a archive     metrics source is a PCP log archive\n\
  -h host        metrics source is PMCD on host\n\
  -L             use local context instead of PMCD\n",
                pmProgname);
        exit(1);
    }


    if (type == 0) {
	type = PM_CONTEXT_HOST;
	(void)gethostname(local, MAXHOSTNAMELEN);
	local[MAXHOSTNAMELEN-1] = '\0';
	host = local;
    }
    if ((ctx1 = pmNewContext(type, host)) < 0) {
	if (type == PM_CONTEXT_HOST)
	    fprintf(stderr, "%s: Cannot connect to PMCD on host \"%s\": %s\n",
		pmProgname, host, pmErrStr(ctx1));
	else
	    fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		pmProgname, host, pmErrStr(ctx1));
	/* continue on in this case to test a bad context */
    }

    printf("pmGetContextHostName(%d)=\"%s\"\n", ctx1, pmGetContextHostName(ctx1));
    printf("pmGetContextHostName_r(%d, ...) => \"%s\"\n", ctx1, pmGetContextHostName_r(ctx1, buf, sizeof(buf)));

    ctx2 = pmDupContext();
    if (ctx2 >= 0)
	printf("pmDupConext()=%d\n", ctx2);
    else
	printf("pmDupContext()=%d - %s\n", ctx2, pmErrStr(ctx2));

    printf("dup ctx pmGetContextHostName(%d)=\"%s\"\n", ctx2, pmGetContextHostName(ctx2));
    printf("dup ctx pmGetContextHostName_r(%d, ...) => \"%s\"\n", ctx2, pmGetContextHostName_r(ctx2, buf, sizeof(buf)));

    printf("original ctx pmGetContextHostName(%d)=\"%s\"\n", ctx1, pmGetContextHostName(ctx1));
    printf("original ctx pmGetContextHostName_r(%d, ...) => \"%s\"\n", ctx1, pmGetContextHostName_r(ctx1, buf, sizeof(buf)));


    return 0;
}
