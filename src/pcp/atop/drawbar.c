/*
** ATOP - System & Process Monitor 
**
** The program 'atop' offers the possibility to view the activity of
** the system on system-level as well as process-level.
** 
** This source-file contains functions for bar graph representation of
** system-level statistics about processors, memory, disks and network
** interfaces.
** ==========================================================================
** Author:      Gerlof Langeveld
** E-mail:      gerlof.langeveld@atoptool.nl
** Date:        March/April 2023 (initial)
** --------------------------------------------------------------------------
** Copyright (C) 2023 Gerlof Langeveld
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
*/

/////////////////////////////////////////////////////////////////////////////
// Screen layout in bar graph mode will be based on one of these two models:
//
//   ============
//   Memory model
//   ============
//   This model is preferred when the number of disks and network interfaces
//   is limited, because it shows more memory details.
//
//     +---------------------------------------------------+
//     | ATOP - host     <header line>         ... elapsed |
//     |                 <empty  line>                     |
//     |                                 |                 |
//     |  Processor bar graph            |                 |
//     |                                 |                 |
//     |                                 |                 |
//     |                                 |                 |
//     |---------------------------------|  Memory graph   |
//     |               |                 |                 |
//     |               |                 |                 |
//     |  Disk graph   | Interface graph |                 |
//     |               |                 |                 |
//     |               |                 |                 |
//     +---------------------------------------------------+
//
//			OR
//
//   ============
//   I/O model
//   ============
//   When many disks and/or network interfaces are present, more space
//   is needed for these windows in the lower half of the screen. So the
//   memory window only uses the upper half of the screen.
//
//     +---------------------------------------------------+
//     | ATOP - host     <header line>         ... elapsed |
//     |                 <empty  line>                     |
//     |                                 |                 |
//     |                                 |                 |
//     |  Processor bar graph            |  Memory graph   |
//     |                                 |                 |
//     |                                 |                 |
//     |---------------------------------------------------|
//     |                            |                      |
//     |                            |                      |
//     |  Disk bar graph            | Interface bar graph  |
//     |                            |                      |
//     |                            |                      |
//     +---------------------------------------------------+
//
// For every bar graph (processor, memory, disk and interface) a
// separate window is created. Apart from these four windows,
// other windows are created:
//
// 1. A window for the header line (always).
//
// 2. A window for the horizontal ruler line in between the upper
//    and lower half of the screen (always).
//
// 3. A window for the vertical ruler between the disk and interface
//    window in the lower half (always).
//
// 4. A window for the vertical ruler between the processor and memory
//    window in the upper half (in case of I/O model) or a screen-size
//    vertical ruler (in case of memory model).
//
// The choice between the memory model and I/O model is dynamically made
// based on the number of columns in the screen (terminal window) and the
// number of disks/interfaces to be presented. When the terminal window is
// horizontally scaled by the user, atop might switch from one model to
// the other.
/////////////////////////////////////////////////////////////////////////////

#include <pcp/pmapi.h>
#ifdef HAVE_NCURSES_CURSES_H
#include <ncurses/curses.h>
#else
#include <curses.h>
#endif
#include <locale.h>

#include "atop.h"
#include "photoproc.h"
#include "showgeneric.h"
#include "photosyst.h"

extern char usecolors;


// maximum X axis label length
//
#define	MAXLABLEN	7

// number of columns in memory management bars
//
#define	MEMORYBARSZ	11
#define	SWAPBARSZ	8
#define	EVENTBARSZ	10


// four windows are created to show graphs
//
// - processor stats	(left upper)
// - memory stats	(right upper or right full)
// - disk stats		(left lower)
// - interface stats	(right lower)
//
// metadata for each of these windows: struct perwindow
//
struct perwindow {
	WINDOW	*win;
	int	nlines, ncols;
};

static struct perwindow 	wincpu, winmem, windsk, winnet;

// additional windows for the header line and for line drawing
//
static WINDOW 	*headwin, *midline, *colupper, *collower;

// struct for calling drawvertbars()
//
#define	MAXCAT		5	// maximum number of categories within one bar
#define	MAXHEIGHT	25	// maximum bar height in lines

struct vertval {
	int	barval;			// total value of bar
	char	*barlab;		// bar label
	char	basecolor;		// bar color or fill color

	char 	barmap[MAXHEIGHT];	// color map
	char 	barchr[MAXHEIGHT];	// character map

	int	numcat;			// number of categories in bar

	struct {
		int   cval;		// per-category value
		int   ccol;		// per-category color
		char  clab;		// per-category label
	} category[MAXCAT];
};


// struct for calling drawnetbars()
//
struct netval {
	count_t	pvals;		// send bytes-per-second
	count_t	pvalr;		// recv bytes-per-second
	int	speed;		// speed in Mbits/sec
	int	maxmbits;	// bar scale
	char 	*barlab;	// bar label
};


// header message
//
static char	*headmsg = "Press '?' for help";


// function prototypes
//
static int	headergetch(double, double, char *, int);
static int	wininit(struct sstat *);
static void	winexit(void);
static void	showhelp(void);
static long	getwininput(char *, char *, int, char);
static void	colorswon(WINDOW *, int);
static void	colorswoff(WINDOW *, int);
static int	severitycolor(char);
static char	setseverity(long, long, long);
static float	getwinratio(struct sstat *, char *);

static void	do_cpubars(struct sstat *, int, char, char);
static void	do_dskbars(struct sstat *, int, char, char);
static void	do_netbars(struct sstat *, int, char, char);

static void	sortvertbars(int, int, struct vertval **);
static int	compvertval(const void *, const void *);

static void	sortnetbars(int, struct netval **);
static int	compnetval(const void *, const void *);

static void	fillbarmaps(int, struct vertval *, float, int);

static int	drawvertbars(struct perwindow *, float, float,
			int, int, struct vertval *, int, char *,char *, int);

static int	drawnetbars(struct perwindow *, int, struct netval *,
						int, char *,char *);

static int	drawmemory(struct perwindow *w, struct sstat *, double, double, char);

static int	drawmemlines(struct perwindow *, int, int, int, int,
			int, char *, char *);

static int	drawevent(struct perwindow *, int, int, int,
			char *, char *, long);


/////////////////////////////////////////////////////
// entry point to display the deviation counters
// on system level in bar graph mode
/////////////////////////////////////////////////////
char
draw_samp(double sampletime, double nsecs, struct sstat *sstat,
					char flag, int sorted)
{
	static char	winitialized, wassorted, initlabels,
			swapinuse, ttyrescaled;
	static double	lasttime;
	struct timespec	ts;
	static int	nrdisk, nrallintf, nrintf;

	long		newinterval;
	int		lastchar, i, statuscol;
	char		*statusmsg = NULL, buf[32], lower=0, answer[16];

	// when needed (re)initialize the windows for bar graphs
	//
	if (!winitialized || nrdisk    != sstat->dsk.ndsk    ||
	                     nrallintf != sstat->intf.nrintf   )
	{
		// determine the number of disks and network interfaces
		// only the physical disks and network interfaces apply
		//
		nrdisk    = sstat->dsk.ndsk;
		nrallintf = sstat->intf.nrintf;

		for (i=0, nrintf=0; i < sstat->intf.nrintf; i++)
		{
			if (sstat->intf.intf[i].type != 'v')
				nrintf++;
		}

		// initialize graph windows
		//
		swapinuse = sstat->mem.totswap ? 1 : 0;

		if (winitialized)	// already initialized -> first remove
			winexit();

		wininit(sstat);

		winitialized = 1;
	}

	// verify if situation related to swap usage has been
	// changed (in that case the memory window has
	// to be redefined)
	//
	if (( sstat->mem.totswap && !swapinuse) ||
	    (!sstat->mem.totswap &&  swapinuse)   )
	{
		swapinuse = sstat->mem.totswap ? 1 : 0;
		winexit();
		wininit(sstat);
	}

	// main loop:
	// - draw bar graphs
	// - wait for keystroke or alarm expiration
	//
	while (1)
	{
		initlabels = sorted != wassorted;

		if (!initlabels && lasttime != sampletime && sorted)
			initlabels = 1;

		// show processor graph
		//
		do_cpubars(sstat, nsecs, initlabels, barmono);

		// show disk graph
		//
		do_dskbars(sstat, nsecs, initlabels, barmono);

		// show network (interfaces) graph
		//
		do_netbars(sstat, nsecs, initlabels, lower);

		// show memory graph
		//
		drawmemory(&winmem, sstat, nsecs, sampletime, flag);

		// reset label initialization
		//
		initlabels = 0;
		wassorted  = sorted;

		// verify if status message is required
		//
		statusmsg = NULL;
		statuscol = FGCOLORBORDER;

		if (flag&RRBOOT)
			statusmsg = "SINCE BOOT";
 
		if (paused)	// might overrule other message
			statusmsg = "PAUSED";

		if (ttyrescaled)
		{
			pmsprintf(buf, sizeof buf, " %dx%d ", LINES, COLS);
			statusmsg = buf;

			ttyrescaled = 0;

			// show critical color when getting close to minimum
			// screen width
			//
                	if (COLS  < MINCOLUMNS+5 || LINES  < MINLINES+5)
				statuscol = FGCOLORCRIT;
		}

		// wait for keystroke or alarm expiration
		//
		switch (lastchar = headergetch(sampletime, nsecs, statusmsg, statuscol))
		{
		   case ERR:			// alarm expired?
		   case 0:
			return lastchar;

		   case MPROCGEN:
		   case MPROCMEM:
		   case MPROCDSK:
		   case MPROCNET:
		   case MPROCGPU:
		   case MPROCSCH:
		   case MPROCVAR:
		   case MPROCARG:

		   case MBARGRAPH:
			winexit();		// close windows
			winitialized = 0;

			return lastchar;	// switch to text mode

		   case KEY_RESIZE:		// terminal window resize?
			winexit();

			// when window gets too small for bar graph mode,
			// switch to text mode
			//
                	if (COLS  < MINCOLUMNS || LINES  < MINLINES)
                	{
				winitialized = 0;
				return MBARGRAPH;
			}

			// terminal window size still fine: 
			// 	reinitialize all ncurses windows
			//
			wininit(sstat);
			ttyrescaled = 1;
			break;		// redraw with current values

		   case MSAMPNEXT:	// manual trigger for next sample?
			if (paused)
				break;

			getalarm(0);
			return lastchar;

		   case MSAMPPREV:	// manual trigger for previous sample?
			if (!rawreadflag)
			{
				beep();
				break;
			}

			if (paused)
				break;

			/*
			** back up two steps, advancing again shortly
			*/
			pmtimespecDec(&curtime, &interval);
			pmtimespecDec(&curtime, &interval);

			return lastchar;

		   case MRESET:		// reset to begin?
			getalarm(0);
			paused = 0;
			return lastchar;

		   case MSAMPBRANCH:	// branch to other time?
			// only possible when viewing raw file
			//
			if (!rawreadflag)
			{
				beep();
				break;
			}

			if (getwininput("Enter new time (format [YYYYMMDD]hhmm): ",
					answer, sizeof answer, 1) >= 0)
			{
                                ts = curtime;

                                if ( !getbranchtime(answer, &ts) )
                                {
                                        beep();
                                        break;	// branch failed
                                }

				/*
				** back up one step before the branch time,
				** to advance onto it in the next sample
				*/
				curtime = ts;
				pmtimespecDec(&curtime, &interval);
				if (time_less_than(&curtime, &start))
					curtime = start;

				return lastchar;
			}

			break;

		   case MBARLOWER:	// reset network scale?
			lower = 1;
			break;

		   case MBARMONO:	// categorized busy bars?
			if (barmono)
				barmono = 0;
			else
				barmono = 1;
			break;

		   case MPAUSE:		// pause key (toggle)?
			if (paused)
			{
				paused=0;

				if (!rawreadflag)
					setalarm2(1, 0);
			}
			else
			{
				paused=1;
				setalarm2(0, 0);       // stop the clock
			}
			break;

		   case MINTERVAL:	// modify interval?

			setalarm2(0, 0);     	// stop the clock

			if ( (newinterval = getwininput("Interval: ", answer,
							sizeof answer, 1)) >= 0)
			{
				interval.tv_sec = newinterval;
				interval.tv_nsec = 0;
				setup_step_mode(0);
			}

			if (!paused)
				setalarm2(1, 0); // set short timer

			break;

		   case MQUIT:		// quit entirely?
			winexit();
			move(LINES-1, 0);
			clrtoeol();
			refresh();
			cleanstop(0);

		   case MHELP1:		// help wanted?
		   case MHELP2:
			setalarm2(0, 0);       // stop the clock

			// show help lines
			// 
			showhelp();

			// reinitialize original windows
			//
			winexit();
			wininit(sstat);

			if ((interval.tv_sec || interval.tv_nsec) && !paused && !rawreadflag)
				setalarm2(1, 0); // force new sample

			break;

		   default:		// any other key pressed?
			break;		// ignore
		}
	}
}

/////////////////////////////////////////////////////
// prepare the CPU specific bar graph
/////////////////////////////////////////////////////
static void
do_cpubars(struct sstat *sstat, int nsecs, char initlabels, char mono)
{
	static int		labellen, numcpus, numlabs;
	static char		*labarea, *p;
	static struct vertval	*vertvals;

	count_t			alltics;
	int 			i;
	char			buf[16];

	// check if the number of CPUs has been changed since
	// previous sample and create X axis labels for all
	// CPUs
	//
	if (numcpus != sstat->cpu.nrcpu || initlabels)
	{
		// remove old label space
		//
		if (vertvals)
		{
			free(vertvals);
			free(labarea);
		}

		// create new label space
		// - for one CPU, one label is enough ('Avg')
		// - for more than one CPU, one label is added ('Avg')
		//
		numcpus = sstat->cpu.nrcpu;

		numlabs = numcpus > 1 ? numcpus + 1 : 1;

		labellen = pmsprintf(buf, sizeof buf, "%d", numcpus);

		vertvals = calloc(numlabs, sizeof(struct vertval));
		ptrverify(vertvals, "Malloc failed for %d vertval structs\n", numlabs);

		labarea = calloc(numlabs, (labellen+1));
		ptrverify(labarea, "Malloc failed for %d CPU labels\n", numlabs);

		// create new X axis labels
		//
		if (numcpus == 1)
			vertvals->barlab = "0";
		else
		{
			vertvals->barlab = "Avg      ";

			for (i=0, p=labarea; i < numcpus; i++)
			{
				(vertvals+i+1)->barlab = p;
				pmsprintf(p, labellen+1, "%-*d", labellen,
						sstat->cpu.cpu[i].cpunr);
				p += labellen+1;
			}
		}
	}

	// calculate overall busy percentage and
	// fill first busy value (average)
	//
	alltics =	sstat->cpu.all.stime +
                        sstat->cpu.all.utime +
                        sstat->cpu.all.ntime +
                        sstat->cpu.all.itime +
                        sstat->cpu.all.wtime +
                        sstat->cpu.all.Itime +
                        sstat->cpu.all.Stime +
                        sstat->cpu.all.steal;

	vertvals->barval = 100 - (sstat->cpu.all.itime + sstat->cpu.all.wtime)
						* 100 / alltics;
	vertvals->basecolor = WHITE_BLUE0;

	if (!mono)
	{
		vertvals->category[0].ccol	= COLORCPUSYS;
		vertvals->category[0].clab	= 'S';
		vertvals->category[0].cval	= sstat->cpu.all.stime * 100 /
		 				  alltics;
	
		vertvals->category[1].ccol	= COLORCPUUSR;
		vertvals->category[1].clab	= 'U';
		vertvals->category[1].cval	=
			(sstat->cpu.all.utime +
			 sstat->cpu.all.ntime -
			 sstat->cpu.all.guest) * 100 / alltics;

		vertvals->category[2].ccol	= COLORCPUIDLE;
		vertvals->category[2].clab	= 'I';
		vertvals->category[2].cval	=
			(sstat->cpu.all.Stime + sstat->cpu.all.Itime) * 100 /
						  alltics;

		vertvals->category[3].ccol	= COLORCPUSTEAL;
		vertvals->category[3].clab	= 's';
		vertvals->category[3].cval	=
			sstat->cpu.all.steal * 100 / alltics;

		vertvals->category[4].ccol	= COLORCPUGUEST;
		vertvals->category[4].clab	= 'G';
		vertvals->category[4].cval	=
			sstat->cpu.all.guest * 100 / alltics;

		vertvals->numcat = 5;
	}
	else
	{
		vertvals->numcat = 0;
	}

	// if more than one CPU: calculate per CPU
	//
	if (numcpus > 1)
	{
		// total ticks during last interval for CPU 0
		//
		alltics =	sstat->cpu.cpu[0].stime +
				sstat->cpu.cpu[0].utime +
				sstat->cpu.cpu[0].ntime +
				sstat->cpu.cpu[0].itime +
				sstat->cpu.cpu[0].wtime +
				sstat->cpu.cpu[0].Itime +
				sstat->cpu.cpu[0].Stime +
				sstat->cpu.cpu[0].steal;

		// busy percentage per CPU
		//
		for (i=0; i < numcpus; i++)
		{
			(vertvals+i+1)->barval =
				100 - (sstat->cpu.cpu[i].itime +
				       sstat->cpu.cpu[i].wtime  ) *100/alltics;

			if ((vertvals+i+1)->barval < 0)
				(vertvals+i+1)->barval = 0;

			(vertvals+i+1)->basecolor = WHITE_BLUE0;

			if (!mono)
			{
				(vertvals+i+1)->category[0].ccol = COLORCPUSYS;
				(vertvals+i+1)->category[0].clab = 'S';
				(vertvals+i+1)->category[0].cval =
					sstat->cpu.cpu[i].stime * 100 /
								alltics;

				(vertvals+i+1)->category[1].ccol = COLORCPUUSR;
				(vertvals+i+1)->category[1].clab = 'U';
				(vertvals+i+1)->category[1].cval =
					(sstat->cpu.cpu[i].utime +
				 	 sstat->cpu.cpu[i].ntime -
				 	 sstat->cpu.cpu[i].guest) *100/alltics;

				(vertvals+i+1)->category[2].ccol = COLORCPUIDLE;
				(vertvals+i+1)->category[2].clab = 'I';
				(vertvals+i+1)->category[2].cval =
					(sstat->cpu.cpu[i].Stime +
					 sstat->cpu.cpu[i].Itime) *100/alltics;

				(vertvals+i+1)->category[3].ccol = COLORCPUSTEAL;
				(vertvals+i+1)->category[3].clab = 's';
				(vertvals+i+1)->category[3].cval =
					sstat->cpu.cpu[i].steal *100/alltics;

				(vertvals+i+1)->category[4].ccol = COLORCPUGUEST;
				(vertvals+i+1)->category[4].clab = 'G';
				(vertvals+i+1)->category[4].cval =
					sstat->cpu.cpu[i].guest *100/alltics;

				(vertvals+i+1)->numcat = 5;
			}
			else
			{
				(vertvals+i+1)->numcat = 0;
			}
		}
	}

	// draw bar graph showing busy percentages of CPUs
	//
	drawvertbars(&wincpu, 100.0, cpubadness, 
			numlabs, numcpus == 1 ? 0 : 1, vertvals, 
			labellen, "Busy%", "Processors", 0);
}


/////////////////////////////////////////////////////
// prepare the disk specific bar graph
/////////////////////////////////////////////////////
static void
do_dskbars(struct sstat *sstat, int nsecs, char initlabels, char mono)
{
	static int		labellen, numdisks;
	static char		*labarea, *p;
	static struct vertval	*vertvals;

	count_t			mstot;
	int 			i, namlen;

	// check if the number of disks has been changed since
	// previous sample and create X axis labels for all disks
	//
	if (numdisks != sstat->dsk.ndsk || initlabels)
	{
		// remove old label space
		//
		if (vertvals)
		{
			free(vertvals);
			free(labarea);
		}

		// create new label space
		//
		numdisks = sstat->dsk.ndsk;

		vertvals = calloc(numdisks, sizeof(struct vertval));
		ptrverify(vertvals, "Malloc failed for %d vertval structs\n", numdisks);

		labarea = calloc(numdisks, (MAXLABLEN+1));
		ptrverify(labarea, "Malloc failed for %d disk labels\n", numdisks);

		// create new X axis labels
		//
		for (i=0, labellen=0, p=labarea; i < numdisks; i++)
		{
			(vertvals+i)->barlab = p;

			namlen = strlen(sstat->dsk.dsk[i].name);

			if (labellen < namlen)
				labellen = namlen;

			if (namlen > MAXLABLEN)
				pmsprintf(p, MAXLABLEN+1, "%-*s", MAXLABLEN,
				      sstat->dsk.dsk[i].name+namlen-MAXLABLEN);
			else
				pmsprintf(p, MAXLABLEN+1, "%-*s", MAXLABLEN,
				      sstat->dsk.dsk[i].name);

			p += MAXLABLEN+1;
		}

		if (labellen > MAXLABLEN)
			labellen = MAXLABLEN;
	}

	// calculate total number of milliseconds in the interval
	//
	mstot  = (sstat->cpu.all.stime + sstat->cpu.all.utime +
                  sstat->cpu.all.ntime + sstat->cpu.all.itime +
                  sstat->cpu.all.wtime + sstat->cpu.all.Itime +
                  sstat->cpu.all.Stime + sstat->cpu.all.steal  )
                                / sstat->cpu.nrcpu;

	if (!mstot)	// avoid division by zero
		mstot = 1;

	// per disk: fill total busy percentage and fill two
	// sidebars for the ratio between reads and writes
	//
	for (i=0; i < numdisks; i++)
	{
		count_t	totsect 	= sstat->dsk.dsk[i].nrsect +
					  sstat->dsk.dsk[i].nwsect;
		int perc		= sstat->dsk.dsk[i].io_ms *100/mstot;

		(vertvals+i)->barval 	= perc;		// total disk busy%
		(vertvals+i)->basecolor = WHITE_GREEN0;

		if (!mono)
		{
			if (!totsect)
				totsect = 1;	// avoid division by zero

			(vertvals+i)->category[0].cval	= (sstat->dsk.dsk[i].nrsect * perc + totsect/2) / totsect;
			(vertvals+i)->category[0].ccol	= COLORDSKREAD;
			(vertvals+i)->category[0].clab	= 'R';

			(vertvals+i)->category[1].cval	= perc - (vertvals+i)->category[0].cval;
			(vertvals+i)->category[1].ccol	= COLORDSKWRITE;
			(vertvals+i)->category[1].clab	= 'W';

			(vertvals+i)->numcat = 2;
		}
		else
		{
			(vertvals+i)->numcat = 0;
		}
	}

	drawvertbars(&windsk, 100.0, dskbadness,
		numdisks, 0, vertvals, labellen, "Busy%", "Disks", 3);
}

/////////////////////////////////////////////////////
// return the ratio between network interfaces
// and disks, and determine the window model
/////////////////////////////////////////////////////
static float
getwinratio(struct sstat *sstat, char *winmodel)
{
	int	disklabellen, intflabellen, i, namlen, nrintf, nrdisk;
	int	dskcols, intcols, memcols;
	float	dsk2netratio;

	// determine disk label length
	//
	for (i=0, disklabellen=0, nrdisk=sstat->dsk.ndsk; i < nrdisk; i++)
	{
		namlen = strlen(sstat->dsk.dsk[i].name);

		if (disklabellen < namlen)
			disklabellen = namlen;
	}

	if (disklabellen > MAXLABLEN)
		disklabellen = MAXLABLEN;

	// determine interface label length
	//
	for (i=0, intflabellen=0, nrintf=0; i < sstat->intf.nrintf; i++)
	{
		if (sstat->intf.intf[i].type != 'v')
		{
			nrintf++;

			namlen = strlen(sstat->intf.intf[i].name);

			if (intflabellen < namlen)
				intflabellen = namlen;
		}
	}

	if (intflabellen > MAXLABLEN)
		intflabellen = MAXLABLEN;

	// determine the number of columns needed
	// for all disks and for all interfaces
	//
	dskcols = 7 + nrdisk * (disklabellen+1);
	intcols = 1 + nrintf * (intflabellen+5);

	// determine the ratio between the size of the
	// disk window and interface window
	//
	dsk2netratio = 1.0 * dskcols / intcols;

	// determine window model:
	// 'm'	- memory model (preferred)
	// 'i'	- I/O model when lots of disks and/or interfaces are present
	//
	memcols = 1 + MEMORYBARSZ + 2 + EVENTBARSZ + 1 + 1 +
			(sstat->mem.totswap ? SWAPBARSZ+1 : 0);

	if (dskcols + intcols + 1 + memcols > COLS)
		*winmodel = 'i';
	else
		*winmodel = 'm';

	return dsk2netratio;
}

/////////////////////////////////////////////////////
// maintain hash list to register the current scale
// per network interface
/////////////////////////////////////////////////////
#define	IFRESERVED	5
#define	ISNUM		16	// factor of 2!

struct ifscale {
	struct ifscale	*next;
	char		*interface;
	int		curscale;
};

static struct ifscale	*ishash[ISNUM];
 
// search the ifscale struct for a specific interface
// and return pointer
//
static struct ifscale *
getifscale(char *interface)
{
	int		hash = 0;
	struct ifscale	*isp;
	char		*p = interface;

	for (; *p; p++)
		hash += *p;

	for (isp=ishash[hash&(ISNUM-1)]; isp; isp=isp->next)
	{
		if ( strcmp(interface, isp->interface) == 0 )
			return isp;
	}	
	
	return NULL;	// not found
}

// create new ifscale struct for an interface, add it to
// a hash list and return pointer
//
static struct ifscale *
addifscale(char *interface, int scale)
{
	int		len, hash = 0;
	struct ifscale	*isp;
	char		*p = interface;

	for (len=0; *p; p++, len++)
		hash += *p;

	isp = malloc(sizeof(struct ifscale));
	ptrverify(isp, "Malloc failed for ifscale struct\n");

	isp->interface 	= malloc(len+1);
	ptrverify(isp->interface, "Malloc failed for ifscale name\n");
	strcpy(isp->interface, interface);

	isp->next 	= ishash[hash&(ISNUM-1)];
	isp->curscale 	= scale > winnet.nlines - IFRESERVED ?
				scale : winnet.nlines - IFRESERVED;

	ishash[hash&(ISNUM-1)] = isp;

	return isp;
}

// lower scale of all interfaces to the new scale (e.g. in case that
// the number of lines in the window has changed), but only if the
// old scale of an interface was zero or the given old scale (which
// is the initial scale related to the number of lines in the current
// window)
//
static void
lowerifscales(int oldlines, int newlines)
{
	int		i;
	struct ifscale	*isp;

	for (i=0; i < ISNUM; i++)	// all hash buckets
	{
		for (isp=ishash[i]; isp; isp=isp->next)
		{
			if (oldlines == 0 ||
			    isp->curscale == oldlines-IFRESERVED)
				isp->curscale = newlines - IFRESERVED;
		}
	}
}

/////////////////////////////////////////////////////
// prepare the interface specific bar graph
/////////////////////////////////////////////////////
static void
do_netbars(struct sstat *sstat, int nsecs, char initlabels, char lower)
{
	static long	totints, numints;
	static char	*labarea, *p;
	static int	labellen;

	static struct netval	*netvals;
	struct ifscale		*isp;

	count_t		ival, oval;
	int 		i, j, namlen;

	// check if the number of interfaces has been changed since the
	// previous sample and create X axis labels for all interfaces
	//
	if (totints != sstat->intf.nrintf || initlabels)
	{
		totints = sstat->intf.nrintf;

		// calculate how many physical interfaces (ethernet and wlan)
		//
		for (i=0, numints=0; i < totints; i++)
		{
			if (sstat->intf.intf[i].type != 'v')
				numints++;
		}

		// remove old label space
		//
		if (netvals)
		{
			free(netvals);
			free(labarea);
		}

		// create new label space
		//
		netvals = calloc(numints, sizeof(struct netval));
		ptrverify(netvals, "Malloc failed for %ld netvals structs\n", numints);

		labarea = calloc(numints, (MAXLABLEN+1));
		ptrverify(labarea, "Malloc failed for %ld interface labels\n", numints);

		// create new X axis labels
		//
		for (i=j=0, labellen=0, p=labarea; i < totints; i++)
		{
			if (sstat->intf.intf[i].type != 'v')
			{
				(netvals+j)->barlab = p;

				namlen = strlen(sstat->intf.intf[i].name);

				if (labellen < namlen)
					labellen = namlen;

				if (namlen > MAXLABLEN)
					pmsprintf(p, MAXLABLEN+1, "%-.*s",
					   MAXLABLEN, sstat->intf.intf[i].name);
				else
					pmsprintf(p, MAXLABLEN+1, "%s",
						sstat->intf.intf[i].name);

				p += MAXLABLEN+1;

				j++;
			}
		}

		if (labellen > MAXLABLEN)
			labellen = MAXLABLEN;
	}

	// lower of all scales required by the user?
	//
	if (lower)
		lowerifscales(0, winnet.nlines);

	// fill traffic values per physical interface
	//
	for (i=j=0; i < totints; i++)
	{
		if (sstat->intf.intf[i].type != 'v')
		{
			int	maxmbits = 0;

			ival = sstat->intf.intf[i].rbyte/125/1000/nsecs;
			oval = sstat->intf.intf[i].sbyte/125/1000/nsecs;

			(netvals+j)->pvalr = ival;
			(netvals+j)->pvals = oval;
			(netvals+j)->speed = sstat->intf.intf[i].speed;

			// determine minimum scale for bar graph
			//
			maxmbits = ival > oval ? ival : oval;

			// search for worst-case vertical scale so far and
			// check if the new scale is larger --> exchange
			//
			// first verify if scale is known
			//
			if ( (isp = getifscale( (netvals+j)->barlab)) )
			{
				if (isp->curscale < maxmbits)
					isp->curscale = maxmbits;
			}
			else	// first time: scale not known
			{
				isp = addifscale( (netvals+j)->barlab,
								maxmbits);
			}

			(netvals+j)->maxmbits = isp->curscale;

			j++;
		}
	}

	drawnetbars(&winnet, numints, netvals,
		labellen, "Mbits/s", "Interfaces");
}


/////////////////////////////////////////////////////
// draw vertical bars in a graph window
// mainly for CPUs and disks
/////////////////////////////////////////////////////
#define BARCHAR		' '

// arguments:
// - w		pointer to struct perwindow
// - barscale	maximum value for Y axis
// - hthreshold	high threshold, i.e. critical value
// - numbars	number of bars to draw
// - avgbar	boolean: first value is for average bar?
// - vvp	list of vertval structs with the bar values (numbars elements)
// - barlabsize	length of label for each bar
// - ytitle	title for Y axis
// - xtitle	title for X axis
// - barwidth	number of columns for one bar
// 		 0 = automatic (as many columns as possible, max. 3)
// 		 1 = single column bars with empty bar in between
// 		 2 = double column bars with empty bar in between
// 		 3 = triple column bars with empty bar in between
//
// returns:	number of bars drawn
//
static int
drawvertbars(struct perwindow *w,
		float barscale, float hthreshold,
		int numbars, int avgbar, struct vertval *vvp,
		int barlabsize, char *ytitle, char *xtitle, int barwidth)
{
	char	buf[16], *ychar, horizontalxlab=0, barch;
	int 	i, j, curline, curcol, barlines, realbars,
		realcols, availcols, needcols;
	int	autoscale, color, labwidth = 0, spacing = 1;
	float	valperunit, level;
	int	ytitlelen  = strlen(ytitle), xtitlelen  = strlen(xtitle);
	int	ytitleline, xtitlespace;
	int	scalelen = pmsprintf(buf, sizeof buf, "%d", (int)barscale),
		xindent;
	void	*vvporig = vvp;

	struct vertval *vp;

	// calculate indentation in front of first bar column
	// considering line layout:
	// - position 1: 	character of Y title
	// - position 2: 	space
	// - position 3: 	space
	// - position 4..n:	number of positions taken for Y label (scalelen)
	// - position n+1:	Y axis vertical line
	//
	xindent   = 3 + scalelen + 1;
	availcols = w->ncols - xindent;		// number of columns for bars

	// verify if there is enough horizontal space in the
	// window to show all per-bar X labels horizontally in one line
	// instead of vertically in several lines
	//
	if (numbars*barlabsize+numbars-1 < availcols)
		horizontalxlab = 1;

	// calculate effective number of lines for bar graph
	// reserving 3 lines:
	// - one line for x axis line
	// - one line empty below per-bar labels
	// - one line for x axis title
	//
	barlines = w->nlines - 3 - (horizontalxlab ? 1 : barlabsize);

	if (barlines <= 0)
		return 0;

	if (barlines > MAXHEIGHT)
		barlines = MAXHEIGHT;

	// calculate value represented by each bar graph line
	//
	valperunit = barscale / barlines;

	// determine the width of each bar
	//
	// variables involved:
	// - realbars:	how many bars can be drawn (realbars <= numbars)
	// - realcols:	how many screen columns will be used
	//
	autoscale = barwidth ? 0 : 1;

	if (autoscale)	// define preferred bar width
		barwidth = 3;

	while (1)
	{
		switch (barwidth)
		{
	   	   // single bars and space between each bar
	    	   //
	   	   case 1:
			needcols  = numbars * 2 - 1;
			realbars  = needcols < availcols ?
						numbars : (availcols+1)/2;
			realcols  = realbars * 2 - 1;
			break;

	   	   // double bars and space between each bar
	   	   //
	   	   case 2:
			needcols  = numbars * 3 - 1;
			realbars  = needcols <= availcols ?
						numbars : (availcols+1)/3;
			realcols  = realbars * 3 - 1;
			break;

	   	   // triple bars and space between each bar
	   	   //
	   	   default:
			needcols  = numbars * 4 - 1;
			realbars  = needcols <= availcols ?
						numbars : (availcols+1)/4;
			realcols  = realbars * 4 - 1;
		}

		if (!autoscale || realbars == numbars || --barwidth < 1)
			break;
	}

	if (barwidth < 1)
		barwidth = 1;

	// when all bars do not fit in the window width,
	// sort the values in decreasing order to show
	// the most relevant ones
	//
	if (availcols < needcols)
		sortvertbars(numbars, avgbar, &vvp);

	// calculate horizontal position of X title
	// (centered below bar columns)
	//
	if (horizontalxlab)
	{
		if (barlabsize < barwidth)
		{
			labwidth = barwidth;
		}	
		else
		{
			labwidth = barlabsize;
			spacing  = barlabsize + 1 - barwidth;
			realcols = realbars * barlabsize + realbars - 1;
		}
	}

	// calculate vertical position of Y title
	// (centered left from bar lines)
	//
	if (ytitlelen > barlines)
		ytitleline = 0;
	else
		ytitleline = (barlines - ytitlelen + 1) / 2;

	// create colormap and character map for each bar
	//
	fillbarmaps(numbars, vvp, valperunit, barlines);

	// wipe window contents
	//
	werase(w->win);

	// draw bar graph line-by-line
	//
	for (curline=0; curline < barlines; curline++)
	{
		int filler;

		// calculate value represented by this level and
		// draw the high-threshold line if needed
		//
		level = (barlines - curline) * valperunit;

		if (level + valperunit/2  > hthreshold &&
		    level - valperunit/2 <= hthreshold   )
			filler = ACS_HLINE;
		else
			filler = ' ';

		// select the character that has to be printed from
		// the vertical title
		//
		if (curline >= ytitleline && (curline-ytitleline) < ytitlelen)
			ychar = ytitle+curline-ytitleline;
		else
			ychar = " ";

		// print vertical Y title character and Y axis value
		//
		mvwprintw(w->win, curline, 0, "%c  %*d", *ychar, scalelen,
								(int)level);
		waddch(w->win, ACS_VLINE);

		// for each bar
		//
		for (curcol=0, vp=vvp; curcol < realbars; curcol++, vp++)
		{
			color = vp->barmap[barlines-curline-1];
			barch = vp->barchr[barlines-curline-1];

			// print colored character(s)
			//
			if (color)
			{
				colorswon(w->win, color);

				switch (barwidth)
				{
				   case 1:
					waddch(w->win, barch);
					break;
				   case 2:
					waddch(w->win, barch);
					waddch(w->win, BARCHAR);
					break;
				   case 3:
					waddch(w->win, BARCHAR);
					waddch(w->win, barch);
					waddch(w->win, BARCHAR);
				}

				colorswoff(w->win, color);
			}
			else
			{
				// print fillers
				//
				if (filler != ' ')
					colorswon(w->win, FGCOLORCRIT);

				for (i=0; i < barwidth; i++)
					waddch(w->win, filler);

				if (filler != ' ')
					colorswoff(w->win, FGCOLORCRIT);
			}

			// add fillers between the bars
			//
			if (filler != ' ')
				colorswon(w->win, FGCOLORCRIT);

			for (i=0; i < (curcol < realbars-1 ?
						spacing : spacing-1); i++)
				waddch(w->win, filler);

			if (filler != ' ')
				colorswoff(w->win, FGCOLORCRIT);
		}
	}

	// print line for X axis
	//
	if (curline >= ytitleline && (curline-ytitleline) < ytitlelen)
		ychar = ytitle+curline-ytitleline;
	else
		ychar = " ";

	mvwprintw(w->win, curline++, 0, "%c  %*d", *ychar, scalelen, 0);
	waddch(w->win, ACS_LLCORNER);

	for (i=0; i < realcols; i++)
		waddch(w->win, ACS_HLINE);

	if (horizontalxlab)
	{
		// print X axis values horizontally (one line)
		//
		wmove(w->win, curline++, xindent);

		if (barlabsize == 1 && barwidth == 3)
			waddch(w->win, ' ');	// alignment

		for (i=0, vp=vvp; i < realbars; i++)
			wprintw(w->win, "%-*.*s ", labwidth, barlabsize,
							(vp+i)->barlab);
	}
	else
	{
		// print X axis values vertically
		//
		for (i=0; i < barlabsize; i++)
		{
			wmove(w->win, curline++, xindent);

			for (curcol=0, vp=vvp; curcol < realbars; curcol++,vp++)
			{
				waddch(w->win, *((vp->barlab)+i));

				for (j=0; j < barwidth-1; j++)
					waddch(w->win, BARCHAR);

				if (spacing)
					waddch(w->win, ' ');
			}
		}
	}

	// print X title centered under bar graph
	//
	curline++;	// empty line

	if (xtitlelen > realcols)
		xtitlespace = xindent + xtitlelen;
	else
		xtitlespace = xindent + (xtitlelen + realcols)/2;

	mvwprintw(w->win, curline++, 0, "%*s", xtitlespace, xtitle);

        wrefresh(w->win);

	if (vvporig != vvp)		// reallocated by sortvertbars()?
		free(vvp);

	return realbars;
}

/////////////////////////////////////////////////////
// fill the color map and character map of each bar
//
// the color map describes a color code per bar line
// and the character map describes a character per
// bar line
/////////////////////////////////////////////////////
static void
fillbarmaps(int numbars, struct vertval *vvp, float valperunit, int barlines)
{
	int 		i, c, n, m;
	struct vertval	*vp;
	char		*mp, *cp;

	// for each bar, fill the color map and character map
	//
	for (i=0, vp=vvp; i < numbars; i++, vp++)
	{
		// initialize both maps
		//
		memset(vp->barmap, '\0',    MAXHEIGHT);
		memset(vp->barchr, BARCHAR, MAXHEIGHT);

		// when no separate categories defined,
		// fill the entire bar with the base color
		//
		if (vp->numcat == 0)
		{		
			m = (vp->barval + valperunit/2) / valperunit;
			memset(vp->barmap, vp->basecolor, m);
			continue;
		}

		// for each bar category, fill the corresponding color
		// in the color map and on the lowest position in the
		// character map fill the category character
		//
		for (c=0, mp=vp->barmap, cp=vp->barchr; c < vp->numcat; c++)
		{
			// determine the number of color/character positions 
			// for this category
			//
			n = (vp->category[c].cval + valperunit/2) / valperunit;

			if (n > 0 && mp - vp->barmap + n < MAXHEIGHT)
			{
				// fill all color positions (lines)
				//
				memset(mp, vp->category[c].ccol, n);
				mp += n;

				// fill one character position
				//
				*cp = vp->category[c].clab;
				cp += n;
			}
		}

		// verify that all positions of the total bar value are
		// filled; if not (due to rounding issues), add a filler
		//
		// rounding issues:
		//   suppose that the total percentage is 49, subdivided
		//   into percentage 22 for category 'read' and 27 for
		//   category 'write'
		//   when every bar line represents 5% the total number
		//   of bar lines should be 10 (49 rounded to 50)
		//   for the category 'read' 4 bar lines will be used
		//   (22 rounded to 20) and for the category 'write'
		//   5 bar lines (27 rounded to 25)
		//   then 1 line should be filled with a neutral color
		//
		n = mp - vp->barmap;	// number of units filled
		m = (vp->barval + valperunit/2) / valperunit;	// total units

		if (n < m && m < MAXHEIGHT)
			memset(mp, vp->basecolor, m-n);
	}
}

/////////////////////////////////////////////////////
// draw vertical bars for network interfaces
/////////////////////////////////////////////////////
// arguments:
// - w		pointer to struct perwindow
// - numbars	number of bars to draw, i.e. number of interfaces
// - nvp	pointer to struct containing traffic info per interface
// - barlabsize	length of label per bar
// - ytitle	title for Y axis
// - xtitle	title for X axis
//
// returns:	number of bars drawn
//
static int
drawnetbars(struct perwindow *w, int numbars, struct netval *nvp,
		int barlabsize, char *ytitle, char *xtitle)
{
	char	*ychar;
	int 	i, ifbar, curline, barlines, realbars, color, perifcols;
	float	*valperline, level;
	int	ytitlelen  = strlen(ytitle), xtitlelen=strlen(xtitle),
		ytitleline;
	void	*nvporig = nvp;

	// define number of columns needed per interface
	// considering layout:
	// - position 1: 	space
	// - position 2..7:	six positions for Y value
	// - position 8:	Y axis vertical line
	// - position 9..12:	double column for transmit + receive
	//
	perifcols = 12;

	if ((w->ncols-1)/perifcols >= numbars)
		realbars =  numbars;		// real number of interfaces
	else
		realbars = (w->ncols-1) / perifcols;

	// calculate effective number of lines for bar graph
	// reserving 3 lines:
	// - one line for x axis line
	// - one line for speed
	// - one line for x axis label
	// - one line blank under labels
	// - one line containing x title
	//
	barlines = w->nlines - IFRESERVED;

	if (barlines <= 0)
		return 0;

	// when all network interfaces do not fit in the window width,
	// sort the values in decreasing order to show
	// the most relevant ones
	//
	if (numbars > realbars)
		sortnetbars(numbars, &nvp);

	// calculate value represented by each bar graph line
	// (different for each interface)
	//
	valperline = calloc(numbars, sizeof(float));
	ptrverify(valperline, "Malloc failed for %d values per line\n",
								numbars);

	for (i=0; i < numbars; i++)
		*(valperline+i) = (float)(nvp+i)->maxmbits / barlines;

	// calculate vertical position of y title
	// (centered left from bar lines)
	//
	if (ytitlelen > barlines)
		ytitleline = 0;
	else
		ytitleline = (barlines - ytitlelen + 1) / 2;

	// wipe window contents
	//
	werase(w->win);

	// draw bar graphs line-by-line
	//
	for (curline=0; curline < barlines; curline++)
	{
		// select the character that has to be printed from
		// the vertical Y title
		//
		if (curline >= ytitleline && (curline-ytitleline) < ytitlelen)
			ychar = ytitle+curline-ytitleline;
		else
			ychar = " ";

		// print vertical y title character
		//
		mvwaddch(w->win, curline, 0, *ychar);

		// print four columns per interface
		//
		for (ifbar=0;  ifbar < realbars; ifbar++)
		{
			// calculate value represented by this level
			//
			level = (barlines - curline) * *(valperline+ifbar);

			wprintw(w->win, " %6d", (int)level);

			waddch(w->win, ACS_VLINE);

			// rounded receive bar reaches this level?
			//
			if ((nvp+ifbar)->pvalr + *(valperline+ifbar) / 2 >= level)
			{
				// print colored bar
				//
				color = COLORNETRECV;

				colorswon(w->win, color);

				if (curline == barlines-1)
				{
					wprintw(w->win, "RX");
				}
				else
				{
					waddch(w->win, BARCHAR);
					waddch(w->win, BARCHAR);
				}

				colorswoff(w->win, color);
			}
			else
			{
				// print spaces
				//
				wprintw(w->win, "  ");
			}

			// rounded send second bar reaches this level?
			//
			if ((nvp+ifbar)->pvals + *(valperline+ifbar) / 2 >= level)
			{
				// print colored bar
				//
				color = COLORNETSEND;
				colorswon(w->win, color);

				if (curline == barlines-1)
				{
					wprintw(w->win, "TX");
				}
				else
				{
					waddch(w->win, BARCHAR);
					waddch(w->win, BARCHAR);
				}

				colorswoff(w->win, color);
			}
			else
			{
				// print space
				//
				wprintw(w->win, "  ");
			}
		}
	}

	// print lines for X axis
	//
	if (curline >= ytitleline && (curline-ytitleline) < ytitlelen)
		ychar = ytitle+curline-ytitleline;
	else
		ychar = " ";

	mvwaddch(w->win, curline++, 0, *ychar);

	for (ifbar=0;  ifbar < realbars; ifbar++)
	{
		wprintw(w->win, "  %5d", 0);
		waddch(w->win, ACS_LLCORNER);

		for (i=0; i < perifcols-8; i++)
			waddch(w->win, ACS_HLINE);
	}

	// print speed per interface
	//
	wmove(w->win, curline++, 1);

	for (ifbar=0;  ifbar < realbars; ifbar++)
	{
		if ((nvp+ifbar)->speed)
			wprintw(w->win, "%*dM", perifcols-1, 
					(nvp+ifbar)->speed);
		else
			wprintw(w->win, "%*sM", perifcols-1, "?");
	}

	// print horizontal label per interface bar
	//
	wmove(w->win, curline++, 1);

	for (ifbar=0;  ifbar < realbars; ifbar++)
		wprintw(w->win, "%*.*s", perifcols, barlabsize,
						(nvp+ifbar)->barlab);

	// print X title centered under bar graph
	//
	curline++;	// empty line

	mvwprintw(w->win, curline++, 0, "%*s",
			3+(realbars*perifcols)/2+(xtitlelen/2), xtitle);

        wrefresh(w->win);

	// free allocated memory
	//
	free(valperline);

	if (nvp != nvporig)
		free(nvp);	// allocated by sortnetbars()

	return realbars;
}


/////////////////////////////////////////////////////
// draw specific window with memory management info
/////////////////////////////////////////////////////
static int
drawmemory(struct perwindow *w, struct sstat *sstat, double nsecs,
					double sampletime, char flag)
{
	static double	lastoomkills;

	long long	totalmem, cachemem, shmemrss, tmpfsmem,
			slabmem, freemem, hugefree, hugeused, shmrssreal;
	long long	totalswp, shmemswp, freeswp;
	char		scanseverity, swapseverity, killseverity;
	int 		curline=0, barlines, color;
	int 		usedlines, freelines, cachelines, tmpfslines,
			slablines, shmemlines, hugelines;
	int		memorycol = 1,
			swapcol   = memorycol + MEMORYBARSZ + 1,
			eventcol  = swapcol   + SWAPBARSZ   + 2;
	float		valperunit;
	char		formatbuf[16];

	// calculate all memory values, keeping in mind:
	//
	// - shmem		resident System V shared memory
	// 				including resident tmpfs (POSIX shamem)
	// 				excluding static huge pages
	//
	// - shmrss		resident System V shared memory
	// 				including static huge pages
	//
	// - cachemem		page cache including shmem
	//
	totalmem	=  sstat->mem.physmem * 1024;

	cachemem	= (sstat->mem.cachemem + sstat->mem.buffermem -
			   sstat->mem.shmem)  * 1024;

	shmemrss	=  sstat->mem.shmrss; // in bytes!

	slabmem		=  sstat->mem.slabmem * 1024;

	freemem		=  sstat->mem.freemem * 1024;

	hugefree	=  sstat->mem.sfreehugepage * 1024 +
			   sstat->mem.lfreehugepage * 1024;

	totalswp	=  sstat->mem.totswap * 1024;

	shmemswp	=  sstat->mem.shmswp; // in bytes!

	freeswp		=  sstat->mem.freeswap* 1024;

	// assumption:	most of static huge pages use for SYSV shared memory,
	// 		although static hige pages can also be used for mmap()
	//
	hugeused	= (sstat->mem.stothugepage - sstat->mem.sfreehugepage)
							* 1024+
			  (sstat->mem.ltothugepage - sstat->mem.lfreehugepage)
							* 1024;
							
	shmrssreal	= sstat->mem.shmrss - hugeused;	// in bytes!

	if (shmrssreal < 0)	// (partly) wrong assumption about static huge pages
		shmrssreal = 0;

	tmpfsmem	= (sstat->mem.shmem * 1024) - shmemswp - shmrssreal;

	// determine severity for pagescans, swapouts and oomkills
	// 'n' - normal,
	// 'w' - warning,
	// 'c' - critical
	//
	// for oomkills specifically:
	// 	show warning level for 15 minutes after the last oomkill
	// 	occurred (current time can be smaller than last time in
	// 	case of 'T' key when viewing raw logs)
	//
	scanseverity	= setseverity(sstat->mem.pgscans/nsecs, 100000, 1000);
 	swapseverity	= setseverity(sstat->mem.swouts/nsecs,     500,  100);

	if (sstat->mem.oomkills <= 0)	// no new oomkills?
	{
		if (sampletime > lastoomkills && sampletime - lastoomkills < 900)
			killseverity = 'w';
		else
			killseverity = 'n';
	}
	else				// new oomkills during last interval
	{
		killseverity = 'c';

		if (flag&RRBOOT)
			lastoomkills = sampletime - nsecs;
		else
			lastoomkills = sampletime;
	}

	// calculate effective number of lines for bar graph
	//
	barlines = w->nlines - 2;

	if (barlines <= 5)
		return 0;

	// calculate value represented by each bar graph line
	//
	valperunit = totalmem / barlines;

	// calculate number of lines for free, shared memory,
	// tmpfs, slab and page cache
	//
	freelines  = (freemem  + valperunit/2) / valperunit;
	shmemlines = (shmemrss + valperunit/2) / valperunit;
	tmpfslines = (tmpfsmem + valperunit/2) / valperunit;
	slablines  = (slabmem  + valperunit/2) / valperunit;
	cachelines = (cachemem + valperunit/2) / valperunit;
	hugelines  = (hugefree + valperunit/2) / valperunit;
	usedlines  =  barlines - freelines - shmemlines - hugelines -
	              tmpfslines - slablines - cachelines;

	// wipe window contents
	//
	werase(w->win);

	// draw lines for free memory
	//
	curline += drawmemlines(w, curline, memorycol, freelines,
			MEMORYBARSZ, COLORMEMFREE, "free", NULL);

	// draw lines for cache memory
	//
	curline += drawmemlines(w, curline, memorycol, cachelines,
			MEMORYBARSZ, COLORMEMCACH, "pagecache", NULL);

	// draw lines for free static huge pages memory
	// (occupied static huge pages are already part of processes
	// or shared memory)
	//
	curline += drawmemlines(w, curline, memorycol, hugelines,
			MEMORYBARSZ, COLORMEMHUGE, "free huge", "pages");

	// draw lines for tmpfs memory
	//
	curline += drawmemlines(w, curline, memorycol, tmpfslines,
			MEMORYBARSZ, COLORMEMTMP, "tmpfs", NULL);

	// draw lines for shared memory
	//
	curline += drawmemlines(w, curline, memorycol, shmemlines,
			MEMORYBARSZ, COLORMEMSHM, "sharedmem", NULL);

	// draw lines for slab memory
	//
	curline += drawmemlines(w, curline, memorycol, slablines,
			MEMORYBARSZ, COLORMEMSLAB, "slab", "caches");

	// draw lines for other used memory
	//
	if ((totalmem-cachemem-freemem)*100 / totalmem >= membadness)
		color = COLORBAD;
	else
		color = COLORMEMUSED;

	curline += drawmemlines(w, curline, memorycol, usedlines,
			MEMORYBARSZ, color, "processes", "&kernel");

	// show memory size
	//
	mvwprintw(w->win, curline++, memorycol, "%*s",
			MEMORYBARSZ-(MEMORYBARSZ-7+1)/2,
			val2memstr(totalmem, formatbuf, sizeof formatbuf, MBFORMAT, 0, 0));

	mvwprintw(w->win, curline, memorycol, "%*s",
			MEMORYBARSZ-(MEMORYBARSZ-6)/2, "Memory");

        wrefresh(w->win);

	// show swap space (if used)
	//
	if (totalswp)
	{
		// calculate value represented by each bar graph line
		//
		valperunit = totalswp / barlines;

		// calculate number of lines for free swap
		//
		freelines  = (freeswp  + (valperunit/2)) / valperunit;
		shmemlines = (shmemswp + (valperunit/2)) / valperunit;
		usedlines  = barlines  - shmemlines - freelines;

		// draw lines for free swap
		//
		curline = 0;

		curline += drawmemlines(w, curline, swapcol, freelines,
				SWAPBARSZ, COLORMEMFREE, "free", NULL);

		// draw lines for swapped shared memory swap
		//
		curline += drawmemlines(w, curline, swapcol, shmemlines,
				SWAPBARSZ, COLORMEMSHM, "shamem", NULL);

		// draw lines for occupied swap
		// highly occupied swap is only an issue when also memory
		// is highly occupied
		//
		if ((totalswp-freeswp) * 100 / totalswp >= swpbadness &&
	            (totalmem-cachemem-freemem) * 100 / totalmem >= membadness)
			color = COLORBAD;
		else
			color = COLORMEMUSED;
	
		curline += drawmemlines(w, curline, swapcol, usedlines,
				SWAPBARSZ, color, "procs", "&tmpfs");

		// show swap size
		//
		mvwprintw(w->win, curline++, swapcol, "%*s",
			SWAPBARSZ-(SWAPBARSZ-7+1)/2,
			val2memstr(totalswp, formatbuf, sizeof formatbuf, MBFORMAT, 0, 0));

		mvwprintw(w->win, curline, swapcol, "%*s",
			SWAPBARSZ-(SWAPBARSZ-4-1)/2, "Swap");
	}
	else
	{
		eventcol = swapcol+1;
	}

	// show events
	//
	mvwprintw(w->win, curline, eventcol, "  Events ");

	if (barlines > 1)	// show oomkilling?
		curline = drawevent(w, curline, eventcol,
				severitycolor(killseverity),
				" oomkills ", " %8ld ",
				sstat->mem.oomkills);	

	if (barlines > 4)	// show swapouts?
		curline = drawevent(w, curline, eventcol,
				severitycolor(swapseverity),
				" swapouts ", "%7ld/s ",
				sstat->mem.swouts/nsecs);	

	if (barlines > 7)	// show pagescans?
		curline = drawevent(w, curline, eventcol,
				severitycolor(scanseverity),
				" pagscans ", "%7ld/s ",
				sstat->mem.pgscans / nsecs);	

	if (barlines > 10)	// show swapins?
		curline = drawevent(w, curline, eventcol,
				COLORMEMBAR,
				"  swapins ", "%7ld/s ",
				sstat->mem.swins / nsecs);	

	if (barlines > 13)	// show pageouts?
		curline = drawevent(w, curline, eventcol,
				COLORMEMBAR,
				"  pagouts ", "%7ld/s ",
				sstat->mem.pgouts / nsecs);	

	if (barlines > 16)	// show pageins?
		curline = drawevent(w, curline, eventcol,
				COLORMEMBAR,
				"  pageins ", "%7ld/s ",
				sstat->mem.pgins / nsecs);	

        wrefresh(w->win);

	return 1;
}

/////////////////////////////////////////////////////
// draw lines for specific memory category
/////////////////////////////////////////////////////
static int
drawmemlines(struct perwindow *w, int startline, int startcolumn,
			int numlines, int width, int color,
			char *cat1, char *cat2)
{
	int	line=startline, catline, targetline=startline+numlines, len;

	if (numlines == 0)
		return 0;

	if (usecolors)
        	wattron(w->win, COLOR_PAIR(color));

	wattron(w->win, A_BOLD);

	for (catline=startline+(numlines-1)/2; line < targetline; line++)
	{
		wmove(w->win, line, startcolumn);

		if (line == catline)
		{
			len = strlen(cat1);

			if (len > width)
			{
				// truncate
				//
				wprintw(w->win, "%.*s", width, cat1);
			}
			else
			{
				int cw = width - (width-len+1)/2;

				wprintw(w->win, "%*s%*s", cw, cat1, width-cw, " ");
			}
		}
		else
		{
			if (line == catline+1 && cat2)
			{
				len = strlen(cat2);

				if (len > width)
				{
					wprintw(w->win, "%.*s", width, cat2);
				}
				else
				{
					int cw = width - (width-len+1)/2;

					wprintw(w->win, "%*s%*s", cw,
							cat2, width-cw, " ");
				}
			}
			else
			{
				wprintw(w->win, "%*s", width, " ");
			}
		}
	}

       	wattroff(w->win, A_BOLD);

	if (usecolors)
       		wattroff(w->win, COLOR_PAIR(color));

	return numlines;
}

/////////////////////////////////////////////////////
// draw lines for specific memory event
/////////////////////////////////////////////////////
static int
drawevent(struct perwindow *w, int line, int column, int color,
		char *text, char *format, long value)
{
	colorswon(w->win, color);

	line -= 2;

	if (value >= 0)
		mvwprintw(w->win, line, column, format, value);
	else
		mvwprintw(w->win, line, column, "        ? ");

	line -= 1;
       	wattron(w->win, A_BOLD);
	mvwprintw(w->win, line, column, "%s", text);
       	wattroff(w->win, A_BOLD);

	colorswoff(w->win, color);

	return line;
}

/////////////////////////////////////////////////////
// fill the header line (separate window)
// and wait for keyboard input event
/////////////////////////////////////////////////////
static int
headergetch(double sampletime, double nsecs, char *statusmsg, int statuscol)
{
	int	colsunused, fill1, fill2, fill3, statcol;
	int	headlen = strlen(headmsg);
	char	buf[64], timestr[16], datestr[16];
	int	seclen  = val2elapstr(nsecs, buf, sizeof buf);

	convdate(sampletime, datestr, sizeof datestr);       /* date to ascii string   */
	convtime(sampletime, timestr, sizeof timestr);       /* time to ascii string   */

	// calculate subdivision of areas in header line
	//
	colsunused = COLS - 35 - seclen - nodenamelen - headlen;
	fill1 =  colsunused / 6;
	fill2 = (colsunused - fill1) / 2;
	fill3 =  colsunused - fill1 - fill2;

	// fill header line
	//
	werase(headwin);
	wattron(headwin, A_REVERSE);
	wprintw(headwin, "ATOP - %s %*s%s %s%*s%s%*s%s elapsed",
			sysname.nodename, 
			fill1, " ", datestr, timestr,
			fill2, " ", headmsg,
			fill3, " ", buf);

	wattroff(headwin, A_REVERSE);

	// display specific status if needed
	//
	statcol = 27 + nodenamelen + fill1 + 2;

	if (statusmsg)
	{
        	colorswon(headwin, statuscol);
		wattron(headwin, A_REVERSE);
		mvwprintw(headwin, 0, statcol, "%s", statusmsg);
		wattroff(headwin, A_REVERSE);
        	colorswoff(headwin, statuscol);
	}

	wrefresh(headwin);

	// wait for keystroke
	//
	return mvwgetch(headwin, 1, 0);
}


/////////////////////////////////////////////////////
// create all windows
/////////////////////////////////////////////////////
static int
wininit(struct sstat *sstat)
{
	int 	lpw, c4c, c4m, c4d, c4n, i, avail;
	float	col, dsk2netratio;
	char 	winmodel;

	// determine the ratio between the size of the
	// disk window and interface window
	//
	// determine window model:
	// 'm'	- memory model (preferred)
	// 'i'	- I/O model when lots of disks and/or interfaces are present
	//
	dsk2netratio = getwinratio(sstat, &winmodel);

	// cleanup underlying standard screen
	//
	werase(stdscr);
	refresh();

	// calculate number of lines for a half-screen bar graph window
	//
        lpw = (LINES-1) / 2 - 1;	// lines per window

	// calculate number of columns for the windows in the upper half
	//
	// - memory window gets fixed horizontal size (columns),
	//   either with or without swap bar
	//
	// - cpu window gets rest of the colums in upper half
	//
	c4m  = MEMORYBARSZ + EVENTBARSZ + 4;
	c4m += sstat->mem.totswap ? SWAPBARSZ+1 : 0;

	c4c  = COLS - c4m - 1;			// cpu: rest of columns

	// calculate number of columns for windows in lower half
	//
	// - width of the disk and the interface windows will be
	//   calculated according to the requested ratio
	//   depending of the model, the full screen width can be used
	//   (I/O model) or the remaining width without the memory columns
	//   (memory model)
	//
	avail = COLS - (winmodel == 'm' ? c4m + 1: 0);	// available columns

        col   = avail / (dsk2netratio+1.0);
        c4d   = col * dsk2netratio;		// columns for disk
        c4n   = avail - c4d - 1;		// columns for interfaces

	// create window of two lines for the header line
	// (second line only meant to 'park' the cursor)
	//
        headwin = newwin(2, COLS, 0, 0);

	// create window of one line as horizonal ruler between the
	// upper and lower half, and draw horizontal ruler
	//
	avail   = winmodel=='m' ? COLS - c4m : COLS;

        midline = newwin(1, avail, lpw+2, 0);

        colorswon(midline, FGCOLORBORDER);

 	for (i=0; i < avail; i++)
	{
		if (i == c4c && c4c != c4d)
		{
			waddch(midline, ACS_BTEE);
			continue;
		}

		if (i == c4d)
		{
			if (c4c != c4d)
				waddch(midline, ACS_TTEE);
			else
				waddch(midline, ACS_PLUS);
			continue;
		}

		waddch(midline, ACS_HLINE);
	}

        colorswoff(midline, FGCOLORBORDER);

	wrefresh(midline);

	// create window of one column for vertical ruler 
	// (half or full) and draw vertical ruler
	//
	avail = winmodel=='m' ? LINES : lpw+1;		// available lines

        colupper = newwin(avail, 1, 1, c4c);
	
        colorswon(colupper, FGCOLORBORDER);

 	for (i=0; i < avail; i++)
		if (i == lpw+1)
			waddch(colupper, ACS_RTEE);
		else
			waddch(colupper, ACS_VLINE);

        colorswoff(colupper, FGCOLORBORDER);

	wrefresh(colupper);

	// create window of one column for vertical ruler 
	// in the lower half and draw vertical ruler
	//
        collower = newwin(LINES-lpw-3, 1, lpw+3, c4d);
	
        colorswon(collower, FGCOLORBORDER);

 	for (i=0; i < lpw+1; i++)
		waddch(collower, ACS_VLINE);

        colorswoff(collower, FGCOLORBORDER);

	wrefresh(collower);

	// create four windows for the resource graphs
	// and fill dimensions
	//
	wincpu.nlines	= lpw;
	wincpu.ncols	= c4c;
        wincpu.win 	= newwin(wincpu.nlines, wincpu.ncols, 2, 0);

	winmem.nlines	= winmodel == 'i' ? lpw : LINES-2;
	winmem.ncols	= c4m;
        winmem.win 	= newwin(winmem.nlines, winmem.ncols, 2, c4c+1);

	lpw = LINES - 3 - lpw;	// recalc for extra line in case of odd lines

	windsk.nlines	= lpw;
	windsk.ncols	= c4d;
        windsk.win 	= newwin(windsk.nlines, windsk.ncols,
			  wincpu.nlines+3, 0);

	lowerifscales(winnet.nlines, lpw);	// lower initial scales

	winnet.nlines	= lpw;
	winnet.ncols	= c4n;
        winnet.win 	= newwin(winnet.nlines, winnet.ncols,
			  wincpu.nlines+3, c4d+1);

	wrefresh(wincpu.win);
	wrefresh(winmem.win);
	wrefresh(windsk.win);
	wrefresh(winnet.win);

	return 1;
}


/////////////////////////////////////////////////////
// delete all windows
/////////////////////////////////////////////////////
static void
winexit(void)
{
	delwin(wincpu.win);
	delwin(winmem.win);
	delwin(windsk.win);
	delwin(winnet.win);

	delwin(colupper);
	delwin(collower);
	delwin(midline);
	delwin(headwin);
}


/////////////////////////////////////////////////////
// create a separate window to request the user
// to enter a value
//
// arguments
// - prompt	pointer to prompt string
// - answer	pointer to buffer in which
//   		the answer will be returned
// - maxanswer	maximum size of answer buffer
// - numerical	boolean: convert value to integer?
// 
// return value:
//   if 'numerical' is true, integer value or
//   		-1 when input was not numeric
//
//   if 'numerical' is false, value 0 or
//   		-1 when no input was given
/////////////////////////////////////////////////////
static long
getwininput(char *prompt, char *answer, int maxanswer, char numerical)
{
	WINDOW	*mywin;
	long	inumval = -1;
	int	numcols = strlen(prompt) + maxanswer + 1;

	// create a boxed window of three lines
	// for the conversation
	//
        mywin = newwin(3, numcols, (LINES-3)/3, (COLS-numcols)/3);

	box(mywin, ACS_VLINE, ACS_HLINE);

	// show the prompt
	//
	mvwprintw(mywin, 1, 1, "%s", prompt);

	// prepare reading input
	//
	echo();		// switch echoing on
	answer[0] = 0;

	if (wgetnstr(mywin, answer, maxanswer-1) != ERR)
	{
		// conversion to integer required?
		//
		if (numerical)
		{
			if (answer[0])  // data entered?
			{
				if ( numeric(answer) )
				{
					inumval = atol(answer);
				}
				else
				{
					beep();
					wmove(mywin, 1, 1);
                                	wclrtoeol(mywin);
					box(mywin, ACS_VLINE, ACS_HLINE);
					mvwprintw(mywin, 1, 1, "Not numeric!");
					wrefresh(mywin);
                        		sleep(2);
				}
			}
		}
		else
		{
			inumval = 0;
		}
	}
	else
	{
		beep();
	}

	noecho();
	delwin(mywin);

	return inumval;
}


/////////////////////////////////////////////////////
// create a separate window with help text and
// wait for any keyboard input 
/////////////////////////////////////////////////////
#define	HELPLINES	25
#define	HELPCOLS	70

static void
showhelp(void)
{
	WINDOW	*helpwin;
	int	line=1, inputkey;

	// create centered window for help text
	//
	// notice that this window is bigger than the required
	// minimum size of the terminal
	//
        helpwin = newwin(HELPLINES, HELPCOLS,
			(LINES-HELPLINES)/2, (COLS-HELPCOLS)/2);

	if (!helpwin)	// window allocation failed?
		return;

	box(helpwin, ACS_VLINE, ACS_HLINE);

	// show help text
	//
	mvwprintw(helpwin, line++, 2, "Display mode:");
        mvwprintw(helpwin, line++, 2,
		" '%c'  - text mode: keep same process info",	MBARGRAPH);
        mvwprintw(helpwin, line++, 2,
        	" '%c'  - text mode: generic info",             MPROCGEN);
        mvwprintw(helpwin, line++, 2,
        	" '%c'  - text mode: memory details",           MPROCMEM);

	if (supportflags & IOSTAT)
        	mvwprintw(helpwin, line++, 2,
        	" '%c'  - text mode: disk details",             MPROCDSK);

	if (supportflags & NETATOP || supportflags & NETATOPBPF)
        	mvwprintw(helpwin, line++, 2,
        	" '%c'  - text mode: network details",          MPROCNET);

	if (supportflags & GPUSTAT)
        	mvwprintw(helpwin, line++, 2,
        	" '%c'  - text mode: GPU details",              MPROCGPU);

        mvwprintw(helpwin, line++, 2,
        	" '%c'  - text mode: scheduling and thread-group info",
								MPROCSCH);
        mvwprintw(helpwin, line++, 2,
        	" '%c'  - text mode: various info",		MPROCVAR);
        mvwprintw(helpwin, line++, 2,
        	" '%c'  - text mode: full command line per process", 
								MPROCARG);

	line++;

	// show context dependent help text for raw file viewing or
	// live measurement 
	//
	if (rawreadflag)
	{
        	mvwprintw(helpwin, line++, 2, "Raw file viewing:");
        	mvwprintw(helpwin, line++, 2,
			" '%c'  - show next sample in raw file", MSAMPNEXT);
        	mvwprintw(helpwin, line++, 2,
			" '%c'  - show previous sample in raw file", MSAMPPREV);
        	mvwprintw(helpwin, line++, 2,
			" '%c'  - rewind to begin of raw file", MRESET);
        	mvwprintw(helpwin, line++, 2,
			" '%c'  - branch to certain time in raw file",
								MSAMPBRANCH);
	}
	else
	{
        	mvwprintw(helpwin, line++, 2, "Control:");
        	mvwprintw(helpwin, line++, 2,
			" '%c'  - change interval timer (0 = only manual trigger)", MINTERVAL);
        	mvwprintw(helpwin, line++, 2,
			" '%c'  - manual trigger to force next sample",
								MSAMPNEXT);
        	mvwprintw(helpwin, line++, 2,
			" '%c'  - reset counters to boot time values", MRESET);
        	mvwprintw(helpwin, line++, 2,
			" '%c'  - pause button to freeze current sample (toggle)", MPAUSE);
	}

	line++;
        mvwprintw(helpwin, line++, 2, "General:");
        mvwprintw(helpwin, line++, 2,
		" '%c'  - reset network scale (otherwise keeps highest level)", MBARLOWER);
        mvwprintw(helpwin, line++, 2,
		" '%c'  - busy bars with/without categories (toggle)",
								MBARMONO);
        mvwprintw(helpwin, line++, 2,
		" '%c'  - quit this program", 			MQUIT);

	line++;
        mvwprintw(helpwin, line++, 2,
			"Select one of these keys (except '%c') or ", MQUIT);
        mvwprintw(helpwin, line++, 2,
			"any other key to leave help...");

	wrefresh(helpwin);

	// wait for any keystroke
	//
	inputkey = wgetch(helpwin);

	// push this keystroke back to be received by the main loop
	//
	if (inputkey != MQUIT)
		ungetch(inputkey);

	// remove help window
	delwin(helpwin);
}

/////////////////////////////////////////////////////
// switch certain color on
/////////////////////////////////////////////////////
static void
colorswon(WINDOW *win, int color)
{
	if (usecolors)
       		wattron(win, COLOR_PAIR(color));
	else
       		wattron(win, A_REVERSE);
}

/////////////////////////////////////////////////////
// switch certain color off
/////////////////////////////////////////////////////
static void
colorswoff(WINDOW *win, int color)
{
	if (usecolors)
       		wattroff(win, COLOR_PAIR(color));
	else
       		wattroff(win, A_REVERSE);
}

/////////////////////////////////////////////////////
// return background color depending on severity
/////////////////////////////////////////////////////
static int
severitycolor(char severity)
{
	int color;

	switch (severity)
	{
	   case 'n':	// normal
		color = COLOROKAY;
		break;

	   case 'w':	// warning
		color = COLORWARN;
		break;

	   case 'c':	// critical
		color = COLORBAD;
		break;

	   default:
		color = 0;
	}

	return color;
}

/////////////////////////////////////////////////////
// return character to represent the severity
/////////////////////////////////////////////////////
// return value
// - n	normal
// - w	warning
// - c	critical
//
static char
setseverity(long val, long cthreshold, long wthreshold)
{
	if (val < wthreshold)
		return 'n';

	if (val < cthreshold)
		return 'w';

	return 'c';
}

/////////////////////////////////////////////////////
// sort bar values in descending order
// for CPU and disk stats
/////////////////////////////////////////////////////
static void
sortvertbars(int nbars, int avgbar, struct vertval **valpp)
{
	// copy original array to be sorted
	//
	struct vertval *sortlist = calloc(nbars, sizeof(struct vertval));
	ptrverify(sortlist, "Malloc failed for %d sortitems\n", nbars);

	memcpy(sortlist, *valpp, sizeof(struct vertval) * nbars);

	// sort the copied list 
	//
	qsort(sortlist+avgbar, nbars-avgbar, sizeof(struct vertval),
								compvertval);

	*valpp = sortlist;
}

/////////////////////////////////////////////////////
// function to be called by qsort in sortvertbars()
/////////////////////////////////////////////////////
static int
compvertval(const void *a, const void *b)
{
        const struct vertval	*sia = a;
        const struct vertval	*sib = b;

        if (sia->barval > sib->barval)
                return -1;

        if (sia->barval < sib->barval)
                return  1;

        return  0;
}

/////////////////////////////////////////////////////
// sort network bar values in descending order
// for network stats
/////////////////////////////////////////////////////
static void
sortnetbars(int nbars, struct netval **valpp)
{
	// copy original array to be sorted
	//
	struct netval *sortlist = calloc(nbars, sizeof(struct netval));
	ptrverify(sortlist, "Malloc failed for %d sortitems\n", nbars);

	memcpy(sortlist, *valpp, sizeof(struct netval) * nbars);

	// sort the copied list 
	//
	qsort(sortlist, nbars, sizeof(struct netval), compnetval);

	*valpp = sortlist;
}

/////////////////////////////////////////////////////
// function to be called by qsort in sortnetbars()
/////////////////////////////////////////////////////
static int
compnetval(const void *a, const void *b)
{
        const struct netval	*nva = a;
        const struct netval	*nvb = b;
	long long		v1, v2;

	v1 = nva->pvals + nva->pvalr;
	v2 = nvb->pvals + nvb->pvalr;

        if (v1 > v2)
                return -1;

        if (v1 < v2)
                return  1;

        return  0;
}
