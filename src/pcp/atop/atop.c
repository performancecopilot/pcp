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
** ==========================================================================
** Author:      Gerlof Langeveld
** E-mail:      gerlof.langeveld@atoptool.nl
** Date:        November 1996
** Linux-port:  June 2000
** Modified: 	May 2001 - Ported to kernel 2.4
** --------------------------------------------------------------------------
** Copyright (C) 2000-2012 Gerlof Langeveld
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
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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
**
** $Log: atop.c,v $
** Revision 1.49  2010/10/23 14:01:00  gerlof
** Show counters for total number of running and sleep (S and D) threads.
**
** Revision 1.48  2010/10/23 08:18:15  gerlof
** Catch signal SIGUSR2 to take a final sample and stop.
** Needed for improved of suspend/hibernate.
**
** Revision 1.47  2010/04/23 12:20:19  gerlof
** Modified mail-address in header.
**
** Revision 1.46  2010/04/23 09:57:28  gerlof
** Version (flag -V) handled earlier after startup.
**
** Revision 1.45  2010/04/17 17:19:41  gerlof
** Allow modifying the layout of the columns in the system lines.
**
** Revision 1.44  2010/04/16 13:00:23  gerlof
** Automatically start another version of atop if the logfile to
** be read has not been created by the current version.
**
** Revision 1.43  2010/03/04 10:51:10  gerlof
** Support I/O-statistics on logical volumes and MD devices.
**
** Revision 1.42  2009/12/31 11:33:33  gerlof
** Sanity-check to bypass kernel-bug showing 497 days of CPU-consumption.
**
** Revision 1.41  2009/12/17 10:51:31  gerlof
** Allow own defined process line with key 'o' and a definition
** in the atoprc file.
**
** Revision 1.40  2009/12/17 08:15:15  gerlof
** Introduce branch-key to go to specific time in raw file.
**
** Revision 1.39  2009/12/10 13:34:32  gerlof
** Cosmetical changes.
**
** Revision 1.38  2009/12/10 11:55:38  gerlof
** Introduce -L flag for line length.
**
** Revision 1.37  2009/12/10 10:43:33  gerlof
** Correct calculation of node name.
**
** Revision 1.36  2009/12/10 09:19:06  gerlof
** Various changes related to redesign of user-interface.
** Made by JC van Winkel.
**
** Revision 1.35  2009/11/27 15:11:55  gerlof
** *** empty log message ***
**
** Revision 1.34  2009/11/27 15:07:25  gerlof
** Give up root-priviliges at a earlier stage.
**
** Revision 1.33  2009/11/27 14:01:01  gerlof
** Introduce system-wide configuration file /etc/atoprc
**
** Revision 1.32  2008/01/07 10:16:13  gerlof
** Implement summaries for atopsar.
**
** Revision 1.31  2007/11/06 09:16:05  gerlof
** Add keyword atopsarflags to configuration-file ~/.atoprc
**
** Revision 1.30  2007/08/16 11:58:35  gerlof
** Add support for atopsar reporting.
**
** Revision 1.29  2007/03/20 13:01:36  gerlof
** Introduction of variable supportflags.
**
** Revision 1.28  2007/03/20 12:13:00  gerlof
** Be sure that all tstat struct's are initialized with binary zeroes.
**
** Revision 1.27  2007/02/19 11:55:04  gerlof
** Bug-fix: flag -S was not recognized any more.
**
** Revision 1.26  2007/02/13 10:34:20  gerlof
** Support parseable output with flag -P
**
** Revision 1.25  2007/01/26 12:10:40  gerlof
** Add configuration-value 'swoutcritsec'.
**
** Revision 1.24  2007/01/18 10:29:22  gerlof
** Improved syntax-checking for ~/.atoprc file.
** Support for network-interface busy-percentage.
**
** Revision 1.23  2006/02/07 08:27:04  gerlof
** Cosmetic changes.
**
** Revision 1.22  2005/10/28 09:50:29  gerlof
** All flags/subcommands are defined as macro's.
**
** Revision 1.21  2005/10/21 09:48:48  gerlof
** Per-user accumulation of resource consumption.
**
** Revision 1.20  2004/12/14 15:05:38  gerlof
** Implementation of patch-recognition for disk and network-statistics.
**
** Revision 1.19  2004/10/26 13:42:49  gerlof
** Also lock current physical pages in memory.
**
** Revision 1.18  2004/09/15 08:23:42  gerlof
** Set resource limit for locked memory to infinite, because
** in certain environments it is set to 32K (causes atop-malloc's
** to fail).
**
** Revision 1.17  2004/05/06 09:45:44  gerlof
** Ported to kernel-version 2.6.
**
** Revision 1.16  2003/07/07 09:18:22  gerlof
** Cleanup code (-Wall proof).
**
** Revision 1.15  2003/07/03 11:16:14  gerlof
** Implemented subcommand `r' (reset).
**
** Revision 1.14  2003/06/30 11:29:12  gerlof
** Handle configuration file ~/.atoprc
**
** Revision 1.13  2003/01/14 09:01:10  gerlof
** Explicit clearing of malloced space for exited processes.
**
** Revision 1.12  2002/10/30 13:44:51  gerlof
** Generate notification for statistics since boot.
**
** Revision 1.11  2002/10/08 11:34:52  gerlof
** Modified storage of raw filename.
**
** Revision 1.10  2002/09/26 13:51:47  gerlof
** Limit header lines by not showing disks.
**
** Revision 1.9  2002/09/17 10:42:00  gerlof
** Copy functions rawread() and rawwrite() to separate source-file rawlog.c
**
** Revision 1.8  2002/08/30 07:49:35  gerlof
** Implement possibility to store and retrieve atop-data in raw format.
**
** Revision 1.7  2002/08/27 12:09:16  gerlof
** Allow raw data file to be written and to be read (with compression).
**
** Revision 1.6  2002/07/24 11:12:07  gerlof
** Redesigned to ease porting to other UNIX-platforms.
**
** Revision 1.5  2002/07/11 09:15:53  root
** *** empty log message ***
**
** Revision 1.4  2002/07/08 09:20:45  root
** Bug solution: flag list overflow.
**
** Revision 1.3  2001/11/07 09:17:41  gerlof
** Use /proc instead of /dev/kmem for process-level statistics.
**
** Revision 1.2  2001/10/04 13:03:15  gerlof
** Separate kopen() function called i.s.o. implicit with first kmem-read
**
** Revision 1.1  2001/10/02 10:43:19  gerlof
** Initial revision
**
*/

static const char rcsid[] = "$Id: atop.c,v 1.49 2010/10/23 14:01:00 gerlof Exp $";

#include <sys/types.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/utsname.h>
#include <string.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <regex.h>

#include "atop.h"
#include "acctproc.h"
#include "ifprop.h"
#include "photoproc.h"
#include "photosyst.h"
#include "showgeneric.h"
#include "parseable.h"

#define	allflags  "ab:cde:fghijklmnopqrstuvwxyz1ABCDEFGHIJKL:MNOP:QRSTUVWXYZ"
#define	PROCCHUNK	100	/* process-entries for future expansion  */
#define	MAXFL		64      /* maximum number of command-line flags  */

/*
** declaration of global variables
*/
struct utsname	utsname;
int		utsnodenamelen;
time_t 		pretime;	/* timing info				*/
time_t 		curtime;	/* timing info				*/
unsigned long	interval = 10;
unsigned long 	sampcnt;
char		screen;
int		linelen  = 80;
char		acctreason;	/* accounting not active (return val) 	*/
char		rawname[RAWNAMESZ];
char		rawreadflag;
unsigned int	begintime, endtime;
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
	struct rlimit	rlim;

	/*
	** since priviliged actions will be done later on, at this stage
	** the root-priviliges are dropped by switching effective user-id
	** to real user-id (security reasons)
	*/
        if (! droprootprivs() )
	{
		fprintf(stderr, "not possible to drop root privs\n");
                exit(42);
	}

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
		char path[1024];

		snprintf(path, sizeof path, "%s/.atoprc", p);

		readrc(path, 0);
	}

	/*
	** check if we are supposed to behave as 'atopsar'
	** i.e. system statistics only
	*/
	if ( (p = strrchr(argv[0], '/')))
		p++;
	else
		p = argv[0];

	if ( memcmp(p, "atopsar", 7) == 0)
		return atopsar(argc, argv);

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

		while (i < MAXFL-1 && (c=getopt(argc, argv, allflags)) != EOF)
		{
			switch (c)
			{
			   case '?':		/* usage wanted ?             */
				prusage(argv[0]);
				break;

			   case 'V':		/* version wanted ?           */
				printf("%s\n", getstrvers());
				exit(0);

			   case 'w':		/* writing of raw data ?      */
				if (optind >= argc)
					prusage(argv[0]);

				strncpy(rawname, argv[optind++], RAWNAMESZ-1);
				vis.show_samp = rawwrite;
				break;

			   case 'r':		/* reading of raw data ?      */
				if (optind < argc && *(argv[optind]) != '-')
					strncpy(rawname, argv[optind++],
							RAWNAMESZ-1);

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
				if ( !hhmm2secs(optarg, &begintime) )
					prusage(argv[0]);
				break;

                           case 'e':		/* end   time ?               */
				if ( !hhmm2secs(optarg, &endtime) )
					prusage(argv[0]);
				break;

                           case 'P':		/* parseable output?          */
				if ( !parsedef(optarg) )
					prusage(argv[0]);

				vis.show_samp = parseout;
				break;

                           case 'L':		/* line length                */
				if ( !numeric(optarg) )
					prusage(argv[0]);

				linelen = atoi(optarg);
				break;

			   default:		/* gather other flags */
				flaglist[i++] = c;
			}
		}

		/*
		** get optional interval-value and optional number of samples	
		*/
			if (optind < argc && optind < MAXFL)
		{
			if (!numeric(argv[optind]))
				prusage(argv[0]);
	
			interval = atoi(argv[optind]);
	
			optind++;
	
			if (optind < argc)
			{
				if (!numeric(argv[optind]) )
					prusage(argv[0]);

				if ( (nsamples = atoi(argv[optind])) < 1)
					prusage(argv[0]);
			}
		}
	}

	/*
	** determine the name of this node (without domain-name)
	** and the kernel-version
	*/
	(void) uname(&utsname);

	if ( (p = strchr(utsname.nodename, '.')) )
		*p = '\0';

	utsnodenamelen = strlen(utsname.nodename);

	sscanf(utsname.release, "%d.%d.%d", &osrel, &osvers, &ossub);

	/*
	** determine the clock rate and memory page size for this machine
	*/
	hertz		= sysconf(_SC_CLK_TCK);
	pagesize	= sysconf(_SC_PAGESIZE);

	/*
	** check if raw data from a file must be viewed
	*/
	if (rawreadflag)
	{
		rawread();
		cleanstop(0);
	}

	/*
	** determine start-time for gathering current statistics
	*/
	curtime = getboot() / hertz;

	/*
	** catch signals for proper close-down
	*/
	signal(SIGHUP,  cleanstop);
	signal(SIGTERM, cleanstop);

	/*
	** regain the root-priviliges that we dropped at the beginning
	** to do some priviliged work
	*/
	regainrootprivs();

	/*
	** lock ATOP in memory to get reliable samples (also when
	** memory is low);
	** ignored if not running under superuser priviliges!
	*/
	rlim.rlim_cur	= RLIM_INFINITY;
	rlim.rlim_max	= RLIM_INFINITY;
	(void) setrlimit(RLIMIT_MEMLOCK, &rlim);

	(void) mlockall(MCL_CURRENT|MCL_FUTURE);

	/*
	** increment CPU scheduling-priority to get reliable samples (also
	** during heavy CPU load);
	** ignored if not running under superuser priviliges!
	*/
	if ( nice(-20) == -1)
		;

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
	** since priviliged activities are finished now, there is no
	** need to keep running under root-priviliges, so switch
	** effective user-id to real user-id
	*/
        if (! droprootprivs() )
		cleanstop(42);

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
	static time_t		timelimit;
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
	cursstat = calloc(1, sizeof(struct sstat));
	presstat = calloc(1, sizeof(struct sstat));
	devsstat = calloc(1, sizeof(struct sstat));

	curtlen  = countprocs() * 3 / 2;	/* add 50% for threads */
	curtpres = calloc(curtlen, sizeof(struct tstat));

	ptrverify(cursstat, "Malloc failed for current sysstats\n");
	ptrverify(presstat, "Malloc failed for prev    sysstats\n");
	ptrverify(devsstat, "Malloc failed for deviate sysstats\n");
	ptrverify(curtpres, "Malloc failed for %d procstats\n", curtlen);

	/*
	** install the signal-handler for ALARM, USR1 and USR2 (triggers
	* for the next sample)
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

	if (interval > 0)
		alarm(interval);

	if (midnightflag)
	{
		time_t		timenow = time(0);
		struct tm	*tp = localtime(&timenow);

		tp->tm_hour = 23;
		tp->tm_min  = 59;
		tp->tm_sec  = 59;

		timelimit = mktime(tp);
	}

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
		** if the limit-flag is specified:
		**  check if the next sample is expected before midnight;
		**  if not, stop atop now 
		*/
		if (midnightflag && (curtime+interval) > timelimit)
			break;

		/*
		** wait for alarm-signal to arrive (except first sample)
		** or wait for SIGUSR1/SIGUSR2
		*/
		if (sampcnt > 0 && awaittrigger)
			pause();

		awaittrigger = 1;

		/*
		** gather time info for this sample
		*/
		pretime  = curtime;
		curtime  = time(0);		/* seconds since 1-1-1970 */

		/*
		** take a snapshot of the current system-level statistics 
		** and calculate the deviations (i.e. calculate the activity
		** during the last sample)
		*/
		hlpsstat = cursstat;	/* swap current/prev. stats */
		cursstat = presstat;
		presstat = hlpsstat;

		photosyst(cursstat);	/* obtain new counters      */

		deviatsyst(cursstat, presstat, devsstat);

		/*
		** take a snapshot of the current task-level statistics 
		** and calculate the deviations (i.e. calculate the activity
		** during the last sample)
		**
		** first register active tasks
		**  --> atop malloc's a minimal amount of space which is
		**      only extended when needed
		*/
		memset(curtpres, 0, curtlen * sizeof(struct tstat));

		while ( (ntaskpres = photoproc(curtpres, curtlen)) == curtlen)
		{
			curtlen += PROCCHUNK;

			curtpres = realloc(curtpres,
					curtlen * sizeof(struct tstat));

			ptrverify(curtpres,
			          "Realloc failed for %d tasks\n", curtlen);

			memset(curtpres, 0, curtlen * sizeof(struct tstat));
		}

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
		** calculate deviations
		*/
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

		/*
		** activate the installed print-function to visualize
		** the deviations
		*/
		lastcmd = (vis.show_samp)( curtime,
				     curtime-pretime > 0 ? curtime-pretime : 1,
		           	     devsstat,  devtstat, devpstat,
		                     ntaskdev,  ntaskpres, nprocdev, nprocpres, 
		                     totrun, totslpi, totslpu, totzombie, 
		                     nprocexit, noverflow, sampcnt==0);

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

			curtime = getboot() / hertz;	// reset current time

			/* set current (will be 'previous') counters to 0 */
			memset(cursstat, 0,           sizeof(struct sstat));
			memset(curtpres, 0, curtlen * sizeof(struct tstat));

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
	printf("       %s -r [file] [-b hh:mm] [-e hh:mm] [-flags]\n",
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
	printf("\t  -w  write raw data to   file (compressed)\n");
	printf("\t  -r  read  raw data from file (compressed)\n");
	printf("\t      special file: y[y...] for yesterday (repeated)\n");
	printf("\t  -S  finish atop automatically before midnight "
	                "(i.s.o. #samples)\n");
	printf("\t  -b  begin showing data from specified time\n");
	printf("\t  -e  finish showing data after specified time\n");
	printf("\n");
	printf("\tinterval: number of seconds   (minimum 0)\n");
	printf("\tsamples:  number of intervals (minimum 1)\n");
	printf("\n");
	printf("If the interval-value is zero, a new sample can be\n");
	printf("forced manually by sending signal USR1"
			" (kill -USR1 pid_atop)\n");
	printf("or with the keystroke '%c' in interactive mode.\n", MSAMPNEXT);
	printf("\n");
	printf("Please refer to the man-page of 'atop' for more details.\n");

	cleanstop(1);
}

/*
** handler for ALRM-signal
*/
void
getalarm(int sig)
{
	awaittrigger=0;

	if (interval > 0)
		alarm(interval);	/* restart the timer */
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
	interval = get_posval(name, val);
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
