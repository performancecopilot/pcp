#include <pcp/pmapi.h>
#include "libpcp.h"

static struct timeval	start;

#ifdef IS_MINGW
void pause(void) { SleepEx(INFINITE, TRUE); }
#endif

static int	reg[4];

static void
printstamp(struct timeval *tp)
{
    static struct tm    *tmp;
    time_t		tt =  (time_t)tp->tv_sec;

    tmp = localtime(&tt);
    fprintf(stderr, "%02d:%02d:%02d.%06d", tmp->tm_hour, tmp->tm_min, tmp->tm_sec, (int)tp->tv_usec);
}

void
onevent(int afid, void *data)
{
    struct timeval	now;
    static int		delay = -3;
    int			evnum;
    double		elapsed;
    struct timeval	sec = { 0, 0 };

    gettimeofday(&now, NULL);

    if (pmDebugOptions.af) {
	fprintf(stderr, "onevent(%d, " PRINTF_P_PFX "%p) called: ", afid, data);
	printstamp(&now);
	fputc('\n', stderr);
    }

    elapsed = pmtimevalSub(&now, &start);

    if (afid == reg[2])
	printf("event %d callback\n", afid);
    else {
	if (afid == reg[0])
	    evnum = (int)(elapsed / 2.5);
	else
	    evnum = (int)(elapsed / 1.5);
	/* evnum not reliable for small elapsed intervals */
	if (evnum >= 3)
	    printf("event %d callback #%d\n", afid, evnum);
	else
	    printf("event %d callback #?\n", afid);
    }

    if (delay > 6) {
	/* only report the unexpected */
	if (__pmAFunregister(reg[0]) < 0)
	    printf("unregister %d failed\n", reg[0]);
	if (__pmAFunregister(reg[1]) == 0)
	    printf("unregister %d success\n", reg[1]);
	if (__pmAFunregister(reg[2]) == 0)
	    printf("unregister %d success\n", reg[2]);
	if (__pmAFunregister(reg[3]) < 0)
	    printf("unregister %d failed\n", reg[0]);
	exit(0);
    }
    if (delay > 0) {
	/*
	 * was sginap(delay * CLK_TCK) ... usleep() for
	 * delay*CLK_TCK*10^6/CLK_TCK microseconds so "delay" sec
	 */
	sec.tv_sec = delay;
	__pmtimevalSleep(sec);
    }
    delay++;

    if (pmDebugOptions.af) {
	gettimeofday(&now, NULL);
	fprintf(stderr, "onevent done: ");
	printstamp(&now);
	fputc('\n', stderr);
    }

    fflush(stderr);
    fflush(stdout);
}

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		errflag = 0;
    struct timeval	delta = { 2, 500000 };
    struct timeval	now;

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:?")) != EOF) {
	switch (c) {

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmGetProgname(), optarg);
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
"Usage: %s options ...\n\
\n\
Options\n\
  -D debug	standard PCP debug options\n",
		pmGetProgname());
	exit(1);
    }

    __pmAFblock();
    gettimeofday(&start, NULL);
    reg[0] = __pmAFregister(&delta, NULL, onevent);
    delta.tv_sec = 1;
    reg[1] = __pmAFregister(&delta, NULL, onevent);
    delta.tv_sec = 0;
    delta.tv_usec = 0;
    reg[2] = __pmAFregister(&delta, NULL, onevent);
    delta.tv_sec = 60;	/* will never fire */
    reg[3] = __pmAFregister(&delta, NULL, onevent);
    __pmAFunblock();

    for ( ; ; ) {
	fflush(stderr);
	pause();
	if (pmDebugOptions.af) {
	    gettimeofday(&now, NULL);
	    fprintf(stderr, "returned from pause(): ");
	    printstamp(&now);
	    fputc('\n', stderr);
	}
    }
}
