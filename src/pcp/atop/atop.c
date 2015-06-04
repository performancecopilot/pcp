/*
** ATOP - System & Process Monitor
**
** The program 'atop' offers the possibility to view the activity of 
** the system on system-level as well as process-level.
**
** This source-file contains the main-function, which verifies the
** calling-parameters and takes care of initialization. 
** The engine-function drives the main sample-loop in which after the
** indicated interval-time a snapshot is taken of the system-level and
** process-level counters and the deviations are calculated and
** visualized for the user.
** 
** Copyright (C) 2000-2012 Gerlof Langeveld
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
**
** After initialization, the main-function calls the ENGINE.
** For every cycle (so after another interval) the ENGINE calls various 
** functions as shown below:
**
** +---------------------------------------------------------------------+
** |                           E  N  G  I  N  E                          |
** |                                                                     |
** |                                                                     |
** |    _____________________await interval-timer_____________________   |
** |   |                                                              ^  |
** |   |      ________       ________      ________      ________     |  |
** |   |     ^        |     ^        |    ^        |    ^        |    |  |
** +---|-----|--------|-----|--------|----|--------|----|--------|----|--+
**     |     |        |     |        |    |        |    |        |    |
**  +--V-----|--+  +--V-----|--+  +--V----|--+  +--V----|--+  +--V----|-+  
**  |           |  |           |  |          |  |          |  |         |
**  | photosyst |  | photoproc |  |   acct   |  | deviate  |  |  print  |
**  |           |  |           |  |photoproc |  |  ...syst |  |         |
**  |           |  |           |  |          |  |  ...proc |  |         |
**  +-----------+  +-----------+  +----------+  +----------+  +---------+  
**        ^              ^             ^              ^            |
**        |              |             |              |            |
**        |              |             |              V            V 
**      ______       _________     __________     ________     _________
**     /      \     /         \   /          \   /        \   /         \
**      /proc          /proc       accounting       task       screen or
**                                    file        database        file
**     \______/     \_________/   \__________/   \________/   \_________/
**
**    -	photosyst()
**	Takes a snapshot of the counters related to resource-usage on
** 	system-level (cpu, disk, memory, network).
**	This code is UNIX-flavor dependent; in case of Linux the counters
**	are retrieved from /proc.
**
**    -	photoproc()
**	Takes a snapshot of the counters related to resource-usage of
**	tasks which are currently active. For this purpose the whole
**	task-list is read.
**	This code is UNIX-flavor dependent; in case of Linux the counters
**	are retrieved from /proc.
**
**    -	acctphotoproc()
**	Takes a snapshot of the counters related to resource-usage of
**	tasks which have been finished during the last interval.
**	For this purpose all new records in the accounting-file are read.
**
** When all counters have been gathered, functions are called to calculate
** the difference between the current counter-values and the counter-values
** of the previous cycle. These functions operate on the system-level
** as well as on the task-level counters. 
** These differences are stored in a new structure(-table). 
**
**    -	deviatsyst()
**	Calculates the differences between the current system-level
** 	counters and the corresponding counters of the previous cycle.
**
**    -	deviattask()
**	Calculates the differences between the current task-level
** 	counters and the corresponding counters of the previous cycle.
**	The per-task counters of the previous cycle are stored in the
**	task-database; this "database" is implemented as a linked list
**	of taskinfo structures in memory (so no disk-accesses needed).
**	Within this linked list hash-buckets are maintained for fast searches.
**	The entire task-database is handled via a set of well-defined 
** 	functions from which the name starts with "pdb_..." (see the
**	source-file procdbase.c).
**	The processes which have been finished during the last cycle
** 	are also treated by deviattask() in order to calculate what their
**	resource-usage was before they finished.
**
** All information is ready to be visualized now.
** There is a structure which holds the start-address of the
** visualization-function to be called. Initially this structure contains
** the address of the generic visualization-function ("generic_samp"), but
** these addresses can be modified in the main-function depending on particular
** flags. In this way various representation-layers (ASCII, graphical, ...)
** can be linked with 'atop'; the one to use can eventually be chosen
** at runtime. 
*/

#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <regex.h>

#include "atop.h"
#include "ifprop.h"
#include "photoproc.h"
#include "photosyst.h"
#include "showgeneric.h"
#include "parseable.h"

#define	allflags  "ab:cde:fghijklmnopqr:stuvw:xyz1ABCDEFGHIJKL:MNOP:QRSTUVWXYZ?"
#define	MAXFL	64      /* maximum number of command-line flags  */

/*
** declaration of global variables
*/
struct sysname	sysname;
int		nodenamelen;
struct timeval	origin;
struct timeval	pretime;	/* timing info				*/
struct timeval	curtime;	/* timing info				*/
struct timeval	interval = { 10, 0 };
unsigned long 	sampcnt;
int		linelen  = 80;
char		screen;
char		acctreason;	/* accounting not active (return val) 	*/
char		rawreadflag;
char		rawwriteflag;
char		*rawname;
char		flaglist[MAXFL];
char		deviatonly = 1;
char      	usecolors  = 1;  /* boolean: colors for high occupation  */
char		threadview = 0;	 /* boolean: show individual threads     */
char      	calcpss    = 0;  /* boolean: read/calculate process PSS  */

unsigned short	hertz;
unsigned int	pagesize;
int 		osrel;
int 		osvers;
int 		ossub;

int		supportflags;	/* supported features             	*/
char		**argvp;

int		fetchmode;
int		fetchstep;


struct visualize vis = {generic_samp, generic_error,
			generic_end,  generic_usage};

/*
** argument values
*/
static char		awaittrigger;	/* boolean: awaiting trigger */
static unsigned int 	nsamples = 0xffffffff;
static char		midnightflag;

/*
** interpretation of defaults-file /etc/atoprc and $HOME/.atop
*/
static void		readrc(char *, int);

void do_flags(char *, char *);
void do_interval(char *, char *);
void do_linelength(char *, char *);
void do_username(char *, char *);
void do_procname(char *, char *);
void do_maxcpu(char *, char *);
void do_maxdisk(char *, char *);
void do_maxmdd(char *, char *);
void do_maxlvm(char *, char *);
void do_maxintf(char *, char *);
void do_colinfo(char *, char *);
void do_colalmost(char *, char *);
void do_colcrit(char *, char *);
void do_colthread(char *, char *);
void do_ownsysprcline(char *, char *);
void do_ownallcpuline(char *, char *);
void do_ownindivcpuline(char *, char *);
void do_owncplline(char *, char *);
void do_ownmemline(char *, char *);
void do_ownswpline(char *, char *);
void do_ownpagline(char *, char *);
void do_owndskline(char *, char *);
void do_ownnettransportline(char *, char *);
void do_ownnetnetline(char *, char *);
void do_ownnetinterfaceline(char *, char *);
void do_ownprocline(char *, char *);
void do_cpucritperc(char *, char *);
void do_memcritperc(char *, char *);
void do_swpcritperc(char *, char *);
void do_dskcritperc(char *, char *);
void do_netcritperc(char *, char *);
void do_swoutcritsec(char *, char *);
void do_almostcrit(char *, char *);
void do_atopsarflags(char *, char *);
void do_pacctdir(char *, char *);

static struct {
	char	*tag;
	void	(*func)(char *, char *);
	int	sysonly;
} manrc[] = {
	{	"flags",		do_flags,		0, },
	{	"interval",		do_interval,		0, },
	{	"linelen",		do_linelength,		0, },
	{	"username",		do_username,		0, },
	{	"procname",		do_procname,		0, },
	{	"maxlinecpu",		do_maxcpu,		0, },
	{	"maxlinedisk",		do_maxdisk,		0, },
	{	"maxlinemdd",		do_maxmdd,		0, },
	{	"maxlinelvm",		do_maxlvm,		0, },
	{	"maxlineintf",		do_maxintf,		0, },
	{	"colorinfo",		do_colinfo,		0, },
	{	"coloralmost",		do_colalmost,		0, },
	{	"colorcritical",	do_colcrit,		0, },
	{	"colorthread",		do_colthread,		0, },
	{	"ownallcpuline",	do_ownallcpuline,	0, },
	{	"ownonecpuline",	do_ownindivcpuline,	0, },
	{	"owncplline",		do_owncplline,		0, },
	{	"ownmemline",		do_ownmemline,		0, },
	{	"ownswpline",		do_ownswpline,		0, },
	{	"ownpagline",		do_ownpagline,		0, },
	{	"owndskline",		do_owndskline,		0, },
	{	"ownnettrline",		do_ownnettransportline,	0, },
	{	"ownnetnetline",	do_ownnetnetline,	0, },
	{	"ownnetifline",	        do_ownnetinterfaceline,	0, },
	{	"ownprocline",		do_ownprocline,		0, },
	{	"ownsysprcline",	do_ownsysprcline,	0, },
	{	"owndskline",	        do_owndskline,		0, },
	{	"cpucritperc",		do_cpucritperc,		0, },
	{	"memcritperc",		do_memcritperc,		0, },
	{	"swpcritperc",		do_swpcritperc,		0, },
	{	"dskcritperc",		do_dskcritperc,		0, },
	{	"netcritperc",		do_netcritperc,		0, },
	{	"swoutcritsec",		do_swoutcritsec,	0, },
	{	"almostcrit",		do_almostcrit,		0, },
	{	"atopsarflags",		do_atopsarflags,	0, },
	{	"pacctdir",		do_pacctdir,		1, },
};

/*
** internal prototypes
*/
static void	engine(void);

int
main(int argc, char *argv[])
{
	register int	i;
	int		c;
	char		*p;
	char		path[MAXPATHLEN];
	pmOptions	opts = {
		.short_options = allflags,
		.flags = PM_OPTFLAG_BOUNDARIES,
	};

	/*
	** preserve command arguments to allow restart of other version
	*/
	argvp = argv;

	/*
	** read defaults-files /etc/atoprc en $HOME/.atoprc (if any)
	*/
	readrc("/etc/atoprc", 1);

	if ( (p = getenv("HOME")) )
	{
		snprintf(path, sizeof(path), "%s/.atoprc", p);
		path[sizeof(path)-1] = '\0';
		readrc(path, 0);
	}

	/*
	** check if we are supposed to behave as 'atopsar'
	** i.e. system statistics only
	*/
	__pmSetProgname(argv[0]);
	if (strcmp(pmProgname, "atopsar") == 0)
		return atopsar(argc, argv);

	__pmStartOptions(&opts);
	if (opts.narchives > 0)
		rawreadflag++;

	/* 
	** interpret command-line arguments & flags 
	*/
	if (argc > 1)
	{
		/* 
		** gather all flags for visualization-functions
		**
		** generic flags will be handled here;
		** unrecognized flags are passed to the print-routines
		*/
		i = 0;

		while (i < MAXFL-1 && (c = pmgetopt_r(argc, argv, &opts)) != EOF)
		{
			switch (c)
			{
			   case '?':		/* usage wanted ?             */
				prusage(pmProgname);
				break;

			   case 'V':		/* version wanted ?           */
				printf("%s\n", getstrvers());
				exit(0);

			   case 'w':		/* writing of raw data ?      */
				rawname = opts.optarg;
				rawwriteflag++;
				break;

			   case 'r':		/* reading of raw data ?      */
				snprintf(path, sizeof(path), "%s/%s.folio",
					opts.optarg, basename(opts.optarg));
				path[sizeof(path)-1] = '\0';
				p = (!access(path, R_OK)) ? path : opts.optarg;
				__pmAddOptArchiveFolio(&opts, p);
				rawreadflag++;
				break;

			   case 'S':		/* midnight limit ?           */
				midnightflag++;
				break;

                           case 'a':		/* all processes per sample ? */
				deviatonly = 0;
				break;

                           case 'R':		/* all processes per sample ? */
				calcpss = 1;
				break;

                           case 'b':		/* begin time ?               */
				opts.start_optarg = opts.optarg;
				break;

                           case 'e':		/* end   time ?               */
				opts.finish_optarg = opts.optarg;
				break;

                           case 'P':		/* parseable output?          */
				if ( !parsedef(opts.optarg) )
					prusage(pmProgname);

				vis.show_samp = parseout;
				break;

                           case 'L':		/* line length                */
				if ( !numeric(opts.optarg) )
					prusage(pmProgname);

				linelen = atoi(opts.optarg);
				break;

			   default:		/* gather other flags */
				flaglist[i++] = c;
			}
		}

		/*
		** get optional interval-value and optional number of samples	
		*/
		if (opts.optind < argc && opts.optind < MAXFL)
		{
			char	*endnum, *arg;

			arg = argv[opts.optind++];
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
					prusage(pmProgname);
				if ((opts.samples = atoi(arg)) < 1)
					prusage(pmProgname);
				nsamples = opts.samples;
			}
		}
	}

	__pmEndOptions(&opts);

	if (opts.errors)
		prusage(pmProgname);

	/*
	** find local host details (no privileged access required)
	*/
	setup_globals(&opts);

	/*
	** check if we are in data recording mode
	*/
	if (rawwriteflag)
	{
		char *source = (opts.nhosts > 0) ? opts.hosts[0] : "local:";
		rawwrite(rawname, source, &interval, nsamples, midnightflag);
		cleanstop(0);
	}

	/*
	** catch signals for proper close-down
	*/
	signal(SIGHUP,  cleanstop);
	signal(SIGTERM, cleanstop);

	/*
	** switch-on the process-accounting mechanism to register the
	** (remaining) resource-usage by processes which have finished
	*/
	acctreason = acctswon();

	/*
	** determine properties (like speed) of all interfaces
	*/
	initifprop();

	/*
 	** open socket to the IP layer to issue getsockopt() calls later on
	*/
	netatop_ipopen();

	/*
	** start the engine now .....
	*/
	engine();

	cleanstop(0);

	return 0;	/* never reached */
}

/*
** The engine() drives the main-loop of the program
*/
static void
engine(void)
{
	int 			i, j;
	struct sigaction 	sigact;
	double 			timed, delta;
	void			getusr1(int), getusr2(int);

	/*
	** reserve space for system-level statistics
	*/
	static struct sstat	*cursstat; /* current   */
	static struct sstat	*presstat; /* previous  */
	static struct sstat	*devsstat; /* deviation */
	static struct sstat	*hlpsstat;

	/*
	** reserve space for task-level statistics
	*/
	static struct tstat	*curtpres;	/* current present list      */
	static int		 curtlen;	/* size of present list      */

	struct tstat		*curpexit;	/* exited process list	     */
	struct tstat		*devtstat;	/* deviation list	     */
	struct tstat		**devpstat;	/* pointers to processes     */
						/* in deviation list         */

	unsigned int		ntaskpres;	/* number of tasks present   */
	unsigned int		nprocexit;	/* number of exited procs    */
	unsigned int		nprocexitnet;	/* number of exited procs    */
						/* via netatopd daemon       */
	unsigned int		ntaskdev;       /* nr of tasks deviated      */
	unsigned int		nprocdev;       /* nr of procs deviated      */
	int			nprocpres;	/* nr of procs present       */
	int			totrun, totslpi, totslpu, totzombie;
	unsigned int		noverflow;

	/*
	** initialization: allocate required memory dynamically
	*/
	cursstat = sstat_alloc("current sysstats");
	presstat = sstat_alloc("prev    sysstats");
	devsstat = sstat_alloc("deviate sysstats");

	curtlen  = PROCMIN * 3 / 2;	/* add 50% for threads */
	curtpres = calloc(curtlen, sizeof(struct tstat));
	ptrverify(curtpres, "Malloc failed for %d procstats\n", curtlen);

	/*
	** install the signal-handler for ALARM, USR1 and USR2 (triggers
	** for the next sample)
	*/
	memset(&sigact, 0, sizeof sigact);
	sigact.sa_handler = getusr1;
	sigaction(SIGUSR1, &sigact, (struct sigaction *)0);

	memset(&sigact, 0, sizeof sigact);
	sigact.sa_handler = getusr2;
	sigaction(SIGUSR2, &sigact, (struct sigaction *)0);

	memset(&sigact, 0, sizeof sigact);
	sigact.sa_handler = getalarm;
	sigaction(SIGALRM, &sigact, (struct sigaction *)0);

	if (interval.tv_sec || interval.tv_usec)
		setalarm(&interval);

	/*
	** MAIN-LOOP:
	**    -	Wait for the requested number of seconds or for other trigger
	**
	**    -	System-level counters
	**		get current counters
	**		calculate the differences with the previous sample
	**
	**    -	Process-level counters
	**		get current counters from running & exited processes
	**		calculate the differences with the previous sample
	**
	**    -	Call the print-function to visualize the differences
	*/
	for (sampcnt=0; sampcnt < nsamples; sampcnt++)
	{
		char	lastcmd;

		/*
		** wait for alarm-signal to arrive (except first sample)
		** or wait for SIGUSR1/SIGUSR2
		*/
		if (sampcnt > 0 && awaittrigger && !rawreadflag)
			pause();

		awaittrigger = 1;

		/*
		** take a snapshot of the current system-level statistics 
		*/
		hlpsstat = cursstat;	/* swap current/prev. stats */
		cursstat = presstat;
		presstat = hlpsstat;

		photosyst(cursstat);	/* obtain new counters     */

		/*
		** take a snapshot of the current task-level statistics 
		**
		** first register active tasks
		**  --> atop malloc's a minimal amount of space which is
		**      only extended when needed
		*/
		memset(curtpres, 0, curtlen * sizeof(struct tstat));

		ntaskpres = photoproc(curtpres, &curtlen);

		/*
		** register processes that exited during last sample;
		** first determine how many processes exited
		**
		** the number of exited processes is limited to avoid
		** that atop explodes in memory and introduces OOM killing
		*/
		nprocexit = acctprocnt();	/* number of exited processes */

		if (nprocexit > MAXACCTPROCS)
		{
			noverflow = nprocexit - MAXACCTPROCS;
			nprocexit = MAXACCTPROCS;
		}
		else
			noverflow = 0;

		/*
		** determine how many processes have been exited
		** for the netatop module (only processes that have
		** used the network)
		*/
		if (nprocexit > 0 && (supportflags & NETATOPD))
			nprocexitnet = netatop_exitstore();
		else
			nprocexitnet = 0;

		/*
		** reserve space for the exited processes and read them
		*/
		if (nprocexit > 0)
		{
			curpexit = malloc(nprocexit * sizeof(struct tstat));

			ptrverify(curpexit,
			          "Malloc failed for %d exited processes\n",
			          nprocexit);

			memset(curpexit, 0, nprocexit * sizeof(struct tstat));

			nprocexit = acctphotoproc(curpexit, nprocexit);

			/*
 			** reposition offset in accounting file when not
			** all exited processes have been read (i.e. skip
			** those processes)
			*/
			if (noverflow)
				acctrepos(noverflow);
		}
		else
		{
			curpexit    = NULL;
		}

		/*
		** calculate the deviations (i.e. calculate the activity
		** during the last sample).  Note for PMAPI calls we had
		** to delay changing curtime until after sampling due to
		** the way pmSetMode(3) works.
		*/
		pretime  = curtime;	/* timestamp for previous sample */
		curtime  = cursstat->stamp; /* timestamp for this sample */

		deviatsyst(cursstat, presstat, devsstat);

		devtstat = malloc((ntaskpres+nprocexit) * sizeof(struct tstat));

		ptrverify(devtstat, "Malloc failed for %d modified tasks\n",
			          			ntaskpres+nprocexit);

		ntaskdev = deviattask(curtpres,  ntaskpres,
		                      curpexit,  nprocexit, deviatonly,
		                      devtstat,  devsstat,
		                      &nprocdev, &nprocpres,
		                      &totrun, &totslpi, &totslpu, &totzombie);

  	      	/*
 		** create list of pointers specifically to the process entries
		** in the task list
		*/
       		devpstat = malloc(sizeof (struct tstat *) * nprocdev);

		ptrverify(devpstat, "Malloc failed for %d process ptrs\n",
			          				nprocdev);

		for (i=0, j=0; i < ntaskdev; i++)
		{
			if ( (devtstat+i)->gen.isproc)
				devpstat[j++] = devtstat+i;
		}

		timed = __pmtimevalToReal(&curtime);
		delta = timed - __pmtimevalToReal(&pretime);

		/*
		** activate the installed print-function to visualize
		** the deviations
		*/
		lastcmd = (vis.show_samp)(timed,
				     delta > 1.0 ? delta : 1.0,
		           	     devsstat,  devtstat, devpstat,
		                     ntaskdev,  ntaskpres, nprocdev, nprocpres, 
		                     totrun, totslpi, totslpu, totzombie, 
		                     nprocexit, noverflow, sampcnt==0);

		if (rawreadflag)
			__pmtimevalInc(&curtime, &interval);

		/*
		** release dynamically allocated memory
		*/
		if (nprocexit > 0)
			free(curpexit);

		if (nprocexitnet > 0)
			netatop_exiterase();

		free(devtstat);
		free(devpstat);

		if (lastcmd == 'r')	/* reset requested ? */
		{
			sampcnt = -1;

			curtime = origin;

			/* set current (will be 'previous') counters to 0 */
			memset(curtpres, 0, curtlen * sizeof(struct tstat));
			sstat_reset(cursstat);

			/* remove all tasks in database */
			pdb_makeresidue();
			pdb_cleanresidue();
		}
	} /* end of main-loop */
}

/*
** print usage of this command
*/
void
prusage(char *myname)
{
	printf("Usage: %s [-flags] [interval [samples]]\n",
					myname);
	printf("\t\tor\n");
	printf("Usage: %s -w  file  [-S] [-%c] [interval [samples]]\n",
					myname, MALLPROC);
	printf("       %s -r  file  [-b hh:mm] [-e hh:mm] [-flags]\n",
					myname);
	printf("\n");
	printf("\tgeneric flags:\n");
	printf("\t  -%c  show or log all processes (i.s.o. active processes "
	                "only)\n", MALLPROC);
	printf("\t  -%c  calculate proportional set size (PSS) per process\n", 
	                MCALCPSS);
	printf("\t  -P  generate parseable output for specified label(s)\n");
	printf("\t  -L  alternate line length (default 80) in case of "
			"non-screen output\n");

	(*vis.show_usage)();

	printf("\n");
	printf("\tspecific flags for raw logfiles:\n");
	printf("\t  -w  write raw data to PCP archive folio\n");
	printf("\t  -r  read  raw data from PCP archive folio\n");
	printf("\t  -S  finish %s automatically before midnight "
	                "(i.s.o. #samples)\n", pmProgname);
	printf("\t  -b  begin showing data from specified time\n");
	printf("\t  -e  finish showing data after specified time\n");
	printf("\n");
	printf("\tinterval: number of seconds   (minimum 0)\n");
	printf("\tsamples:  number of intervals (minimum 1)\n");
	printf("\n");
	printf("If the interval-value is zero, a new sample can be\n");
	printf("forced manually by sending signal USR1"
			" (kill -USR1 %s-pid)\n", pmProgname);
	printf("or with the keystroke '%c' in interactive mode.\n", MSAMPNEXT);
	printf("\n");
	printf("Please refer to the man-page of '%s' for more details.\n", pmProgname);

	cleanstop(1);
}

/*
** handler for ALRM-signal
*/
void
getalarm(int sig)
{
	awaittrigger=0;

	if (interval.tv_sec || interval.tv_usec)
		setalarm(&interval);	/* restart the timer */
}

/*
** handler for USR1-signal
*/
void
getusr1(int sig)
{
	awaittrigger=0;
}

/*
** handler for USR2-signal
*/
void
getusr2(int sig)
{
	awaittrigger=0;
	nsamples = sampcnt;	// force stop after next sample
}

/*
** functions to handle a particular tag in the .atoprc file
*/
extern int get_posval(char *name, char *val);

void
do_interval(char *name, char *val)
{
	interval.tv_sec = get_posval(name, val);
	interval.tv_usec = 0;
}

void
do_linelength(char *name, char *val)
{
	linelen = get_posval(name, val);
}

/*
** read RC-file and modify defaults accordingly
*/
static void
readrc(char *path, int syslevel)
{
	int	i, nr, line=0, errorcnt = 0;

	/*
	** check if this file is readable with the user's
	** *real uid/gid* with syscall access()
	*/
	if ( access(path, R_OK) == 0)
	{
		FILE	*fp;
		char	linebuf[256], tagname[20], tagvalue[256];

		fp = fopen(path, "r");

		while ( fgets(linebuf, sizeof linebuf, fp) )
		{
			line++;

			i = strlen(linebuf);

			if (i > 0 && linebuf[i-1] == '\n')
				linebuf[i-1] = 0;

			nr = sscanf(linebuf, "%19s %255[^#]",
						tagname, tagvalue);

			switch (nr)
			{
			   case 0:
				continue;

			   case 1:
				if (tagname[0] == '#')
					continue;

				fprintf(stderr,
					"%s: syntax error line "
					"%d (no value specified)\n",
					path, line);

				cleanstop(1);
				break;		/* not reached */

			   default:
				if (tagname[0] == '#')
					continue;
				
				if (tagvalue[0] != '#')
					break;

				fprintf(stderr,
					"%s: syntax error line "
					"%d (no value specified)\n",
					path, line);

				cleanstop(1);
			}

			/*
			** tag name and tag value found
			** try to recognize tag name
			*/
			for (i=0; i < sizeof manrc/sizeof manrc[0]; i++)
			{
				if ( strcmp(tagname, manrc[i].tag) == 0)
				{
					if (manrc[i].sysonly && !syslevel)
					{
						fprintf(stderr,
						   "%s: warning at line %2d "
						   "- tag name %s not allowed "
						   "in private atoprc\n",
							path, line, tagname);

						errorcnt++;
						break;
					}

					manrc[i].func(tagname, tagvalue);
					break;
				}
			}

			/*
			** tag name not recognized
			*/
			if (i == sizeof manrc/sizeof manrc[0])
			{
				fprintf(stderr,
					"%s: warning at line %2d "
					"- tag name %s not valid\n",
					path, line, tagname);

				errorcnt++;
			}
		}

		if (errorcnt)
			sleep(2);

		fclose(fp);
	}
}
