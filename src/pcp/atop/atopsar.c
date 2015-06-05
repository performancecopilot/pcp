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
** Copyright (C) 2015 Red Hat.
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
#include <pcp/impl.h>
#include <sys/ioctl.h>

#include "atop.h"
#include "ifprop.h"
#include "photosyst.h"
#include "photoproc.h"

#define	MAXFL	64      /* maximum number of command-line flags  */


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
static unsigned int 	nsamples = 9999999;
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
	void    (*prihead)();   /* print header of list                   */
	int     (*priline)(struct sstat *, struct tstat *, struct tstat **,
		           int, time_t, time_t, time_t,
		           int, int, int, char *,
        	           int, int, int, int, int, int);
		                /* print counters per line (excl. time)   */
	char    *about;         /* statistics about what                  */
};

extern struct pridef	pridef[];      /* table of print-functions        */
extern int		pricnt;	       /* total number of print-functions */

static time_t		daylim;        /* last second of day in epoch     */
static int		prinow;        /* current selection               */
static char 		coloron;       /* boolean: colors active now      */

/*
** local prototypes
*/
static void	engine(void);
static void	pratopsaruse(char *);
static void	reportlive(double, double, struct sstat *);
static char	reportraw (double, double,
		           struct sstat *, struct tstat *, struct tstat **,
		           int, int, int, int, int, int, int, int,
		           int, unsigned int, char);
static void	reportheader(struct sysname *, time_t);
static time_t	daylimit(time_t);

int
atopsar(int argc, char *argv[])
{
	register int	i, c;
	char		*flaglist;
	pmOptions	opts = { 0 };

	usecolors = 't';

	/* 
	** gather all flags for the print-functions
	*/
	flaglist = malloc(pricnt+32);

	ptrverify(flaglist, "Malloc failed for %d flags\n", pricnt+32);

	for (i=0; i < pricnt; i++)
		flaglist[i] = pridef[i].flag;

	flaglist[i] = 0;

	/*
	** add generic flags
	*/
	strcat(flaglist, "b:e:SxCMHr:R:aA");

	opts.short_options = flaglist;
	__pmStartOptions(&opts);

	/* 
	** interpret command-line arguments & flags 
	*/
	if (argc > 1)
	{
		while ((c = pmgetopt_r(argc, argv, &opts)) != EOF)
		{
			switch (c)
			{
			   case '?':		/* usage wanted ?        */
				pratopsaruse(pmProgname);
				break;

                           case 'b':		/* begin time ?          */
				opts.start_optarg = opts.optarg;
				break;

                           case 'e':		/* end   time ?          */
				opts.finish_optarg = opts.optarg;
				break;

			   case 'r':		/* reading of file data ? */
				__pmAddOptArchiveFolio(&opts, opts.optarg);
				rawreadflag++;
				break;

			   case 'R':		/* summarize samples */
				if (!numeric(opts.optarg))
					pratopsaruse(pmProgname);

				summarycnt = atoi(opts.optarg);

				if (summarycnt < 1)
					pratopsaruse(pmProgname);
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

				if (isatty(1))
				{
					struct winsize wsz;

					if ( ioctl(1, TIOCGWINSZ, &wsz) != -1)
						repeathead = wsz.ws_row - 1;
				}
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
					pratopsaruse(argv[0]);
			}
		}

		/*
		** get optional interval-value and
		** optional number of samples	
		*/
		if (opts.optind < argc && opts.optind < MAXFL)
		{
			char	*endnum, *arg;

			if (rawreadflag)
				pratopsaruse(pmProgname);

			arg = argv[opts.optind++];
			if (!numeric(arg))
				pratopsaruse(pmProgname);
	
			if (pmParseInterval(arg, &opts.interval, &endnum) < 0)
			{
				pmprintf(
			"%s: %s option not in pmParseInterval(3) format:\n%s\n",
					pmProgname, arg, endnum);
				free(endnum);
				opts.errors++;
			}
			else
				interval = opts.interval;
	
			if (opts.optind < argc)
			{
				arg = argv[opts.optind];
				if (!numeric(arg))
					pratopsaruse(pmProgname);
				if ((opts.samples = atoi(arg)) < 1)
					pratopsaruse(pmProgname);
				nsamples = opts.samples;
			}
		}
		else	/* if no interval specified, read from logfile */
		{
			rawreadflag++;
		}
	}
	else	/* if no flags specified at all, read from logfile */
	{
		rawreadflag++;
	}
	if (opts.errors)
		prusage(pmProgname);

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
		if (! isatty(1) )
			usecolors = 0;
	}

	/*
	** check if raw data from a file must be viewed
	*/
	if (rawreadflag)
	{
		/*
		** select own reportraw-function to be called
		** by the rawread function
		*/
		vis.show_samp  = reportraw;

		for (i=0; i < pricnt; i++)
		{
			if ( pridef[i].wanted )
			{
				prinow    = i;
				daylim    = 0;
				// TODO: PMAPI reading
				// begintime = saved_begintime;
				// rawread();
				printf("\n");
			}
		}

		cleanstop(0);
	}

	/*
	** live data must be gathered
	**
	** determine the name of this node (without domain-name)
	** and the kernel-version
	*/
	setup_globals(&opts);

	/*
	** determine properties (like speed) of all interfaces
	*/
	initifprop();

	/*
	** start live reporting
	*/
	engine();

	return 0;
}

/*
** engine() that drives the main-loop for atopsar
*/
static void
engine(void)
{
	struct sigaction 	sigact;
	double			timed;
	double			delta;
	void			getusr1(int);

	/*
	** reserve space for system-level statistics
	*/
	static struct sstat	*cursstat; /* current   */
	static struct sstat	*presstat; /* previous  */
	static struct sstat	*devsstat; /* deviation */
	static struct sstat	*hlpsstat;

	/*
	** initialization: allocate required memory dynamically
	*/
	cursstat = calloc(1, sizeof(struct sstat));
	presstat = calloc(1, sizeof(struct sstat));
	devsstat = calloc(1, sizeof(struct sstat));

	ptrverify(cursstat,  "Malloc failed for current sysstats\n");
	ptrverify(presstat,  "Malloc failed for prev    sysstats\n");
	ptrverify(devsstat,  "Malloc failed for deviate sysstats\n");

	/*
	** install the signal-handler for ALARM and SIGUSR1 (both triggers
	** for the next sample)
	*/
	memset(&sigact, 0, sizeof sigact);
	sigact.sa_handler = getusr1;
	sigaction(SIGUSR1, &sigact, (struct sigaction *)0);

	memset(&sigact, 0, sizeof sigact);
	sigact.sa_handler = getalarm;
	sigaction(SIGALRM, &sigact, (struct sigaction *)0);

	if (interval.tv_sec || interval.tv_usec)
		setalarm(&interval);

	/*
	** print overall report header
	*/
	reportheader(&sysname, time(0));

	/*
	** MAIN-LOOP:
	**    -	Wait for the requested number of seconds or for other trigger
	**
	**    -	System-level counters
	**		get current counters
	**		calculate the differences with the previous sample
	**
	**    -	Call the print-function to visualize the differences
	*/
	for (sampcnt=0; sampcnt < nsamples+1; sampcnt++)
	{
		/*
		** wait for alarm-signal to arrive or
		** wait for SIGUSR1 in case of an interval of 0.
		*/
		if (sampcnt > 0)
			pause();

		/*
		** take a snapshot of the current system-level statistics 
		** and calculate the deviations (i.e. calculate the activity
		** during the last sample)
		*/
		hlpsstat = cursstat;	/* swap current/prev. stats */
		cursstat = presstat;
		presstat = hlpsstat;

		photosyst(cursstat);	/* obtain new counters      */

		pretime  = curtime;	/* timestamp for previous sample */
		curtime  = cursstat->stamp; /* timestamp for this sample */

		deviatsyst(cursstat, presstat, devsstat);

		timed = __pmtimevalToReal(&curtime);
		delta = timed - __pmtimevalToReal(&pretime);

		/*
		** activate the report-function to visualize the deviations
		*/
		reportlive(timed, delta > 1.0 ? delta : 1.0, devsstat);
	} /* end of main-loop */
}

/*
** report function to print a new sample in case of live measurements
*/
static void
reportlive(double curtime, double numsecs, struct sstat *ss)
{
	char			timebuf[16], datebuf[16];
	int			i, nr = numreports, rv;
	static unsigned int	curline, headline;

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
			return;

		printf(datemsg, convdate(curtime, datebuf, sizeof(datebuf)-1));

		for (i=0; i < pricnt && nr > 0; i++)
		{
			if ( !pridef[i].wanted )
				continue;

			nr--;

			/*
			** print header-line
			*/
			if (usecolors)
				printf(COLSETHEAD);

			printf("\n%s  ", convtime(curtime-numsecs, timebuf, sizeof(timebuf)-1));
	
			(pridef[i].prihead)(osvers, osrel, ossub);
	
			if (usecolors)
				printf(COLRESET);

			printf("\n");

			/*
			** print line with statistical counters
			*/
			printf("%s  ", convtime(curtime, timebuf, sizeof(timebuf)-1));
	
			if ( !(pridef[i].priline)(ss, (struct tstat *)0, 0, 0,
				numsecs, numsecs*hertz, hertz,
				osvers, osrel, ossub,
				stampalways ? timebuf : "        ",
	                        0, 0, 0, 0, 0, 0) )
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
		if (curtime > daylim)
		{
			printf(datemsg, convdate(curtime, datebuf, sizeof(datebuf)-1));
			daylim = daylimit(curtime);
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
			if (usecolors)
				printf(COLSETHEAD);

			printf("\n%s  ", convtime(curtime, timebuf, sizeof(timebuf)-1));
	
			(pridef[i].prihead)(osvers, osrel, ossub);

			if (usecolors)
				printf(COLRESET);

			printf("\n");

			curline+=2;

			headline = repeathead;
			return;
		}

		/*
		** print line with statistical counters
		*/
		printf("%s  ", convtime(curtime, timebuf, sizeof(timebuf)-1));
	
		if ( !(rv = (pridef[i].priline)(ss, (struct tstat *)0, 0, 0,
					numsecs, numsecs*hertz, hertz,
					osvers, osrel, ossub, 
		                        stampalways ? timebuf : "        ",
	                        	0, 0, 0, 0, 0, 0) ) )
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
			if (usecolors)
				printf(COLSETHEAD);

			printf("\n%s  ", convtime(curtime, timebuf, sizeof(timebuf)-1));
	
			(pridef[i].prihead)(osvers, osrel, ossub);

			if (usecolors)
				printf(COLRESET);

			printf("\n");

			curline+=2;
		}
	}
}

/*
** report function to print a new sample in case of logged measurements
*/
static char
reportraw(double curtime, double numsecs,
         	struct sstat *ss, struct tstat *ts, struct tstat **proclist,
         	int ndeviat, int ntask, int nactproc,
		int totproc, int totrun, int totslpi, int totslpu, int totzomb,
		int nexit, unsigned int noverflow, char flags)
{
	static char		firstcall = 1;
	char			timebuf[16], datebuf[16];
	double			timed;
	unsigned int		rv;
	static unsigned int	curline, headline, sampsum,
				totalsec, totalexit, lastnpres,
				lastntrun, lastntslpi, lastntslpu, lastnzomb;
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
	if (curtime > daylim)
	{
		printf(datemsg, convdate(curtime, datebuf, sizeof(datebuf)-1));
		daylim = daylimit(curtime);
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
		__pmtimevalFromReal(curtime, &pretime);

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
		if (usecolors)
			printf(COLSETHEAD);

		timed = __pmtimevalToReal(&pretime);
		printf("\n%s  ", convtime(timed, timebuf, sizeof(timebuf)-1));

		(pridef[prinow].prihead)(osvers, osrel, ossub);

		if (usecolors)
			printf(COLRESET);

		printf("\n");

		curline+=2;
	}

	/*
	** when current record contains log-restart indicator,
	** print message and reinitialize variables
	*/
	if (flags & RRBOOT)
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
			        osvers, osrel, ossub,
		                stampalways ? timebuf : "        ",
	                        lastnpres, lastntrun, lastntslpi, lastntslpu,
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
		printf("%s  ", convtime(curtime, timebuf, sizeof(timebuf)-1));

		printf("......................... logging restarted "
		       ".........................\n");

		__pmtimevalFromReal(curtime, &pretime);
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
		printf("%s  ", convtime(curtime, timebuf, sizeof(timebuf)-1));

		rv = (pridef[prinow].priline) (ss, ts, proclist, nactproc,
				numsecs, numsecs*hertz, hertz,
				osvers, osrel, ossub,
	               		stampalways ? timebuf : "        ",
				ndeviat, totrun, totslpi, totslpu,
				nexit, totzomb);

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
			totalsyst(*cp, ss, &totsyst);
			cp++;
		}

		totalsec  += numsecs;
		totalexit += nexit;

		/*
		** remember some values in case the next record
		** contains the log-restart indicator
		*/
		lasttime   = curtime;
		lastnpres  = totproc;
		lastntrun  = totrun;
		lastntslpi = totslpi;
		lastntslpu = totslpu;
		lastnzomb  = totzomb;

		/*
		** print line only if needed
		*/
		if (sampcnt >= sampsum || ( (flags&RRLAST) && totalsec) )
		{
			/*
			** print output line for required report
			*/
			printf("%s  ", convtime(curtime, timebuf, sizeof(timebuf)-1));

			rv = (pridef[prinow].priline) (&totsyst,
					(struct tstat *)0, 0, 0,
					totalsec, totalsec*hertz, hertz,
					osvers, osrel, ossub,
					stampalways ? timebuf : "        ",
					ndeviat, totrun, totslpi, totslpu,
					totalexit, totzomb);

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

	__pmtimevalFromReal(curtime, &pretime);

	return '\0';
}

/*
** print overall header
*/
static void
reportheader(struct sysname *sysname, time_t mtime)
{
        char            cdate[16];

        printf("\n%s  %s  %s  %s  %s\n\n",
                sysname->nodename,
                sysname->release,
                sysname->version,
                sysname->machine,
        	convdate(mtime, cdate, sizeof(cdate)-1));
}

/*
** print usage of atopsar command
*/
void
pratopsaruse(char *myname)
{
	int	i;

	fprintf(stderr,
		"Usage: %s [-flags] [-r file|date] [-R cnt] [-b hh:mm] [-e hh:mm]\n",
								myname);
	fprintf(stderr, "\t\tor\n");
	fprintf(stderr,
		"Usage: %s [-flags] interval [samples]\n", myname);
	fprintf(stderr, "\n");
	fprintf(stderr,
		"\tToday's atop logfile is used by default!\n");
	fprintf(stderr, "\n");
	fprintf(stderr,
		"\tGeneric flags:\n");
	fprintf(stderr,
		"\t  -r  read statistical data from specific atop logfile\n");
	fprintf(stderr,
		"\t      (pathname, or date in format YYYYMMDD, or y[y..])\n");
	fprintf(stderr,
		"\t  -R  summarize <cnt> samples into one sample\n");
	fprintf(stderr,
		"\t  -b  begin  showing data from  specified time\n");
	fprintf(stderr,
		"\t  -e  finish showing data after specified time\n");
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

	fprintf(stderr, "\n");
	fprintf(stderr,
                "Please refer to the man-page of 'atopsar' "
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
	struct tm  *tp = localtime(&timval);

	tp->tm_hour = 23;
	tp->tm_min  = 59;
	tp->tm_sec  = 59;

	return mktime(tp);
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
** function the handle the default flags for atopsar as
** read from the file ~/.atoprc
*/
void
do_atopsarflags(char *val)
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

			if (isatty(1))
			{
				struct winsize wsz;

				if ( ioctl(1, TIOCGWINSZ, &wsz) != -1)
					repeathead = wsz.ws_row - 1;
			}
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
        int ppres,  int ntrun, int ntslpi, int ntslpu, int pexit, int pzombie)
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
        int ppres,  int ntrun, int ntslpi, int ntslpu, int pexit, int pzombie)
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
	printf("clones/s pexit/s  curproc curzomb    thrrun thrslpi thrslpu "
	       "_procthr_");
}

static int
taskline(struct sstat *ss, struct tstat *ts, struct tstat **ps, int nactproc,
        time_t deltasec, time_t deltatic, time_t hz,
        int osvers, int osrel, int ossub, char *tstamp,
        int ppres,  int ntrun, int ntslpi, int ntslpu, int pexit, int pzombie)
{
	if (ppres == 0)
	{
		printf("report not available for live measurements.....\n");
		return 0;
	}

	if (ts)		/* process statistics available */
	{
		printf("%8.2lf %7.2lf  %7d %7d    %6d %7d %7d\n",
			(double)ss->cpu.nprocs / deltasec,
			(double)pexit          / deltasec,
			nactproc-pexit, pzombie, ntrun, ntslpi, ntslpu);
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
        int ppres,  int ntrun, int ntslpi, int ntslpu, int pexit, int pzombie)
{
	unsigned int	mbadness, sbadness;

	if (membadness)
		mbadness = ((ss->mem.physmem  - ss->mem.freemem -
	                     ss->mem.cachemem - ss->mem.buffermem)
	                               * 100.0 / ss->mem.physmem) 
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
	printf("pagescan/s   swapin/s  swapout/s      "
	       "  commitspc  commitlim   _swap_");
}

static int
swapline(struct sstat *ss, struct tstat *ts, struct tstat **ps, int nactproc,
        time_t deltasec, time_t deltatic, time_t hz,
        int osvers, int osrel, int ossub, char *tstamp,
        int ppres,  int ntrun, int ntslpi, int ntslpu, int pexit, int pzombie)
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

	printf("%10.2lf  %9.2lf  %9.2lf       %9lluM %9lluM",
		(double)ss->mem.pgscans / deltasec,
		(double)ss->mem.swins   / deltasec,
		(double)ss->mem.swouts  / deltasec,
		        ss->mem.committed / 1024,
		        ss->mem.commitlim / 1024);

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

		if ( (iotot = dp->nread + dp->nwrite) == 0 &&
 		     !firstcall && !allresources             )
			continue;	/* no activity on this disk */

		/*
		** disk was active during last interval; print info
		*/
		if (nlines++)
			printf("%s  ", tstamp);

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
		       "%5.1lf %6.2lf ms",
		    	pn,
			mstot ? (double)dp->io_ms  *  100.0 / mstot   : 0.0,
			mstot ? (double)dp->nread  * 1000.0 / mstot   : 0.0,
			dp->nread  ?
			        (double)dp->nrsect / dp->nread / 2.0  : 0.0,
			mstot ? (double)dp->nwrite * 1000.0 / mstot   : 0.0,
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
        int ppres,  int ntrun, int ntslpi, int ntslpu, int pexit, int pzombie)
{
	return gendskline(ss, tstamp, 'l');
}

static int
mddline(struct sstat *ss, struct tstat *ts, struct tstat **ps, int nactproc,
        time_t deltasec, time_t deltatic, time_t hz,
        int osvers, int osrel, int ossub, char *tstamp,
        int ppres,  int ntrun, int ntslpi, int ntslpu, int pexit, int pzombie)
{
	return gendskline(ss, tstamp, 'm');
}

static int
dskline(struct sstat *ss, struct tstat *ts, struct tstat **ps, int nactproc,
        time_t deltasec, time_t deltatic, time_t hz,
        int osvers, int osrel, int ossub, char *tstamp,
        int ppres,  int ntrun, int ntslpi, int ntslpu, int pexit, int pzombie)
{
	return gendskline(ss, tstamp, 'd');
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
        int ppres,  int ntrun, int ntslpi, int ntslpu, int pexit, int pzombie)
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

			snprintf(busyval, sizeof busyval,
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
		       "%5lld %5lld %7ld %c", 
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
        int ppres,  int ntrun, int ntslpi, int ntslpu, int pexit, int pzombie)
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
        int ppres,  int ntrun, int ntslpi, int ntslpu, int pexit, int pzombie)
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
        int ppres,  int ntrun, int ntslpi, int ntslpu, int pexit, int pzombie)
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
        int ppres,  int ntrun, int ntslpi, int ntslpu, int pexit, int pzombie)
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
        int ppres,  int ntrun, int ntslpi, int ntslpu, int pexit, int pzombie)
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
        int ppres,  int ntrun, int ntslpi, int ntslpu, int pexit, int pzombie)
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
        int ppres,  int ntrun, int ntslpi, int ntslpu, int pexit, int pzombie)
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
        int ppres,  int ntrun, int ntslpi, int ntslpu, int pexit, int pzombie)
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
        int ppres,  int ntrun, int ntslpi, int ntslpu, int pexit, int pzombie)
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
        int ppres,  int ntrun, int ntslpi, int ntslpu, int pexit, int pzombie)
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
        int ppres,  int ntrun, int ntslpi, int ntslpu, int pexit, int pzombie)
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
        int ppres,  int ntrun, int ntslpi, int ntslpu, int pexit, int pzombie)
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
        int ppres,  int ntrun, int ntslpi, int ntslpu, int pexit, int pzombie)
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
        int ppres,  int ntrun, int ntslpi, int ntslpu, int pexit, int pzombie)
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
        int ppres,  int ntrun, int ntslpi, int ntslpu, int pexit, int pzombie)
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

	printf("%5d %-8.8s %3.0lf%% | %5d %-8.8s %3.0lf%% | "
	       "%5d %-8.8s %3.0lf%%\n",
	      (ps[0])->gen.pid, (ps[0])->gen.name,
	      (double)((ps[0])->cpu.stime + (ps[0])->cpu.utime)*100.0/availcpu,
	      (ps[1])->gen.pid, (ps[1])->gen.name,
	      (double)((ps[1])->cpu.stime + (ps[1])->cpu.utime)*100.0/availcpu,
	      (ps[2])->gen.pid, (ps[2])->gen.name,
	      (double)((ps[2])->cpu.stime + (ps[2])->cpu.utime)*100.0/availcpu);

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
        int ppres,  int ntrun, int ntslpi, int ntslpu, int pexit, int pzombie)
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

	printf("%5d %-8.8s %3.0lf%% | %5d %-8.8s %3.0lf%% | "
	       "%5d %-8.8s %3.0lf%%\n",
		(ps[0])->gen.pid, (ps[0])->gen.name,
		(double)(ps[0])->mem.rmem * 100.0 / availmem,
		(ps[1])->gen.pid, (ps[1])->gen.name,
		(double)(ps[1])->mem.rmem * 100.0 / availmem,
		(ps[2])->gen.pid, (ps[2])->gen.name,
		(double)(ps[2])->mem.rmem * 100.0 / availmem);

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
        int ppres,  int ntrun, int ntslpi, int ntslpu, int pexit, int pzombie)
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

	printf("%5d %-8.8s %3.0lf%% | %5d %-8.8s %3.0lf%% | "
	       "%5d %-8.8s %3.0lf%%\n",
		(ps[0])->gen.pid, (ps[0])->gen.name,
	     	(double)((ps[0])->dsk.rio+(ps[0])->dsk.wio) *100.0/availdsk,
		(ps[1])->gen.pid, (ps[1])->gen.name,
		(double)((ps[1])->dsk.rio+(ps[1])->dsk.wio) *100.0/availdsk,
		(ps[2])->gen.pid, (ps[2])->gen.name,
		(double)((ps[2])->dsk.rio+(ps[2])->dsk.wio) *100.0/availdsk);

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
        int ppres,  int ntrun, int ntslpi, int ntslpu, int pexit, int pzombie)
{
	int		i;
	count_t		availnet;

	if (!ts)
	{
		printf("report not available.....\n");
		return 0;
	}

	if ( !(supportflags & NETATOP) )
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

	printf("%5d %-8.8s %3.0lf%% | %5d %-8.8s %3.0lf%% | "
	       "%5d %-8.8s %3.0lf%%\n",
		(ps[0])->gen.pid, (ps[0])->gen.name,
		(double)((ps[0])->net.tcpssz + (ps[0])->net.tcprsz +
		         (ps[0])->net.udpssz + (ps[0])->net.udprsz  )
							* 100.0 / availnet,
		(ps[1])->gen.pid, (ps[1])->gen.name,
		(double)((ps[1])->net.tcpssz + (ps[1])->net.tcprsz +
		         (ps[1])->net.udpssz + (ps[1])->net.udprsz  )
							* 100.0 / availnet,
		(ps[2])->gen.pid, (ps[2])->gen.name,
		(double)((ps[2])->net.tcpssz + (ps[2])->net.tcprsz +
		         (ps[2])->net.udpssz + (ps[2])->net.udprsz  )
							* 100.0 / availnet);
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
   {0,  "m",  'm',  memhead,	memline,	"memory & swapspace",     },
   {0,  "m",  's',  swaphead,	swapline,	"swap rate",              },
   {0,  "cd", 'l',  lvmhead,	lvmline,	"logical volume activity", },
   {0,  "cd", 'f',  mddhead,	mddline,	"multiple device activity",},
   {0,  "cd", 'd',  dskhead,	dskline,	"disk activity",          },
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
   {0,  "n",  'h',  httphead,	httpline,  	"HTTP activity",          },
   {0,  "",   'O',  topchead,	topcline,  	"top-3 processes cpu",    },
   {0,  "",   'G',  topmhead,	topmline,  	"top-3 processes memory", },
   {0,  "",   'D',  topdhead,	topdline,  	"top-3 processes disk",   },
   {0,  "",   'N',  topnhead,	topnline,  	"top-3 processes network",},
};

int	pricnt = sizeof(pridef)/sizeof(struct pridef);
