#include "pmlogreduce.h"

/*
 *  Usage
 */
void
usage(void)
{
    fprintf(stderr,
"Usage: %s [options] input-archive output-archive\n\
\n\
Options:\n\
  -A align       align sample times on natural boundaries\n\
  -S starttime   start of the reduction time window\n\
  -s samples     terminate after this many log records have been written\n\
  -T endtime     end of the reduction time window\n\
  -t interval    sample output interval [default 10min]\n\
  -v volsamples  switch log volumes after this many samples\n\
  -Z timezone    set reporting timezone\n\
  -z             set reporting timezone to local time of input-archive\n",
	pmProgname);
}

/*
 *  convert timeval to double
 */
double
tv2double(struct timeval *tv)
{
    return tv->tv_sec + (double)tv->tv_usec / 1000000.0;
}

/*
 * parse command line arguments
 */
int
parseargs(int argc, char *argv[])
{
    extern int		pmDebug;	/* used in parseargs() */
    extern char		*optarg;	/* used in parseargs() */

    int			c;
    int			sts;
    int			errflag = 0;
    char		*endnum;
    char		*msg;
    struct timeval	interval;

    while ((c = getopt(argc, argv, "A:D:S:s:T:t:v:Z:z?")) != EOF) {
	switch (c) {

	case 'A':	/* output time alignment */
	    Aarg = optarg;
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

	case 's':	/* number of samples to write out */
	    sarg = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || sarg < 0) {
		fprintf(stderr, "%s: -s requires numeric argument\n",
			pmProgname);
		errflag++;
	    }
	    break;

	case 'S':	/* start time for reduction */
	    Sarg = optarg;
	    break;

	case 'T':	/* end time for reduction */
	    Targ = optarg;
	    break;

	case 't':	/* output sample interval */
	    if (pmParseInterval(optarg, &interval, &msg) < 0) {
		fputs(msg, stderr);
		free(msg);
		errflag++;
	    }
	    targ = tv2double(&interval);
	    break;

	case 'v':	/* number of samples per volume */
	    varg = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || varg < 0) {
		fprintf(stderr, "%s: -v requires numeric argument\n",
			pmProgname);
		errflag++;
	    }
	    break;

	case 'Z':	/* use timezone from command line */
	    if (zarg) {
		fprintf(stderr, "%s: at most one of -Z and/or -z allowed\n",
			pmProgname);
		errflag++;

	    }
	    tz = optarg;
	    break;

	case 'z':	/* use timezone from archive */
	    if (tz != NULL) {
		fprintf(stderr, "%s: at most one of -Z and/or -z allowed\n",
			pmProgname);
		errflag++;
	    }
	    zarg++;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag == 0 && optind > argc-2) {
	fprintf(stderr, "%s: Error: insufficient arguments\n", pmProgname);
	errflag++;
    }

    return(-errflag);
}

