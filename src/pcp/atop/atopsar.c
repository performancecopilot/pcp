/*
** ATOP - System & Process Monitor
**
** The program 'atop'/'atopsar' offers the possibility to view the activity of 
** the system on system-level as well as process-level.
**
** This source-file contains the 'atopsar'-functionality, that makes use
** of the 'atop'-framework.
** 
** Copyright (C) 2007-2010 Gerlof Langeveld
** Copyright (C) 2015-2021 Red Hat.
**
** This program is free software; you can redistribute it and/or modify it
** under the terms of the GNU General Public License as published by the
** Free Software Foundation; either version 2, or (at your option) any
** later version.
**
** This program is distributed in the hope that it will be useful, but
** WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
** See the GNU General Public License for more details.
** --------------------------------------------------------------------------
*/

#include <pcp/pmapi.h>
#include <pcp/libpcp.h>
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#include "atop.h"
#include "ifprop.h"
#include "photosyst.h"
#include "photoproc.h"
#include "gpucom.h"

#define	MAXFL		64      /* maximum number of command-line flags  */
#define	sarflags	"b:e:SxCMh:Hr:R:aA"

/*
** color definitions
*/
#define	COLSETHEAD 	"\033[30;43m"	/* black on yellow	*/
#define	COLSETMED  	"\033[36m"	/* cyan			*/
#define	COLSETHIGH   	"\033[31m"	/* red			*/
#define	COLRESET    	"\033[00m"	/* reset any color	*/

/*
** miscellaneous values
*/
static char		stampalways;
static char		usemarkers;
static char		allresources;
static int		numreports;
static unsigned int	repeathead = 9999999;
static unsigned int	summarycnt = 1;
static char		*datemsg = "-------------------------- analysis "
			           "date: %s --------------------------\n";

/*
** structure definition for print-functions
*/
struct pridef { 
	char    wanted;         /* selected option (boolean)              */
	char    *cntcat;        /* used categories of counters            */
	char    flag;           /* flag on command line                   */
	void    (*prihead)(int, int, int);   /* print header of list      */
	int     (*priline)(struct sstat *, struct tstat *, struct tstat **,
		           int, time_t, time_t, time_t,
		           int, int, int, char *,
        	           int, int, int, int, int, int, int);
		                /* print counters per line (excl. time)   */
	char    *about;         /* statistics about what                  */
};

extern unsigned int 	nsamples;
extern struct pridef	pridef[];      /* table of print-functions        */
extern int		pricnt;	       /* total number of print-functions */

static time_t		daylim;        /* last second of day in epoch     */
static int		prinow;        /* current selection               */
static char 		coloron;       /* boolean: colors active now      */

/*
** local prototypes
*/
static void	pratopsaruse(char *, pmOptions *opts);
static char	reportlive(double, double,
		           struct devtstat *, struct sstat *,
		           int, unsigned int, int);
static char	reportraw (double, double,
		           struct devtstat *, struct sstat *,
		           int, unsigned int, int);
static void	prep(void);
static int	next_prinow(void);
static void	reportheader(struct sysname *, time_t);
static time_t	daylimit(time_t);

static char *
saroptions(void)
{
	static char	*allflags;	/* static for valgrind */
	int		i;

	/* 
	** gather all flags for the print-functions
	*/
	allflags = calloc(1, pricnt+32);

	ptrverify(allflags, "Malloc failed for %d flags\n", pricnt+32);

	for (i=0; i < pricnt; i++)
		allflags[i] = pridef[i].flag;

	/*
	** add generic flags
	*/
	return strncat(allflags, sarflags, pricnt+32 - 1);
}

int
atopsar(int argc, char *argv[])
{
	register int	i, c;
	pmOptions	opts;

	usecolors = 't';

	/*
	** setup for option processing by PMAPI getopts interface
	** (adds PCP long options and env processing transparently
	** alongside the atopsar short options).
	*/
	setup_options(&opts, argv, saroptions());

	/*
	** detect archives passed in via environment (PCP_ARCHIVE)
	*/
	if (opts.narchives > 0)
		rawreadflag++;

	/* 
	** interpret command-line arguments & flags 
	*/
	if (argc > 1)
	{
		while ((c = pmgetopt_r(argc, argv, &opts)) != EOF)
		{
			if (c == 0)
			{
				__pmGetLongOptions(&opts);
				continue;
			}
			switch (c)
			{
			   case '?':		/* usage wanted ?        */
				pratopsaruse(pmGetProgname(), &opts);
				break;

                           case 'b':		/* begin time ?          */
				opts.start_optarg = abstime(opts.optarg);
				opts.origin_optarg = opts.start_optarg;
				break;

                           case 'e':		/* end   time ?          */
				opts.finish_optarg = abstime(opts.optarg);
				break;

			   case 'r':		/* reading of file data ? */
				rawarchive(&opts, opts.optarg);
				rawreadflag++;
				break;

			   case 'R':		/* summarize samples */
				if (!numeric(opts.optarg))
					pratopsaruse(pmGetProgname(), &opts);

				summarycnt = atoi(opts.optarg);

				if (summarycnt < 1)
					pratopsaruse(pmGetProgname(), &opts);
				break;

			   case 'S':		/* timestamp on every line */
				stampalways = 1;
				break;

			   case 'x':		/* never use colors        */
				usecolors = 0;
				break;

			   case 'C':		/* always use colors       */
				usecolors = 'a';
				break;

			   case 'M':		/* markers for overload    */
				usemarkers = 1;
				break;

			   case 'H':		/* repeat headers          */
				repeathead = 23;	/* define default  */
#if defined(TIOCGWINSZ)
				if (isatty(fileno(stdout)))
				{
					struct winsize wsz;

					if ( ioctl(1, TIOCGWINSZ, &wsz) != -1)
						repeathead = wsz.ws_row - 1;
				}
#endif
				break;

			   case 'a':		/* every interval all units */
				allresources = 1;
				break;

			   case 'A':		/* all reports wanted ?  */
				for (i=0; i < pricnt; i++)
					pridef[i].wanted = 1;

				numreports = pricnt;
				break;

			   default:		/* gather report-flags    */
				for (i=0; i < pricnt; i++)
				{
					if (pridef[i].flag   == c && 
					    pridef[i].wanted == 0   )
					{
						pridef[i].wanted = 1;
						numreports++;
						break;
					}
				}

				if (i == pricnt)
					pratopsaruse(argv[0], &opts);
			}
		}

		/*
		** get optional interval-value and
		** optional number of samples	
		*/
		if (opts.optind < argc && opts.optind < MAXFL)
		{
			char	*endnum, *arg;

			arg = argv[opts.optind++];
			if (pmParseInterval(arg, &opts.interval, &endnum) < 0)
			{
				pmprintf(
			"%s: %s option not in pmParseInterval(3) format:\n%s\n",
					pmGetProgname(), arg, endnum);
				free(endnum);
				opts.errors++;
			}

			if (opts.optind < argc)
			{
				arg = argv[opts.optind];
				if (!numeric(arg))
					pratopsaruse(pmGetProgname(), &opts);
				if ((opts.samples = atoi(arg)) < 1)
					pratopsaruse(pmGetProgname(), &opts);
			}
		}
		/* if no interval specified, read from midnight in todays logfile */
		else if (!rawreadflag)
		{
			rawarchive_from_midnight(&opts);
			rawreadflag++;
		}
	}
	/* if no flags specified at all, read midnight in todays from logfile */
	else if (!rawreadflag)
	{
		rawarchive_from_midnight(&opts);
		rawreadflag++;
	}

	close_options(&opts);

	if (opts.errors)
		pratopsaruse(pmGetProgname(), &opts);

	if (opts.samples > 0)
		nsamples = opts.samples + 1;

	if (opts.interval.tv_sec || opts.interval.tv_usec)
		interval = opts.interval;

	/*
	** if no report-flags have been specified, take the first
	** option in the print-list as default
	*/
	if (numreports == 0)
	{
		pridef[0].wanted = 1;
		numreports       = 1;
	}

	/*
	** set stdout output on line-basis
	*/
	setvbuf(stdout, (char *)0, _IOLBF, BUFSIZ);

	/*
	** if only use colors when the output is directed to a tty,
	** be sure that output is directed to a tty
	*/
	if (usecolors == 't')
	{
		if (! isatty(fileno(stdout)) )
			usecolors = 0;
	}

	/*
	** check if raw data from a file must be viewed
	*/
	if (rawreadflag)
	{
		vis.show_samp = reportraw;
		vis.prep = prep;
		vis.next = next_prinow;
		prinow = 0;
	}
	else
		vis.show_samp = reportlive;

	/*
	** determine the name of this node (without domain-name)
	** and the kernel-version
	*/
	setup_globals(&opts);

	/*
	** determine properties (like speed) of all interfaces
	*/
	initifprop();

	/*
	** start the engine now .....
	*/
	engine();

	return 0;
}

/*
** report function to print a new sample in case of live measurements
*/
static char
reportlive(double timenow, double numsecs,
         	struct devtstat *devtstat, struct sstat *ss,
		int nexit, unsigned int noverflow, int flags)
{
	char			timebuf[16], datebuf[16];
	int			i, nr = numreports, rv;
	static unsigned int	curline, headline;
	static char		firstcall = 1;

	(void)devtstat; (void)nexit; (void)noverflow; (void)flags;

	/*
	** when this is first call to this function,
	** print overall header with system information
	*/
	if (firstcall)
	{
		reportheader(&sysname, time(0));
		firstcall = 0;
	}

	/*
	** printing more reports needs another way of handling compared
	** to printing one report
	*/
	if (numreports > 1)
	{
		/*
		** skip first sample
		*/
		if (sampcnt == 0)
			return '\0';

		printf(datemsg, convdate(timenow, datebuf, sizeof(datebuf)-1));

		for (i=0; i < pricnt && nr > 0; i++)
		{
			if ( !pridef[i].wanted )
				continue;

			nr--;

			/*
			** print header-line
			*/
			printf("\n");

			if (usecolors)
				printf(COLSETHEAD);

			printf("%s  ", convtime(timenow-numsecs, timebuf, sizeof(timebuf)-1));
	
			(pridef[i].prihead)(os_vers, os_rel, os_sub);
	
			if (usecolors)
				printf(COLRESET);

			printf("\n");

			/*
			** print line with statistical counters
			*/
			printf("%s  ", convtime(timenow, timebuf, sizeof(timebuf)-1));
	
			if ( !(pridef[i].priline)(ss, (struct tstat *)0, 0, 0,
				numsecs, numsecs*hertz, hertz,
				os_vers, os_rel, os_sub,
				stampalways ? timebuf : "        ",
	                        0, 0, 0, 0, 0, 0, 0) )
			{
				/*
				** print line has failed;
				** do not call function again
				*/
				pridef[i].wanted = 0;
	
				if (--numreports == 0)
					cleanstop(1);
			}
		}

		printf("\n");
	}
	else		/* just one report to be printed */
	{
		/*
		** search required report
		*/
		for (i=0; i < pricnt; i++)
			if ( pridef[i].wanted )
				break;

		/*
		** verify if we have passed midnight of some day
		*/
		if (timenow > daylim)
		{
			printf(datemsg, convdate(timenow, datebuf, sizeof(datebuf)-1));
			daylim = daylimit(timenow);
			curline++;
		}

		/*
		** print first header
		*/
		if (sampcnt == 0)
		{
			/*
			** print header-line
			*/
			printf("\n");

			if (usecolors)
				printf(COLSETHEAD);

			printf("%s  ", convtime(timenow, timebuf, sizeof(timebuf)-1));
	
			(pridef[i].prihead)(os_vers, os_rel, os_sub);

			if (usecolors)
				printf(COLRESET);

			printf("\n");

			curline+=2;

			headline = repeathead;
			return '\0';
		}

		/*
		** print line with statistical counters
		*/
		printf("%s  ", convtime(timenow, timebuf, sizeof(timebuf)-1));
	
		if ( !(rv = (pridef[i].priline)(ss, (struct tstat *)0, 0, 0,
					numsecs, numsecs*hertz, hertz,
					os_vers, os_rel, os_sub, 
		                        stampalways ? timebuf : "        ",
	                        	0, 0, 0, 0, 0, 0, 0) ) )
		{
			/*
			** print line has failed;
			** do not call function again
			*/
			cleanstop(1);
		}

		curline+=rv;

		if (curline >= headline)
		{
			headline = curline + repeathead;

			/*
			** print header-line
			*/
			printf("\n");

			if (usecolors)
				printf(COLSETHEAD);

			printf("%s  ", convtime(timenow, timebuf, sizeof(timebuf)-1));
	
			(pridef[i].prihead)(os_vers, os_rel, os_sub);

			if (usecolors)
				printf(COLRESET);

			printf("\n");

			curline+=2;
		}
	}

	return '\0';
}

static int
next_prinow()
{
    if (rawreadflag)
    {
	int	i;

        for (i=0; i < pricnt; i++)
        {
            if ( pridef[i].wanted ) {
                pridef[i].wanted = 0;
                prinow    = i+1;
                return 1;
            }
        }
    }
    return -1;
}

static void
prep()
{
    if (rawreadflag)
    {
	int	i;

        for (i=0; i < pricnt; i++)
        {
            if ( pridef[i].wanted ) {
                prinow    = i;
                break;
            }
        }
    }
}

/*
** report function to print a new sample in case of logged measurements
*/
static char
reportraw(double timenow, double numsecs,
         	struct devtstat *devtstat, struct sstat *sstat,
		int nexit, unsigned int noverflow, int flags)
{
	static char		firstcall = 1;
	char			timebuf[16], datebuf[16];
	double			timed;
	unsigned int		rv;
	static unsigned int	curline, headline, sampsum,
				totalexit, lastnpres,
				lastntrun, lastntslpi, lastntslpu,
				lastntidle, lastnzomb;
	static time_t		totalsec;
	static double		lasttime;
	static struct sstat	totsyst;

	/*
	** is this function still wanted?
	*/
	if ( ! pridef[prinow].wanted )
		return '\0';

	/*
	** when this is first call to this function,
	** print overall header with system information
	*/
	if (firstcall)
	{
		reportheader(&sysname, time(0));
		firstcall = 0;
	}

	/*
	** verify if we have passed midnight
	*/
	if (timenow > daylim)
	{
		printf(datemsg, convdate(timenow, datebuf, sizeof(datebuf)-1));
		daylim = daylimit(timenow);
		curline++;
	}

	/*
	** when this is the first record for a new report,
	** initialize various variables
	*/
	if (sampcnt == 1)
	{
		/*
		** initialize variables for new report
		*/
		pmtimevalFromReal(timenow, &pretime);

		curline   = 1;
		headline  = 0;

 		sampsum   = summarycnt + 1;
		totalsec  = 0;
		totalexit = 0;
		memset(&totsyst, 0, sizeof totsyst);

		return '\0';
	}

	/*
	** check if a (new) report header needs to be printed
	*/
	if (curline >= headline)
	{
		headline = curline + repeathead;

		/*
		** print header-line
		*/
		printf("\n");

		if (usecolors)
			printf(COLSETHEAD);

		timed = pmtimevalToReal(&pretime);
		printf("%s  ", convtime(timed, timebuf, sizeof(timebuf)-1));

		(pridef[prinow].prihead)(os_vers, os_rel, os_sub);

		if (usecolors)
			printf(COLRESET);

		printf("\n");

		curline+=2;
	}

	/*
	** when current record contains log-restart indicator,
	** print message and reinitialize variables
	*/
	if (flags & (RRBOOT | RRMARK))
	{
		/*
		** when accumulating counters, print results upto
		** the *previous* record
		*/
		if (summarycnt > 1 && sampcnt <= sampsum && totalsec)
		{
			printf("%s  ", convtime(lasttime, timebuf, sizeof(timebuf)-1));

			rv = (pridef[prinow].priline)(&totsyst,
				(struct tstat *)0, 0, 0,
				totalsec, totalsec*hertz, hertz,
			        os_vers, os_rel, os_sub,
		                stampalways ? timebuf : "        ",
				lastnpres, lastntrun, lastntslpi,
				lastntslpu, lastntidle,
	                        totalexit, lastnzomb);

			if (rv == 0)
			{
				curline++;
				pridef[prinow].wanted = 0; /* not call again */

				if (--numreports == 0)
					cleanstop(1);
			}
			else
			{
				curline += rv;
			}
		}

		/*
		** print restart-line in case of logging restarted
		*/
		if (flags & RRMARK)
		{
			printf("%s  ", convtime(timenow, timebuf, sizeof(timebuf)-1));

			printf("......................... logging restarted "
			       ".........................\n");
		}
		pmtimevalFromReal(timenow, &pretime);
		curline++;

		/*
		** reinitialize variables
		*/
 		sampsum   = summarycnt + sampcnt;
		totalsec  = 0;
		totalexit = 0;
		memset(&totsyst, 0, sizeof totsyst);

		return '\0';
	}

	/*
	** when no accumulation is required,
	** just print current sample
	*/
	if (summarycnt == 1)
	{
		printf("%s  ", convtime(timenow, timebuf, sizeof(timebuf)-1));

		rv = (pridef[prinow].priline) (sstat, devtstat->taskall,
				devtstat->procall, devtstat->nprocall,
				numsecs, numsecs*hertz, hertz,
				os_vers, os_rel, os_sub,
	               		stampalways ? timebuf : "        ",
				devtstat->ntaskall, devtstat->totrun,
				devtstat->totslpi, devtstat->totslpu,
				devtstat->totidle,
				nexit, devtstat->totzombie);

		if (rv == 0)
		{
			curline++;
			pridef[prinow].wanted = 0; /* not call again */

			if (--numreports == 0)
				cleanstop(1);
		}
		else
		{
			curline += rv;
		}
	}
	else 		/* accumulation is required */
	{
		char  *cp = pridef[prinow].cntcat;

		/*
		** maintain totals per category
		*/
		while (*cp)
		{
			totalsyst(*cp, sstat, &totsyst);
			cp++;
		}

		totalsec  += numsecs;
		totalexit += nexit;

		/*
		** remember some values in case the next record
		** contains the log-restart indicator
		*/
		lasttime   = timenow;
		lastnpres  = devtstat->nprocall;
		lastntrun  = devtstat->totrun;
		lastntslpi = devtstat->totslpi;
		lastntslpu = devtstat->totslpu;
		lastntidle = devtstat->totidle;
		lastnzomb  = devtstat->totzombie;

		/*
		** print line only if needed
		*/
		if (sampcnt >= sampsum || ( (flags&RRLAST) && totalsec) )
		{
			/*
			** print output line for required report
			*/
			printf("%s  ", convtime(timenow, timebuf, sizeof(timebuf)-1));

			rv = (pridef[prinow].priline) (&totsyst,
					(struct tstat *)0, 0, 0,
					totalsec, totalsec*hertz, hertz,
					os_vers, os_rel, os_sub,
					stampalways ? timebuf : "        ",
					devtstat->ntaskall, devtstat->totrun,
					devtstat->totslpi, devtstat->totslpu,
					devtstat->totidle,
					totalexit, devtstat->totzombie);

			if (rv == 0)
			{
				curline++;
				pridef[prinow].wanted = 0; /* not call again */

				if (--numreports == 0)
					cleanstop(1);
			}
			else
			{
				curline += rv;
			}

		 	sampsum   = summarycnt + sampcnt;
			totalsec  = 0;
			totalexit = 0;
			memset(&totsyst, 0, sizeof totsyst);
		}
		else
		{
			rv = 1;
		}
	}

	if (!rv)
	{
		/*
		** print for line has failed;
		** never call this function again
		*/
		pridef[prinow].wanted = 0;

		if (--numreports == 0)
			cleanstop(1);
	}

	pmtimevalFromReal(timenow, &pretime);

	return '\0';
}

/*
** print overall header
*/
static void
reportheader(struct sysname *sys, time_t mtime)
{
        char            cdate[16];

        printf("\n%s  %s  %s  %s  %s\n\n",
                sys->nodename, sys->release, sys->version, sys->machine,
        	convdate(mtime, cdate, sizeof(cdate)-1));
}

/*
** print usage of atopsar command
*/
void
pratopsaruse(char *myname, pmOptions *opts)
{
	int	i;

	fprintf(stderr,
		"Usage: %s [-flags] [-r file|date|y...] [-R cnt] [-b date|time] [-e date|time]\n",
								myname);
	fprintf(stderr, "\t\tor\n");
	fprintf(stderr,
		"Usage: %s [-flags] interval [samples]\n", myname);
	fprintf(stderr, "\n");
	fprintf(stderr,
		"\tToday's pmlogger archive is used by default!\n");
	fprintf(stderr, "\n");
	fprintf(stderr,
		"\tGeneric flags:\n");
	fprintf(stderr,
		"\t  -r  read statistical data from pmlogger archive\n");
	fprintf(stderr,
		"\t      (pathname, or date in format YYYYMMDD, or y[y..])\n");
	fprintf(stderr,
		"\t  -R  summarize <cnt> samples into one sample\n");
	fprintf(stderr,
		"\t  -b  begin  showing data from  specified time as [YYYYMMDD]hhmm[ss]\n");
	fprintf(stderr,
		"\t  -e  finish showing data after specified time as [YYYYMMDD]hhmm[ss]\n");
	fprintf(stderr,
		"\t  -S  print timestamp on every line in case of more "
		"resources\n");
	fprintf(stderr,
		"\t  -x  never  use colors to indicate overload"
		" (default: only if tty)\n");
	fprintf(stderr,
		"\t  -C  always use colors to indicate overload"
		" (default: only if tty)\n");
	fprintf(stderr,
		"\t  -M  use markers to indicate overload "
		"(* = critical, + = almost)\n");
	fprintf(stderr,
		"\t  -H  repeat report headers "
		"(in case of tty: depending on screen lines)\n");
	fprintf(stderr,
		"\t  -a  print all resources, even when inactive\n");
	fprintf(stderr, "\n");
	fprintf(stderr,
		"\tSpecific flags to select reports:\n");
	fprintf(stderr,
		"\t  -A  print all available reports\n");

	for (i=0; i < pricnt; i++)
		fprintf(stderr,
		"\t  -%c  %s\n", pridef[i].flag, pridef[i].about);

	if (opts)
	{
		fprintf(stderr, "\n");
		fprintf(stderr,
			"\tAdditional PCP flags (long options only):\n");
		show_pcp_usage(opts);
	}

	fprintf(stderr, "\n");
	fprintf(stderr,
                "Please refer to the man-page of 'pcp-atopsar' "
	        "for more details.\n");


	cleanstop(1);
}

/*
** calculate the epoch-value for the last second
** of the day given a certain epoch
*/
static time_t
daylimit(time_t timval)
{
	struct tm  tt, *tp;

	tp = pmLocaltime(&timval, &tt);

	tp->tm_hour = 23;
	tp->tm_min  = 59;
	tp->tm_sec  = 59;

	return __pmMktime(tp);
}

/*
** function to be called before printing a statistics line
** to switch on colors when necessary
*/
static void
preprint(unsigned int badness)
{
	if (usecolors)
	{
		if (badness >= 100)
		{
			coloron = 1;
			printf(COLSETHIGH);
		}
		else
		{
			if (almostcrit && badness >= almostcrit)
			{
				coloron = 1;
				printf(COLSETMED);
			}
		}
	}
}

/*
** function to be called after printing a statistics line
** to switch off colors when necessary and print a line feed
*/
static void
postprint(unsigned int badness)
{
	if (coloron)
	{
		coloron = 0;
		printf(COLRESET);
	}

	if (usemarkers)
	{
		if (badness >= 100)
		{
			printf(" *");
		}
		else
		{
			if (almostcrit && badness >= almostcrit)
				printf(" +");
		}
	}

	printf("\n");
}

/*
** function to handle the default flags for atopsar as
** read from the files ~/.atoprc and /etc/atoprc
*/
void
do_atopsarflags(char *name, char *val)
{
	int     i, j;

	for (i=0; val[i]; i++)
	{
		switch (val[i])
		{
		   case '-':
			break;

		   case 'S':		/* timestamp on every line */
			stampalways = 1;
			break;

		   case 'x':		/* always colors for overload */
			usecolors = 0;
			break;

		   case 'C':		/* always colors for overload */
			usecolors = 'a';
			break;

		   case 'M':		/* markers for overload    */
			usemarkers = 1;
			break;

		   case 'H':		/* repeat headers          */
			repeathead = 23;	/* define default  */
#if defined(TIOCGWINSZ)
			if (isatty(fileno(stdout)))
			{
				struct winsize wsz;

				if ( ioctl(1, TIOCGWINSZ, &wsz) != -1)
					repeathead = wsz.ws_row - 1;
			}
#endif
			break;

		   case 'a':		/* every interval all units */
			allresources = 1;
			break;

		   case 'A':		/* all reports wanted ?  */
			for (j=0; j < pricnt; j++)
				pridef[j].wanted = 1;

			numreports = pricnt;
			break;

		   default:		/* gather report-flags    */
			for (j=0; j < pricnt; j++)
			{
				if (pridef[j].flag   == val[i] && 
				    pridef[j].wanted == 0        )
				{
					pridef[j].wanted = 1;
					numreports++;
					break;
				}
			}
		}
	}
}

/**************************************************************************/
/*                 Functions to print statistics                          */
/**************************************************************************/
/*
** CPU statistics
*/
static void
cpuhead(int osvers, int osrel, int ossub)
{
	printf("cpu  %%usr %%nice %%sys %%irq %%softirq  %%steal %%guest "
	       " %%wait %%idle  _cpu_");
}

static int
cpuline(struct sstat *ss, struct tstat *ts, struct tstat **ps, int nactproc,
        time_t deltasec, time_t deltatic, time_t hz,
        int osvers, int osrel, int ossub, char *tstamp,
        int ppres,  int ntrun, int ntslpi, int ntslpu, int ntidle,
	int pexit, int pzombie)
{
	register int	i, nlines = 1;
	count_t		cputot;
	unsigned int	badness;

	/*
	** print overall statistics
	*/
        cputot = ss->cpu.all.stime + ss->cpu.all.utime +
                 ss->cpu.all.ntime + ss->cpu.all.itime +
                 ss->cpu.all.wtime + ss->cpu.all.Itime +
                 ss->cpu.all.Stime + ss->cpu.all.steal;

	if (cputot == 0)
		cputot = 1;	/* avoid divide-by-zero */

	if (cpubadness)
		badness = ((cputot - ss->cpu.all.itime - ss->cpu.all.wtime) *
                            100.0 / cputot) * 100 / cpubadness;
	else
		badness = 0;

	preprint(badness);

	printf("all %5.0lf %5.0lf %4.0lf %4.0lf %8.0lf %7.0f %6.0f %6.0lf %5.0lf",
                (double) (ss->cpu.all.utime * 100.0) / cputot * ss->cpu.nrcpu,
                (double) (ss->cpu.all.ntime * 100.0) / cputot * ss->cpu.nrcpu,
                (double) (ss->cpu.all.stime * 100.0) / cputot * ss->cpu.nrcpu,
                (double) (ss->cpu.all.Itime * 100.0) / cputot * ss->cpu.nrcpu,
                (double) (ss->cpu.all.Stime * 100.0) / cputot * ss->cpu.nrcpu,
                (double) (ss->cpu.all.steal * 100.0) / cputot * ss->cpu.nrcpu,
                (double) (ss->cpu.all.guest * 100.0) / cputot * ss->cpu.nrcpu,
                (double) (ss->cpu.all.wtime * 100.0) / cputot * ss->cpu.nrcpu,
                (double) (ss->cpu.all.itime * 100.0) / cputot * ss->cpu.nrcpu);

	postprint(badness);

	/*
	** print per-cpu statistics
	*/
	if (ss->cpu.nrcpu > 1)
	{
		for (i=0; i < ss->cpu.nrcpu; i++)
		{
        		cputot = ss->cpu.cpu[i].stime + ss->cpu.cpu[i].utime +
                 	         ss->cpu.cpu[i].ntime + ss->cpu.cpu[i].itime +
                 	         ss->cpu.cpu[i].wtime + ss->cpu.cpu[i].Itime +
                 	         ss->cpu.cpu[i].Stime + ss->cpu.cpu[i].steal;

			if (cputot == 0)
				cputot = 1;	/* avoid divide-by-zero */

			if (cpubadness)
				badness = ((cputot - ss->cpu.cpu[i].itime -
					    ss->cpu.cpu[i].wtime) * 100.0 /
				            cputot) * 100 / cpubadness;
			else
				badness = 0;

			printf("%s ", tstamp);

			preprint(badness);

			printf("%4d %5.0lf %5.0lf %4.0lf %4.0lf %8.0lf "
			       "%7.0f %6.0lf %6.0lf %5.0lf",
			     i,
                 	     (double)(ss->cpu.cpu[i].utime * 100.0) / cputot,
                	     (double)(ss->cpu.cpu[i].ntime * 100.0) / cputot,
                	     (double)(ss->cpu.cpu[i].stime * 100.0) / cputot,
                	     (double)(ss->cpu.cpu[i].Itime * 100.0) / cputot,
                	     (double)(ss->cpu.cpu[i].Stime * 100.0) / cputot,
                	     (double)(ss->cpu.cpu[i].steal * 100.0) / cputot,
                	     (double)(ss->cpu.cpu[i].guest * 100.0) / cputot,
                	     (double)(ss->cpu.cpu[i].wtime * 100.0) / cputot,
                	     (double)(ss->cpu.cpu[i].itime * 100.0) / cputot);

			postprint(badness);

			nlines++;
		}
	}

	return nlines;
}


/*
** GPU statistics
*/
static void
gpuhead(int osvers, int osrel, int ossub)
{
	printf("   busaddr   gpubusy  membusy  memocc  memtot memuse  gputype"
	       "   _gpu_");
}

static int
gpuline(struct sstat *ss, struct tstat *ts, struct tstat **ps, int nactproc,
        time_t deltasec, time_t deltatic, time_t hz,
        int osvers, int osrel, int ossub, char *tstamp,
        int ppres,  int ntrun, int ntslpi, int ntslpu, int ntidle,
	int pexit, int pzombie)
{
	static char	firstcall = 1;
	register long	i, nlines = 0;
	char		fmt1[16], fmt2[16];
	count_t		avgmemuse;

	for (i=0; i < ss->gpu.nrgpus; i++)	/* per GPU */
	{
		/*
		** determine whether or not the GPU has been active
		** during interval
		*/
		int wasactive;

		wasactive = ss->gpu.gpu[i].gpuperccum +
		            ss->gpu.gpu[i].memperccum;

		if (wasactive == -2)      // metrics not available?
			wasactive = 0;

		if (ss->gpu.gpu[i].samples == 0)
			avgmemuse = ss->gpu.gpu[i].memusenow;
		else
			avgmemuse = ss->gpu.gpu[i].memusecum /
			            ss->gpu.gpu[i].samples;

		// memusage > 512 MiB (rather arbitrary)?
		//
		if (avgmemuse > 512*1024)
			wasactive = 1;

		/*
		** print for the first sample all GPUs that are found;
		** afterwards print only info about the GPUs
		** that were really active during the interval
		*/
		if (!firstcall && !allresources && !wasactive)
			continue;

		if (nlines++)
			printf("%s  ", tstamp);

		if (ss->gpu.gpu[i].samples == 0)
			ss->gpu.gpu[i].samples = 1;

		if (ss->gpu.gpu[i].gpuperccum == -1)
			strcpy(fmt1, "N/A");
		else
			pmsprintf(fmt1, sizeof fmt1, "%lld%%",
			   ss->gpu.gpu[i].gpuperccum / ss->gpu.gpu[i].samples);

		if (ss->gpu.gpu[i].memperccum == -1)
			strcpy(fmt2, "N/A");
		else
			pmsprintf(fmt2, sizeof fmt2, "%lld%%",
			   ss->gpu.gpu[i].memperccum / ss->gpu.gpu[i].samples);

		if (ss->gpu.gpu[i].memtotnow == 0)
			ss->gpu.gpu[i].memtotnow = 1;

		printf("%2ld/%9.9s %7s  %7s  %5lld%%  %5lldM %5lldM  %s\n",
			i, ss->gpu.gpu[i].busid,
			fmt1, fmt2,
			ss->gpu.gpu[i].memusenow*100/ss->gpu.gpu[i].memtotnow,
			ss->gpu.gpu[i].memtotnow / 1024,
			ss->gpu.gpu[i].memusenow / 1024,
			ss->gpu.gpu[i].type);
	}

	if (nlines == 0)
	{
		printf("\n");
		nlines++;
	}

	firstcall = 0;
	return nlines;
}

/*
** other processor statistics
*/
static void
prochead(int osvers, int osrel, int ossub)
{
	printf("pswch/s devintr/s  clones/s  loadavg1 loadavg5 loadavg15  "
	       "     _load_");
}

static int
procline(struct sstat *ss, struct tstat *ts, struct tstat **ps, int nactproc,
        time_t deltasec, time_t deltatic, time_t hz,
        int osvers, int osrel, int ossub, char *tstamp,
        int ppres,  int ntrun, int ntslpi, int ntslpu, int ntidle,
	int pexit, int pzombie)
{
	printf("%7.0lf %9.0lf %9.2lf  %8.2lf %8.2lf  %8.2lf\n",
		(double)ss->cpu.csw    / deltasec,
		(double)ss->cpu.devint / deltasec,
		(double)ss->cpu.nprocs / deltasec,
		ss->cpu.lavg1, ss->cpu.lavg5, ss->cpu.lavg15);
	return 1;
}

/*
** process statistics
*/
static void
taskhead(int osvers, int osrel, int ossub)
{
	printf("clones/s pexit/s  curproc curzomb   trun  tslpi tslpu tidle "
	       "_procthr_");
}

static int
taskline(struct sstat *ss, struct tstat *ts, struct tstat **ps, int nactproc,
        time_t deltasec, time_t deltatic, time_t hz,
        int osvers, int osrel, int ossub, char *tstamp,
        int ppres,  int ntrun, int ntslpi, int ntslpu, int ntidle,
	int pexit, int pzombie)
{
	if (ppres == 0)
	{
		printf("report not available for live measurements.....\n");
		return 0;
	}

	if (ts)		/* process statistics available */
	{
		printf("%8.2lf %7.2lf  %7d %7d %6d %6d %5d %5d\n",
			(double)ss->cpu.nprocs / deltasec,
			(double)pexit          / deltasec,
			nactproc-pexit, pzombie,
			ntrun, ntslpi, ntslpu, ntidle);
	}
	else
	{
		printf("%8.2lf %7.2lf  %7d %7d\n",
			(double)ss->cpu.nprocs / deltasec,
			(double)pexit          / deltasec,
			nactproc-pexit, pzombie);
	}

	return 1;
}

/*
** memory- & swap-usage
*/
static void
memhead(int osvers, int osrel, int ossub)
{
	printf("memtotal memfree buffers cached dirty slabmem"
	       "  swptotal swpfree _mem_"             );
}

static int
memline(struct sstat *ss, struct tstat *ts, struct tstat **ps, int nactproc,
        time_t deltasec, time_t deltatic, time_t hz,
        int osvers, int osrel, int ossub, char *tstamp,
        int ppres,  int ntrun, int ntslpi, int ntslpu, int ntidle,
	int pexit, int pzombie)
{
	unsigned int	mbadness, sbadness;

	if (membadness)
		mbadness = ((ss->mem.physmem  - ss->mem.freemem 
	                     - ss->mem.cachemem - ss->mem.buffermem
	                     + ss->mem.shmem) * 100.0 / ss->mem.physmem) 
	                               * 100   / membadness;
	else
		mbadness = 0;

        if (swpbadness)
        	sbadness = ((ss->mem.totswap - ss->mem.freeswap)
	                               * 100.0 / ss->mem.totswap)
                                       * 100   / swpbadness;
        else
                sbadness = 0;

	preprint(mbadness >= sbadness ? mbadness : sbadness);

	printf("%7lldM %6lldM %6lldM %5lldM %4lldM %6lldM  %7lldM %6lldM",
		ss->mem.physmem   /1024,
		ss->mem.freemem   /1024,
		ss->mem.buffermem /1024,
		ss->mem.cachemem  /1024,
		ss->mem.cachedrt  /1024,
		ss->mem.slabmem   /1024,
		ss->mem.totswap   /1024,
		ss->mem.freeswap  /1024);

	postprint(mbadness >= sbadness ? mbadness : sbadness);

	return 1;
}

/*
** swapping statistics
*/
static void
swaphead(int osvers, int osrel, int ossub)
{
	printf("pagescan/s  swapin/s swapout/s oomkill"
	       "  commitspc  commitlim   _swap_");
}

static int
swapline(struct sstat *ss, struct tstat *ts, struct tstat **ps, int nactproc,
        time_t deltasec, time_t deltatic, time_t hz,
        int osvers, int osrel, int ossub, char *tstamp,
        int ppres,  int ntrun, int ntslpi, int ntslpu, int ntidle,
	int pexit, int pzombie)
{
	unsigned int	badness;

	if (membadness)
                badness = (ss->mem.swouts / deltasec * pagbadness)
				* 100 / membadness;
	else
		badness = 0;

	/*
	** take care that this line is anyhow colored for
	** 'almost critical' in case of swapouts > 1 per second
	*/
	if (ss->mem.swouts / deltasec > 0  &&
	    pagbadness && almostcrit && badness < almostcrit)
		badness = almostcrit;

	if (ss->mem.commitlim && ss->mem.committed > ss->mem.commitlim)
		badness = 100;         /* force colored output */

	preprint(badness);

	printf("%10.2lf %9.2lf %9.2lf %7lld %9lluM %9lluM",
		(double)ss->mem.pgscans / deltasec,
		(double)ss->mem.swins   / deltasec,
		(double)ss->mem.swouts  / deltasec,
		        ss->mem.oomkills,
		        ss->mem.committed / 1024,
		        ss->mem.commitlim / 1024);

	postprint(badness);

	return 1;
}

/*
** PSI statistics
*/
static void
psihead(int osvers, int osrel, int ossub)
{
	printf("cpusome    memsome  memfull    iosome  iofull");
}

static int
psiline(struct sstat *ss, struct tstat *ts, struct tstat **ps, int nactproc,
        time_t deltasec, time_t deltatic, time_t hz,
        int osvers, int osrel, int ossub, char *tstamp,
        int ppres,  int ntrun, int ntslpi, int ntslpu, int ntidle,
	int pexit, int pzombie)
{
	// calculate pressure percentages for entire interval
	unsigned int	csperc  = ss->psi.cpusome.total/(deltatic*10000/hz);
	unsigned int	msperc  = ss->psi.memsome.total/(deltatic*10000/hz);
	unsigned int	mfperc  = ss->psi.memfull.total/(deltatic*10000/hz);
	unsigned int	isperc  = ss->psi.iosome.total /(deltatic*10000/hz);
	unsigned int	ifperc  = ss->psi.iofull.total /(deltatic*10000/hz);
	unsigned int	badness = 0;

	if (!ss->psi.present)
	{
		printf("no PSI stats available for this interval...\n");
		return 1;
	}

	// correct percentages if needed
	if (csperc > 100)
		csperc = 100;

	if (msperc > 100)
		msperc = 100;

	if (mfperc > 100)
		mfperc = 100;

	if (isperc > 100)
		isperc = 100;

	if (ifperc > 100)
		ifperc = 100;

	// consider a 'some' percentage > 0 as almost critical
	// (I/O full tends to increase rapidly as well)
	if (csperc || msperc || isperc || ifperc)
		badness = 80;

	// consider a memory 'full' percentage > 0 as critical
	if (mfperc)
		badness = 100;

	// show results
	preprint(badness);

	printf("   %3u%%       %3u%%     %3u%%      %3u%%    %3u%%",
		csperc, msperc, mfperc, isperc, ifperc);

	postprint(badness);

	return 1;
}


/*
** disk statistics
*/
static void
lvmhead(int osvers, int osrel, int ossub)
{
	printf("disk           busy read/s KB/read  "
	       "writ/s KB/writ avque avserv _lvm_");
}

static void
mddhead(int osvers, int osrel, int ossub)
{
	printf("disk           busy read/s KB/read  "
	       "writ/s KB/writ avque avserv _mdd_");
}

static void
dskhead(int osvers, int osrel, int ossub)
{
	printf("disk           busy read/s KB/read  "
	       "writ/s KB/writ avque avserv _dsk_");
}

static int
gendskline(struct sstat *ss, char *tstamp, char selector)
{
	static char	firstcall = 1;
	register int	i, nlines = 0, nunit = 0;
	count_t		mstot, iotot;
	struct perdsk 	*dp;
	unsigned int	badness;

	switch (selector)
	{
	   case 'l':
		dp 	= ss->dsk.lvm;
		nunit	= ss->dsk.nlvm;
		break;

	   case 'm':
		dp 	= ss->dsk.mdd;
		nunit	= ss->dsk.nmdd;
		break;

	   case 'd':
		dp 	= ss->dsk.dsk;
		nunit	= ss->dsk.ndsk;
		break;

	   default:
		return 0;
	}

        mstot  = (ss->cpu.all.stime + ss->cpu.all.utime +
                  ss->cpu.all.ntime + ss->cpu.all.itime +
                  ss->cpu.all.wtime + ss->cpu.all.Itime +
                  ss->cpu.all.Stime + ss->cpu.all.steal  )
				* (count_t)1000 / hertz / ss->cpu.nrcpu;

	for (i=0; i < nunit; i++, dp++)
	{
		char	*pn;
		int	len;

		iotot = dp->nread + dp->nwrite +
		             (dp->ndisc != -1 ? dp->ndisc : 0);

		if (iotot == 0 && !firstcall && !allresources)
			continue;	/* no activity on this disk */

		/*
		** disk was active during last interval; print info
		*/
		if (nlines++)
			printf("%s  ", tstamp);

		if (mstot == 0)
			mstot = 1;	/* avoid divide-by-zero */

		if (dskbadness)
			badness = (dp->io_ms * 100.0 / mstot) * 100/dskbadness;
                else
			badness = 0;

		preprint(badness);

		if ( (len = strlen(dp->name)) > 14)
			pn = dp->name + len - 14;
		else
			pn = dp->name;

		printf("%-14s %3.0lf%% %6.1lf %7.1lf %7.1lf %7.1lf "
		       "%5.1lf %9.5lf ms",
		    	pn,
			(double)dp->io_ms  *  100.0 / mstot,
			(double)dp->nread  * 1000.0 / mstot,
			dp->nread  ?
			        (double)dp->nrsect / dp->nread / 2.0  : 0.0,
			(double)dp->nwrite * 1000.0 / mstot,
			dp->nwrite ?
  			        (double)dp->nwsect / dp->nwrite / 2.0 : 0.0,
			dp->io_ms  ? (double)dp->avque / dp->io_ms    : 0.0,
		      	iotot ? (double)dp->io_ms  / iotot            : 0.0);

		postprint(badness);
	}

	if (nlines == 0)
	{
		printf("\n");
		nlines++;
	}

	firstcall = 0;

	return nlines;
}

static int
lvmline(struct sstat *ss, struct tstat *ts, struct tstat **ps, int nactproc,
        time_t deltasec, time_t deltatic, time_t hz,
        int osvers, int osrel, int ossub, char *tstamp,
        int ppres,  int ntrun, int ntslpi, int ntslpu, int ntidle,
	int pexit, int pzombie)
{
	return gendskline(ss, tstamp, 'l');
}

static int
mddline(struct sstat *ss, struct tstat *ts, struct tstat **ps, int nactproc,
        time_t deltasec, time_t deltatic, time_t hz,
        int osvers, int osrel, int ossub, char *tstamp,
        int ppres,  int ntrun, int ntslpi, int ntslpu, int ntidle,
	int pexit, int pzombie)
{
	return gendskline(ss, tstamp, 'm');
}

static int
dskline(struct sstat *ss, struct tstat *ts, struct tstat **ps, int nactproc,
        time_t deltasec, time_t deltatic, time_t hz,
        int osvers, int osrel, int ossub, char *tstamp,
        int ppres,  int ntrun, int ntslpi, int ntslpu, int ntidle,
	int pexit, int pzombie)
{
	return gendskline(ss, tstamp, 'd');
}

/*
** NFS client statistics
*/
static void
nfmhead(int osvers, int osrel, int ossub)
{
	printf("mounted_device                          physread/s  physwrit/s"
               "  _nfm_");
}

static int
nfmline(struct sstat *ss, struct tstat *ts, struct tstat **ps, int nactproc,
        time_t deltasec, time_t deltatic, time_t hz,
        int osvers, int osrel, int ossub, char *tstamp,
        int ppres,  int ntrun, int ntslpi, int ntslpu, int ntidle,
	int pexit, int pzombie)
{
	static char	firstcall = 1;
	register long	i, nlines = 0;
	char		*pn, state;
	int		len;

	for (i=0; i < ss->nfs.nfsmounts.nrmounts; i++)	/* per NFS mount */
	{
		/*
		** print for the first sample all mounts that
		** are found; afterwards print only the mounts
		** that were really active during the interval
		*/
		if (firstcall                                  ||
		    allresources                               ||
		    ss->nfs.nfsmounts.nfsmnt[i].age < deltasec ||
		    ss->nfs.nfsmounts.nfsmnt[i].bytestotread   ||
		    ss->nfs.nfsmounts.nfsmnt[i].bytestotwrite    )
		{
			if (nlines++)
				printf("%s  ", tstamp);

			if ( (len = strlen(ss->nfs.nfsmounts.nfsmnt[i].mountdev)) > 38)
				pn = ss->nfs.nfsmounts.nfsmnt[i].mountdev + len - 38;
			else
				pn = ss->nfs.nfsmounts.nfsmnt[i].mountdev;

		    	if (ss->nfs.nfsmounts.nfsmnt[i].age < deltasec)
				state = 'M';
			else
				state = ' ';

			printf("%-38s %10.3lfK %10.3lfK    %c\n", 
			    pn,
			    (double)ss->nfs.nfsmounts.nfsmnt[i].bytestotread  /
								1024 / deltasec,
			    (double)ss->nfs.nfsmounts.nfsmnt[i].bytestotwrite /
								1024 / deltasec,
			    state);
		}
	}

	if (nlines == 0)
	{
		printf("\n");
		nlines++;
	}

	firstcall= 0;
	return nlines;
}

static void
nfchead(int osvers, int osrel, int ossub)
{
	printf("     rpc/s   rpcread/s  rpcwrite/s  retrans/s  autrefresh/s   "
               "  _nfc_");
}

static int
nfcline(struct sstat *ss, struct tstat *ts, struct tstat **ps, int nactproc,
        time_t deltasec, time_t deltatic, time_t hz,
        int osvers, int osrel, int ossub, char *tstamp,
        int ppres,  int ntrun, int ntslpi, int ntslpu, int ntidle,
	int pexit, int pzombie)
{
	printf("%10.2lf  %10.2lf  %10.2lf %10.2lf  %12.2lf\n",
		(double)ss->nfs.client.rpccnt        / deltasec,
		(double)ss->nfs.client.rpcread       / deltasec,
		(double)ss->nfs.client.rpcwrite      / deltasec,
		(double)ss->nfs.client.rpcretrans    / deltasec,
		(double)ss->nfs.client.rpcautrefresh / deltasec);

	return 1;
}

static void
nfshead(int osvers, int osrel, int ossub)
{
	printf("  rpc/s  rpcread/s rpcwrite/s MBcr/s  MBcw/s  "
               "nettcp/s netudp/s _nfs_");
}

static int
nfsline(struct sstat *ss, struct tstat *ts, struct tstat **ps, int nactproc,
        time_t deltasec, time_t deltatic, time_t hz,
        int osvers, int osrel, int ossub, char *tstamp,
        int ppres,  int ntrun, int ntslpi, int ntslpu, int ntidle,
	int pexit, int pzombie)
{
	printf("%7.2lf %10.2lf %10.2lf %6.2lf %7.2lf %9.2lf %8.2lf\n",
		(double)ss->nfs.server.rpccnt    / deltasec,
		(double)ss->nfs.server.rpcread   / deltasec,
		(double)ss->nfs.server.rpcwrite  / deltasec,
		(double)ss->nfs.server.nrbytes / 1024.0 / 1024.0 / deltasec,
		(double)ss->nfs.server.nwbytes / 1024.0 / 1024.0 / deltasec,
		(double)ss->nfs.server.nettcpcnt / deltasec,
		(double)ss->nfs.server.netudpcnt / deltasec);

	return 1;
}

/*
** Infiniband statistics
*/
static void
ibhead(int osvers, int osrel, int ossub)
{
	printf("controller port  busy ipack/s opack/s "
	       "igbps ogbps maxgbps lanes  _ib_");
}

static int
ibline(struct sstat *ss, struct tstat *ts, struct tstat **ps, int nactproc,
        time_t deltasec, time_t deltatic, time_t hz,
        int osvers, int osrel, int ossub, char *tstamp,
        int ppres,  int ntrun, int ntslpi, int ntslpu, int ntidle,
	int pexit, int pzombie)
{
	static char	firstcall = 1;
	register long	i, nlines = 0;
	double		busy;
	unsigned int	badness;

	for (i=0; i < ss->ifb.nrports; i++)	/* per interface */
	{
		count_t ival, oval;

		/*
		** print for the first sample all ports that
		** are found; afterwards print only the ports
		** that were really active during the interval
		*/
		if (!firstcall && !allresources &&
		    !ss->ifb.ifb[i].rcvb && !ss->ifb.ifb[i].sndb)
			continue;

		/*
		** convert byte-transfers to bit-transfers     (*          8)
		** convert bit-transfers  to gigabit-transfers (/ 1000000000)
		** per second
		*/
		ival = ss->ifb.ifb[i].rcvb*ss->ifb.ifb[i].lanes/125000000/deltasec;
		oval = ss->ifb.ifb[i].sndb*ss->ifb.ifb[i].lanes/125000000/deltasec;

		/*
		** calculate busy-percentage for port
		*/
		busy = (ival > oval ? ival*100 : oval*100)/ss->ifb.ifb[i].rate;

		if (nlines++)
			printf("%s  ", tstamp);

		if (netbadness)
			badness = busy * 100 / netbadness;
		else
			badness = 0;

		preprint(badness);

		printf("%-10s %4hd %4.0f%% %7.1lf %7.1lf %5lld %5lld %7lld %5d", 
			ss->ifb.ifb[i].ibname,
			ss->ifb.ifb[i].portnr,
			busy,
			(double)ss->ifb.ifb[i].rcvp / deltasec,
			(double)ss->ifb.ifb[i].sndp / deltasec,
			ival, oval,
			ss->ifb.ifb[i].rate / 1000,
			ss->ifb.ifb[i].lanes);

		postprint(badness);
	}

	if (nlines == 0)
	{
		printf("\n");
		nlines++;
	}

	firstcall = 0;
	return nlines;
}

/*
** network-interface statistics
*/
static void
ifhead(int osvers, int osrel, int ossub)
{
	printf("interf busy ipack/s opack/s iKbyte/s oKbyte/s "
	       "imbps ombps maxmbps_if_");
}

static int
ifline(struct sstat *ss, struct tstat *ts, struct tstat **ps, int nactproc,
        time_t deltasec, time_t deltatic, time_t hz,
        int osvers, int osrel, int ossub, char *tstamp,
        int ppres,  int ntrun, int ntslpi, int ntslpu, int ntidle,
	int pexit, int pzombie)
{
	static char	firstcall = 1;
	register long	i, nlines = 0;
	double		busy;
	char		busyval[16], dupval;
	unsigned int	badness;
	char		*pn;
	int		len;

	for (i=0; i < ss->intf.nrintf; i++)	/* per interface */
	{
		count_t ival, oval;

		/*
		** print for the first sample all interfaces which
		** are found; afterwards print only the interfaces
		** which were really active during the interval
		*/
		if (!firstcall && !allresources &&
		    !ss->intf.intf[i].rpack && !ss->intf.intf[i].spack)
			continue;

		/*
		** convert byte-transfers to bit-transfers     (*       8)
		** convert bit-transfers  to megabit-transfers (/ 1000000)
		** per second
		*/
		ival = ss->intf.intf[i].rbyte/125000/deltasec;
		oval = ss->intf.intf[i].sbyte/125000/deltasec;

		/*
		** calculate busy-percentage for interface
		*/
		if (ss->intf.intf[i].speed)	/* speed known? */
		{
			if (ss->intf.intf[i].duplex)
				busy = (ival > oval ? ival*100 : oval*100) /
				        ss->intf.intf[i].speed;
			else
				busy = (ival + oval) * 100 /
				        ss->intf.intf[i].speed;

			// especially with wireless, the speed might have
			// dropped temporarily to a very low value (snapshot)
			// it might be better to take the speed of the
			// previous sample
			if (busy > 100 && ss->intf.intf[i].speed <
			                  	ss->intf.intf[i].speedp )
			{
				ss->intf.intf[i].speed =
					ss->intf.intf[i].speedp;

				if (ss->intf.intf[i].duplex)
					busy = (ival > oval ?
						ival*100 : oval*100) /
				        	ss->intf.intf[i].speed;
				else
					busy = (ival + oval) * 100 /
				        	ss->intf.intf[i].speed;
			}

			pmsprintf(busyval, sizeof busyval,
						"%3.0lf%%", busy);
		}
		else
		{
			strcpy(busyval, "?"); /* speed unknown */
			busy = 0;
		}

		if (nlines++)
			printf("%s  ", tstamp);

		if (ss->intf.intf[i].speed)
		{
			if (ss->intf.intf[i].duplex)
				dupval = 'f';
			else
				dupval = 'h';
		}
		else
		{
			dupval = ' ';
		}

		if (netbadness)
			badness = busy * 100 / netbadness;
		else
			badness = 0;

		if ( (len = strlen(ss->intf.intf[i].name)) > 6)
			pn = ss->intf.intf[i].name + len - 6;
		else
			pn = ss->intf.intf[i].name;

		preprint(badness);

		printf("%-6s %4s %7.1lf %7.1lf %8.0lf %8.0lf "
		       "%5lld %5lld %7lld %c", 
			pn, busyval,
			(double)ss->intf.intf[i].rpack / deltasec,
			(double)ss->intf.intf[i].spack / deltasec,
			(double)ss->intf.intf[i].rbyte / 1024 / deltasec,
			(double)ss->intf.intf[i].sbyte / 1024 / deltasec,
			ival, oval,
			ss->intf.intf[i].speed, dupval);

		postprint(badness);
	}

	if (nlines == 0)
	{
		printf("\n");
		nlines++;
	}

	firstcall = 0;
	return nlines;
}

static void
IFhead(int osvers, int osrel, int ossub)
{
	printf("interf ierr/s oerr/s coll/s idrop/s odrop/s "
	       "iframe/s ocarrier/s  _if_");
}

static int
IFline(struct sstat *ss, struct tstat *ts, struct tstat **ps, int nactproc,
        time_t deltasec, time_t deltatic, time_t hz,
        int osvers, int osrel, int ossub, char *tstamp,
        int ppres,  int ntrun, int ntslpi, int ntslpu, int ntidle,
	int pexit, int pzombie)
{
	static char	firstcall = 1;
	register long	i, nlines = 0;
	char		*pn;
	int		len;

	for (i=0; i < ss->intf.nrintf; i++)	/* per interface */
	{
		/*
		** print for the first sample all interfaces which
		** are found; afterwards print only the interfaces
		** which were really active during the interval
		*/
		if (!firstcall && !allresources &&
		    !ss->intf.intf[i].rpack && !ss->intf.intf[i].spack)
			continue;

		if (nlines++)
			printf("%s  ", tstamp);

		if ( (len = strlen(ss->intf.intf[i].name)) > 6)
			pn = ss->intf.intf[i].name + len - 6;
		else
			pn = ss->intf.intf[i].name;

		printf("%-6s %6.2lf %6.2lf %6.2lf %7.2lf %7.2lf "
		       "%8.2lf %10.2lf\n", 
			pn,
			(double)ss->intf.intf[i].rerrs    / deltasec,
			(double)ss->intf.intf[i].serrs    / deltasec,
			(double)ss->intf.intf[i].scollis  / deltasec,
			(double)ss->intf.intf[i].rdrop    / deltasec,
			(double)ss->intf.intf[i].sdrop    / deltasec,
			(double)ss->intf.intf[i].rframe   / deltasec,
			(double)ss->intf.intf[i].scarrier / deltasec);
	}

	if (nlines == 0)
	{
		printf("\n");
		nlines++;
	}

	firstcall= 0;
	return nlines;
}

/*
** IP version 4 statistics
*/
static void
ipv4head(int osvers, int osrel, int ossub)
{
	printf("inrecv/s outreq/s indeliver/s forward/s "
	       "reasmok/s fragcreat/s  _ipv4_");
}

static int
ipv4line(struct sstat *ss, struct tstat *ts, struct tstat **ps, int nactproc,
        time_t deltasec, time_t deltatic, time_t hz,
        int osvers, int osrel, int ossub, char *tstamp,
        int ppres,  int ntrun, int ntslpi, int ntslpu, int ntidle,
	int pexit, int pzombie)
{
	printf("%8.1lf %8.1lf %11.1lf %9.1lf %9.1lf %11.1lf\n", 
		(double)ss->net.ipv4.InReceives  / deltasec,
		(double)ss->net.ipv4.OutRequests / deltasec,
		(double)ss->net.ipv4.InDelivers  / deltasec,
		(double)ss->net.ipv4.Forwarding  / deltasec,
		(double)ss->net.ipv4.ReasmOKs    / deltasec,
		(double)ss->net.ipv4.FragCreates / deltasec);
	return 1;
}

static void
IPv4head(int osvers, int osrel, int ossub)
{
	printf("in: dsc/s hder/s ader/s unkp/s ratim/s rfail/s "
	       "out: dsc/s nrt/s_ipv4_");
}

static int
IPv4line(struct sstat *ss, struct tstat *ts, struct tstat **ps, int nactproc,
        time_t deltasec, time_t deltatic, time_t hz,
        int osvers, int osrel, int ossub, char *tstamp,
        int ppres,  int ntrun, int ntslpi, int ntslpu, int ntidle,
	int pexit, int pzombie)
{
	printf("    %5.1lf %6.1lf %6.1lf %6.1lf %7.1lf %7.1lf  "
	       "    %5.1lf %5.1lf\n", 
		(double)ss->net.ipv4.InDiscards      / deltasec,
		(double)ss->net.ipv4.InHdrErrors     / deltasec,
		(double)ss->net.ipv4.InAddrErrors    / deltasec,
		(double)ss->net.ipv4.InUnknownProtos / deltasec,
		(double)ss->net.ipv4.ReasmTimeout    / deltasec,
		(double)ss->net.ipv4.ReasmFails      / deltasec,
		(double)ss->net.ipv4.OutDiscards     / deltasec,
		(double)ss->net.ipv4.OutNoRoutes     / deltasec);
	return 1;
}

/*
** ICMP version 4 statistics
*/
static void
icmpv4head(int osvers, int osrel, int ossub)
{
	printf("intot/s outtot/s  inecho/s inerep/s  "
	       "otecho/s oterep/s       _icmpv4_"   );
}

static int
icmpv4line(struct sstat *ss, struct tstat *ts, struct tstat **ps, int nactproc,
        time_t deltasec, time_t deltatic, time_t hz,
        int osvers, int osrel, int ossub, char *tstamp,
        int ppres,  int ntrun, int ntslpi, int ntslpu, int ntidle,
	int pexit, int pzombie)
{
	printf("%7.1lf %8.1lf  %8.2lf %8.2lf  %8.2lf %8.2lf\n", 
		(double)ss->net.icmpv4.InMsgs      / deltasec, 
		(double)ss->net.icmpv4.OutMsgs     / deltasec, 
		(double)ss->net.icmpv4.InEchos     / deltasec, 
		(double)ss->net.icmpv4.OutEchos    / deltasec, 
		(double)ss->net.icmpv4.InEchoReps  / deltasec, 
		(double)ss->net.icmpv4.OutEchoReps / deltasec);
	return 1;
}

static void
ICMPv4head(int osvers, int osrel, int ossub)
{
	printf("ierr/s isq/s ird/s idu/s ite/s "
	       "oerr/s osq/s ord/s odu/s ote/s_icmpv4_");
}

static int
ICMPv4line(struct sstat *ss, struct tstat *ts, struct tstat **ps, int nactproc,
        time_t deltasec, time_t deltatic, time_t hz,
        int osvers, int osrel, int ossub, char *tstamp,
        int ppres,  int ntrun, int ntslpi, int ntslpu, int ntidle,
	int pexit, int pzombie)
{
	printf("%6.2lf %5.2lf %5.2lf %5.2lf %5.2lf "
	       "%6.2lf %5.2lf %5.2lf %5.2lf %5.2lf\n", 
		(double)ss->net.icmpv4.InErrors        / deltasec,
		(double)ss->net.icmpv4.InSrcQuenchs    / deltasec,
		(double)ss->net.icmpv4.InRedirects     / deltasec,
		(double)ss->net.icmpv4.InDestUnreachs  / deltasec,
		(double)ss->net.icmpv4.InTimeExcds     / deltasec,
		(double)ss->net.icmpv4.OutErrors       / deltasec,
		(double)ss->net.icmpv4.OutSrcQuenchs   / deltasec,
		(double)ss->net.icmpv4.OutRedirects    / deltasec,
		(double)ss->net.icmpv4.OutDestUnreachs / deltasec,
		(double)ss->net.icmpv4.OutTimeExcds    / deltasec);
	return 1;
}

/*
** UDP version 4 statistics
*/
static void
udpv4head(int osvers, int osrel, int ossub)
{
	printf("indgram/s outdgram/s   inerr/s  noport/s    "
	       "                  _udpv4_");
}

static int
udpv4line(struct sstat *ss, struct tstat *ts, struct tstat **ps, int nactproc,
        time_t deltasec, time_t deltatic, time_t hz,
        int osvers, int osrel, int ossub, char *tstamp,
        int ppres,  int ntrun, int ntslpi, int ntslpu, int ntidle,
	int pexit, int pzombie)
{
	printf("%9.1lf %10.1lf   %7.2lf %9.2lf\n",
		(double)ss->net.udpv4.InDatagrams  / deltasec,
		(double)ss->net.udpv4.OutDatagrams / deltasec,
		(double)ss->net.udpv4.InErrors     / deltasec,
		(double)ss->net.udpv4.NoPorts      / deltasec);
	return 1;
}

/*
** IP version 6 statistics
*/
static void
ipv6head(int osvers, int osrel, int ossub)
{
	printf("inrecv/s outreq/s inmc/s outmc/s indeliv/s "
	       "reasmok/s fragcre/s _ipv6_");
}

static int
ipv6line(struct sstat *ss, struct tstat *ts, struct tstat **ps, int nactproc,
        time_t deltasec, time_t deltatic, time_t hz,
        int osvers, int osrel, int ossub, char *tstamp,
        int ppres,  int ntrun, int ntslpi, int ntslpu, int ntidle,
	int pexit, int pzombie)
{
	printf("%8.1lf %8.1lf %6.1lf %7.1lf %9.1lf %9.1lf %9.1lf\n", 
		(double)ss->net.ipv6.Ip6InReceives   / deltasec,
		(double)ss->net.ipv6.Ip6OutRequests  / deltasec,
		(double)ss->net.ipv6.Ip6InMcastPkts  / deltasec,
		(double)ss->net.ipv6.Ip6OutMcastPkts / deltasec,
		(double)ss->net.ipv6.Ip6InDelivers   / deltasec,
		(double)ss->net.ipv6.Ip6ReasmOKs     / deltasec,
		(double)ss->net.ipv6.Ip6FragCreates  / deltasec);
	return 1;
}

static void
IPv6head(int osvers, int osrel, int ossub)
{
	printf("in: dsc/s hder/s ader/s unkp/s ratim/s rfail/s "
	       "out: dsc/s nrt/s_ipv6_");
}

static int
IPv6line(struct sstat *ss, struct tstat *ts, struct tstat **ps, int nactproc,
        time_t deltasec, time_t deltatic, time_t hz,
        int osvers, int osrel, int ossub, char *tstamp,
        int ppres,  int ntrun, int ntslpi, int ntslpu, int ntidle,
	int pexit, int pzombie)
{
	printf("    %5.1lf %6.1lf %6.1lf %6.1lf %7.1lf %7.1lf  "
	       "    %5.1lf %5.1lf\n", 
		(double)ss->net.ipv6.Ip6InDiscards      / deltasec,
		(double)ss->net.ipv6.Ip6InHdrErrors     / deltasec,
		(double)ss->net.ipv6.Ip6InAddrErrors    / deltasec,
		(double)ss->net.ipv6.Ip6InUnknownProtos / deltasec,
		(double)ss->net.ipv6.Ip6ReasmTimeout    / deltasec,
		(double)ss->net.ipv6.Ip6ReasmFails      / deltasec,
		(double)ss->net.ipv6.Ip6OutDiscards  / deltasec,
		(double)ss->net.ipv6.Ip6OutNoRoutes     / deltasec);
	return 1;
}

/*
** ICMP version 6 statistics
*/
static void
icmpv6head(int osvers, int osrel, int ossub)
{
	printf("intot/s outtot/s inerr/s innsol/s innadv/s "
	       "otnsol/s otnadv/s  _icmp6_"   );
}

static int
icmpv6line(struct sstat *ss, struct tstat *ts, struct tstat **ps, int nactproc,
        time_t deltasec, time_t deltatic, time_t hz,
        int osvers, int osrel, int ossub, char *tstamp,
        int ppres,  int ntrun, int ntslpi, int ntslpu, int ntidle,
	int pexit, int pzombie)
{
	printf("%7.1lf %8.1lf %7.2lf %8.2lf %8.2lf %8.2lf %8.2lf\n", 
		(double)ss->net.icmpv6.Icmp6InMsgs                  / deltasec, 
		(double)ss->net.icmpv6.Icmp6OutMsgs                 / deltasec, 
		(double)ss->net.icmpv6.Icmp6InErrors                / deltasec, 
		(double)ss->net.icmpv6.Icmp6InNeighborSolicits      / deltasec, 
		(double)ss->net.icmpv6.Icmp6InNeighborAdvertisements/ deltasec, 
		(double)ss->net.icmpv6.Icmp6OutNeighborSolicits     / deltasec, 
		(double)ss->net.icmpv6.Icmp6OutNeighborAdvertisements
								/deltasec);
	return 1;
}

static void
ICMPv6head(int osvers, int osrel, int ossub)
{
	printf("iecho/s ierep/s oerep/s idu/s odu/s ird/s ord/s ite/s "
	       "ote/s  _icmpv6_");
}

static int
ICMPv6line(struct sstat *ss, struct tstat *ts, struct tstat **ps, int nactproc,
        time_t deltasec, time_t deltatic, time_t hz,
        int osvers, int osrel, int ossub, char *tstamp,
        int ppres,  int ntrun, int ntslpi, int ntslpu, int ntidle,
	int pexit, int pzombie)
{
	printf("%7.2lf %7.2lf %7.2lf %5.2lf %5.2lf "
	       "%5.2lf %5.2lf %5.2lf %5.2lf\n", 
		(double)ss->net.icmpv6.Icmp6InEchos         / deltasec,
		(double)ss->net.icmpv6.Icmp6InEchoReplies   / deltasec,
		(double)ss->net.icmpv6.Icmp6OutEchoReplies  / deltasec,
		(double)ss->net.icmpv6.Icmp6InDestUnreachs  / deltasec,
		(double)ss->net.icmpv6.Icmp6OutDestUnreachs / deltasec,
		(double)ss->net.icmpv6.Icmp6InRedirects     / deltasec,
		(double)ss->net.icmpv6.Icmp6OutRedirects    / deltasec,
		(double)ss->net.icmpv6.Icmp6InTimeExcds     / deltasec,
		(double)ss->net.icmpv6.Icmp6OutTimeExcds    / deltasec);
	return 1;
}

/*
** UDP version 6 statistics
*/
static void
udpv6head(int osvers, int osrel, int ossub)
{
	printf("indgram/s outdgram/s   inerr/s  noport/s    "
	       "                  _udpv6_");
}

static int
udpv6line(struct sstat *ss, struct tstat *ts, struct tstat **ps, int nactproc,
        time_t deltasec, time_t deltatic, time_t hz,
        int osvers, int osrel, int ossub, char *tstamp,
        int ppres,  int ntrun, int ntslpi, int ntslpu, int ntidle,
	int pexit, int pzombie)
{
	printf("%9.1lf %10.1lf   %7.2lf %9.2lf\n",
		(double)ss->net.udpv6.Udp6InDatagrams  / deltasec,
		(double)ss->net.udpv6.Udp6OutDatagrams / deltasec,
		(double)ss->net.udpv6.Udp6InErrors     / deltasec,
		(double)ss->net.udpv6.Udp6NoPorts      / deltasec);
	return 1;
}

/*
** TCP statistics
*/
static void
tcphead(int osvers, int osrel, int ossub)
{
	printf("insegs/s outsegs/s  actopen/s pasopen/s  "
	       "nowopen                _tcp_");
}

static int
tcpline(struct sstat *ss, struct tstat *ts, struct tstat **ps, int nactproc,
        time_t deltasec, time_t deltatic, time_t hz,
        int osvers, int osrel, int ossub, char *tstamp,
        int ppres,  int ntrun, int ntslpi, int ntslpu, int ntidle,
	int pexit, int pzombie)
{
	printf("%8.1lf %9.1lf  %9.1lf %9.1lf  %7lld\n",
		(double)ss->net.tcp.InSegs       / deltasec,
		(double)ss->net.tcp.OutSegs      / deltasec,
		(double)ss->net.tcp.ActiveOpens  / deltasec,
		(double)ss->net.tcp.PassiveOpens / deltasec,
		        ss->net.tcp.CurrEstab);
	return 1;
}

static void
TCPhead(int osvers, int osrel, int ossub)
{
	printf("inerr/s  retrans/s  attfail/s  "
	       "estabreset/s  outreset/s         _tcp_");
}

static int
TCPline(struct sstat *ss, struct tstat *ts, struct tstat **ps, int nactproc,
        time_t deltasec, time_t deltatic, time_t hz,
        int osvers, int osrel, int ossub, char *tstamp,
        int ppres,  int ntrun, int ntslpi, int ntslpu, int ntidle,
	int pexit, int pzombie)
{
	printf("%7.1lf  %9.1lf  %9.1lf  %12.1lf  %10.1lf\n",
		(double)ss->net.tcp.InErrs       / deltasec,
		(double)ss->net.tcp.RetransSegs  / deltasec,
		(double)ss->net.tcp.AttemptFails / deltasec,
		(double)ss->net.tcp.EstabResets  / deltasec,
		(double)ss->net.tcp.OutRsts      / deltasec);
	return 1;
}

static void
httphead(int osvers, int osrel, int ossub)
{
	printf("requests/s  Kbytes/s  bytes/req    "
	       "idleworkers busyworkers     _http_");
}

static int
httpline(struct sstat *ss, struct tstat *ts, struct tstat **ps, int nactproc,
        time_t deltasec, time_t deltatic, time_t hz,
        int osvers, int osrel, int ossub, char *tstamp,
        int ppres,  int ntrun, int ntslpi, int ntslpu, int ntidle,
	int pexit, int pzombie)
{
	printf("%10.2lf  %8.2lf  %9.2lf    %11d %11d\n",
		(double)ss->www.accesses      / deltasec,
		(double)ss->www.totkbytes     / deltasec,
		ss->www.accesses ? 
		    (double)ss->www.totkbytes*1024/ss->www.accesses : 0,
		        ss->www.iworkers,
		        ss->www.bworkers);

	return 1;
}

/*
** per-process statistics: top-3 processor consumers
*/
static void
topchead(int osvers, int osrel, int ossub)
{
	printf("  pid command  cpu%% |   pid command  cpu%% | "
	       "  pid command  cpu%%_top3_");
}

static int
topcline(struct sstat *ss, struct tstat *ts, struct tstat **ps, int nactproc,
        time_t deltasec, time_t deltatic, time_t hz,
        int osvers, int osrel, int ossub, char *tstamp,
        int ppres,  int ntrun, int ntslpi, int ntslpu, int ntidle,
	int pexit, int pzombie)
{
	count_t	availcpu;

	if (!ts)
	{
		printf("report not available.....\n");
		return 0;
	}

	/*
	** sort process list in cpu order
	*/
	qsort(ps, nactproc, sizeof(struct tstat *), compcpu);

	availcpu  = ss->cpu.all.stime + ss->cpu.all.utime +
	            ss->cpu.all.ntime + ss->cpu.all.itime +
	            ss->cpu.all.wtime + ss->cpu.all.Itime +
		    ss->cpu.all.Stime + ss->cpu.all.steal;

	availcpu /= ss->cpu.nrcpu;

	if (availcpu == 0)
		availcpu = 1;	/* avoid divide-by-zero */

	if (nactproc >= 1 && (ps[0])->cpu.stime + (ps[0])->cpu.utime > 0)
	    printf("%5d %-8.8s %3.0lf%% | ",
	      (ps[0])->gen.pid, (ps[0])->gen.name,
	      (double)((ps[0])->cpu.stime + (ps[0])->cpu.utime)*100.0/availcpu);
        else
	    printf("%19s | ", " ");

	if (nactproc >= 2 && (ps[1])->cpu.stime + (ps[1])->cpu.utime > 0)
	    printf("%5d %-8.8s %3.0lf%% | ",
	      (ps[1])->gen.pid, (ps[1])->gen.name,
	      (double)((ps[1])->cpu.stime + (ps[1])->cpu.utime)*100.0/availcpu);
        else
	    printf("%19s | ", " ");

	if (nactproc >= 3 && (ps[2])->cpu.stime + (ps[2])->cpu.utime > 0)
	    printf("%5d %-8.8s %3.0lf%%\n",
	      (ps[2])->gen.pid, (ps[2])->gen.name,
	      (double)((ps[2])->cpu.stime + (ps[2])->cpu.utime)*100.0/availcpu);
        else
	    printf("%19s\n", " ");


	return 1;
}

/*
** per-process statistics: top-3 memory consumers
*/
static void
topmhead(int osvers, int osrel, int ossub)
{
	printf("  pid command  mem%% |   pid command  mem%% | "
	       "  pid command  mem%%_top3_");
}

static int
topmline(struct sstat *ss, struct tstat *ts, struct tstat **ps, int nactproc,
        time_t deltasec, time_t deltatic, time_t hz,
        int osvers, int osrel, int ossub, char *tstamp,
        int ppres,  int ntrun, int ntslpi, int ntslpu, int ntidle,
	int pexit, int pzombie)
{
	count_t		availmem;

	if (!ts)
	{
		printf("report not available.....\n");
		return 0;
	}

	/*
	** sort process list in memory order
	*/
	qsort(ps, nactproc, sizeof(struct tstat *), compmem);

	availmem  = ss->mem.physmem;

        if (nactproc >= 1)
	    printf("%5d %-8.8s %3.0lf%% | ",
	      (ps[0])->gen.pid, (ps[0])->gen.name,
	      (double)(ps[0])->mem.rmem * 100.0 / availmem);
        else
	    printf("%19s | ", " ");

        if (nactproc >= 2)
	    printf("%5d %-8.8s %3.0lf%% | ",
	      (ps[1])->gen.pid, (ps[1])->gen.name,
	      (double)(ps[1])->mem.rmem * 100.0 / availmem);
        else
	    printf("%19s | ", " ");

        if (nactproc >= 3)
	    printf("%5d %-8.8s %3.0lf%%\n",
	      (ps[2])->gen.pid, (ps[2])->gen.name,
	      (double)(ps[2])->mem.rmem * 100.0 / availmem);
        else
	    printf("%19s\n", " ");


	return 1;
}

/*
** per-process statistics: top-3 disk consumers
*/
static void
topdhead(int osvers, int osrel, int ossub)
{
	printf("  pid command  dsk%% |   pid command  dsk%% | "
	       "  pid command  dsk%%_top3_");
}

static int
topdline(struct sstat *ss, struct tstat *ts, struct tstat **ps, int nactproc,
        time_t deltasec, time_t deltatic, time_t hz,
        int osvers, int osrel, int ossub, char *tstamp,
        int ppres,  int ntrun, int ntslpi, int ntslpu, int ntidle,
	int pexit, int pzombie)
{
	int		i;
	count_t		availdsk;

	if (!ts)
	{
		printf("report not available.....\n");
		return 0;
	}

	if ( !(supportflags & IOSTAT) )
	{
		printf("no per-process disk counters available.....\n");
		return 0;
	}

	/*
	** determine total disk accesses for all processes
	*/
	for (i=0, availdsk=0; i < nactproc; i++)
	{
		availdsk += (ps[i])->dsk.rio + (ps[i])->dsk.wio;
	}

	if (availdsk == 0)
		availdsk = 1;

	/*
	** sort process list in disk order
	*/
	qsort(ps, nactproc, sizeof(struct tstat *), compdsk);

        if (nactproc >= 1 && (ps[0])->dsk.rio + (ps[0])->dsk.wio > 0)
	    printf("%5d %-8.8s %3.0lf%% | ",
	      (ps[0])->gen.pid, (ps[0])->gen.name,
	      (double)((ps[0])->dsk.rio+(ps[0])->dsk.wio) *100.0/availdsk);
        else
	    printf("%19s | ", " ");

        if (nactproc >= 2 && (ps[1])->dsk.rio + (ps[1])->dsk.wio > 0)
	    printf("%5d %-8.8s %3.0lf%% | ",
	      (ps[1])->gen.pid, (ps[1])->gen.name,
	      (double)((ps[1])->dsk.rio+(ps[1])->dsk.wio) *100.0/availdsk);
        else
	    printf("%19s | ", " ");

        if (nactproc >= 3 && (ps[2])->dsk.rio + (ps[2])->dsk.wio > 0)
	    printf("%5d %-8.8s %3.0lf%%\n",
	      (ps[2])->gen.pid, (ps[2])->gen.name,
	      (double)((ps[2])->dsk.rio+(ps[2])->dsk.wio) *100.0/availdsk);
        else
	    printf("%19s\n", " ");


	return 1;
}

/*
** per-process statistics: top-3 network consumers
*/
static void
topnhead(int osvers, int osrel, int ossub)
{
	printf("  pid command  net%% |   pid command  net%% | "
	       "  pid command  net%%_top3_");
}

static int
topnline(struct sstat *ss, struct tstat *ts, struct tstat **ps, int nactproc,
        time_t deltasec, time_t deltatic, time_t hz,
        int osvers, int osrel, int ossub, char *tstamp,
        int ppres,  int ntrun, int ntslpi, int ntslpu, int ntidle,
	int pexit, int pzombie)
{
	int		i;
	count_t		availnet;
	count_t		totbytes;

	if (!ts)
	{
		printf("report not available.....\n");
		return 0;
	}

	if ( !(supportflags & NETATOP || supportflags & NETATOPBPF) )
	{
		printf("no per-process network counters available.....\n");
		return 0;
	}

	/*
	** determine total network accesses for all processes
	*/
	for (i=0, availnet=0; i < nactproc; i++)
	{
		availnet += (*(ps+i))->net.tcpssz + (*(ps+i))->net.tcprsz +
		            (*(ps+i))->net.udpssz + (*(ps+i))->net.udprsz;
	}

	if (availnet == 0)
		availnet = 1;

	/*
	** sort process list in network order
	*/
	qsort(ps, nactproc, sizeof(struct tstat *), compnet);

        if (nactproc >= 1)
	{
		totbytes = (ps[0])->net.tcpssz + (ps[0])->net.tcprsz +
		           (ps[0])->net.udpssz + (ps[0])->net.udprsz;

		if (totbytes > 0)
			printf("%5d %-8.8s %3.0lf%% | ",
				(ps[0])->gen.pid, (ps[0])->gen.name,
				(double)totbytes * 100.0 / availnet);
        	else
			printf("%19s | ", " ");
	}
        else
		printf("%19s | ", " ");

        if (nactproc >= 2)
	{
		totbytes = (ps[1])->net.tcpssz + (ps[1])->net.tcprsz +
		           (ps[1])->net.udpssz + (ps[1])->net.udprsz;

		if (totbytes > 0)
			printf("%5d %-8.8s %3.0lf%% | ",
				(ps[1])->gen.pid, (ps[1])->gen.name,
				(double)totbytes * 100.0 / availnet);
        	else
			printf("%19s | ", " ");
	}
        else
		printf("%19s | ", " ");

        if (nactproc >= 3)
	{
		totbytes = (ps[2])->net.tcpssz + (ps[2])->net.tcprsz +
		           (ps[2])->net.udpssz + (ps[2])->net.udprsz;

		if (totbytes > 0)
			printf("%5d %-8.8s %3.0lf%%\n",
				(ps[2])->gen.pid, (ps[2])->gen.name,
				(double)totbytes * 100.0 / availnet);
        	else
	    		printf("%19s\n", " ");
	}
        else
		printf("%19s\n", " ");


	return 1;
}

/*********************************************************************/
/* Function definition table.                                        */
/*                                                                   */
/* The layout of this table is as follows:                           */
/*     Column 1:                                                     */
/*        Boolean which indicates if the specified function is       */
/*        active during a run of 'atopsar'. When started,            */
/*        this boolean will be defined 'true' for all entries for    */
/*        which the command-line flag has been specified. Initially  */
/*        this column should contain 0 (false), unless this function */
/*        is always required.                                        */
/*        If no flags are specified for 'atopsar', the first entry   */
/*        in this table is defined active (default flag).            */
/*                                                                   */
/*     Column 2:                                                     */
/*        Categories of counters used by this function.              */
/*           c = cpu  counters,    m = memory  counters,             */
/*           d = disk counters,    n = network counters              */
/*                                                                   */
/*     Column 3:                                                     */
/*        Flag which can be used as command-line argument to         */
/*        select the function defined in this table-entry. Be sure   */
/*        that a unique character is choosen.                        */
/*        Notice that certain flags are reserved!                    */
/*                                                                   */
/*     Column 4:                                                     */
/*        Entry-point of the 'printhead' function.                   */
/*                                                                   */
/*     Column 5:                                                     */
/*        Entry-point of the 'printline' function.                   */
/*                                                                   */
/*     Column 6:                                                     */
/*        Information about the statistics shown by the function     */
/*        specified by the table-entry. This text is printed as      */
/*        command-usage.                                             */
/*********************************************************************/
struct pridef pridef[] =
{
   {0,  "c",  'c',  cpuhead,	cpuline,  	"cpu utilization",        },
   {0,  "c",  'p',  prochead,	procline,  	"process(or) load",       },
   {0,  "c",  'P',  taskhead,	taskline,  	"processes & threads",    },
   {0,  "c",  'g',  gpuhead,	gpuline,  	"gpu utilization",        },
   {0,  "m",  'm',  memhead,	memline,	"memory & swapspace",     },
   {0,  "m",  's',  swaphead,	swapline,	"swap rate",              },
   {0,  "cmd",'B',  psihead,	psiline,	"pressure stall info (PSI)",},
   {0,  "cd", 'l',  lvmhead,	lvmline,	"logical volume activity", },
   {0,  "cd", 'f',  mddhead,	mddline,	"multiple device activity",},
   {0,  "cd", 'd',  dskhead,	dskline,	"disk activity",          },
   {0,  "n",  'h',  ibhead,	ibline,		"infiniband utilization", },
   {0,  "n",  'n',  nfmhead,	nfmline,	"NFS client mounts",      },
   {0,  "n",  'j',  nfchead,	nfcline,	"NFS client activity",    },
   {0,  "n",  'J',  nfshead,	nfsline,	"NFS server activity",    },
   {0,  "n",  'i',  ifhead,	ifline,		"net-interf (general)",   },
   {0,  "n",  'I',  IFhead,	IFline,		"net-interf (errors)",    },
   {0,  "n",  'w',  ipv4head,	ipv4line,	"ip   v4    (general)",   },
   {0,  "n",  'W',  IPv4head,	IPv4line,	"ip   v4    (errors)",    },
   {0,  "n",  'y',  icmpv4head,	icmpv4line,	"icmp v4    (general)",   },
   {0,  "n",  'Y',  ICMPv4head,	ICMPv4line,	"icmp v4    (per type)",  },
   {0,  "n",  'u',  udpv4head,	udpv4line,  	"udp  v4",                },
   {0,  "n",  'z',  ipv6head,	ipv6line,	"ip   v6    (general)",   },
   {0,  "n",  'Z',  IPv6head,	IPv6line,	"ip   v6    (errors)",    },
   {0,  "n",  'k',  icmpv6head,	icmpv6line,	"icmp v6    (general)",   },
   {0,  "n",  'K',  ICMPv6head,	ICMPv6line,	"icmp v6    (per type)",  },
   {0,  "n",  'U',  udpv6head,	udpv6line,  	"udp  v6",                },
   {0,  "n",  't',  tcphead,	tcpline,  	"tcp        (general)",   },
   {0,  "n",  'T',  TCPhead,	TCPline,  	"tcp        (errors)",    },
   {0,  "n",  'o',  httphead,	httpline,  	"HTTP activity",          },
   {0,  "",   'O',  topchead,	topcline,  	"top-3 processes cpu",    },
   {0,  "",   'G',  topmhead,	topmline,  	"top-3 processes memory", },
   {0,  "",   'D',  topdhead,	topdline,  	"top-3 processes disk",   },
   {0,  "",   'N',  topnhead,	topnline,  	"top-3 processes network",},
};

int	pricnt = sizeof(pridef)/sizeof(struct pridef);
