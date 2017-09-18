/*
 * Exercise client-side libpcp timeout settings.
 *
 * Copyright (c) 2015 Red Hat.
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>

int
main(int argc, char *argv[])
{
    int		ctx;
    int		sts;
    int		c;
    int		errflag = 0;
    int		Pflag = 0, pflag = 0, Cflag = 0, sflag = 0, Sflag = 0;
    char	*end_ptr;
    double	req_timeout = 42.0;
    double	conn_timeout = 120.0;
    static char	*usage = "[-CpPsS] [-c sec] [-r sec] [-D debugopts]";

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "c:CpPsSr:D:")) != EOF) {
	switch (c) {

	case 'C':
	    Cflag = 1;
	    break;

	case 'p':
	    pflag = 1;
	    break;

	case 'P':
	    Pflag = 1;
	    break;

	case 's':
	    sflag = 1;
	    break;

	case 'S':
	    Sflag = 1;
	    break;

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmProgname, optarg);
		errflag++;
	    }
	    break;

	case 'c':
	    conn_timeout = strtod(optarg, &end_ptr);
	    if (*end_ptr != '\0') {
		fprintf(stderr, "%s: invalid timeout specification (%s)\n",
                              pmProgname, optarg);
		errflag++;
	    }
	    break;

	case 'r':
	    req_timeout = strtod(optarg, &end_ptr);
	    if (*end_ptr != '\0') {
		fprintf(stderr, "%s: invalid timeout specification (%s)\n",
                              pmProgname, optarg);
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
	printf("Usage: %s %s\n", pmProgname, usage);
	exit(1);
    }

    if (sflag) {
	if ((sts = __pmSetConnectTimeout(conn_timeout)) < 0) {
	    fprintf(stderr, "__pmSetConnectTimeout(%.2f): %s\n",
		    conn_timeout, pmErrStr(sts));
	}
	if ((sts = __pmSetRequestTimeout(req_timeout)) < 0) {
	    fprintf(stderr, "__pmSetRequestTimeout(%.2f): %s\n",
		    req_timeout, pmErrStr(sts));
	}
    }

    if (pflag) {
	printf("Connect timeout pre-ctx: %.3f\n", __pmConnectTimeout());
	printf("Request timeout pre-ctx: %.3f\n", __pmRequestTimeout());
    }

    if ((ctx = pmNewContext(PM_CONTEXT_HOST, "localhost")) < 0) {
	fprintf(stderr, "pmNewContext(..., \"localhost\"): %s\n",
		pmErrStr(ctx));
	exit(1);
    }

    if (Cflag) {
	printf("Connect timeout post-ctx: %.3f\n", __pmConnectTimeout());
	printf("Request timeout post-ctx: %.3f\n", __pmRequestTimeout());
    }

    if (Sflag) {
	if ((sts = __pmSetConnectTimeout(conn_timeout)) < 0) {
	    fprintf(stderr, "__pmSetConnectTimeout(%.2f): %s\n",
		    conn_timeout, pmErrStr(sts));
	}
	if ((sts = __pmSetRequestTimeout(req_timeout)) < 0) {
	    fprintf(stderr, "__pmSetRequestTimeout(%.2f): %s\n",
		    req_timeout, pmErrStr(sts));
	}
    }

    if (Pflag) {
	printf("Connect timeout post-set: %.3f\n", __pmConnectTimeout());
	printf("Request timeout post-set: %.3f\n", __pmRequestTimeout());
    }

    pmDestroyContext(ctx);

    exit(0);
}
