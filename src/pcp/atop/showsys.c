/*
** ATOP - System & Process Monitor 
**
** The program 'atop' offers the possibility to view the activity of
** the system on system-level as well as process-level.
**
** This source-file contains the Linux-specific functions to calculate
** figures to be visualized.
**
** Copyright (C) 2009 JC van Winkel
** Copyright (C) 2000-2012 Gerlof Langeveld
** Copyright (C) 2015,2019,2021 Red Hat.
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
*/

#include <pcp/pmapi.h>
#ifdef HAVE_NCURSES_CURSES_H
#include <ncurses/curses.h>
#else
#include <curses.h>
#endif
#include <pwd.h>
#include <grp.h>
#include <regex.h>

#include "atop.h"
#include "photoproc.h"
#include "photosyst.h"
#include "showgeneric.h"
#include "showlinux.h"

static void	addblanks(double *, double *);
static void	sumscaling(struct sstat *, count_t *, count_t *, count_t *);
static void	psiformatavg(struct psi *, char *, char *, int);
static void	psiformattot(struct psi *, char *, extraparam *, int *, char *, int);

/*******************************************************************/
/*
** print the label of a system-statistics line and switch on
** colors if needed 
*/
static int
syscolorlabel(char *labeltext, unsigned int badness)
{
        if (screen)
        {
                if (badness >= 100)
                {
                        attron (A_BLINK);

		        if (usecolors)
			{
                        	attron(COLOR_PAIR(FGCOLORCRIT));
                        	printg(labeltext);
                        	attroff(COLOR_PAIR(FGCOLORCRIT));
			}
			else
			{
                        	attron(A_BOLD);
                        	printg(labeltext);
                        	attroff(A_BOLD);
			}

                        attroff(A_BLINK);

                        return FGCOLORCRIT;
                }

                if (almostcrit && badness >= almostcrit)
                {
		        if (usecolors)
			{
                        	attron(COLOR_PAIR(FGCOLORALMOST));
                        	printg(labeltext);
                        	attroff(COLOR_PAIR(FGCOLORALMOST));
			}
			else
			{
                        	attron(A_BOLD);
                        	printg(labeltext);
                        	attroff(A_BOLD);
			}

                        return FGCOLORALMOST;
                }
        }

        /*
        ** no colors required or no reason to show colors
        */
        printg(labeltext);
        return 0;
}

static char *sysprt_BLANKBOX(struct sstat *sstat, extraparam *notused, int, int *);

static void
addblanks(double *charslackused, double *charslackover)
{
        *charslackused+=*charslackover;
        while (*charslackused>0.5)
        {
                printg(" ");
                *charslackused-=1;
        }
}

/*
 * showsysline
 * print an array of sys_printpair things.  If the screen contains too
 * few character columns, lower priority items are removed
 *
 */
#define MAXELEMS 40

void
showsysline(sys_printpair* elemptr, 
                 struct sstat* sstat, extraparam *extra,
                 char *labeltext, unsigned int badness)
{
        sys_printdef    *curelem;
        int maxw = screen ? COLS : linelen;

        // every 15-char item is printed as:
        // >>>> | datadatadata<<<<<
        //     012345678901234
        
        /* how many items will fit on one line? */
        int avail = (maxw-5)/15;

        syscolorlabel(labeltext, badness);

        /* count number of items */
	sys_printpair newelems[MAXELEMS];
	int nitems;

	for (nitems=0; nitems < MAXELEMS-1 && elemptr[nitems].f != 0; ++nitems)
		newelems[nitems]=elemptr[nitems];

	newelems[nitems].f=0;

        /* remove lowest priority box to make room as needed */
        while (nitems > avail)
        {
                int lowestprio=999999;
                int lowestprio_index=-1;
                int i;

                for (i=0; i<nitems; ++i) 
                {
                        if (newelems[i].prio < lowestprio) 
                        {
                                lowestprio=newelems[i].prio;
                                lowestprio_index=i;
                        }
                }

                // ensure we do not write outside newelems memory
                if (lowestprio_index < 0)
                        break;
                // lowest priority item found, remove from newelems;
                memmove(newelems+lowestprio_index, 
                        newelems+lowestprio_index+1, 
                        (nitems-lowestprio_index)* sizeof(sys_printpair));   
                       // also copies final 0 entry
                nitems--;
        }

        /* 
         * ``item shortage'' is used to make entire blank boxes
         * these boxes are spread out as much as possible
         * remaining slack is used to add spaces around the vertical
         * bars
         */
        double slackitemsover;

        if (nitems >1)
        {
                slackitemsover=(double)(avail-nitems)/(nitems);
        }
        else 
        {
                slackitemsover=(avail-nitems)/2;
        }

        // charslack: the slack in characters after using as many
        // items as possible
        double charslackover = screen ? ((COLS - 5) % 15) : ((linelen - 5) %15);

        // two places per items where blanks can be added
        charslackover /= (avail * 2);    

        double charslackused=0.0;
        double itemslackused=0.0;
        elemptr=newelems;

        while ((curelem=elemptr->f)!=0) 
        {
		char 	*itemp;
		int	color;

		/*
		** by default no color is shown for this field (color = 0)
		**
		** the format-function can set a color-number (color > 0)
		** when a specific color is wanted or the format-function
		** can leave the decision to display with a color to the piece
		** of code below (color == -1)
		*/
		color = 0;
                itemp = curelem->doformat(sstat, extra, badness, &color);

		if (!itemp)
		{
			itemp = "           ?";
		}

                printg(" | ");
                addblanks(&charslackused, &charslackover);

		if (screen)
		{
			if (color == -1) // default color wanted
			{
				color = 0;

				if (badness >= 100)
                               		color = FGCOLORCRIT;
				else if (almostcrit && badness >= almostcrit)
					color = FGCOLORALMOST;
			}

			if (color)	// after all: has a color been set?
			{
				if (usecolors)
                               		attron(COLOR_PAIR(color));
                        	else
                               		attron(A_BOLD);
			}
		}

                printg("%s", itemp);

		if (color && screen)	// color set for this value?
		{
			if (usecolors)
                                attroff(COLOR_PAIR(color));
                        else
                                attroff(A_BOLD);
		}

                itemslackused+=slackitemsover;
                while (itemslackused>0.5)
                {
                        addblanks(&charslackused, &charslackover);
                        printg(" | ");
                        printg("%s", sysprt_BLANKBOX(0, 0, 0, 0));
                        addblanks(&charslackused, &charslackover);
                        itemslackused-=1;
                }

                elemptr++;

                addblanks(&charslackused, &charslackover);
        }

        printg(" |");

        if (!screen) 
        {
                printg("\n");
        }
}


/*******************************************************************/
/* SYSTEM PRINT FUNCTIONS                                          */
/*******************************************************************/
static char *
sysprt_PRCSYS(struct sstat *notused, extraparam *as, int badness, int *color)
{
        static char buf[15]="sys   ";
        val2cpustr(as->totst, buf+6, sizeof buf-6);
        return buf;
}

sys_printdef syspdef_PRCSYS = {"PRCSYS", sysprt_PRCSYS, NULL};
/*******************************************************************/
static char *
sysprt_PRCUSER(struct sstat *notused, extraparam *as, int badness, int *color) 
{
        static char buf[15]="user  ";
        val2cpustr(as->totut, buf+6, sizeof buf-6);
        return buf;
}

sys_printdef syspdef_PRCUSER = {"PRCUSER", sysprt_PRCUSER, NULL};
/*******************************************************************/
static char *
sysprt_PRCNPROC(struct sstat *notused, extraparam *as, int badness, int *color) 
{
        static char buf[15]="#proc     ";
        val2valstr(as->nproc - as->nexit, buf+6, sizeof buf-6, 6, 0, 0);
        return buf;
}

sys_printdef syspdef_PRCNPROC = {"PRCNPROC", sysprt_PRCNPROC, NULL};
/*******************************************************************/
static char *
sysprt_PRCNRUNNING(struct sstat *notused, extraparam *as, int badness, int *color) 
{
        static char buf[15]="#trun     ";
        val2valstr(as->ntrun, buf+6, sizeof buf-6, 6, 0, 0);
        return buf;
}

sys_printdef syspdef_PRCNRUNNING = {"PRCNRUNNING", sysprt_PRCNRUNNING, NULL};
/*******************************************************************/
static char *
sysprt_PRCNSLEEPING(struct sstat *notused, extraparam *as, int badness, int *color) 
{
        static char buf[15]="#tslpi    ";
        val2valstr(as->ntslpi, buf+8, sizeof buf-8, 4, 0, 0);
        return buf;
}

sys_printdef syspdef_PRCNSLEEPING = {"PRCNSLEEPING", sysprt_PRCNSLEEPING, NULL};
/*******************************************************************/
static char *
sysprt_PRCNDSLEEPING(struct sstat *notused, extraparam *as, int badness, int *color) 
{
        static char buf[15]="#tslpu    ";
        val2valstr(as->ntslpu, buf+8, sizeof buf-8, 4, 0, 0);
        return buf;
}

sys_printdef syspdef_PRCNDSLEEPING = {"PRCNDSLEEPING", sysprt_PRCNDSLEEPING, NULL};
/*******************************************************************/
static char *
sysprt_PRCNIDLE(struct sstat *notused, extraparam *as, int badness, int *color) 
{
        static char buf[15]="#tidle    ";
        val2valstr(as->ntidle, buf+8, sizeof buf-8, 4, 0, 0);
        return buf;
}

sys_printdef syspdef_PRCNIDLE = {"PRCNIDLE", sysprt_PRCNIDLE, NULL};
/*******************************************************************/
static char *
sysprt_PRCNZOMBIE(struct sstat *notused, extraparam *as, int badness, int *color) 
{
        static char buf[15]="#zombie   ";

	if (as->nzomb > 30)
		*color = FGCOLORALMOST;

	if (as->nzomb > 50)
		*color = FGCOLORCRIT;

        val2valstr(as->nzomb, buf+8, sizeof buf-8, 4, 0, 0);
        return buf;
}

sys_printdef syspdef_PRCNZOMBIE = {"PRCNZOMBIE", sysprt_PRCNZOMBIE, NULL};
/*******************************************************************/
static char *
sysprt_PRCNNEXIT(struct sstat *notused, extraparam *as, int badness, int *color) 
{
	static char firstcall = 1;
        static char buf[15]="#exit     ";

        if (supportflags & ACCTACTIVE)
        {
		if (as->noverflow)
		{
			*color = FGCOLORCRIT;
			buf[6] = '>';
			val2valstr(as->nexit, buf+7, sizeof buf-7, 5, as->avgval, as->nsecs);
		}
		else
		{
			val2valstr(as->nexit, buf+6, sizeof buf-6, 6, as->avgval, as->nsecs);
		}

                return buf;
        }
        else
        {
		if (firstcall)
		{
			*color = FGCOLORCRIT;
			firstcall = 0;
		}
		else
		{
			*color = FGCOLORINFO;
		}

		switch (acctreason)
		{
		   case 1:
                	return "no  procacct";	// "no  acctread";
		   case 2:
                	return "no  procacct";	// "no  acctwant";
		   case 3:
                	return "no  procacct";	// "no  acctsema";
		   case 4:
                	return "no  procacct";	// "no acctmkdir";
		   case 5:
                	return "no  procacct";	// "no rootprivs";
		   default:
                	return "no  procacct";
		}
        }
}

sys_printdef syspdef_PRCNNEXIT = {"PRCNNEXIT", sysprt_PRCNNEXIT, NULL};
/*******************************************************************/
static char *
sysprt_CPUSYS(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[15];
	float perc	= (sstat->cpu.all.stime * 100.0) / as->percputot;

	if (perc < 0.0)
		perc = 0.0;
	if (perc > 1.0)
		*color = -1;

        pmsprintf(buf, sizeof buf-1, "sys  %6.0f%%", perc);
        return buf;
}

sys_printdef syspdef_CPUSYS = {"CPUSYS", sysprt_CPUSYS, NULL};
/*******************************************************************/
static char *
sysprt_CPUUSER(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[15];
	float perc	= (sstat->cpu.all.utime + sstat->cpu.all.ntime)
                                        * 100.0 / as->percputot;

	if (perc < 0.0)
		perc = 0.0;
	if (perc > 1.0)
		*color = -1;

        pmsprintf(buf, sizeof buf-1, "user %6.0f%%", perc);
        return buf;
}

sys_printdef syspdef_CPUUSER = {"CPUUSER", sysprt_CPUUSER, NULL};
/*******************************************************************/
static char *
sysprt_CPUIRQ(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[15];
        float perc = (sstat->cpu.all.Itime + sstat->cpu.all.Stime)
                                    * 100.0 / as->percputot;

	if (perc < 0.0)
		perc = 0.0;
	if (perc > 1.0)
		*color = -1;

        pmsprintf(buf, sizeof buf-1, "irq  %6.0f%%", perc);
        return buf;
}

sys_printdef syspdef_CPUIRQ = {"CPUIRQ", sysprt_CPUIRQ, NULL};
/*******************************************************************/
static char *
sysprt_CPUIDLE(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[15];
	float perc = (sstat->cpu.all.itime * 100.0) / as->percputot;

	if (perc < 0.0)
		perc = 0.0;
        pmsprintf(buf, sizeof buf-1, "idle %6.0f%%", perc);
        return buf;
}

sys_printdef syspdef_CPUIDLE = {"CPUIDLE", sysprt_CPUIDLE, NULL};
/*******************************************************************/
static char *
sysprt_CPUWAIT(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[15];
	float perc = (sstat->cpu.all.wtime * 100.0) / as->percputot;

	if (perc < 0.0)
		perc = 0.0;
        pmsprintf(buf, sizeof buf-1, "wait %6.0f%%", perc);
        return buf;
}

sys_printdef syspdef_CPUWAIT = {"CPUWAIT", sysprt_CPUWAIT, NULL};
/*******************************************************************/
static char *
sysprt_CPUISYS(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[15];
	float perc	= sstat->cpu.cpu[as->index].stime * 100.0
							/ as->percputot;

	if (perc < 0.0)
		perc = 0.0;
	if (perc > 1.0)
		*color = -1;

        pmsprintf(buf, sizeof buf-1, "sys  %6.0f%%", perc);
        return buf;
}

sys_printdef syspdef_CPUISYS = {"CPUISYS", sysprt_CPUISYS, NULL};
/*******************************************************************/
static char *
sysprt_CPUIUSER(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[15];
	float perc	= (sstat->cpu.cpu[as->index].utime +
                           sstat->cpu.cpu[as->index].ntime) 
			   * 100.0 / as->percputot;

	if (perc < 0.0)
		perc = 0.0;
	if (perc > 1.0)
		*color = -1;

        pmsprintf(buf, sizeof buf-1, "user %6.0f%%", perc);
        return buf;
}

sys_printdef syspdef_CPUIUSER = {"CPUIUSER", sysprt_CPUIUSER, NULL};
/*******************************************************************/
static char *
sysprt_CPUIIRQ(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[15];
	float perc	= (sstat->cpu.cpu[as->index].Itime +
		  	   sstat->cpu.cpu[as->index].Stime)
			   * 100.0 / as->percputot;

	if (perc < 0.0)
		perc = 0.0;
	if (perc > 1.0)
		*color = -1;

        pmsprintf(buf, sizeof buf-1, "irq  %6.0f%%", perc);
        return buf;
}

sys_printdef syspdef_CPUIIRQ = {"CPUIIRQ", sysprt_CPUIIRQ, NULL};
/*******************************************************************/
static char *
sysprt_CPUIIDLE(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[15];
	float perc = (sstat->cpu.cpu[as->index].itime * 100.0) / as->percputot;

	if (perc < 0.0)
		perc = 0.0;
        pmsprintf(buf, sizeof buf-1, "idle %6.0f%%", perc);
        return buf;
}

sys_printdef syspdef_CPUIIDLE = {"CPUIIDLE", sysprt_CPUIIDLE, NULL};
/*******************************************************************/
static char *
sysprt_CPUIWAIT(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[15];
	float perc = (sstat->cpu.cpu[as->index].wtime * 100.0) / as->percputot;

	if (perc < 0.0)
		perc = 0.0;
        pmsprintf(buf, sizeof buf-1, "cpu%03d w%3.0f%%", 
		 sstat->cpu.cpu[as->index].cpunr, perc);
        return buf;
}

sys_printdef syspdef_CPUIWAIT = {"CPUIWAIT", sysprt_CPUIWAIT, NULL};
/*******************************************************************/
static char *
dofmt_cpufreq(char *buf, size_t buflen, count_t maxfreq, count_t cnt, count_t ticks)
{
        // if ticks != 0, do full output
        if (ticks) 
        {
            count_t curfreq	= cnt/ticks;
            strcpy(buf, "avgf ");
            val2Hzstr(curfreq, buf+5, buflen-6);
        } 
        else if (cnt)       // no max, no %.  if freq is known: print it
        {
            strcpy(buf, "curf ");
            val2Hzstr(cnt, buf+5, buflen-6);
        }
        else                // nothing is known: suppress
        {
            buf = NULL;
        }

	return buf;
}


/*
** sumscaling: sum scaling info for all processors
*/
static void
sumscaling(struct sstat *sstat, count_t *maxfreq,
				count_t *cnt, count_t *ticks)
{
        count_t mymaxfreq = 0;
        count_t mycnt     = 0;
        count_t myticks   = 0;

        int n=sstat->cpu.nrcpu;
        int i;

        for (i=0; i < n; ++i)
        {
                mymaxfreq+= sstat->cpu.cpu[i].freqcnt.maxfreq;
                mycnt    += sstat->cpu.cpu[i].freqcnt.cnt;
                myticks  += sstat->cpu.cpu[i].freqcnt.ticks;
        }

        *maxfreq= mymaxfreq;
        *cnt    = mycnt;
        *ticks  = myticks;
}


static char *
dofmt_cpuscale(char *buf, size_t buflen, count_t maxfreq, count_t cnt, count_t ticks)
{
	if (ticks) 
	{
		count_t curfreq	= cnt/ticks;
		int     perc = maxfreq ? 100 * curfreq / maxfreq : 0;

		strcpy(buf, "avgscal ");
		pmsprintf(buf+7, buflen-8, "%4d%%", perc);
        } 
        else if (maxfreq)   // max frequency is known so % can be calculated
        {
		strcpy(buf, "curscal ");
		pmsprintf(buf+7, buflen-8, "%4lld%%", 100 * cnt / maxfreq);
        }
	else	// nothing is known: suppress
	{
		buf = NULL;
	}

	return buf;
}

/*******************************************************************/
static char *
sysprt_CPUFREQ(struct sstat *sstat, extraparam *as, int badness, int *color) 
{

        static char buf[15];

        count_t maxfreq;
        count_t cnt;
        count_t ticks;
        int     n = sstat->cpu.nrcpu;

        sumscaling(sstat, &maxfreq, &cnt, &ticks);

        return dofmt_cpufreq(buf, sizeof buf, maxfreq/n, cnt/n, ticks/n);
}

static int
sysval_CPUFREQ(struct sstat *sstat)
{
        char    buf[15];
        count_t maxfreq;
        count_t cnt;
        count_t ticks;
        int     n = sstat->cpu.nrcpu;

        sumscaling(sstat, &maxfreq, &cnt, &ticks);

        if (dofmt_cpufreq(buf, sizeof buf, maxfreq/n, cnt/n, ticks/n))
                return 1;
        else
                return 0;
}

sys_printdef syspdef_CPUFREQ = {"CPUFREQ", sysprt_CPUFREQ, sysval_CPUFREQ};
/*******************************************************************/
static char *
sysprt_CPUIFREQ(struct sstat *sstat, extraparam *as, int badness, int *color) 
{

        static char buf[15];

        count_t maxfreq	= sstat->cpu.cpu[as->index].freqcnt.maxfreq;
        count_t cnt	= sstat->cpu.cpu[as->index].freqcnt.cnt;
        count_t ticks	= sstat->cpu.cpu[as->index].freqcnt.ticks;

        dofmt_cpuscale(buf, sizeof buf, maxfreq, cnt, ticks);
        return buf;
}

sys_printdef syspdef_CPUIFREQ = {"CPUIFREQ", sysprt_CPUIFREQ, sysval_CPUFREQ};
/*******************************************************************/
static char *
sysprt_CPUSCALE(struct sstat *sstat, extraparam *as, int badness, int *color) 
{

        static char buf[32] = "scaling    ?";

        count_t maxfreq;
        count_t cnt;
        count_t ticks;
        int     n = sstat->cpu.nrcpu;

        sumscaling(sstat, &maxfreq, &cnt, &ticks);
        dofmt_cpuscale(buf, sizeof buf, maxfreq/n, cnt/n, ticks/n);
        return buf;
}

static int
sysval_CPUSCALE(struct sstat *sstat)
{
        char    buf[32];
        count_t maxfreq;
        count_t cnt;
        count_t ticks;
        int     n = sstat->cpu.nrcpu;

        sumscaling(sstat, &maxfreq, &cnt, &ticks);

        if (dofmt_cpuscale(buf, sizeof buf, maxfreq/n, cnt/n, ticks/n))
		return 1;
	else
		return 0;
}

sys_printdef syspdef_CPUSCALE = {"CPUSCALE", sysprt_CPUSCALE, sysval_CPUSCALE};
/*******************************************************************/
static char *
sysprt_CPUISCALE(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[32] = "scaling    ?";

        count_t maxfreq = sstat->cpu.cpu[as->index].freqcnt.maxfreq;
        count_t cnt     = sstat->cpu.cpu[as->index].freqcnt.cnt;
        count_t ticks   = sstat->cpu.cpu[as->index].freqcnt.ticks;

        dofmt_cpuscale(buf, sizeof buf, maxfreq, cnt, ticks);
	return buf;
}

sys_printdef syspdef_CPUISCALE = {"CPUISCALE", sysprt_CPUISCALE, sysval_CPUSCALE};
/*******************************************************************/
static char *
sysprt_CPUSTEAL(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[15];
	float perc	= sstat->cpu.all.steal * 100.0 / as->percputot;

	if (perc < 0.0)
		perc = 0.0;
	if (perc > 1.0)
		*color = -1;

        pmsprintf(buf, sizeof buf-1, "steal %5.0f%%", perc);
        return buf;
}

sys_printdef syspdef_CPUSTEAL = {"CPUSTEAL", sysprt_CPUSTEAL, NULL};
/*******************************************************************/
static char *
sysprt_CPUISTEAL(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[15];
	float perc	= sstat->cpu.cpu[as->index].steal * 100.0
							/ as->percputot;

	if (perc < 0.0)
		perc = 0.0;
	if (perc > 1.0)
		*color = -1;

        pmsprintf(buf, sizeof buf-1, "steal %5.0f%%", perc);
        return buf;
}

sys_printdef syspdef_CPUISTEAL = {"CPUISTEAL", sysprt_CPUISTEAL, NULL};
/*******************************************************************/
static char *
sysprt_CPUGUEST(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[15];
        float perc = sstat->cpu.all.guest * 100.0 / as->percputot;

	if (perc < 0.0)
		perc = 0.0;
	if (perc > 1.0)
		*color = -1;

        pmsprintf(buf, sizeof buf-1, "guest %5.0f%%", perc);
        return buf;
}

sys_printdef syspdef_CPUGUEST = {"CPUGUEST", sysprt_CPUGUEST, NULL};
/*******************************************************************/
static char *
sysprt_CPUIGUEST(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[15];
        float perc = sstat->cpu.cpu[as->index].guest * 100.0 / as->percputot;

	if (perc < 0.0)
		perc = 0.0;
	if (perc > 1.0)
		*color = -1;

        pmsprintf(buf, sizeof buf-1, "guest %5.0f%%", perc);
        return buf;
}

sys_printdef syspdef_CPUIGUEST = {"CPUIGUEST", sysprt_CPUIGUEST, NULL};
/*******************************************************************/
static char *
sysprt_CPUIPC(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[15];
        float ipc = 0.0;

	switch (sstat->cpu.all.cycle)
	{
	   case 0:
		*color = FGCOLORINFO;
        	pmsprintf(buf, sizeof buf, "ipc notavail");
		break;

	   case 1:
		*color = FGCOLORINFO;
        	pmsprintf(buf, sizeof buf, "ipc  initial");
		break;

	   default:
		ipc = sstat->cpu.all.instr * 100 / sstat->cpu.all.cycle / 100.0;
		if (ipc < 0.0)
			ipc = 0.0;
        	pmsprintf(buf, sizeof buf, "ipc %8.2f", ipc);
	}

        return buf;
}

static int
sysval_IPCVALIDATE(struct sstat *sstat)
{
	return sstat->cpu.all.cycle;
}

sys_printdef syspdef_CPUIPC = {"CPUIPC", sysprt_CPUIPC, sysval_IPCVALIDATE};
/*******************************************************************/
static char *
sysprt_CPUIIPC(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[15];
        float ipc = 0.0;

	switch (sstat->cpu.all.cycle)
	{
	   case 0:
		*color = FGCOLORINFO;
        	pmsprintf(buf, sizeof buf, "ipc notavail");
		break;

	   case 1:
		*color = FGCOLORINFO;
        	pmsprintf(buf, sizeof buf, "ipc  initial");
		break;

	   default:
		if (sstat->cpu.cpu[as->index].cycle)
			ipc = sstat->cpu.cpu[as->index].instr * 100 /
				sstat->cpu.cpu[as->index].cycle / 100.0;
		if (ipc < 0.0)
			ipc = 0.0;
        	pmsprintf(buf, sizeof buf, "ipc %8.2f", ipc);
	}

        return buf;
}

sys_printdef syspdef_CPUIIPC = {"CPUIIPC", sysprt_CPUIIPC, sysval_IPCVALIDATE};
/*******************************************************************/
static char *
sysprt_CPUCYCLE(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[15] = "cycl ";

	switch (sstat->cpu.all.cycle)
	{
	   case 0:
		*color = FGCOLORINFO;
        	pmsprintf(buf+5, sizeof buf-5, "missing");
		break;

	   case 1:
		*color = FGCOLORINFO;
        	pmsprintf(buf+5, sizeof buf-5, "initial");
		break;

	   default:
        	val2Hzstr(sstat->cpu.all.cycle/1000000/as->nsecs/
					sstat->cpu.nrcpu, buf+5, sizeof buf-5);
	}

        return buf;
}

sys_printdef syspdef_CPUCYCLE = {"CPUCYCLE", sysprt_CPUCYCLE, sysval_IPCVALIDATE};
/*******************************************************************/
static char *
sysprt_CPUICYCLE(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[15] = "cycl ";

	switch (sstat->cpu.all.cycle)
	{
	   case 0:
		*color = FGCOLORINFO;
        	pmsprintf(buf+5, sizeof buf-5, "missing");
		break;

	   case 1:
		*color = FGCOLORINFO;
        	pmsprintf(buf+5, sizeof buf-5, "initial");
		break;

	   default:
        	val2Hzstr(sstat->cpu.cpu[as->index].cycle/1000000/
						as->nsecs, buf+5, sizeof buf-5);
	}

        return buf;
}

sys_printdef syspdef_CPUICYCLE = {"CPUICYCLE", sysprt_CPUICYCLE, sysval_IPCVALIDATE};
/*******************************************************************/
static char *
sysprt_CPLAVG1(struct sstat *sstat, extraparam *notused, int badness, int *color) 
{
        static char buf[17]="avg1 ";

        if (sstat->cpu.lavg1 > 999999.0)
	{
                pmsprintf(buf+5, sizeof buf-5, ">999999");
	}
	else if (sstat->cpu.lavg1 > 999.0)
        {
                pmsprintf(buf+5, sizeof buf-5, "%7.0f", sstat->cpu.lavg1);
        }
        else
        {
                pmsprintf(buf+5, sizeof buf-5, "%7.2f", sstat->cpu.lavg1);
        }
        return buf;
}
        
sys_printdef syspdef_CPLAVG1 = {"CPLAVG1", sysprt_CPLAVG1, NULL};
/*******************************************************************/
static char *
sysprt_CPLAVG5(struct sstat *sstat, extraparam *notused, int badness, int *color) 
{
        static char buf[15]="avg5 ";

        if (sstat->cpu.lavg5 > 999999.0)
	{
                pmsprintf(buf+5, sizeof buf-5, ">999999");
	}
	else if (sstat->cpu.lavg5 > 999.0)
        {
                pmsprintf(buf+5, sizeof buf-5, "%7.0f", sstat->cpu.lavg5);
        }
        else
        {
                pmsprintf(buf+5, sizeof buf-5, "%7.2f", sstat->cpu.lavg5);
        }
        return buf;
}
        
sys_printdef syspdef_CPLAVG5 = {"CPLAVG5", sysprt_CPLAVG5, NULL};
/*******************************************************************/
static char *
sysprt_CPLAVG15(struct sstat *sstat, extraparam *notused, int badness, int *color) 
{
        static char buf[15]="avg15 ";

	if (sstat->cpu.lavg15 > (2 * sstat->cpu.nrcpu) )
		*color = FGCOLORALMOST;

        if (sstat->cpu.lavg15 > 99999.0)
	{
                pmsprintf(buf+6, sizeof buf-6, ">99999");
	}
	else if (sstat->cpu.lavg15 > 999.0)
        {
                pmsprintf(buf+6, sizeof buf-6, "%6.0f", sstat->cpu.lavg15);
        }
        else
        {
                pmsprintf(buf+6, sizeof buf-6, "%6.2f", sstat->cpu.lavg15);
        }
        return buf;
}
        
sys_printdef syspdef_CPLAVG15 = {"CPLAVG15", sysprt_CPLAVG15, NULL};
/*******************************************************************/
static char *
sysprt_CPLCSW(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="csw    ";

        val2valstr(sstat->cpu.csw, buf+4, sizeof buf-4, 8,as->avgval,as->nsecs);
        return buf;
}

sys_printdef syspdef_CPLCSW = {"CPLCSW", sysprt_CPLCSW, NULL};
/*******************************************************************/
static char *
sysprt_PRCCLONES(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="clones ";

        val2valstr(sstat->cpu.nprocs, buf+7, sizeof buf-7, 5,as->avgval,as->nsecs);
        return buf;
}

sys_printdef syspdef_PRCCLONES = {"PRCCLONES", sysprt_PRCCLONES, NULL};
/*******************************************************************/
static char *
sysprt_CPLNUMCPU(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="numcpu ";

        val2valstr(sstat->cpu.nrcpu, buf+7, sizeof buf-7, 5,0,as->nsecs);
        return buf;
}

sys_printdef syspdef_CPLNUMCPU = {"CPLNUMCPU", sysprt_CPLNUMCPU, NULL};
/*******************************************************************/
static char *
sysprt_CPLINTR(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="intr   ";

        val2valstr(sstat->cpu.devint, buf+5, sizeof buf-5, 7,as->avgval,as->nsecs);
        return buf;
}

sys_printdef syspdef_CPLINTR = {"CPLINTR", sysprt_CPLINTR, NULL};
/*******************************************************************/
static char *
sysprt_GPUBUS(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char 	buf[16];
	char		*pn;
	int		len;

        if ( (len = strlen(sstat->gpu.gpu[as->index].busid)) > 9)
		pn = sstat->gpu.gpu[as->index].busid + len - 9;
	else
		pn = sstat->gpu.gpu[as->index].busid;

        pmsprintf(buf, sizeof buf, "%9.9s %2d", pn, sstat->gpu.gpu[as->index].gpunr);
        return buf;
}

sys_printdef syspdef_GPUBUS = {"GPUBUS", sysprt_GPUBUS, NULL};
/*******************************************************************/
static char *
sysprt_GPUTYPE(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char 	buf[16];
	char		*pn;
	int		len;

        if ( (len = strlen(sstat->gpu.gpu[as->index].type)) > 12)
		pn = sstat->gpu.gpu[as->index].type + len - 12;
	else
		pn = sstat->gpu.gpu[as->index].type;

        pmsprintf(buf, sizeof buf, "%12.12s", pn);
        return buf;
}

sys_printdef syspdef_GPUTYPE = {"GPUTYPE", sysprt_GPUTYPE, NULL};
/*******************************************************************/
static char *
sysprt_GPUNRPROC(struct sstat *sstat, extraparam *as, int badness, int *color)
{
	static char buf[16] = "#proc    ";

	val2valstr(sstat->gpu.gpu[as->index].nrprocs, buf+6, sizeof buf-6, 6, 0, 0);
	return buf;
}

sys_printdef syspdef_GPUNRPROC = {"GPUNRPROC", sysprt_GPUNRPROC, NULL};
/*******************************************************************/
static char *
sysprt_GPUMEMPERC(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="membusy   ";
	int perc = sstat->gpu.gpu[as->index].mempercnow;

	if (perc == -1)
	{
        	pmsprintf(buf+8, sizeof buf-8, " N/A");
	}
	else
	{
		// preferably take the average percentage over sample
		if (sstat->gpu.gpu[as->index].samples)
			perc = sstat->gpu.gpu[as->index].memperccum /
			       sstat->gpu.gpu[as->index].samples;

		if (perc >= 40)
			*color = FGCOLORALMOST;

        	pmsprintf(buf+8, sizeof buf-8, "%3d%%", perc);
	}

        return buf;
}

sys_printdef syspdef_GPUMEMPERC = {"GPUMEMPERC", sysprt_GPUMEMPERC, NULL};
/*******************************************************************/
static char *
sysprt_GPUGPUPERC(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="gpubusy   ";
	int perc = sstat->gpu.gpu[as->index].gpupercnow;

	if (perc == -1)		// metric not available?
	{
        	pmsprintf(buf+8, sizeof buf-8, " N/A");
	}
	else
	{
		// preferably take the average percentage over sample
		if (sstat->gpu.gpu[as->index].samples)
			perc = sstat->gpu.gpu[as->index].gpuperccum /
			       sstat->gpu.gpu[as->index].samples;

		if (perc >= 90)
			*color = FGCOLORALMOST;

        	pmsprintf(buf+8, sizeof buf-8, "%3d%%", perc);
	}

        return buf;
}

sys_printdef syspdef_GPUGPUPERC = {"GPUGPUPERC", sysprt_GPUGPUPERC, NULL};
/*******************************************************************/
static char *
sysprt_GPUMEMOCC(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="memocc   ";
	int perc;

	perc = sstat->gpu.gpu[as->index].memusenow * 100 /
	      (sstat->gpu.gpu[as->index].memtotnow ?
	       sstat->gpu.gpu[as->index].memtotnow : 1);

        pmsprintf(buf+7, sizeof buf-7, "%4d%%", perc);

        return buf;
}

sys_printdef syspdef_GPUMEMOCC = {"GPUMEMOCC", sysprt_GPUMEMOCC, NULL};
/*******************************************************************/
static char *
sysprt_GPUMEMTOT(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16] = "total   ";

        val2memstr(sstat->gpu.gpu[as->index].memtotnow * 1024, buf+6,
						sizeof buf-6, MBFORMAT, 0, 0);
        return buf;
}

sys_printdef syspdef_GPUMEMTOT = {"GPUMEMTOT", sysprt_GPUMEMTOT, NULL};
/*******************************************************************/
static char *
sysprt_GPUMEMUSE(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16] = "used    ";

        val2memstr(sstat->gpu.gpu[as->index].memusenow * 1024,
				buf+6, sizeof buf-6, MBFORMAT, 0, 0);
        return buf;
}

sys_printdef syspdef_GPUMEMUSE = {"GPUMEMUSE", sysprt_GPUMEMUSE, NULL};
/*******************************************************************/
static char *
sysprt_GPUMEMAVG(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16] = "usavg   ";

	if (sstat->gpu.gpu[as->index].samples)
		val2memstr(sstat->gpu.gpu[as->index].memusecum * 1024 /
		      	   sstat->gpu.gpu[as->index].samples,
				buf+6, sizeof buf-6, MBFORMAT, 0, 0);
	else
		return "usavg ?";

        return buf;
}

sys_printdef syspdef_GPUMEMAVG = {"GPUMEMAVG", sysprt_GPUMEMAVG, NULL};
/*******************************************************************/
static char *
sysprt_MEMTOT(struct sstat *sstat, extraparam *notused, int badness, int *color) 
{
        static char buf[16]="tot   ";
        count_t value = sstat->mem.physmem * 1024;
        val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
	*color = -1;
        return buf;
}

sys_printdef syspdef_MEMTOT = {"MEMTOT", sysprt_MEMTOT, NULL};
/*******************************************************************/
static char *
sysprt_MEMFREE(struct sstat *sstat, extraparam *notused, int badness, int *color) 
{
        static char buf[16]="free  ";
        count_t value = sstat->mem.freemem * 1024;
	*color = -1;
        val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
        return buf;
}

sys_printdef syspdef_MEMFREE = {"MEMFREE", sysprt_MEMFREE, NULL};
/*******************************************************************/
static char *
sysprt_MEMAVAIL(struct sstat *sstat, extraparam *notused, int badness, int *color) 
{
        static char buf[16]="avail ";
	*color = -1;
        val2memstr(sstat->mem.availablemem * pagesize, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
        return buf;
}

sys_printdef syspdef_MEMAVAIL = {"MEMAVAIL", sysprt_MEMAVAIL, NULL};
/*******************************************************************/
static char *
sysprt_MEMCACHE(struct sstat *sstat, extraparam *notused, int badness, int *color) 
{
        static char buf[16]="cache ";
        count_t value = sstat->mem.cachemem * 1024;
        val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
	*color = -1;
        return buf;
}

sys_printdef syspdef_MEMCACHE = {"MEMCACHE", sysprt_MEMCACHE};
/*******************************************************************/
static char *
sysprt_MEMDIRTY(struct sstat *sstat, extraparam *notused, int badness, int *color) 
{
        static char buf[16] = "dirty ";
        count_t value = sstat->mem.cachedrt * 1024;
        val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
	*color = -1;
        return buf;
}

sys_printdef syspdef_MEMDIRTY = {"MEMDIRTY", sysprt_MEMDIRTY, NULL};
/*******************************************************************/
static char *
sysprt_MEMBUFFER(struct sstat *sstat, extraparam *notused, int badness, int *color) 
{
        static char buf[16]="buff  ";
        count_t value = sstat->mem.buffermem * 1024;
        val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
	*color = -1;
        return buf;
}

sys_printdef syspdef_MEMBUFFER = {"MEMBUFFER", sysprt_MEMBUFFER, NULL};
/*******************************************************************/
static char *
sysprt_MEMSLAB(struct sstat *sstat, extraparam *notused, int badness, int *color) 
{
        static char buf[16]="slab  ";
        count_t value = sstat->mem.slabmem * 1024;
        val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
	*color = -1;
        return buf;
}

sys_printdef syspdef_MEMSLAB = {"MEMSLAB", sysprt_MEMSLAB, NULL};
/*******************************************************************/
static char *
sysprt_RECSLAB(struct sstat *sstat, extraparam *notused, int badness, int *color) 
{
        static char buf[16]="slrec ";
        count_t value = sstat->mem.slabreclaim * 1024;
        val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
	*color = -1;
        return buf;
}

sys_printdef syspdef_RECSLAB = {"RECSLAB", sysprt_RECSLAB, NULL};
/*******************************************************************/
static char *
sysprt_PAGETABS(struct sstat *sstat, extraparam *notused, int badness, int *color) 
{
        static char buf[16]="pgtab    ";
        count_t value = sstat->mem.pagetables * 1024;
	*color = -1;
	val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
        return buf;
}

sys_printdef syspdef_PAGETABS = {"PAGETABS", sysprt_PAGETABS, NULL};
/*******************************************************************/
static char *
sysprt_SHMEM(struct sstat *sstat, extraparam *notused, int badness, int *color) 
{
        static char buf[16]="shmem  ";
        count_t value = sstat->mem.shmem * 1024;
        val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
	*color = -1;
        return buf;
}

sys_printdef syspdef_SHMEM = {"SHMEM", sysprt_SHMEM, NULL};
/*******************************************************************/
static char *
sysprt_SHMRSS(struct sstat *sstat, extraparam *notused, int badness, int *color) 
{
        static char buf[16]="shrss  ";
        count_t value = sstat->mem.shmrss * 1024;
        val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
	*color = -1;
        return buf;
}

sys_printdef syspdef_SHMRSS = {"SHMRSS", sysprt_SHMRSS, NULL};
/*******************************************************************/
static char *
sysprt_SHMSWP(struct sstat *sstat, extraparam *notused, int badness, int *color) 
{
        static char buf[16]="shswp  ";
        count_t value = sstat->mem.shmswp * 1024;
        val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
	*color = -1;
        return buf;
}

sys_printdef syspdef_SHMSWP = {"SHMSWP", sysprt_SHMSWP, NULL};
/*******************************************************************/
static char *
sysprt_ANONTHP(struct sstat *sstat, extraparam *notused, int badness, int *color)
{
        static char buf[16]="anthp  ";
	*color = -1;
        val2memstr(sstat->mem.anonhugepage * 1024, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
        return buf;
}

static int
sysval_ANONTHP(struct sstat *sstat)
{
	return sstat->mem.anonhugepage;
}

sys_printdef syspdef_ANONTHP = {"ANONTHP", sysprt_ANONTHP, sysval_ANONTHP};
/*******************************************************************/
static char *
sysprt_HUPTOT(struct sstat *sstat, extraparam *notused, int badness, int *color) 
{
        static char buf[16]="hptot  ";

	*color = -1;
        val2memstr(sstat->mem.stothugepage * 1024 + sstat->mem.ltothugepage * 1024,
						buf+6, sizeof buf-6, MBFORMAT, 0, 0);
        return buf;
}

static int
sysval_HUPTOT(struct sstat *sstat)
{
	return sstat->mem.stothugepage + sstat->mem.ltothugepage;
}

sys_printdef syspdef_HUPTOT = {"HUPTOT", sysprt_HUPTOT, sysval_HUPTOT};
/*******************************************************************/
static char *
sysprt_HUPUSE(struct sstat *sstat, extraparam *notused, int badness, int *color) 
{
        static char buf[16]="hpuse  ";

	*color = -1;
        val2memstr( (sstat->mem.stothugepage - sstat->mem.sfreehugepage) *
							1024 +
                    (sstat->mem.ltothugepage - sstat->mem.lfreehugepage) *
							1024,
				buf+6, sizeof buf-6, MBFORMAT, 0, 0);
        return buf;
}

static int
sysval_HUPUSE(struct sstat *sstat)
{
	return sstat->mem.stothugepage + sstat->mem.ltothugepage;
}

sys_printdef syspdef_HUPUSE = {"HUPUSE", sysprt_HUPUSE, sysval_HUPUSE};
/*******************************************************************/
static char *
sysprt_VMWBAL(struct sstat *sstat, extraparam *notused, int badness, int *color) 
{
        static char buf[16]="vmbal  ";

	*color = -1;
        val2memstr(sstat->mem.vmwballoon, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
        return buf;
}

static int
sysval_VMWBAL(struct sstat *sstat)
{
	if (sstat->mem.vmwballoon == -1)
		return 0;
	else
		return 1;
}

sys_printdef syspdef_VMWBAL = {"VMWBAL", sysprt_VMWBAL, sysval_VMWBAL};
/*******************************************************************/
static char *
sysprt_ZFSARC(struct sstat *sstat, extraparam *notused, int badness, int *color) 
{
        static char buf[16]="zfarc  ";
	count_t value = sstat->mem.zfsarcsize;
        val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
	*color = -1;
        return buf;
}

static int
sysval_ZFSARC(struct sstat *sstat)
{
	if (sstat->mem.zfsarcsize == -1)
		return 0;
	else
		return 1;
}

sys_printdef syspdef_ZFSARC = {"ZFSARC", sysprt_ZFSARC, sysval_ZFSARC};
/*******************************************************************/
static char *
sysprt_SWPTOT(struct sstat *sstat, extraparam *notused, int badness, int *color) 
{
        static char buf[16]="tot    ";
        count_t value = sstat->mem.totswap * 1024;
        val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
	*color = -1;
        return buf;
}

sys_printdef syspdef_SWPTOT = {"SWPTOT", sysprt_SWPTOT, NULL};
/*******************************************************************/
static char *
sysprt_SWPFREE(struct sstat *sstat, extraparam *notused, int badness, int *color) 
{
        static char buf[16]="free  ";
        count_t value = sstat->mem.freeswap * 1024;
        val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
	*color = -1;
        return buf;
}

sys_printdef syspdef_SWPFREE = {"SWPFREE", sysprt_SWPFREE, NULL};
/*******************************************************************/
static char *
sysprt_SWPCACHE(struct sstat *sstat, extraparam *notused, int badness, int *color) 
{
        static char buf[16]="swcac ";
        count_t value = sstat->mem.swapcached * 1024;
        val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
	*color = -1;
        return buf;
}

sys_printdef syspdef_SWPCACHE = {"SWPCACHE", sysprt_SWPCACHE, NULL};
/*******************************************************************/
static char *
sysprt_ZSWPOOL(struct sstat *sstat, extraparam *notused, int badness, int *color) 
{
        static char buf[16]="zswap ";
        count_t value = sstat->mem.zswap * 1024;
        val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
	*color = -1;
        return buf;
}

sys_printdef syspdef_ZSWPOOL = {"ZSWPOOL", sysprt_ZSWPOOL, NULL};
/*******************************************************************/
static char *
sysprt_ZSWSTORED(struct sstat *sstat, extraparam *notused, int badness, int *color) 
{
        static char buf[16]="zstor ";
        count_t value = sstat->mem.zswapped * 1024;
        val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
	*color = -1;
        return buf;
}

sys_printdef syspdef_ZSWSTORED = {"ZSWSTORED", sysprt_ZSWSTORED, NULL};
/*******************************************************************/
static char *
sysprt_TCPSOCK(struct sstat *sstat, extraparam *as, int badness, int *color)
{
        static char buf[16]="tcpsk ";
	count_t value = sstat->mem.tcpsock * pagesize;
        val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
        return buf;
}

sys_printdef syspdef_TCPSOCK = {"TCPSOCK", sysprt_TCPSOCK, NULL};
/*******************************************************************/
static char *
sysprt_UDPSOCK(struct sstat *sstat, extraparam *as, int badness, int *color)
{
        static char buf[16]="udpsk ";
	count_t value = sstat->mem.udpsock * pagesize;
        val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
        return buf;
}

sys_printdef syspdef_UDPSOCK = {"UDPSOCK", sysprt_UDPSOCK, NULL};
/*******************************************************************/
static char *
sysprt_KSMSHARING(struct sstat *sstat, extraparam *notused, int badness, int *color) 
{
        static char buf[16]="kssav ";
        count_t value = sstat->mem.ksmshared;
        val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
	*color = -1;
        return buf;
}

static int
sysval_KSMSHARING(struct sstat *sstat)
{
	if (sstat->mem.ksmsharing == -1)
		return 0;
	else
		return 1;
}

sys_printdef syspdef_KSMSHARING = {"KSMSHARING", sysprt_KSMSHARING, sysval_KSMSHARING};
/*******************************************************************/
static char *
sysprt_KSMSHARED(struct sstat *sstat, extraparam *notused, int badness, int *color) 
{
        static char buf[16]="ksuse ";
        count_t value = sstat->mem.ksmsharing;
        val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
	*color = -1;
        return buf;
}

static int
sysval_KSMSHARED(struct sstat *sstat)
{
	if (sstat->mem.ksmshared == -1)
		return 0;
	else
		return 1;
}

sys_printdef syspdef_KSMSHARED = {"KSMSHARED", sysprt_KSMSHARED, sysval_KSMSHARED};
/*******************************************************************/
static char *
sysprt_NUMNUMA(struct sstat *sstat, extraparam *notused, int badness, int *color) 
{
        static char buf[16]="numnode  ";
        val2valstr(sstat->memnuma.nrnuma, buf+8, sizeof buf-8, 4, 0, 0);
        return buf;
}

sys_printdef syspdef_NUMNUMA = {"NUMNUMA", sysprt_NUMNUMA, NULL};
/*******************************************************************/
static char *
sysprt_SWPCOMMITTED(struct sstat *sstat, extraparam *notused, int badness, int *color) 
{
        static char buf[16]="vmcom  ";
	count_t value = sstat->mem.committed * 1024;

        val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);

	if (sstat->mem.commitlim && sstat->mem.committed > sstat->mem.commitlim)
		*color = FGCOLORALMOST;

        return buf;
}

sys_printdef syspdef_SWPCOMMITTED = {"SWPCOMMITTED", sysprt_SWPCOMMITTED, NULL};
/*******************************************************************/
static char *
sysprt_SWPCOMMITLIM(struct sstat *sstat, extraparam *notused, int badness, int *color) 
{
        static char buf[16]="vmlim  ";
	count_t value = sstat->mem.commitlim * 1024;

        val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);

	if (sstat->mem.commitlim && sstat->mem.committed > sstat->mem.commitlim)
		*color = FGCOLORINFO;

        return buf;
}

sys_printdef syspdef_SWPCOMMITLIM = {"SWPCOMMITLIM", sysprt_SWPCOMMITLIM, NULL};
/*******************************************************************/
static char *
sysprt_PAGSCAN(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="scan  ";
        val2valstr(sstat->mem.pgscans, buf+5, sizeof buf-5, 7, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_PAGSCAN = {"PAGSCAN", sysprt_PAGSCAN, NULL};
/*******************************************************************/
static char *
sysprt_PAGSTEAL(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="steal  ";
        val2valstr(sstat->mem.pgsteal, buf+6, sizeof buf-6, 6, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_PAGSTEAL = {"PAGSTEAL", sysprt_PAGSTEAL, NULL};
/*******************************************************************/
static char *
sysprt_PAGSTALL(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="stall  ";
        val2valstr(sstat->mem.allocstall, buf+6, sizeof buf-6, 6, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_PAGSTALL = {"PAGSTALL", sysprt_PAGSTALL};
/*******************************************************************/
static char *
sysprt_PAGCOMPACT(struct sstat *sstat, extraparam *as, int badness, int *color)
{
	static char buf[16]="compact ";
	val2valstr(sstat->mem.compactstall, buf+8, sizeof buf-8, 4, as->avgval, as->nsecs);
	return buf;
}

sys_printdef syspdef_PAGCOMPACT = {"PAGCOMPACT", sysprt_PAGCOMPACT, NULL};
/*******************************************************************/
static char *
sysprt_NUMAMIGRATE(struct sstat *sstat, extraparam *as, int badness, int *color)
{
        static char buf[16]="numamig  ";

        val2valstr(sstat->mem.numamigrate, buf+8, sizeof buf-8, 4, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NUMAMIGRATE = {"NUMAMIGRATE", sysprt_NUMAMIGRATE, NULL};

/*******************************************************************/
static char *
sysprt_PGMIGRATE(struct sstat *sstat, extraparam *as, int badness, int *color)
{
        static char buf[16]="migrate  ";

        val2valstr(sstat->mem.pgmigrate, buf+8, sizeof buf-8, 4, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_PGMIGRATE = {"PGMIGRATE", sysprt_PGMIGRATE, NULL};

/*******************************************************************/
static char *
sysprt_PAGPGIN(struct sstat *sstat, extraparam *as, int badness, int *color)
{
        static char buf[16]="pgin   ";
        val2valstr(sstat->mem.pgins, buf+5, sizeof buf-5, 7, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_PAGPGIN = {"PAGPGIN", sysprt_PAGPGIN, NULL};
/*******************************************************************/
static char *
sysprt_PAGPGOUT(struct sstat *sstat, extraparam *as, int badness, int *color)
{
        static char buf[16]="pgout  ";
        val2valstr(sstat->mem.pgouts, buf+6, sizeof buf-6, 6, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_PAGPGOUT = {"PAGPGOUT", sysprt_PAGPGOUT, NULL};
/*******************************************************************/
static char *
sysprt_PAGSWIN(struct sstat *sstat, extraparam *as, int badness, int *color)
{
        static char buf[16]="swin   ";
        val2valstr(sstat->mem.swins, buf+5, sizeof buf-5, 7, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_PAGSWIN = {"PAGSWIN", sysprt_PAGSWIN, NULL};
/*******************************************************************/
static char *
sysprt_PAGSWOUT(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="swout  ";
	*color = -1;
        val2valstr(sstat->mem.swouts, buf+6, sizeof buf-6, 6, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_PAGSWOUT = {"PAGSWOUT", sysprt_PAGSWOUT, NULL};
/*******************************************************************/
static char *
sysprt_PAGZSWIN(struct sstat *sstat, extraparam *as, int badness, int *color)
{
        static char buf[16]="zswin   ";
        val2valstr(sstat->mem.zswins, buf+6, sizeof buf-6, 6, as->avgval, as->nsecs);
        return buf;
}


static int
sysval_ZSWAP(struct sstat *sstat)
{
	return sstat->mem.zswstate[0] == 'Y';
}

sys_printdef syspdef_PAGZSWIN = {"PAGZSWIN", sysprt_PAGZSWIN, sysval_ZSWAP};
/*******************************************************************/
static char *
sysprt_PAGZSWOUT(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="zswout  ";
	*color = -1;
        val2valstr(sstat->mem.zswouts, buf+7, sizeof buf-7, 5, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_PAGZSWOUT = {"PAGZSWOUT", sysprt_PAGZSWOUT, sysval_ZSWAP};
/*******************************************************************/
static char *
sysprt_OOMKILLS(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="oomkill  ";

	if (sstat->mem.oomkills)
		*color = FGCOLORCRIT;

        val2valstr(sstat->mem.oomkills, buf+8, sizeof buf-8, 4, as->avgval, as->nsecs);
        return buf;
}

static int
sysval_OOMKILLS(struct sstat *sstat)
{
	if (sstat->mem.oomkills == -1)	// non-existing?
		return 0;
	else
		return 1;
}

sys_printdef syspdef_OOMKILLS = {"OOMKILLS", sysprt_OOMKILLS, sysval_OOMKILLS};
/*******************************************************************/
static char *
sysprt_NUMATOT(struct sstat *sstat, extraparam *as, int badness, int *color)
{
	static char buf[16]="tot   ";
	count_t value = sstat->memnuma.numa[as->index].totmem * 1024;
	*color = -1;
	val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
	return buf;
}

sys_printdef syspdef_NUMATOT = {"NUMATOT", sysprt_NUMATOT, NULL};
/*******************************************************************/
static char *
sysprt_NUMAFREE(struct sstat *sstat, extraparam *as, int badness, int *color)
{
	static char buf[16]="free  ";
	count_t value = sstat->memnuma.numa[as->index].freemem * 1024;
	*color = -1;
	val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
	return buf;
}

sys_printdef syspdef_NUMAFREE = {"NUMAFREE", sysprt_NUMAFREE, NULL};
/*******************************************************************/
static char *
sysprt_NUMAFILE(struct sstat *sstat, extraparam *as, int badness, int *color)
{
	static char buf[16]="file  ";
	count_t value = sstat->memnuma.numa[as->index].filepage * 1024;
	*color = -1;
	val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
	return buf;
}

sys_printdef syspdef_NUMAFILEPAGE = {"NUMAFILEPAGE", sysprt_NUMAFILE, NULL};
/*******************************************************************/
static char *
sysprt_NUMANR(struct sstat *sstat, extraparam *as, int badness, int *color)
{
	static char buf[16];
	*color = -1;
	pmprintf(buf, sizeof buf, "numanode%04d", as->index);
	return buf;
}

sys_printdef syspdef_NUMANR = {"NUMANR", sysprt_NUMANR, NULL};
/*******************************************************************/
static char *
sysprt_NUMADIRTY(struct sstat *sstat, extraparam *as, int badness, int *color)
{
	static char buf[16]="dirty  ";
	count_t value = sstat->memnuma.numa[as->index].dirtymem * 1024;
	*color = -1;
	val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
	return buf;
}

sys_printdef syspdef_NUMADIRTY = {"NUMADIRTY", sysprt_NUMADIRTY, NULL};
/*******************************************************************/
static char *
sysprt_NUMAACTIVE(struct sstat *sstat, extraparam *as, int badness, int *color)
{
	static char buf[16]="activ  ";
	count_t value = sstat->memnuma.numa[as->index].active * 1024;
	*color = -1;
	val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
	return buf;
}

sys_printdef syspdef_NUMAACTIVE = {"NUMAACTIVE", sysprt_NUMAACTIVE, NULL};
/*******************************************************************/
static char *
sysprt_NUMAINACTIVE(struct sstat *sstat, extraparam *as, int badness, int *color)
{
	static char buf[16]="inact   ";
	count_t value = sstat->memnuma.numa[as->index].inactive * 1024;
	*color = -1;
	val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
	return buf;
}

sys_printdef syspdef_NUMAINACTIVE = {"NUMAINACTIVE", sysprt_NUMAINACTIVE, NULL};
/*******************************************************************/
static char *
sysprt_NUMASLAB(struct sstat *sstat, extraparam *as, int badness, int *color)
{
	static char buf[16]="slab  ";
	count_t value = sstat->memnuma.numa[as->index].slabmem * 1024;
	*color = -1;
	val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
	return buf;
}

sys_printdef syspdef_NUMASLAB = {"NUMASLAB", sysprt_NUMASLAB, NULL};
/*******************************************************************/
static char *
sysprt_NUMASLABRECLAIM(struct sstat *sstat, extraparam *as, int badness, int *color)
{
	static char buf[16]="slrec ";
	count_t value = sstat->memnuma.numa[as->index].slabreclaim * 1024;
	*color = -1;
	val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
	return buf;
}

sys_printdef syspdef_NUMASLABRECLAIM = {"NUMASLABRECLAIM", sysprt_NUMASLABRECLAIM, NULL};
/*******************************************************************/
static char *
sysprt_NUMASHMEM(struct sstat *sstat, extraparam *as, int badness, int *color)
{
	static char buf[16]="shmem  ";
	count_t value = sstat->memnuma.numa[as->index].shmem * 1024;
	*color = -1;
	val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
	return buf;
}

sys_printdef syspdef_NUMASHMEM = {"NUMASHMEM", sysprt_NUMASHMEM, NULL};
/*******************************************************************/
static char *
sysprt_NUMAFRAG(struct sstat *sstat, extraparam *as, int badness, int *color)
{
	static char buf[15];
	float perc = sstat->memnuma.numa[as->index].frag * 100.0;

	if (perc < 0.0)
		perc = 0.0;
	if (perc > 1.0)
		*color = -1;

	pmsprintf(buf, sizeof buf, "frag %6.0f%%", perc);
	return buf;
}

sys_printdef syspdef_NUMAFRAG = {"NUMAFRAG", sysprt_NUMAFRAG, NULL};
/*******************************************************************/
static char *
sysprt_NUMAHUPTOT(struct sstat *sstat, extraparam *as, int badness, int *color)
{
	static char buf[16]="hptot  ";
	count_t value;

	if (sstat->mem.stothugepage == 0)
		return NULL;

	*color = -1;
	value = sstat->memnuma.numa[as->index].tothp * 1024;
	val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
	return buf;
}

sys_printdef syspdef_NUMAHUPTOT = {"NUMAHUPTOT", sysprt_NUMAHUPTOT, sysval_HUPTOT};
/*******************************************************************/
static char *
sysprt_NUMAHUPUSE(struct sstat *sstat, extraparam *as, int badness, int *color)
{
	static char buf[16]="hpuse  ";
	if (sstat->mem.stothugepage == 0)
		return NULL;

	*color = -1;
	val2memstr( (sstat->memnuma.numa[as->index].tothp - sstat->memnuma.numa[as->index].freehp)
			* 1024, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
	return buf;
}

sys_printdef syspdef_NUMAHUPUSE = {"NUMAHUPUSE", sysprt_NUMAHUPUSE, sysval_HUPUSE};
/*******************************************************************/
static char *
sysprt_NUMANUMCPU(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="numcpu ";
	count_t value = sstat->cpunuma.numa[as->index].nrcpu;

        val2valstr(value, buf+7, sizeof buf-7, 5, 0, as->nsecs);
        return buf;
}

sys_printdef syspdef_NUMANUMCPU = {"NUMANUMCPU", sysprt_NUMANUMCPU, NULL};
/*******************************************************************/
static char *
sysprt_NUMACPUSYS(struct sstat *sstat, extraparam *as, int badness, int *color)
{
	static char buf[15];
	float perc = sstat->cpunuma.numa[as->index].stime * 100.0 / as->percputot;

	if (perc < 0.0)
		perc = 0.0;
	if (perc > 1.0)
		*color = -1;

	pmsprintf(buf, sizeof buf, "sys  %6.0f%%", perc);
	return buf;
}

sys_printdef syspdef_NUMACPUSYS = {"NUMACPUSYS", sysprt_NUMACPUSYS, NULL};
/*******************************************************************/
static char *
sysprt_NUMACPUUSER(struct sstat *sstat, extraparam *as, int badness, int *color)
{
	static char buf[15];
	float perc = sstat->cpunuma.numa[as->index].utime * 100.0 / as->percputot;

	if (perc < 0.0)
		perc = 0.0;
	if (perc > 1.0)
		*color = -1;

	pmsprintf(buf, sizeof buf, "user %6.0f%%", perc);
	return buf;
}

sys_printdef syspdef_NUMACPUUSER = {"NUMACPUUSER", sysprt_NUMACPUUSER, NULL};
/*******************************************************************/
static char *
sysprt_NUMACPUNICE(struct sstat *sstat, extraparam *as, int badness, int *color)
{
	static char buf[15];
	float perc = sstat->cpunuma.numa[as->index].ntime * 100.0 / as->percputot;

	if (perc < 0.0)
		perc = 0.0;
	if (perc > 1.0)
		*color = -1;

	pmsprintf(buf, sizeof buf, "nice %6.0f%%", perc);
	return buf;
}

sys_printdef syspdef_NUMACPUNICE = {"NUMACPUNICE", sysprt_NUMACPUNICE, NULL};
/*******************************************************************/
static char *
sysprt_NUMACPUIRQ(struct sstat *sstat, extraparam *as, int badness, int *color)
{
	static char buf[15];
	float perc = sstat->cpunuma.numa[as->index].Itime * 100.0 / as->percputot;

	if (perc < 0.0)
		perc = 0.0;
	if (perc > 1.0)
		*color = -1;

	pmsprintf(buf, sizeof buf, "irq  %6.0f%%", perc);
	return buf;
}

sys_printdef syspdef_NUMACPUIRQ = {"NUMACPUIRQ", sysprt_NUMACPUIRQ, NULL};
/*******************************************************************/
static char *
sysprt_NUMACPUSOFTIRQ(struct sstat *sstat, extraparam *as, int badness, int *color)
{
	static char buf[15];
	float perc = sstat->cpunuma.numa[as->index].Stime * 100.0 / as->percputot;

	if (perc < 0.0)
		perc = 0.0;
	if (perc > 1.0)
		*color = -1;

	pmsprintf(buf, sizeof buf, "sirq %6.0f%%", perc);
	return buf;
}

sys_printdef syspdef_NUMACPUSOFTIRQ = {"NUMACPUSOFTIRQ", sysprt_NUMACPUSOFTIRQ, NULL};
/*******************************************************************/
static char *
sysprt_NUMACPUIDLE(struct sstat *sstat, extraparam *as, int badness, int *color)
{
	static char buf[15];

	pmsprintf(buf, sizeof buf, "idle %6.0f%%",
		(sstat->cpunuma.numa[as->index].itime * 100.0) / as->percputot);
	return buf;
}

sys_printdef syspdef_NUMACPUIDLE = {"NUMACPUIDLE", sysprt_NUMACPUIDLE, NULL};
/*******************************************************************/
static char *
sysprt_NUMACPUWAIT(struct sstat *sstat, extraparam *as, int badness, int *color)
{
	static char buf[15];

	pmsprintf(buf, sizeof buf, "nod%03d w%3.0f%%",
		as->index,
		(sstat->cpunuma.numa[as->index].wtime * 100.0) / as->percputot);
	return buf;
}

sys_printdef syspdef_NUMACPUWAIT = {"NUMACPUWAIT", sysprt_NUMACPUWAIT, NULL};
/*******************************************************************/
static char *
sysprt_NUMACPUSTEAL(struct sstat *sstat, extraparam *as, int badness, int *color)
{
	static char buf[15];
	float perc = (sstat->cpunuma.numa[as->index].steal * 100.0)
							/ as->percputot;

	if (perc < 0.0)
		perc = 0.0;
	if (perc > 1.0)
		*color = -1;

	pmsprintf(buf, sizeof buf, "steal %5.0f%%", perc);
	return buf;
}

sys_printdef syspdef_NUMACPUSTEAL = {"NUMACPUSTEAL", sysprt_NUMACPUSTEAL, NULL};
/*******************************************************************/
static char *
sysprt_NUMACPUGUEST(struct sstat *sstat, extraparam *as, int badness, int *color)
{
	static char buf[15];
	float perc = (sstat->cpunuma.numa[as->index].guest * 100.0)
							/ as->percputot;

	if (perc < 0.0)
		perc = 0.0;
	if (perc > 1.0)
		*color = -1;

	pmsprintf(buf, sizeof buf, "guest %5.0f%%", perc);
	return buf;
}

sys_printdef syspdef_NUMACPUGUEST = {"NUMACPUGUEST", sysprt_NUMACPUGUEST, NULL};
/*******************************************************************/
static char *
sysprt_LLCMBMTOTAL(struct sstat *sstat, extraparam *as, int badness, int *color)
{
	static char buf[16]="tot   ";
	count_t value = sstat->llc.perllc[as->index].mbm_total;
	*color = -1;

	val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, as->avgval, as->nsecs);
	return buf;
}

sys_printdef syspdef_LLCMBMTOTAL = {"LLCMBMTOTAL", sysprt_LLCMBMTOTAL, NULL};
/*******************************************************************/
static char *
sysprt_LLCMBMLOCAL(struct sstat *sstat, extraparam *as, int badness, int *color)
{
	static char buf[16]="loc   ";
	count_t value = sstat->llc.perllc[as->index].mbm_local;
	*color = -1;
	val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, as->avgval, as->nsecs);
	return buf;
}

sys_printdef syspdef_LLCMBMLOCAL = {"LLCMBMLOCAL", sysprt_LLCMBMLOCAL, NULL};
/*******************************************************************/
static char *
sysprt_NUMLLC(struct sstat *sstat, extraparam *as, int badness, int *color)
{
	static char buf[16];

	*color = -1;
	pmsprintf(buf, sizeof buf, "LLC%02d %5.0f%%",
			sstat->llc.perllc[as->index].id,
			sstat->llc.perllc[as->index].occupancy * 100);
	return buf;
}

sys_printdef syspdef_NUMLLC = {"NUMLLC", sysprt_NUMLLC, NULL};
/*******************************************************************/
// general formatting of PSI field in avg10/avg60/avg300
static void
psiformatavg(struct psi *p, char *head, char *buf, int bufsize)
{
	static char	formats[] = "%.0f/%.0f/%.0f";
	char		tmpbuf[32];

	pmsprintf(tmpbuf, sizeof tmpbuf, formats, p->avg10, p->avg60, p->avg300);

	if (strlen(tmpbuf) > 9)	// reformat needed?
	{
		float avg10  = p->avg10;
		float avg60  = p->avg60;
		float avg300 = p->avg300;

		if (avg10 > 99.0)
			avg10 = 99.0;
		if (avg60 > 99.0)
			avg60 = 99.0;
		if (avg300 > 99.0)
			avg300 = 99.0;

		pmsprintf(tmpbuf, sizeof tmpbuf, formats, avg10, avg60, avg300);
	}

	pmsprintf(buf, bufsize, "%s %9.9s", head, tmpbuf);
}

static char *
sysprt_PSICPUS(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16];
	psiformatavg(&(sstat->psi.cpusome), "cs", buf, sizeof buf);
        return buf;
}
sys_printdef syspdef_PSICPUS = {"PSICPUS", sysprt_PSICPUS, NULL};

static char *
sysprt_PSIMEMS(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16];
	psiformatavg(&(sstat->psi.memsome), "ms", buf, sizeof buf);
        return buf;
}
sys_printdef syspdef_PSIMEMS = {"PSIMEMS", sysprt_PSIMEMS, NULL};

static char *
sysprt_PSIMEMF(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16];
	psiformatavg(&(sstat->psi.memfull), "mf", buf, sizeof buf);
        return buf;
}
sys_printdef syspdef_PSIMEMF = {"PSIMEMF", sysprt_PSIMEMF, NULL};

static char *
sysprt_PSIIOS(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16];
	psiformatavg(&(sstat->psi.iosome), "is", buf, sizeof buf);
        return buf;
}
sys_printdef syspdef_PSIIOS = {"PSIIOS", sysprt_PSIIOS, NULL};

static char *
sysprt_PSIIOF(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16];
	psiformatavg(&(sstat->psi.iofull), "if", buf, sizeof buf);
        return buf;
}
sys_printdef syspdef_PSIIOF = {"PSIIOF", sysprt_PSIIOF, NULL};

/*******************************************************************/
// general formatting of PSI field in total percentage
static void
psiformattot(struct psi *p, char *head, extraparam *as, int *color,
						char *buf, int bufsize)
{
	static char	formats[] = "%-7.7s %3lu%%";
	unsigned long 	perc = p->total/((count_t)as->nsecs*10000);

	if (perc > 100)
		perc = 100;

	if (perc >= 1)
		*color = FGCOLORALMOST;

	pmsprintf(buf, bufsize, formats, head, perc);
}

static char *
sysprt_PSICPUSTOT(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[32];

	psiformattot(&(sstat->psi.cpusome), "cpusome", as, color, buf, sizeof buf);
        return buf;
}
sys_printdef syspdef_PSICPUSTOT = {"PSICPUSTOT", sysprt_PSICPUSTOT, NULL};

static char *
sysprt_PSIMEMSTOT(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[32];

	psiformattot(&(sstat->psi.memsome), "memsome", as, color, buf, sizeof buf);
        return buf;
}
sys_printdef syspdef_PSIMEMSTOT = {"PSIMEMSTOT", sysprt_PSIMEMSTOT, NULL};

static char *
sysprt_PSIMEMFTOT(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[32];

	psiformattot(&(sstat->psi.memfull), "memfull", as, color, buf, sizeof buf);
        return buf;
}
sys_printdef syspdef_PSIMEMFTOT = {"PSIMEMFTOT", sysprt_PSIMEMFTOT, NULL};

static char *
sysprt_PSIIOSTOT(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[32];

	psiformattot(&(sstat->psi.iosome), "iosome", as, color, buf, sizeof buf);
        return buf;
}
sys_printdef syspdef_PSIIOSTOT = {"PSIIOSTOT", sysprt_PSIIOSTOT, NULL};


static char *
sysprt_PSIIOFTOT(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[32];

	psiformattot(&(sstat->psi.iofull), "iofull", as, color, buf, sizeof buf);
        return buf;
}
sys_printdef syspdef_PSIIOFTOT = {"PSIIOFTOT", sysprt_PSIIOFTOT, NULL};

/*******************************************************************/
static char *
sysprt_CONTNAME(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char 	buf[32] = "ctid ";

	*color = -1;

        pmsprintf(buf+5, sizeof buf-5, "%7lu", sstat->cfs.cont[as->index].ctid);
        return buf;
}

sys_printdef syspdef_CONTNAME = {"CONTNAME", sysprt_CONTNAME, NULL};
/*******************************************************************/
static char *
sysprt_CONTNPROC(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="nproc  ";

	*color = -1;

        val2valstr(sstat->cfs.cont[as->index].numproc, 
                 	  buf+6, sizeof buf-6, 6, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_CONTNPROC = {"CONTNPROC", sysprt_CONTNPROC, NULL};
/*******************************************************************/
static char *
sysprt_CONTCPU(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16];
	float  perc;

	count_t	used = sstat->cfs.cont[as->index].system + 
                       sstat->cfs.cont[as->index].user + 
                       sstat->cfs.cont[as->index].nice;

	*color = -1;

	if (sstat->cfs.cont[as->index].uptime)
	{
		perc = used * 100.0 / sstat->cfs.cont[as->index].uptime;
		if (perc < 0)
			perc = 0;
        	pmsprintf(buf, sizeof buf, "cpubusy %3.0f%%", perc);
	}
	else
        	pmsprintf(buf, sizeof buf, "cpubusy   ?%%");

        return buf;
}

sys_printdef syspdef_CONTCPU = {"CONTCPU", sysprt_CONTCPU, NULL};
/*******************************************************************/
static char *
sysprt_CONTMEM(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="mem   ";

	*color = -1;

        val2memstr(sstat->cfs.cont[as->index].physpages * pagesize,
					buf+6, sizeof buf-6, MBFORMAT, 0, 0);
        return buf;
}

sys_printdef syspdef_CONTMEM = {"CONTMEM", sysprt_CONTMEM, NULL};
/*******************************************************************/
static char *
sysprt_DSKNAME(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char 	buf[16];
	char		*pn;
	int		len;

	*color = -1;

        if ( (len = strlen(as->perdsk[as->index].name)) > 12)
		pn = as->perdsk[as->index].name + len - 12;
	else
		pn = as->perdsk[as->index].name;

        pmsprintf(buf, sizeof buf, "%12.12s", pn);
        return buf;
}

sys_printdef syspdef_DSKNAME = {"DSKNAME", sysprt_DSKNAME, NULL};
/*******************************************************************/
static char *
sysprt_DSKBUSY(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
	double		perc;
        static char 	buf[16]="busy  ";

	*color = -1;

	perc = as->perdsk[as->index].io_ms * 100.0 / as->mstot;

	if (perc >= 0.0 && perc < 1000000.0)
		pmsprintf(buf+5, sizeof buf-5, "%6.0lf%%", perc);
	else
		pmsprintf(buf+5, sizeof buf-5, "%6.0lf%%",  999999.0);

        return buf;
}

sys_printdef syspdef_DSKBUSY = {"DSKBUSY", sysprt_DSKBUSY, NULL};
/*******************************************************************/
static char *
sysprt_DSKNREAD(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char	buf[16]="read  ";

	*color = -1;

        val2valstr(as->perdsk[as->index].nread >= 0 ?
                   as->perdsk[as->index].nread : 0,
                   buf+5, sizeof buf-5, 7, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_DSKNREAD = {"DSKNREAD", sysprt_DSKNREAD, NULL};
/*******************************************************************/
static char *
sysprt_DSKNWRITE(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char	buf[16]="write ";

	*color = -1;

        val2valstr(as->perdsk[as->index].nwrite >= 0 ?
        	           as->perdsk[as->index].nwrite : 0,
        	           buf+6, sizeof buf-6, 6, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_DSKNWRITE = {"DSKNWRITE", sysprt_DSKNWRITE, NULL};
/*******************************************************************/
static char *
sysprt_DSKNDISC(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char	buf[16]="discrd ";

	*color = -1;

	// value might be -1 in case not supported --> "?"
       	val2valstr(as->perdsk[as->index].ndisc,
                   	buf+7, sizeof buf-7, 5, as->avgval, as->nsecs);
        return buf;
}

static int
sysval_DSKNDISK(struct sstat *sstat)
{
	if (sstat->dsk.ndsk > 0 && sstat->dsk.dsk[0].ndisc != -1)
		return 1;
	else
		return 0;
}

sys_printdef syspdef_DSKNDISC = {"DSKNDISC", sysprt_DSKNDISC, sysval_DSKNDISK};
/*******************************************************************/
static char *
sysprt_DSKKBPERRD(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char	buf[16]="KiB/r ";
	struct perdsk 	*dp = &(as->perdsk[as->index]);

        val2valstr(dp->nread > 0 ?  dp->nrsect / dp->nread / 2 : 0,
                   buf+6, sizeof buf-6, 6, 0, as->nsecs);
        return buf;
}

sys_printdef syspdef_DSKKBPERRD = {"DSKKBPERRD", sysprt_DSKKBPERRD, NULL};
/*******************************************************************/
static char *
sysprt_DSKKBPERWR(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char	buf[16]="KiB/w ";
	struct perdsk 	*dp = &(as->perdsk[as->index]);

        val2valstr(dp->nwrite > 0 ?  dp->nwsect / dp->nwrite / 2 : 0,
                   buf+6, sizeof buf-6, 6, 0, as->nsecs);
        return buf;
}

sys_printdef syspdef_DSKKBPERWR = {"DSKKBPERWR", sysprt_DSKKBPERWR, NULL};
/*******************************************************************/
static char *
sysprt_DSKKBPERDS(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char	buf[16]="KiB/d      ?";
	struct perdsk 	*dp = &(as->perdsk[as->index]);

        val2valstr(dp->nread > 0 ?  dp->nrsect / dp->nread / 2 : 0,
                   buf+6, sizeof buf-6, 6, 0, as->nsecs);
        return buf;
}

sys_printdef syspdef_DSKKBPERDS = {"DSKKBPERDS", sysprt_DSKKBPERDS, sysval_DSKNDISK};
/*******************************************************************/
static char *
sysprt_DSKMBPERSECWR(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char	buf[16]="MBw/s ";
	struct perdsk 	*dp = &(as->perdsk[as->index]);

        pmsprintf(buf+6, sizeof buf-6, "%6.1lf",
                               dp->nwsect / 2.0 / 1024 / as->nsecs);
        return buf;
}

sys_printdef syspdef_DSKMBPERSECWR = {"DSKMBPERSECWR", sysprt_DSKMBPERSECWR, NULL};
/*******************************************************************/
static char *
sysprt_DSKMBPERSECRD(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char	buf[16]="MBr/s ";
	struct perdsk 	*dp = &(as->perdsk[as->index]);

        pmsprintf(buf+6, sizeof buf-6, "%6.1lf",
                               dp->nrsect / 2.0 / 1024 / as->nsecs);
        return buf;
}

sys_printdef syspdef_DSKMBPERSECRD = {"DSKMBPERSECRD", sysprt_DSKMBPERSECRD, NULL};
/*******************************************************************/
static char *
sysprt_DSKINFLIGHT(struct sstat *sstat, extraparam *as, int badness, int *color)
{
	static char    buf[16]="inflt  ";
	struct perdsk  *dp = &(as->perdsk[as->index]);

	val2valstr(dp->inflight, buf+6, sizeof buf-6, 6, 0, 0);
	return buf;
}

sys_printdef syspdef_DSKINFLIGHT = {"DSKINFLIGHT", sysprt_DSKINFLIGHT, NULL};
/*******************************************************************/
static char *
sysprt_DSKAVQUEUE(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char	buf[16]="avq  ";
	struct perdsk 	*dp = &(as->perdsk[as->index]);

	pmsprintf(buf+4, sizeof buf-4, "%8.2f", dp->io_ms ?
                               (double)dp->avque / dp->io_ms : 0.0);
        return buf;
}

sys_printdef syspdef_DSKAVQUEUE = {"DSKAVQUEUE", sysprt_DSKAVQUEUE, NULL};
/*******************************************************************/
static char *
sysprt_DSKAVIO(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char	buf[32]="avio  ";
        double 		avioms = as->iotot > 0 ? 
                     	(double)(as->perdsk[as->index].io_ms)/as->iotot:0.0;

	*color = -1;

	if (avioms >= 9999.0)
	{
		val2valstr((unsigned long long)avioms / 1000, buf+5, sizeof buf-5, 5, 0, 0);
		pmsprintf(buf+10, sizeof buf-10, " s");
	}
        else if (avioms >= 9.995) 
        {
                pmsprintf(buf+5, sizeof buf-5, "%4.1lf ms", avioms);
        }
        else if (avioms >= 0.09995)
        {
                pmsprintf(buf+5, sizeof buf-5, "%4.2lf ms", avioms);
        }
        else if (avioms >= 0.01)
        {
                pmsprintf(buf+5, sizeof buf-5, "%4.1lf µs", avioms * 1000.0);
        }
        else if (avioms >= 0.0001)
        {
                pmsprintf(buf+5, sizeof buf-5, "%4.2lf µs", avioms * 1000.0);
        }
        else
        {
	        if (avioms < 0)
                        avioms = 0;
                pmsprintf(buf+5, sizeof buf-5, "%4.1lf ns", avioms * 1000000.0);
        }

        return buf;
}

sys_printdef syspdef_DSKAVIO = {"DSKAVIO", sysprt_DSKAVIO, NULL};
/*******************************************************************/
static char *
sysprt_NETTRANSPORT(struct sstat *sstat, extraparam *notused, int badness, int *color) 
{
        return "transport   ";
}

sys_printdef syspdef_NETTRANSPORT = {"NETTRANSPORT", sysprt_NETTRANSPORT, NULL};
/*******************************************************************/
static char *
sysprt_NETTCPI(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="tcpi   ";
        val2valstr(sstat->net.tcp.InSegs,  buf+5, sizeof buf-5, 7, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETTCPI = {"NETTCPI", sysprt_NETTCPI, NULL};
/*******************************************************************/
static char *
sysprt_NETTCPO(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="tcpo   ";
        val2valstr(sstat->net.tcp.OutSegs,  buf+5, sizeof buf-5, 7, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETTCPO = {"NETTCPO", sysprt_NETTCPO, NULL};
/*******************************************************************/
static char *
sysprt_NETTCPACTOPEN(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="tcpao  ";
        val2valstr(sstat->net.tcp.ActiveOpens,  buf+6, sizeof buf-6, 6, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETTCPACTOPEN = {"NETTCPACTOPEN", sysprt_NETTCPACTOPEN, NULL};
/*******************************************************************/
static char *
sysprt_NETTCPPASVOPEN(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="tcppo  ";
        val2valstr(sstat->net.tcp.PassiveOpens, buf+6, sizeof buf-6, 6, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETTCPPASVOPEN = {"NETTCPPASVOPEN", sysprt_NETTCPPASVOPEN, NULL};
/*******************************************************************/
static char *
sysprt_NETTCPRETRANS(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="tcprs  ";
        val2valstr(sstat->net.tcp.RetransSegs,  buf+6, sizeof buf-6, 6, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETTCPRETRANS = {"NETTCPRETRANS", sysprt_NETTCPRETRANS, NULL};
/*******************************************************************/
static char *
sysprt_NETTCPINERR(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="tcpie  ";
        val2valstr(sstat->net.tcp.InErrs,  buf+6, sizeof buf-6, 6, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETTCPINERR = {"NETTCPINERR", sysprt_NETTCPINERR, NULL};
/*******************************************************************/
static char *
sysprt_NETTCPORESET(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="tcpor  ";
        val2valstr(sstat->net.tcp.OutRsts,  buf+6, sizeof buf-6, 6, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETTCPORESET = {"NETTCPORESET", sysprt_NETTCPORESET, NULL};
/*******************************************************************/
static char *
sysprt_NETTCPCSUMERR(struct sstat *sstat, extraparam *as, int badness, int *color)
{
        static char buf[16]="csumie  ";
        val2valstr(sstat->net.tcp.InCsumErrors,  buf+7, sizeof buf-7, 5, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETTCPCSUMERR = {"NETTCPCSUMERR", sysprt_NETTCPCSUMERR, NULL};
/*******************************************************************/
static char *
sysprt_NETUDPNOPORT(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="udpnp  ";
        val2valstr(sstat->net.udpv4.NoPorts,  buf+6, sizeof buf-6, 6, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETUDPNOPORT = {"NETUDPNOPORT", sysprt_NETUDPNOPORT, NULL};
/*******************************************************************/
static char *
sysprt_NETUDPINERR(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="udpie  ";
        val2valstr(sstat->net.udpv4.InErrors,  buf+6, sizeof buf-6, 6, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETUDPINERR = {"NETUDPINERR", sysprt_NETUDPINERR, NULL};
/*******************************************************************/
static char *
sysprt_NETUDPI(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="udpi   ";
        count_t udpin  = sstat->net.udpv4.InDatagrams  +
                        sstat->net.udpv6.Udp6InDatagrams;
        val2valstr(udpin,   buf+5, sizeof buf-5, 7, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETUDPI = {"NETUDPI", sysprt_NETUDPI, NULL};
/*******************************************************************/
static char *
sysprt_NETUDPO(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="udpo   ";
        count_t udpout = sstat->net.udpv4.OutDatagrams +
                        sstat->net.udpv6.Udp6OutDatagrams;
        val2valstr(udpout,   buf+5, sizeof buf-5, 7, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETUDPO = {"NETUDPO", sysprt_NETUDPO, NULL};
/*******************************************************************/
static char *
sysprt_NETNETWORK(struct sstat *sstat, extraparam *notused, int badness, int *color) 
{
        return "network     ";
}

sys_printdef syspdef_NETNETWORK = {"NETNETWORK", sysprt_NETNETWORK, NULL};
/*******************************************************************/
static char *
sysprt_NETIPI(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="ipi    ";
        count_t ipin    = sstat->net.ipv4.InReceives  +
                        sstat->net.ipv6.Ip6InReceives;
        val2valstr(ipin, buf+4, sizeof buf-4, 8, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETIPI = {"NETIPI", sysprt_NETIPI, NULL};
/*******************************************************************/
static char *
sysprt_NETIPO(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="ipo    ";
        count_t ipout   = sstat->net.ipv4.OutRequests +
                        sstat->net.ipv6.Ip6OutRequests;
        val2valstr(ipout, buf+4, sizeof buf-4, 8, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETIPO = {"NETIPO", sysprt_NETIPO, NULL};
/*******************************************************************/
static char *
sysprt_NETIPFRW(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="ipfrw  ";
        count_t ipfrw   = sstat->net.ipv4.ForwDatagrams +
                        sstat->net.ipv6.Ip6OutForwDatagrams;
        val2valstr(ipfrw, buf+6, sizeof buf-6, 6, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETIPFRW = {"NETIPFRW", sysprt_NETIPFRW, NULL};
/*******************************************************************/
static char *
sysprt_NETIPDELIV(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="deliv  ";
        count_t ipindel = sstat->net.ipv4.InDelivers +
                        sstat->net.ipv6.Ip6InDelivers;
        val2valstr(ipindel, buf+6, sizeof buf-6, 6, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETIPDELIV = {"NETIPDELIV", sysprt_NETIPDELIV, NULL};
/*******************************************************************/
static char *
sysprt_NETICMPIN(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="icmpi  ";
        count_t icmpin = sstat->net.icmpv4.InMsgs+
                        sstat->net.icmpv6.Icmp6InMsgs;
        val2valstr(icmpin , buf+6, sizeof buf-6, 6, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETICMPIN = {"NETICMPIN", sysprt_NETICMPIN, NULL};
/*******************************************************************/
static char *
sysprt_NETICMPOUT(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="icmpo  ";
        count_t icmpin = sstat->net.icmpv4.OutMsgs+
                        sstat->net.icmpv6.Icmp6OutMsgs;
        val2valstr(icmpin , buf+6, sizeof buf-6, 6, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETICMPOUT = {"NETICMPOUT", sysprt_NETICMPOUT, NULL};
/*******************************************************************/
static char *
sysprt_NETNAME(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        count_t busy;
        count_t ival = sstat->intf.intf[as->index].rbyte/125/as->nsecs;
        count_t oval = sstat->intf.intf[as->index].sbyte/125/as->nsecs;

        static char buf[16] = "ethxxxx ----";
                      //       012345678901

	*color = -1;

        if (sstat->intf.intf[as->index].speed)  /* speed known? */
        {
                if (sstat->intf.intf[as->index].duplex)
                        busy = (ival > oval ? ival : oval) /
                               (sstat->intf.intf[as->index].speed *10);
                else
                        busy = (ival + oval) /
                               (sstat->intf.intf[as->index].speed *10);
                if (busy < 0)
                        busy = 0;

		// especially with wireless, the speed might have dropped
		// temporarily to a very low value (snapshot)
		// then it might be better to take the speed of the previous
		// sample
		if (busy > 100 && sstat->intf.intf[as->index].speed <
					sstat->intf.intf[as->index].speedp)
		{
			sstat->intf.intf[as->index].speed =
				sstat->intf.intf[as->index].speedp;

                	if (sstat->intf.intf[as->index].duplex)
                        	busy = (ival > oval ? ival : oval) /
                               		(sstat->intf.intf[as->index].speed *10);
                	else
                        	busy = (ival + oval) /
                               		(sstat->intf.intf[as->index].speed *10);
		}

		if( busy < -99 )
		{
			// when we get wrong values, show wrong values
			busy = -99;
		}		
		else if( busy > 999 )
		{
			busy = 999;
		}
	        pmsprintf(buf, sizeof buf, "%-7.7s %3lld%%", 
       		          sstat->intf.intf[as->index].name, busy);

        } 
        else 
        {
                pmsprintf(buf, sizeof buf, "%-7.7s ----",
                               sstat->intf.intf[as->index].name);
                strcpy(buf+8, "----");
        } 
        return buf;
}

sys_printdef syspdef_NETNAME = {"NETNAME", sysprt_NETNAME, NULL};
/*******************************************************************/
static char *
sysprt_NETPCKI(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="pcki  ";

	*color = -1;

        val2valstr(sstat->intf.intf[as->index].rpack, 
                   buf+5, sizeof buf-5, 7, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETPCKI = {"NETPCKI", sysprt_NETPCKI, NULL};
/*******************************************************************/
static char *
sysprt_NETPCKO(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="pcko  ";

	*color = -1;

        val2valstr(sstat->intf.intf[as->index].spack, 
                   buf+5, sizeof buf-5, 7, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETPCKO = {"NETPCKO", sysprt_NETPCKO, NULL};
/*******************************************************************/
/*
** convert byte-transfers to bit-transfers     (*    8)
** convert bit-transfers  to kilobit-transfers (/ 1000)
** per second
*/
char *makenetspeed(count_t val, double nsecs)
{
        char 		c;
        static char	buf[16]="si      ?bps";
                              // 012345678901

        val=val/125/nsecs;	// convert to Kbps

        if (val < 10000)
        {
                c='K';
        } 
        else if (val < (count_t)10000 * 1000)
        {
                val/=1000;
                c = 'M';
        } 
        else if (val < (count_t)10000 * 1000 * 1000)
        {
                val/=1000 * 1000;
                c = 'G';
        } 
        else 
        {
                val = val / 1000 / 1000 / 1000;
                c = 'T';
        }

        if(val < -999)
        {
		// when we get wrong values, show wrong values
                val = -999;
        }
        else if(val > 9999)
        {
                val = 9999;
        }

        pmsprintf(buf+3, sizeof buf-3, "%4lld %cbps", val, c);

        return buf;
}
/*******************************************************************/
static char *
sysprt_NETSPEEDMAX(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16];
        count_t speed = sstat->intf.intf[as->index].speed;

	*color = -1;

	if (speed < 0 )
		speed = 0;

	if (speed < 10000)
	{
        	pmsprintf(buf, sizeof buf, "sp %4lld Mbps", speed);
	}
	else
	{
		speed /= 1000;

		if (speed > 9999)
		{
			speed = 9999;
		}
        	pmsprintf(buf, sizeof buf, "sp %4lld Gbps", speed);
	}

        return buf;
}

sys_printdef syspdef_NETSPEEDMAX = {"NETSPEEDMAX", sysprt_NETSPEEDMAX, NULL};
/*******************************************************************/
static char *
sysprt_NETSPEEDIN(struct sstat *sstat, extraparam *as, int badness, int *color) 
{

	*color = -1;

        char *ps=makenetspeed(sstat->intf.intf[as->index].rbyte,as->nsecs);
        ps[0]='s';
        ps[1]='i';
        return ps;
}

sys_printdef syspdef_NETSPEEDIN = {"NETSPEEDIN", sysprt_NETSPEEDIN, NULL};
/*******************************************************************/
static char *
sysprt_NETSPEEDOUT(struct sstat *sstat, extraparam *as, int badness, int *color) 
{

	*color = -1;

        char *ps=makenetspeed(sstat->intf.intf[as->index].sbyte,as->nsecs);
        ps[0]='s';
        ps[1]='o';
        return ps;
}

sys_printdef syspdef_NETSPEEDOUT = {"NETSPEEDOUT", sysprt_NETSPEEDOUT, NULL};
/*******************************************************************/
static char *
sysprt_NETCOLLIS(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="coll  ";
        val2valstr(sstat->intf.intf[as->index].scollis, 
                   buf+5, sizeof buf-5, 7, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETCOLLIS = {"NETCOLLIS", sysprt_NETCOLLIS, NULL};
/*******************************************************************/
static char *
sysprt_NETMULTICASTIN(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="mlti ";
        val2valstr(sstat->intf.intf[as->index].rmultic, 
                   buf+5, sizeof buf-5, 7, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETMULTICASTIN = {"NETMULTICASTIN", sysprt_NETMULTICASTIN, NULL};
/*******************************************************************/
static char *
sysprt_NETRCVERR(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="erri   ";
        val2valstr(sstat->intf.intf[as->index].rerrs, 
                   buf+5, sizeof buf-5, 7, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETRCVERR = {"NETRCVERR", sysprt_NETRCVERR, NULL};
/*******************************************************************/
static char *
sysprt_NETSNDERR(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="erro   ";
        val2valstr(sstat->intf.intf[as->index].serrs, 
                   buf+5, sizeof buf-5, 7, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETSNDERR = {"NETSNDERR", sysprt_NETSNDERR, NULL};
/*******************************************************************/
static char *
sysprt_NETRCVDROP(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="drpi   ";
        val2valstr(sstat->intf.intf[as->index].rdrop,
                   buf+5, sizeof buf-5, 7, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETRCVDROP = {"NETRCVDROP", sysprt_NETRCVDROP, NULL};
/*******************************************************************/
static char *
sysprt_NETSNDDROP(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="drpo   ";
        val2valstr(sstat->intf.intf[as->index].sdrop,
                   buf+5, sizeof buf-5, 7, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETSNDDROP = {"NETSNDDROP", sysprt_NETSNDDROP, NULL};
/*******************************************************************/
static char *
sysprt_IFBNAME(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        count_t busy;
        count_t ival = sstat->ifb.ifb[as->index].rcvb/125/as->nsecs;
        count_t oval = sstat->ifb.ifb[as->index].sndb/125/as->nsecs;
	int     len;
        static char buf[16] = "ethxxxx ----", tmp[32], *ps=tmp;
                      //       012345678901

	*color = -1;

	busy = (ival > oval ? ival : oval) * sstat->ifb.ifb[as->index].lanes /
                               (sstat->ifb.ifb[as->index].rate * 10);
        if (busy < 0)
                busy = 0;
	else if (busy > 100)
                busy = 100;

	pmsprintf(tmp, sizeof tmp, "%s/%d",
                 sstat->ifb.ifb[as->index].ibname,
	         sstat->ifb.ifb[as->index].portnr);

	len = strlen(ps);
        if (len > 7)
		ps = ps + len - 7;

	pmsprintf(buf, sizeof buf, "%-7.7s %3lld%%", ps, busy);
        return buf;
}

sys_printdef syspdef_IFBNAME = {"IFBNAME", sysprt_IFBNAME, NULL};
/*******************************************************************/
static char *
sysprt_IFBPCKI(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="pcki  ";

	*color = -1;

        val2valstr(sstat->ifb.ifb[as->index].rcvp, 
                   buf+5, sizeof buf-5, 7, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_IFBPCKI = {"IFBPCKI", sysprt_IFBPCKI, NULL};
/*******************************************************************/
static char *
sysprt_IFBPCKO(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="pcko  ";

	*color = -1;

        val2valstr(sstat->ifb.ifb[as->index].sndp, 
                   buf+5, sizeof buf-5, 7, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_IFBPCKO = {"IFBPCKO", sysprt_IFBPCKO, NULL};
/*******************************************************************/
static char *
sysprt_IFBSPEEDMAX(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[64];
        count_t rate = sstat->ifb.ifb[as->index].rate;

        if (rate < 0)
                rate = 0;

	*color = -1;

	if (rate < 10000)
	{
        	pmsprintf(buf, sizeof buf, "sp %4lld Mbps", rate);
	}
	else
	{
		rate /= 1000;

		if (rate > 9999)
		{
			rate = 9999;
		}
        	pmsprintf(buf, sizeof buf, "sp %4lld Gbps", rate);
	}

        return buf;
}

sys_printdef syspdef_IFBSPEEDMAX = {"IFBSPEEDMAX", sysprt_IFBSPEEDMAX, NULL};
/*******************************************************************/
static char *
sysprt_IFBLANES(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="lanes   ";
        val2valstr(sstat->ifb.ifb[as->index].lanes, buf+6, sizeof buf-6, 6, 0, 0);
        return buf;
}

sys_printdef syspdef_IFBLANES = {"IFBLANES", sysprt_IFBLANES, NULL};
/*******************************************************************/
static char *
sysprt_IFBSPEEDIN(struct sstat *sstat, extraparam *as, int badness, int *color) 
{

	*color = -1;

        char *ps=makenetspeed(sstat->ifb.ifb[as->index].rcvb *
	                      sstat->ifb.ifb[as->index].lanes, as->nsecs);
        ps[0]='s';
        ps[1]='i';
        return ps;
}

sys_printdef syspdef_IFBSPEEDIN = {"IFBSPEEDIN", sysprt_IFBSPEEDIN, NULL};
/*******************************************************************/
static char *
sysprt_IFBSPEEDOUT(struct sstat *sstat, extraparam *as, int badness, int *color) 
{

	*color = -1;

	char *ps=makenetspeed(sstat->ifb.ifb[as->index].sndb *
	                      sstat->ifb.ifb[as->index].lanes, as->nsecs);
        ps[0]='s';
        ps[1]='o';
        return ps;
}

sys_printdef syspdef_IFBSPEEDOUT = {"IFBSPEEDOUT", sysprt_IFBSPEEDOUT, NULL};
/*******************************************************************/
static char *
sysprt_NFMSERVER(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
	static char buf[16] = "srv ";
        char mntdev[128], *ps;

        memcpy(mntdev, sstat->nfs.nfsmounts.nfsmnt[as->index].mountdev,
								sizeof mntdev);

	if ( (ps = strchr(mntdev, ':')) )		// colon found?
		*ps = '\0';
	else
		strcpy(mntdev, "?");

	pmsprintf(buf+4, sizeof buf-4, "%8.8s", mntdev);
        return buf;
}

sys_printdef syspdef_NFMSERVER = {"NFMSERVER", sysprt_NFMSERVER, NULL};
/*******************************************************************/
static char *
sysprt_NFMPATH(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
	static char buf[16];
        char mntdev[128], *ps;
	int len;

        memcpy(mntdev, sstat->nfs.nfsmounts.nfsmnt[as->index].mountdev,
								sizeof mntdev);

	if ( (ps = strchr(mntdev, ':')) )		// colon found?
		ps++;
	else
		ps = mntdev;

	len = strlen(ps);
        if (len > 12)
		ps = ps + len - 12;

	pmsprintf(buf, sizeof buf, "%12.12s", ps);
        return buf;
}

sys_printdef syspdef_NFMPATH = {"NFMPATH", sysprt_NFMPATH, NULL};
/*******************************************************************/
static char *
sysprt_NFMTOTREAD(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="read   ";

        val2memstr(sstat->nfs.nfsmounts.nfsmnt[as->index].bytestotread,
			buf+6, sizeof buf-6, KBFORMAT, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFMTOTREAD = {"NFMTOTREAD", sysprt_NFMTOTREAD, NULL};
/*******************************************************************/
static char *
sysprt_NFMTOTWRITE(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="write   ";

        val2memstr(sstat->nfs.nfsmounts.nfsmnt[as->index].bytestotwrite,
			buf+6, sizeof buf-6, KBFORMAT, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFMTOTWRITE = {"NFMTOTWRITE", sysprt_NFMTOTWRITE, NULL};
/*******************************************************************/
static char *
sysprt_NFMNREAD(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="nread    ";

        val2memstr(sstat->nfs.nfsmounts.nfsmnt[as->index].bytesread,
			buf+6, sizeof buf-6, KBFORMAT, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFMNREAD = {"NFMNREAD", sysprt_NFMNREAD, NULL};
/*******************************************************************/
static char *
sysprt_NFMNWRITE(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="nwrit    ";

        val2memstr(sstat->nfs.nfsmounts.nfsmnt[as->index].byteswrite,
			buf+6, sizeof buf-6, KBFORMAT, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFMNWRITE = {"NFMNWRITE", sysprt_NFMNWRITE, NULL};
/*******************************************************************/
static char *
sysprt_NFMDREAD(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="dread    ";

        val2memstr(sstat->nfs.nfsmounts.nfsmnt[as->index].bytesdread,
			buf+6, sizeof buf-6, KBFORMAT, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFMDREAD = {"NFMDREAD", sysprt_NFMDREAD, NULL};
/*******************************************************************/
static char *
sysprt_NFMDWRITE(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="dwrit    ";

        val2memstr(sstat->nfs.nfsmounts.nfsmnt[as->index].bytesdwrite,
			buf+6, sizeof buf-6, KBFORMAT, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFMDWRITE = {"NFMDWRITE", sysprt_NFMDWRITE, NULL};
/*******************************************************************/
static char *
sysprt_NFMMREAD(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="mread    ";

        val2memstr(sstat->nfs.nfsmounts.nfsmnt[as->index].pagesmread *pagesize,
			buf+6, sizeof buf-6, KBFORMAT, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFMMREAD = {"NFMMREAD", sysprt_NFMMREAD, NULL};
/*******************************************************************/
static char *
sysprt_NFMMWRITE(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="mwrit    ";

        val2memstr(sstat->nfs.nfsmounts.nfsmnt[as->index].pagesmwrite *pagesize,
			buf+6, sizeof buf-6, KBFORMAT, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFMMWRITE = {"NFMMWRITE", sysprt_NFMMWRITE, NULL};
/*******************************************************************/
static char *
sysprt_NFCRPCCNT(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="rpc   ";
        val2valstr(sstat->nfs.client.rpccnt,
                   buf+4, sizeof buf-4, 8, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFCRPCCNT = {"NFCRPCCNT", sysprt_NFCRPCCNT, NULL};
/*******************************************************************/
static char *
sysprt_NFCRPCREAD(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="read   ";
        val2valstr(sstat->nfs.client.rpcread,
                   buf+5, sizeof buf-5, 7, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFCRPCREAD = {"NFCRPCREAD", sysprt_NFCRPCREAD, NULL};
/*******************************************************************/
static char *
sysprt_NFCRPCWRITE(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="write   ";
        val2valstr(sstat->nfs.client.rpcwrite,
                   buf+6, sizeof buf-6, 6, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFCRPCWRITE = {"NFCRPCWRITE", sysprt_NFCRPCWRITE, NULL};
/*******************************************************************/
static char *
sysprt_NFCRPCRET(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="retxmit ";
        val2valstr(sstat->nfs.client.rpcretrans,
                   buf+8, sizeof buf-8, 4, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFCRPCRET = {"NFCRPCRET", sysprt_NFCRPCRET, NULL};
/*******************************************************************/
static char *
sysprt_NFCRPCARF(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="autref  ";
        val2valstr(sstat->nfs.client.rpcautrefresh,
                   buf+7, sizeof buf-7, 5, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFCRPCARF = {"NFCRPCARF", sysprt_NFCRPCARF, NULL};
/*******************************************************************/
static char *
sysprt_NFSRPCCNT(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="rpc   ";
        val2valstr(sstat->nfs.server.rpccnt,
                   buf+4, sizeof buf-4, 8, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFSRPCCNT = {"NFSRPCCNT", sysprt_NFSRPCCNT, NULL};
/*******************************************************************/
static char *
sysprt_NFSRPCREAD(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="cread   ";
        val2valstr(sstat->nfs.server.rpcread,
                   buf+6, sizeof buf-6, 6, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFSRPCREAD = {"NFSRPCREAD", sysprt_NFSRPCREAD, NULL};
/*******************************************************************/
static char *
sysprt_NFSRPCWRITE(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="cwrit   ";
        val2valstr(sstat->nfs.server.rpcwrite,
                   buf+6, sizeof buf-6, 6, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFSRPCWRITE = {"NFSRPCWRITE", sysprt_NFSRPCWRITE, NULL};
/*******************************************************************/
static char *
sysprt_NFSBADFMT(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="badfmt   ";
        val2valstr(sstat->nfs.server.rpcbadfmt,
                   buf+7, sizeof buf-7, 5, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFSBADFMT = {"NFSBADFMT", sysprt_NFSBADFMT, NULL};
/*******************************************************************/
static char *
sysprt_NFSBADAUT(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="badaut   ";
        val2valstr(sstat->nfs.server.rpcbadaut,
                   buf+7, sizeof buf-7, 5, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFSBADAUT = {"NFSBADAUT", sysprt_NFSBADAUT, NULL};
/*******************************************************************/
static char *
sysprt_NFSBADCLN(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="badcln   ";
        val2valstr(sstat->nfs.server.rpcbadcln,
                   buf+7, sizeof buf-7, 5, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFSBADCLN = {"NFSBADCLN", sysprt_NFSBADCLN, NULL};
/*******************************************************************/
static char *
sysprt_NFSNETTCP(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="nettcp   ";
        val2valstr(sstat->nfs.server.nettcpcnt,
                   buf+7, sizeof buf-7, 5, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFSNETTCP = {"NFSNETTCP", sysprt_NFSNETTCP, NULL};
/*******************************************************************/
static char *
sysprt_NFSNETUDP(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="netudp   ";
        val2valstr(sstat->nfs.server.netudpcnt,
                   buf+7, sizeof buf-7, 5, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFSNETUDP = {"NFSNETUDP", sysprt_NFSNETUDP, NULL};
/*******************************************************************/
static char *
sysprt_NFSNRBYTES(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char	buf[32]="MBcr/s ";

        pmsprintf(buf+7, sizeof buf-7, "%5.1lf",
		sstat->nfs.server.nrbytes / 1024.0 / 1024.0 / as->nsecs);

        return buf;
}

sys_printdef syspdef_NFSNRBYTES = {"NFSNRBYTES", sysprt_NFSNRBYTES, NULL};
/*******************************************************************/
static char *
sysprt_NFSNWBYTES(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char	buf[32]="MBcw/s ";

        pmsprintf(buf+7, sizeof buf-7, "%5.1lf",
		sstat->nfs.server.nwbytes / 1024.0 / 1024.0 / as->nsecs);

        return buf;
}

sys_printdef syspdef_NFSNWBYTES = {"NFSNWBYTES", sysprt_NFSNWBYTES, NULL};
/*******************************************************************/
static char *
sysprt_NFSRCHITS(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="rchits   ";
        val2valstr(sstat->nfs.server.rchits,
                   buf+8, sizeof buf-8, 4, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFSRCHITS = {"NFSRCHITS", sysprt_NFSRCHITS, NULL};
/*******************************************************************/
static char *
sysprt_NFSRCMISS(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="rcmiss   ";
        val2valstr(sstat->nfs.server.rcmiss,
                   buf+8, sizeof buf-8, 4, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFSRCMISS = {"NFSRCMISS", sysprt_NFSRCMISS, NULL};
/*******************************************************************/
static char *
sysprt_NFSRCNOCA(struct sstat *sstat, extraparam *as, int badness, int *color) 
{
        static char buf[16]="rcnoca   ";
        val2valstr(sstat->nfs.server.rcnoca,
                   buf+8, sizeof buf-8, 4, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFSRCNOCA = {"NFSRCNOCA", sysprt_NFSRCNOCA};
/*******************************************************************/
static char *
sysprt_BLANKBOX(struct sstat *sstat, extraparam *notused, int badness, int *color) 
{
        return "            ";
}

sys_printdef syspdef_BLANKBOX = {"BLANKBOX", sysprt_BLANKBOX, NULL};
