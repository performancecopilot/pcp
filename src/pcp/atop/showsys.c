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
*/

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <termio.h>
#include <unistd.h>
#include <stdarg.h>
#include <curses.h>
#include <pwd.h>
#include <grp.h>
#include <regex.h>

#include "atop.h"
#include "photoproc.h"
#include "photosyst.h"
#include "showgeneric.h"
#include "showlinux.h"

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
                        	attron(COLOR_PAIR(COLORCRIT));
                        	printg(labeltext);
                        	attroff(COLOR_PAIR(COLORCRIT));
			}
			else
			{
                        	attron(A_BOLD);
                        	printg(labeltext);
                        	attroff(A_BOLD);
			}

                        attroff(A_BLINK);

                        return COLORCRIT;
                }

                if (almostcrit && badness >= almostcrit)
                {
		        if (usecolors)
			{
                        	attron(COLOR_PAIR(COLORALMOST));
                        	printg(labeltext);
                        	attroff(COLOR_PAIR(COLORALMOST));
			}
			else
			{
                        	attron(A_BOLD);
                        	printg(labeltext);
                        	attroff(A_BOLD);
			}

                        return COLORALMOST;
                }
        }

        /*
        ** no colors required or no reason to show colors
        */
        printg(labeltext);
        return 0;
}

char *sysprt_BLANKBOX(void *p, void *notused, int, int *);

void
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
 * print an array of sys_printpair things.  If the screen contains to
 * few character columns, lower priority items are removed
 *
 */
void showsysline(sys_printpair* elemptr, 
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
#define MAXELEMS 40
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

                printg(" | ");
                addblanks(&charslackused, &charslackover);

		/*
		** by default no color is shown for this field (color = 0)
		**
		** the convert-function can set a color-number (color > 0)
		** when a specific color is wanted or the convert-function
		** can leave the decision to display with a color to the piece
		** of code below (color == -1)
		*/
		color = 0;
                itemp = curelem->doconvert(sstat, extra, badness, &color);

		if (screen)
		{
			if (color == -1) // default color wanted
			{
				color = 0;

				if (badness >= 100)
                               		color = COLORCRIT;
				else if (almostcrit && badness >= almostcrit)
					color = COLORALMOST;
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
/* SYSTEM PRINT FUNCTIONS */
/*******************************************************************/
char *
sysprt_PRCSYS(void *notused, void *q, int badness, int *color)
{
        extraparam *as=q;
        static char buf[16]="sys   ";
        val2cpustr(as->totst, buf+6, sizeof buf-6);
        return buf;
}

sys_printdef syspdef_PRCSYS = {"PRCSYS", sysprt_PRCSYS};
/*******************************************************************/
char *
sysprt_PRCUSER(void *notused, void *q, int badness, int *color) 
{
        extraparam *as=q;
        static char buf[16]="user  ";
        val2cpustr(as->totut, buf+6, sizeof buf-6);
        return buf;
}

sys_printdef syspdef_PRCUSER = {"PRCUSER", sysprt_PRCUSER};
/*******************************************************************/
char *
sysprt_PRCNPROC(void *notused, void *q, int badness, int *color) 
{
        extraparam *as=q;
        static char buf[16]="#proc     ";
        val2valstr(as->nproc, buf+6, sizeof buf-6, 6, 0, 0);
        return buf;
}

sys_printdef syspdef_PRCNPROC = {"PRCNPROC", sysprt_PRCNPROC};
/*******************************************************************/
char *
sysprt_PRCNRUNNING(void *notused, void *q, int badness, int *color) 
{
        extraparam *as=q;
        static char buf[15]="#trun     ";
        val2valstr(as->ntrun, buf+6, sizeof buf-6, 6, 0, 0);
        return buf;
}

sys_printdef syspdef_PRCNRUNNING = {"PRCNRUNNING", sysprt_PRCNRUNNING};
/*******************************************************************/
char *
sysprt_PRCNSLEEPING(void *notused, void *q, int badness, int *color) 
{
        extraparam *as=q;
        static char buf[16]="#tslpi    ";
        val2valstr(as->ntslpi, buf+8, sizeof buf-8, 4, 0, 0);
        return buf;
}

sys_printdef syspdef_PRCNSLEEPING = {"PRCNSLEEPING", sysprt_PRCNSLEEPING};
/*******************************************************************/
char *
sysprt_PRCNDSLEEPING(void *notused, void *q, int badness, int *color) 
{
        extraparam *as=q;
        static char buf[16]="#tslpu    ";
        val2valstr(as->ntslpu, buf+8, sizeof buf-8, 4, 0, 0);
        return buf;
}

sys_printdef syspdef_PRCNDSLEEPING = {"PRCNDSLEEPING", sysprt_PRCNDSLEEPING};
/*******************************************************************/
char *
sysprt_PRCNZOMBIE(void *notused, void *q, int badness, int *color) 
{
        extraparam *as=q;
        static char buf[16]="#zombie   ";

	if (as->nzomb > 30)
		*color = COLORALMOST;

	if (as->nzomb > 50)
		*color = COLORCRIT;

        val2valstr(as->nzomb, buf+8, sizeof buf-8, 4, 0, 0);
        return buf;
}

sys_printdef syspdef_PRCNZOMBIE = {"PRCNZOMBIE", sysprt_PRCNZOMBIE};
/*******************************************************************/
char *
sysprt_PRCNNEXIT(void *notused, void *q, int badness, int *color) 
{
	static char firstcall = 1;

        extraparam *as=q;
        static char buf[16]="#exit     ";

        if (supportflags & ACCTACTIVE)
        {
		if (as->noverflow)
		{
			*color = COLORCRIT;
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
			*color = COLORCRIT;
			firstcall = 0;
		}
		else
		{
			*color = COLORINFO;
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

sys_printdef syspdef_PRCNNEXIT = {"PRCNNEXIT", sysprt_PRCNNEXIT};
/*******************************************************************/
char *
sysprt_CPUSYS(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16];
	float perc	= (sstat->cpu.all.stime * 100.0) / as->percputot;

	if (perc > 1.0)
		*color = -1;

        snprintf(buf, sizeof buf-1, "sys  %6.0f%%", perc);
        return buf;
}

sys_printdef syspdef_CPUSYS = {"CPUSYS", sysprt_CPUSYS};
/*******************************************************************/
char *
sysprt_CPUUSER(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16];
	float perc	= (sstat->cpu.all.utime + sstat->cpu.all.ntime)
                                        * 100.0 / as->percputot;

	if (perc > 1.0)
		*color = -1;

        snprintf(buf, sizeof buf-1, "user %6.0f%%", perc);
        return buf;
}

sys_printdef syspdef_CPUUSER = {"CPUUSER", sysprt_CPUUSER};
/*******************************************************************/
char *
sysprt_CPUIRQ(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16];
        float perc = (sstat->cpu.all.Itime + sstat->cpu.all.Stime)
                                    * 100.0 / as->percputot;

	if (perc > 1.0)
		*color = -1;

        snprintf(buf, sizeof buf-1, "irq  %6.0f%%", perc);
        return buf;
}

sys_printdef syspdef_CPUIRQ = {"CPUIRQ", sysprt_CPUIRQ};
/*******************************************************************/
char *
sysprt_CPUIDLE(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16];
        snprintf(buf, sizeof buf-1, "idle %6.0f%%", 
                (sstat->cpu.all.itime * 100.0) / as->percputot);
        return buf;
}

sys_printdef syspdef_CPUIDLE = {"CPUIDLE", sysprt_CPUIDLE};
/*******************************************************************/
char *
sysprt_CPUWAIT(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16];
        snprintf(buf, sizeof buf-1, "wait %6.0f%%", 
                (sstat->cpu.all.wtime * 100.0) / as->percputot);
        return buf;
}

sys_printdef syspdef_CPUWAIT = {"CPUWAIT", sysprt_CPUWAIT};
/*******************************************************************/
char *
sysprt_CPUISYS(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16];
	float perc	= sstat->cpu.cpu[as->index].stime * 100.0
							/ as->percputot;

	if (perc > 1.0)
		*color = -1;

        snprintf(buf, sizeof buf-1, "sys  %6.0f%%", perc);
        return buf;
}

sys_printdef syspdef_CPUISYS = {"CPUISYS", sysprt_CPUISYS};
/*******************************************************************/
char *
sysprt_CPUIUSER(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16];
	float perc	= (sstat->cpu.cpu[as->index].utime +
                           sstat->cpu.cpu[as->index].ntime) 
			   * 100.0 / as->percputot;

	if (perc > 1.0)
		*color = -1;

        snprintf(buf, sizeof buf-1, "user %6.0f%%", perc);
        return buf;
}

sys_printdef syspdef_CPUIUSER = {"CPUIUSER", sysprt_CPUIUSER};
/*******************************************************************/
char *
sysprt_CPUIIRQ(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16];
	float perc	= (sstat->cpu.cpu[as->index].Itime +
		  	   sstat->cpu.cpu[as->index].Stime)
			   * 100.0 / as->percputot;

	if (perc > 1.0)
		*color = -1;

        snprintf(buf, sizeof buf-1, "irq  %6.0f%%", perc);
        return buf;
}

sys_printdef syspdef_CPUIIRQ = {"CPUIIRQ", sysprt_CPUIIRQ};
/*******************************************************************/
char *
sysprt_CPUIIDLE(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16];
        snprintf(buf, sizeof buf-1, "idle %6.0f%%", 
                (sstat->cpu.cpu[as->index].itime * 100.0) / as->percputot);
        return buf;
}

sys_printdef syspdef_CPUIIDLE = {"CPUIIDLE", sysprt_CPUIIDLE};
/*******************************************************************/
char *
sysprt_CPUIWAIT(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16];
        snprintf(buf, sizeof buf-1, "cpu%03d w%3.0f%%", 
		 sstat->cpu.cpu[as->index].cpunr,
                (sstat->cpu.cpu[as->index].wtime * 100.0) / as->percputot);
        return buf;
}

sys_printdef syspdef_CPUIWAIT = {"CPUIWAIT", sysprt_CPUIWAIT};
/*******************************************************************/
void dofmt_cpufreq(char *buf, size_t buflen, count_t maxfreq, count_t cnt, count_t ticks)
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
        else                // nothing is known: print ?????
        {
            strcpy(buf, "curf    ?MHz");
        }
}


/*
 * sumscaling: sum scaling info for all processors
 *
 */
void sumscaling(struct sstat *sstat, count_t *maxfreq,
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


static void dofmt_cpuscale(char *buf, size_t buflen, count_t maxfreq, count_t cnt, count_t ticks)
{
	if (ticks) 
	{
		count_t curfreq	= cnt/ticks;
		int     perc = maxfreq ? 100 * curfreq / maxfreq : 0;

		strcpy(buf, "avgscal ");
		snprintf(buf+7, buflen-8, "%4d%%", perc);
        } 
        else if (maxfreq)   // max frequency is known so % can be calculated
        {
		strcpy(buf, "curscal ");
		snprintf(buf+7, buflen-8, "%4lld%%", 100 * cnt / maxfreq);
        }
	else	// nothing is known: print ?????
	{
		strcpy(buf, "curscal   ?%");
	}
}

/*******************************************************************/
char *
sysprt_CPUIFREQ(void *p, void *q, int badness, int *color) 
{

        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16];

        count_t maxfreq	= sstat->cpu.cpu[as->index].freqcnt.maxfreq;
        count_t cnt	= sstat->cpu.cpu[as->index].freqcnt.cnt;
        count_t ticks	= sstat->cpu.cpu[as->index].freqcnt.ticks;

        dofmt_cpufreq(buf, sizeof buf, maxfreq, cnt, ticks);
        return buf;
}

sys_printdef syspdef_CPUIFREQ = {"CPUIFREQ", sysprt_CPUIFREQ};
/*******************************************************************/
char *
sysprt_CPUFREQ(void *p, void *q, int badness, int *color) 
{

        struct sstat *sstat=p;
        static char buf[16];

        count_t maxfreq;
        count_t cnt;
        count_t ticks;
        int     n = sstat->cpu.nrcpu;

        sumscaling(sstat, &maxfreq, &cnt, &ticks);
        dofmt_cpufreq(buf, sizeof buf, maxfreq/n, cnt/n, ticks/n);
        return buf;
}

sys_printdef syspdef_CPUFREQ = {"CPUFREQ", sysprt_CPUFREQ};
/*******************************************************************/
char *
sysprt_CPUISCALE(void *p, void *q, int badness, int *color) 
{

        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16];

        count_t maxfreq = sstat->cpu.cpu[as->index].freqcnt.maxfreq;
        count_t cnt     = sstat->cpu.cpu[as->index].freqcnt.cnt;
        count_t ticks   = sstat->cpu.cpu[as->index].freqcnt.ticks;

        dofmt_cpuscale(buf, sizeof buf, maxfreq, cnt, ticks);
        return buf;
}

sys_printdef syspdef_CPUISCALE = {"CPUISCALE", sysprt_CPUISCALE};
/*******************************************************************/
char *
sysprt_CPUSCALE(void *p, void *q, int badness, int *color) 
{

        struct sstat *sstat=p;
        static char buf[16];

        count_t maxfreq;
        count_t cnt;
        count_t ticks;
        int     n = sstat->cpu.nrcpu;

        sumscaling(sstat, &maxfreq, &cnt, &ticks);
        dofmt_cpuscale(buf, sizeof buf, maxfreq/n, cnt/n, ticks/n);
        return buf;
}

sys_printdef syspdef_CPUSCALE = {"CPUSCALE", sysprt_CPUSCALE};
/*******************************************************************/
char *
sysprt_CPUSTEAL(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16];
	float perc	= sstat->cpu.all.steal * 100.0 / as->percputot;

	if (perc > 1.0)
		*color = -1;

        snprintf(buf, sizeof buf-1, "steal %5.0f%%", perc);
        return buf;
}

sys_printdef syspdef_CPUSTEAL = {"CPUSTEAL", sysprt_CPUSTEAL};
/*******************************************************************/
char *
sysprt_CPUISTEAL(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16];
	float perc	= sstat->cpu.cpu[as->index].steal * 100.0
							/ as->percputot;

	if (perc > 1.0)
		*color = -1;

        snprintf(buf, sizeof buf-1, "steal %5.0f%%", perc);
        return buf;
}

sys_printdef syspdef_CPUISTEAL = {"CPUISTEAL", sysprt_CPUISTEAL};
/*******************************************************************/
char *
sysprt_CPUGUEST(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16];
        float perc = sstat->cpu.all.guest * 100.0 / as->percputot;

	if (perc > 1.0)
		*color = -1;

        snprintf(buf, sizeof buf-1, "guest %5.0f%%", perc);
        return buf;
}

sys_printdef syspdef_CPUGUEST = {"CPUGUEST", sysprt_CPUGUEST};
/*******************************************************************/
char *
sysprt_CPUIGUEST(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16];
        float perc = sstat->cpu.cpu[as->index].guest * 100.0 / as->percputot;

	if (perc > 1.0)
		*color = -1;

        snprintf(buf, sizeof buf-1, "guest %5.0f%%", perc);
        return buf;
}

sys_printdef syspdef_CPUIGUEST = {"CPUIGUEST", sysprt_CPUIGUEST};
/*******************************************************************/
char *
sysprt_CPLAVG1(void *p, void *notused, int badness, int *color) 
{
        struct sstat *sstat=p;
        static char buf[16]="avg1 ";

        if (sstat->cpu.lavg1  > 999.0)
        {
                snprintf(buf+5, sizeof buf-5, "%7.0f", sstat->cpu.lavg1);
        }
        else
        {
                snprintf(buf+5, sizeof buf-5, "%7.2f", sstat->cpu.lavg1);
        }
        return buf;
}
        
sys_printdef syspdef_CPLAVG1 = {"CPLAVG1", sysprt_CPLAVG1};
/*******************************************************************/
char *
sysprt_CPLAVG5(void *p, void *notused, int badness, int *color) 
{
        struct sstat *sstat=p;
        static char buf[16]="avg5 ";

        if (sstat->cpu.lavg5  > 999.0)
        {
                snprintf(buf+5, sizeof buf-5, "%7.0f", sstat->cpu.lavg5);
        }
        else
        {
                snprintf(buf+5, sizeof buf-5, "%7.2f", sstat->cpu.lavg5);
        }
        return buf;
}
        
sys_printdef syspdef_CPLAVG5 = {"CPLAVG5", sysprt_CPLAVG5};
/*******************************************************************/
char *
sysprt_CPLAVG15(void *p, void *notused, int badness, int *color) 
{
        struct sstat *sstat=p;
        static char buf[16]="avg15 ";

	if (sstat->cpu.lavg15 > (2 * sstat->cpu.nrcpu) )
		*color = COLORALMOST;

        if (sstat->cpu.lavg15  > 999.0)
        {
                snprintf(buf+6, sizeof buf-6, "%6.0f", sstat->cpu.lavg15);
        }
        else
        {
                snprintf(buf+6, sizeof buf-6, "%6.2f", sstat->cpu.lavg15);
        }
        return buf;
}
        
sys_printdef syspdef_CPLAVG15 = {"CPLAVG15", sysprt_CPLAVG15};
/*******************************************************************/
char *
sysprt_CPLCSW(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="csw    ";

        val2valstr(sstat->cpu.csw, buf+4, sizeof buf-4, 8,as->avgval,as->nsecs);
        return buf;
}

sys_printdef syspdef_CPLCSW = {"CPLCSW", sysprt_CPLCSW};
/*******************************************************************/
char *
sysprt_PRCCLONES(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="clones ";

        val2valstr(sstat->cpu.nprocs, buf+7, sizeof buf-7, 5,as->avgval,as->nsecs);
        return buf;
}

sys_printdef syspdef_PRCCLONES = {"PRCCLONES", sysprt_PRCCLONES};
/*******************************************************************/
char *
sysprt_CPLNUMCPU(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="numcpu ";

        val2valstr(sstat->cpu.nrcpu, buf+7, sizeof buf-7, 5,0,as->nsecs);
        return buf;
}

sys_printdef syspdef_CPLNUMCPU = {"CPLNUMCPU", sysprt_CPLNUMCPU};
/*******************************************************************/
char *
sysprt_CPLINTR(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="intr   ";

        val2valstr(sstat->cpu.devint, buf+5, sizeof buf-5, 7,as->avgval,as->nsecs);
        return buf;
}

sys_printdef syspdef_CPLINTR = {"CPLINTR", sysprt_CPLINTR};
/*******************************************************************/
char *
sysprt_MEMTOT(void *p, void *notused, int badness, int *color) 
{
        struct sstat *sstat=p;
        static char buf[16]="tot   ";
        count_t value = sstat->mem.physmem * 1024;
        val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
	*color = -1;
        return buf;
}

sys_printdef syspdef_MEMTOT = {"MEMTOT", sysprt_MEMTOT};
/*******************************************************************/
char *
sysprt_MEMFREE(void *p, void *notused, int badness, int *color) 
{
        struct sstat *sstat=p;
        static char buf[16]="free  ";
        count_t value = sstat->mem.freemem * 1024;
	*color = -1;
        val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
        return buf;
}

sys_printdef syspdef_MEMFREE = {"MEMFREE", sysprt_MEMFREE};
/*******************************************************************/
char *
sysprt_MEMCACHE(void *p, void *notused, int badness, int *color) 
{
        struct sstat *sstat=p;
        static char buf[16]="cache ";
        count_t value = sstat->mem.cachemem * 1024;
        val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
	*color = -1;
        return buf;
}

sys_printdef syspdef_MEMCACHE = {"MEMCACHE", sysprt_MEMCACHE};
/*******************************************************************/
char *
sysprt_MEMDIRTY(void *p, void *notused, int badness, int *color) 
{
        struct sstat *sstat=p;
        static char buf[16] = "dirty ";
        count_t value = sstat->mem.cachedrt * 1024;
        val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
	*color = -1;
        return buf;
}

sys_printdef syspdef_MEMDIRTY = {"MEMDIRTY", sysprt_MEMDIRTY};
/*******************************************************************/
char *
sysprt_MEMBUFFER(void *p, void *notused, int badness, int *color) 
{
        struct sstat *sstat=p;
        static char buf[16]="buff  ";
        count_t value = sstat->mem.buffermem * 1024;
        val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
	*color = -1;
        return buf;
}

sys_printdef syspdef_MEMBUFFER = {"MEMBUFFER", sysprt_MEMBUFFER};
/*******************************************************************/
char *
sysprt_MEMSLAB(void *p, void *notused, int badness, int *color) 
{
        struct sstat *sstat=p;
        static char buf[16]="slab  ";
        count_t value = sstat->mem.slabmem * 1024;
        val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
	*color = -1;
        return buf;
}

sys_printdef syspdef_MEMSLAB = {"MEMSLAB", sysprt_MEMSLAB};
/*******************************************************************/
char *
sysprt_RECSLAB(void *p, void *notused, int badness, int *color) 
{
        struct sstat *sstat=p;
        static char buf[16]="slrec ";
        count_t value = sstat->mem.slabreclaim * 1024;
        val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
	*color = -1;
        return buf;
}

sys_printdef syspdef_RECSLAB = {"RECSLAB", sysprt_RECSLAB};
/*******************************************************************/
char *
sysprt_SHMEM(void *p, void *notused, int badness, int *color) 
{
        struct sstat *sstat=p;
        static char buf[16]="shmem  ";
        count_t value = sstat->mem.shmem * 1024;
        val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
	*color = -1;
        return buf;
}

sys_printdef syspdef_SHMEM = {"SHMEM", sysprt_SHMEM};
/*******************************************************************/
char *
sysprt_SHMRSS(void *p, void *notused, int badness, int *color) 
{
        struct sstat *sstat=p;
        static char buf[16]="shrss  ";
        count_t value = sstat->mem.shmrss * 1024;
        val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
	*color = -1;
        return buf;
}

sys_printdef syspdef_SHMRSS = {"SHMRSS", sysprt_SHMRSS};
/*******************************************************************/
char *
sysprt_SHMSWP(void *p, void *notused, int badness, int *color) 
{
        struct sstat *sstat=p;
        static char buf[16]="shswp  ";
        count_t value = sstat->mem.shmswp * 1024;
        val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
	*color = -1;
        return buf;
}

sys_printdef syspdef_SHMSWP = {"SHMSWP", sysprt_SHMSWP};
/*******************************************************************/
char *
sysprt_HUPTOT(void *p, void *notused, int badness, int *color) 
{
        struct sstat *sstat=p;
        static char buf[16]="hptot  ";
	*color = -1;
        val2memstr(sstat->mem.tothugepage * sstat->mem.hugepagesz,
						buf+6, sizeof buf-6, MBFORMAT, 0, 0);
        return buf;
}

sys_printdef syspdef_HUPTOT = {"HUPTOT", sysprt_HUPTOT};
/*******************************************************************/
char *
sysprt_HUPUSE(void *p, void *notused, int badness, int *color) 
{
        struct sstat *sstat=p;
        static char buf[16]="hpuse  ";
	*color = -1;
        val2memstr( (sstat->mem.tothugepage - sstat->mem.freehugepage) *
				sstat->mem.hugepagesz, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
        return buf;
}

sys_printdef syspdef_HUPUSE = {"HUPUSE", sysprt_HUPUSE};
/*******************************************************************/
char *
sysprt_VMWBAL(void *p, void *notused, int badness, int *color) 
{
        struct sstat *sstat=p;
        static char buf[16]="vmbal  ";
	*color = -1;
        val2memstr(sstat->mem.vmwballoon, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
        return buf;
}

sys_printdef syspdef_VMWBAL = {"VMWBAL", sysprt_VMWBAL};
/*******************************************************************/
char *
sysprt_SWPTOT(void *p, void *notused, int badness, int *color) 
{
        struct sstat *sstat=p;
        static char buf[16]="tot    ";
	*color = -1;
        val2memstr(sstat->mem.totswap, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
        return buf;
}

sys_printdef syspdef_SWPTOT = {"SWPTOT", sysprt_SWPTOT};
/*******************************************************************/
char *
sysprt_SWPFREE(void *p, void *notused, int badness, int *color) 
{
        struct sstat *sstat=p;
        static char buf[16]="free  ";
	*color = -1;
        val2memstr(sstat->mem.freeswap, buf+6, sizeof buf-6, MBFORMAT, 0, 0);
        return buf;
}

sys_printdef syspdef_SWPFREE = {"SWPFREE", sysprt_SWPFREE};
/*******************************************************************/
char *
sysprt_SWPCOMMITTED(void *p, void *notused, int badness, int *color) 
{
        struct sstat *sstat=p;
        static char buf[16]="vmcom  ";
	count_t value = sstat->mem.committed * 1024;

        val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);

	if (sstat->mem.commitlim && sstat->mem.committed > sstat->mem.commitlim)
		*color = COLORALMOST;

        return buf;
}

sys_printdef syspdef_SWPCOMMITTED = {"SWPCOMMITTED", sysprt_SWPCOMMITTED};
/*******************************************************************/
char *
sysprt_SWPCOMMITLIM(void *p, void *notused, int badness, int *color) 
{
        struct sstat *sstat=p;
        static char buf[16]="vmlim  ";
	count_t value = sstat->mem.commitlim * 1024;

        val2memstr(value, buf+6, sizeof buf-6, MBFORMAT, 0, 0);

	if (sstat->mem.commitlim && sstat->mem.committed > sstat->mem.commitlim)
		*color = COLORINFO;

        return buf;
}

sys_printdef syspdef_SWPCOMMITLIM = {"SWPCOMMITLIM", sysprt_SWPCOMMITLIM};
/*******************************************************************/
char *
sysprt_PAGSCAN(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="scan  ";
        val2valstr(sstat->mem.pgscans, buf+5, sizeof buf-5, 7, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_PAGSCAN = {"PAGSCAN", sysprt_PAGSCAN};
/*******************************************************************/
char *
sysprt_PAGSTEAL(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="steal  ";
        val2valstr(sstat->mem.pgsteal, buf+6, sizeof buf-6, 6, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_PAGSTEAL = {"PAGSTEAL", sysprt_PAGSTEAL};
/*******************************************************************/
char *
sysprt_PAGSTALL(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="stall  ";
        val2valstr(sstat->mem.allocstall, buf+6, sizeof buf-6, 6, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_PAGSTALL = {"PAGSTALL", sysprt_PAGSTALL};
/*******************************************************************/
char *
sysprt_PAGSWIN(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="swin   ";
        val2valstr(sstat->mem.swins, buf+5, sizeof buf-5, 7, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_PAGSWIN = {"PAGSWIN", sysprt_PAGSWIN};
/*******************************************************************/
char *
sysprt_PAGSWOUT(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="swout  ";
	*color = -1;
        val2valstr(sstat->mem.swouts, buf+6, sizeof buf-6, 6, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_PAGSWOUT = {"PAGSWOUT", sysprt_PAGSWOUT};
/*******************************************************************/
char *
sysprt_CONTNAME(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam 	*as=q;
        static char 	buf[32] = "ctid ";

	*color = -1;

        snprintf(buf+5, sizeof buf-5, "%7lu", sstat->cfs.cont[as->index].ctid);
        return buf;
}

sys_printdef syspdef_CONTNAME = {"CONTNAME", sysprt_CONTNAME};
/*******************************************************************/
char *
sysprt_CONTNPROC(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam      *as=q;
        static char buf[16]="nproc  ";

	*color = -1;

        val2valstr(sstat->cfs.cont[as->index].numproc, 
                 	  buf+6, sizeof buf-6, 6, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_CONTNPROC = {"CONTNPROC", sysprt_CONTNPROC};
/*******************************************************************/
char *
sysprt_CONTCPU(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam      *as=q;
        static char buf[16];
	float  perc;

	count_t	used = sstat->cfs.cont[as->index].system + 
                       sstat->cfs.cont[as->index].user + 
                       sstat->cfs.cont[as->index].nice;

	*color = -1;

	if (sstat->cfs.cont[as->index].uptime)
	{
		perc = used * 100.0 / sstat->cfs.cont[as->index].uptime;
        	snprintf(buf, sizeof buf, "cpubusy %3.0f%%", perc);
	}
	else
        	snprintf(buf, sizeof buf, "cpubusy   ?%%");

        return buf;
}

sys_printdef syspdef_CONTCPU = {"CONTCPU", sysprt_CONTCPU};
/*******************************************************************/
char *
sysprt_CONTMEM(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam      *as=q;
        static char buf[16]="mem   ";

	*color = -1;

        val2memstr(sstat->cfs.cont[as->index].physpages * pagesize,
					buf+6, sizeof buf-6, MBFORMAT, 0, 0);
        return buf;
}

sys_printdef syspdef_CONTMEM = {"CONTMEM", sysprt_CONTMEM};
/*******************************************************************/
char *
sysprt_DSKNAME(void *p, void *q, int badness, int *color) 
{
        extraparam 	*as=q;
        static char 	buf[16];
	char		*pn;
	int		len;

	*color = -1;

        if ( (len = strlen(as->perdsk[as->index].name)) > 12)
		pn = as->perdsk[as->index].name + len - 12;
	else
		pn = as->perdsk[as->index].name;

        snprintf(buf, sizeof buf, "%12.12s", pn);
        return buf;
}

sys_printdef syspdef_DSKNAME = {"DSKNAME", sysprt_DSKNAME};
/*******************************************************************/
char *
sysprt_DSKBUSY(void *p, void *q, int badness, int *color) 
{
        extraparam	*as=q;
	double		perc;
        static char 	buf[16]="busy  ";

	*color = -1;
	perc = as->perdsk[as->index].io_ms * 100.0 / as->mstot;

	if (perc >= 0.0 && perc < 1000000.0)
		snprintf(buf+5, sizeof buf-5, "%6.0lf%%", perc);
	else
		snprintf(buf+5, sizeof buf-5, "%6.0lf%%",  999999.0);

        return buf;
}

sys_printdef syspdef_DSKBUSY = {"DSKBUSY", sysprt_DSKBUSY};
/*******************************************************************/
char *
sysprt_DSKNREAD(void *p, void *q, int badness, int *color) 
{
        extraparam	*as=q;
        static char	buf[16]="read  ";

	*color = -1;

        val2valstr(as->perdsk[as->index].nread >= 0 ?
                   as->perdsk[as->index].nread : 0,
                   buf+5, sizeof buf-5, 7, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_DSKNREAD = {"DSKNREAD", sysprt_DSKNREAD};
/*******************************************************************/
char *
sysprt_DSKNWRITE(void *p, void *q, int badness, int *color) 
{
        extraparam	*as=q;
        static char	buf[16]="write ";

	*color = -1;

        val2valstr(as->perdsk[as->index].nwrite >= 0 ?
        	           as->perdsk[as->index].nwrite : 0,
        	           buf+6, sizeof buf-6, 6, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_DSKNWRITE = {"DSKNWRITE", sysprt_DSKNWRITE};
/*******************************************************************/
char *
sysprt_DSKKBPERWR(void *p, void *q, int badness, int *color) 
{
        extraparam	*as=q;
        static char	buf[16]="KiB/w ";
	struct perdsk 	*dp = &(as->perdsk[as->index]);

        val2valstr(dp->nwrite > 0 ?  dp->nwsect / dp->nwrite / 2 : 0,
                   buf+6, sizeof buf-6, 6, 0, as->nsecs);
        return buf;
}

sys_printdef syspdef_DSKKBPERWR = {"DSKKBPERWR", sysprt_DSKKBPERWR};
/*******************************************************************/
char *
sysprt_DSKKBPERRD(void *p, void *q, int badness, int *color) 
{
        extraparam	*as=q;
        static char	buf[16]="KiB/r ";
	struct perdsk 	*dp = &(as->perdsk[as->index]);

        val2valstr(dp->nread > 0 ?  dp->nrsect / dp->nread / 2 : 0,
                   buf+6, sizeof buf-6, 6, 0, as->nsecs);
        return buf;
}

sys_printdef syspdef_DSKKBPERRD = {"DSKKBPERRD", sysprt_DSKKBPERRD};
/*******************************************************************/
char *
sysprt_DSKMBPERSECWR(void *p, void *q, int badness, int *color) 
{
        extraparam	*as=q;
        static char	buf[16]="MBw/s ";
	struct perdsk 	*dp = &(as->perdsk[as->index]);

        snprintf(buf+6, sizeof buf-6, "%6.1lf",
                               dp->nwsect / 2.0 / 1024 / as->nsecs);
        return buf;
}

sys_printdef syspdef_DSKMBPERSECWR = {"DSKMBPERSECWR", sysprt_DSKMBPERSECWR};
/*******************************************************************/
char *
sysprt_DSKMBPERSECRD(void *p, void *q, int badness, int *color) 
{
        extraparam	*as=q;
        static char	buf[16]="MBr/s ";
	struct perdsk 	*dp = &(as->perdsk[as->index]);

        snprintf(buf+6, sizeof buf-6, "%6.1lf",
                               dp->nrsect / 2.0 / 1024 / as->nsecs);
        return buf;
}

sys_printdef syspdef_DSKMBPERSECRD = {"DSKMBPERSECRD", sysprt_DSKMBPERSECRD};
/*******************************************************************/
char *
sysprt_DSKAVQUEUE(void *p, void *q, int badness, int *color) 
{
        extraparam	*as=q;
        static char	buf[16]="avq  ";
	struct perdsk 	*dp = &(as->perdsk[as->index]);

	snprintf(buf+4, sizeof buf-4, "%8.2f", dp->io_ms ?
                               (double)dp->avque / dp->io_ms : 0.0);
        return buf;
}

sys_printdef syspdef_DSKAVQUEUE = {"DSKAVQUEUE", sysprt_DSKAVQUEUE};
/*******************************************************************/
char *
sysprt_DSKAVIO(void *p, void *q, int badness, int *color) 
{
        extraparam	*as=q;
        static char	buf[16]="avio  ";
        double 		tim= as->iotot > 0 ? 
                     	(double)(as->perdsk[as->index].io_ms) / as->iotot : 0;

	*color = -1;

        if (tim > 100.0)
        {
                snprintf(buf+5, sizeof buf-5, "%4.0lf ms", tim);
        } 
        else if (tim > 10.0) 
        {
                snprintf(buf+5, sizeof buf-5, "%4.1lf ms", tim);
        }
        else 
        {
                snprintf(buf+5, sizeof buf-5, "%4.2lf ms", tim);
        }

        return buf;
}

sys_printdef syspdef_DSKAVIO = {"DSKAVIO", sysprt_DSKAVIO};
/*******************************************************************/
char *
sysprt_NETTRANSPORT(void *p, void *notused, int badness, int *color) 
{
        return "transport   ";
}

sys_printdef syspdef_NETTRANSPORT = {"NETTRANSPORT", sysprt_NETTRANSPORT};
/*******************************************************************/
char *
sysprt_NETTCPI(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="tcpi   ";
        val2valstr(sstat->net.tcp.InSegs,  buf+5, sizeof buf-5, 7, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETTCPI = {"NETTCPI", sysprt_NETTCPI};
/*******************************************************************/
char *
sysprt_NETTCPO(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="tcpo   ";
        val2valstr(sstat->net.tcp.OutSegs,  buf+5, sizeof buf-5, 7, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETTCPO = {"NETTCPO", sysprt_NETTCPO};
/*******************************************************************/
char *
sysprt_NETTCPACTOPEN(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="tcpao  ";
        val2valstr(sstat->net.tcp.ActiveOpens,  buf+6, sizeof buf-6, 6, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETTCPACTOPEN = {"NETTCPACTOPEN", sysprt_NETTCPACTOPEN};
/*******************************************************************/
char *
sysprt_NETTCPPASVOPEN(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="tcppo  ";
        val2valstr(sstat->net.tcp.PassiveOpens, buf+6, sizeof buf-6, 6, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETTCPPASVOPEN = {"NETTCPPASVOPEN", sysprt_NETTCPPASVOPEN};
/*******************************************************************/
char *
sysprt_NETTCPRETRANS(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="tcprs  ";
        val2valstr(sstat->net.tcp.RetransSegs,  buf+6, sizeof buf-6, 6, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETTCPRETRANS = {"NETTCPRETRANS", sysprt_NETTCPRETRANS};
/*******************************************************************/
char *
sysprt_NETTCPINERR(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="tcpie  ";
        val2valstr(sstat->net.tcp.InErrs,  buf+6, sizeof buf-6, 6, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETTCPINERR = {"NETTCPINERR", sysprt_NETTCPINERR};
/*******************************************************************/
char *
sysprt_NETTCPORESET(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="tcpor  ";
        val2valstr(sstat->net.tcp.OutRsts,  buf+6, sizeof buf-6, 6, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETTCPORESET = {"NETTCPORESET", sysprt_NETTCPORESET};
/*******************************************************************/
char *
sysprt_NETUDPNOPORT(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="udpnp  ";
        val2valstr(sstat->net.udpv4.NoPorts,  buf+6, sizeof buf-6, 6, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETUDPNOPORT = {"NETUDPNOPORT", sysprt_NETUDPNOPORT};
/*******************************************************************/
char *
sysprt_NETUDPINERR(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="udpie  ";
        val2valstr(sstat->net.udpv4.InErrors,  buf+6, sizeof buf-6, 6, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETUDPINERR = {"NETUDPINERR", sysprt_NETUDPINERR};
/*******************************************************************/
char *
sysprt_NETUDPI(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="udpi   ";
        count_t udpin  = sstat->net.udpv4.InDatagrams  +
                        sstat->net.udpv6.Udp6InDatagrams;
        val2valstr(udpin,   buf+5, sizeof buf-5, 7, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETUDPI = {"NETUDPI", sysprt_NETUDPI};
/*******************************************************************/
char *
sysprt_NETUDPO(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="udpo   ";
        count_t udpout = sstat->net.udpv4.OutDatagrams +
                        sstat->net.udpv6.Udp6OutDatagrams;
        val2valstr(udpout,   buf+5, sizeof buf-5, 7, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETUDPO = {"NETUDPO", sysprt_NETUDPO};
/*******************************************************************/
char *
sysprt_NETNETWORK(void *p, void *notused, int badness, int *color) 
{
        return "network     ";
}

sys_printdef syspdef_NETNETWORK = {"NETNETWORK", sysprt_NETNETWORK};
/*******************************************************************/
char *
sysprt_NETIPI(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="ipi    ";
        count_t ipin    = sstat->net.ipv4.InReceives  +
                        sstat->net.ipv6.Ip6InReceives;
        val2valstr(ipin, buf+4, sizeof buf-4, 8, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETIPI = {"NETIPI", sysprt_NETIPI};
/*******************************************************************/
char *
sysprt_NETIPO(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="ipo    ";
        count_t ipout   = sstat->net.ipv4.OutRequests +
                        sstat->net.ipv6.Ip6OutRequests;
        val2valstr(ipout, buf+4, sizeof buf-4, 8, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETIPO = {"NETIPO", sysprt_NETIPO};
/*******************************************************************/
char *
sysprt_NETIPFRW(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="ipfrw  ";
        count_t ipfrw   = sstat->net.ipv4.ForwDatagrams +
                        sstat->net.ipv6.Ip6OutForwDatagrams;
        val2valstr(ipfrw, buf+6, sizeof buf-6, 6, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETIPFRW = {"NETIPFRW", sysprt_NETIPFRW};
/*******************************************************************/
char *
sysprt_NETIPDELIV(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="deliv  ";
        count_t ipindel = sstat->net.ipv4.InDelivers +
                        sstat->net.ipv6.Ip6InDelivers;
        val2valstr(ipindel, buf+6, sizeof buf-6, 6, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETIPDELIV = {"NETIPDELIV", sysprt_NETIPDELIV};
/*******************************************************************/
char *
sysprt_NETICMPIN(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="icmpi  ";
        count_t icmpin = sstat->net.icmpv4.InMsgs+
                        sstat->net.icmpv6.Icmp6InMsgs;
        val2valstr(icmpin , buf+6, sizeof buf-6, 6, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETICMPIN = {"NETICMPIN", sysprt_NETICMPIN};
/*******************************************************************/
char *
sysprt_NETICMPOUT(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="icmpo  ";
        count_t icmpin = sstat->net.icmpv4.OutMsgs+
                        sstat->net.icmpv6.Icmp6OutMsgs;
        val2valstr(icmpin , buf+6, sizeof buf-6, 6, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETICMPOUT = {"NETICMPOUT", sysprt_NETICMPOUT};
/*******************************************************************/
char *
sysprt_NETNAME(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
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

	        snprintf(buf, sizeof buf, "%-7.7s %3lld%%", 
       		          sstat->intf.intf[as->index].name, busy);

        } 
        else 
        {
                snprintf(buf, sizeof buf, "%-7.7s ----",
                               sstat->intf.intf[as->index].name);
                strcpy(buf+8, "----");
        } 
        return buf;
}

sys_printdef syspdef_NETNAME = {"NETNAME", sysprt_NETNAME};
/*******************************************************************/
char *
sysprt_NETPCKI(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="pcki  ";

	*color = -1;

        val2valstr(sstat->intf.intf[as->index].rpack, 
                   buf+5, sizeof buf-5, 7, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETPCKI = {"NETPCKI", sysprt_NETPCKI};
/*******************************************************************/
char *
sysprt_NETPCKO(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="pcko  ";

	*color = -1;

        val2valstr(sstat->intf.intf[as->index].spack, 
                   buf+5, sizeof buf-5, 7, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETPCKO = {"NETPCKO", sysprt_NETPCKO};
/*******************************************************************/
/*
** convert byte-transfers to bit-transfers     (*    8)
** convert bit-transfers  to kilobit-transfers (/ 1000)
** per second
*/
char *makenetspeed(count_t val, int nsecs)
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

        snprintf(buf+3, sizeof buf-3, "%4lld %cbps", val, c);

        return buf;
}
/*******************************************************************/
char *
sysprt_NETSPEEDMAX(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat = p;
        extraparam *as = q;
        static char buf[16];
        count_t speed = sstat->intf.intf[as->index].speed;

	*color = -1;

	if (speed < 10000)
	{
        	snprintf(buf, sizeof buf, "sp %4lld Mbps", speed);
	}
	else
	{
		speed /= 1000;
        	snprintf(buf, sizeof buf, "sp %4lld Gbps", speed);
	}

        return buf;
}

sys_printdef syspdef_NETSPEEDMAX = {"NETSPEEDMAX", sysprt_NETSPEEDMAX};
/*******************************************************************/
char *
sysprt_NETSPEEDIN(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;

	*color = -1;

        char *ps=makenetspeed(sstat->intf.intf[as->index].rbyte,as->nsecs);
        ps[0]='s';
        ps[1]='i';
        return ps;
}

sys_printdef syspdef_NETSPEEDIN = {"NETSPEEDIN", sysprt_NETSPEEDIN};
/*******************************************************************/
char *
sysprt_NETSPEEDOUT(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;

	*color = -1;

        char *ps=makenetspeed(sstat->intf.intf[as->index].sbyte,as->nsecs);
        ps[0]='s';
        ps[1]='o';
        return ps;
}

sys_printdef syspdef_NETSPEEDOUT = {"NETSPEEDOUT", sysprt_NETSPEEDOUT};
/*******************************************************************/
char *
sysprt_NETCOLLIS(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="coll  ";
        val2valstr(sstat->intf.intf[as->index].scollis, 
                   buf+5, sizeof buf-5, 7, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETCOLLIS = {"NETCOLLIS", sysprt_NETCOLLIS};
/*******************************************************************/
char *
sysprt_NETMULTICASTIN(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="mlti ";
        val2valstr(sstat->intf.intf[as->index].rmultic, 
                   buf+5, sizeof buf-5, 7, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETMULTICASTIN = {"NETMULTICASTIN", sysprt_NETMULTICASTIN};
/*******************************************************************/
char *
sysprt_NETRCVERR(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="erri   ";
        val2valstr(sstat->intf.intf[as->index].rerrs, 
                   buf+5, sizeof buf-5, 7, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETRCVERR = {"NETRCVERR", sysprt_NETRCVERR};
/*******************************************************************/
char *
sysprt_NETSNDERR(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="erro   ";
        val2valstr(sstat->intf.intf[as->index].serrs, 
                   buf+5, sizeof buf-5, 7, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETSNDERR = {"NETSNDERR", sysprt_NETSNDERR};
/*******************************************************************/
char *
sysprt_NETRCVDROP(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="drpi   ";
        val2valstr(sstat->intf.intf[as->index].rdrop,
                   buf+5, sizeof buf-5, 7, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETRCVDROP = {"NETRCVDROP", sysprt_NETRCVDROP};
/*******************************************************************/
char *
sysprt_NETSNDDROP(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="drpo   ";
        val2valstr(sstat->intf.intf[as->index].sdrop,
                   buf+5, sizeof buf-5, 7, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NETSNDDROP = {"NETSNDDROP", sysprt_NETSNDDROP};
/*******************************************************************/
char *
sysprt_NFMSERVER(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
	static char buf[16] = "srv ";
        char mntdev[128], *ps;

        memcpy(mntdev, sstat->nfs.nfsmnt[as->index].mountdev, sizeof mntdev);

	if ( (ps = strchr(mntdev, ':')) )		// colon found?
		*ps = '\0';
	else
		strcpy(mntdev, "?");

	snprintf(buf+4, sizeof buf-4, "%8.8s", mntdev);
        return buf;
}

sys_printdef syspdef_NFMSERVER = {"NFMSERVER", sysprt_NFMSERVER};
/*******************************************************************/
char *
sysprt_NFMPATH(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
	static char buf[16];
        char mntdev[128], *ps;
	int len;

        memcpy(mntdev, sstat->nfs.nfsmnt[as->index].mountdev, sizeof mntdev);

	if ( (ps = strchr(mntdev, ':')) )		// colon found?
		ps++;
	else
		ps = mntdev;

	len = strlen(ps);
        if (len > 12)
		ps = ps + len - 12;

	snprintf(buf, sizeof buf, "%12.12s", ps);
        return buf;
}

sys_printdef syspdef_NFMPATH = {"NFMPATH", sysprt_NFMPATH};
/*******************************************************************/
char *
sysprt_NFMTOTREAD(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="read   ";

        val2memstr(sstat->nfs.nfsmnt[as->index].bytestotread,
			buf+6, sizeof buf-6, KBFORMAT, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFMTOTREAD = {"NFMTOTREAD", sysprt_NFMTOTREAD};
/*******************************************************************/
char *
sysprt_NFMTOTWRITE(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="write   ";

        val2memstr(sstat->nfs.nfsmnt[as->index].bytestotwrite,
			buf+6, sizeof buf-6, KBFORMAT, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFMTOTWRITE = {"NFMTOTWRITE", sysprt_NFMTOTWRITE};
/*******************************************************************/
char *
sysprt_NFMNREAD(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="nread    ";

        val2memstr(sstat->nfs.nfsmnt[as->index].bytesread,
			buf+6, sizeof buf-6, KBFORMAT, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFMNREAD = {"NFMNREAD", sysprt_NFMNREAD};
/*******************************************************************/
char *
sysprt_NFMNWRITE(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="nwrit    ";

        val2memstr(sstat->nfs.nfsmnt[as->index].byteswrite,
			buf+6, sizeof buf-6, KBFORMAT, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFMNWRITE = {"NFMNWRITE", sysprt_NFMNWRITE};
/*******************************************************************/
char *
sysprt_NFMDREAD(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="dread    ";

        val2memstr(sstat->nfs.nfsmnt[as->index].bytesdread,
			buf+6, sizeof buf-6, KBFORMAT, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFMDREAD = {"NFMDREAD", sysprt_NFMDREAD};
/*******************************************************************/
char *
sysprt_NFMDWRITE(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="dwrit    ";

        val2memstr(sstat->nfs.nfsmnt[as->index].bytesdwrite,
			buf+6, sizeof buf-6, KBFORMAT, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFMDWRITE = {"NFMDWRITE", sysprt_NFMDWRITE};
/*******************************************************************/
char *
sysprt_NFMMREAD(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="mread    ";

        val2memstr(sstat->nfs.nfsmnt[as->index].pagesmread *pagesize,
			buf+6, sizeof buf-6, KBFORMAT, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFMMREAD = {"NFMMREAD", sysprt_NFMMREAD};
/*******************************************************************/
char *
sysprt_NFMMWRITE(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="mwrit    ";

        val2memstr(sstat->nfs.nfsmnt[as->index].pagesmwrite *pagesize,
			buf+6, sizeof buf-6, KBFORMAT, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFMMWRITE = {"NFMMWRITE", sysprt_NFMMWRITE};
/*******************************************************************/
char *
sysprt_NFCRPCCNT(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="rpc   ";
        val2valstr(sstat->nfs.client.rpccnt,
                   buf+4, sizeof buf-4, 8, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFCRPCCNT = {"NFCRPCCNT", sysprt_NFCRPCCNT};
/*******************************************************************/
char *
sysprt_NFCRPCREAD(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="read   ";
        val2valstr(sstat->nfs.client.rpcread,
                   buf+5, sizeof buf-5, 7, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFCRPCREAD = {"NFCRPCREAD", sysprt_NFCRPCREAD};
/*******************************************************************/
char *
sysprt_NFCRPCWRITE(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="write   ";
        val2valstr(sstat->nfs.client.rpcwrite,
                   buf+6, sizeof buf-6, 6, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFCRPCWRITE = {"NFCRPCWRITE", sysprt_NFCRPCWRITE};
/*******************************************************************/
char *
sysprt_NFCRPCRET(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="retxmit ";
        val2valstr(sstat->nfs.client.rpcretrans,
                   buf+8, sizeof buf-8, 4, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFCRPCRET = {"NFCRPCRET", sysprt_NFCRPCRET};
/*******************************************************************/
char *
sysprt_NFCRPCARF(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="autref  ";
        val2valstr(sstat->nfs.client.rpcautrefresh,
                   buf+7, sizeof buf-7, 5, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFCRPCARF = {"NFCRPCARF", sysprt_NFCRPCARF};
/*******************************************************************/
char *
sysprt_NFSRPCCNT(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="rpc   ";
        val2valstr(sstat->nfs.server.rpccnt,
                   buf+4, sizeof buf-4, 8, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFSRPCCNT = {"NFSRPCCNT", sysprt_NFSRPCCNT};
/*******************************************************************/
char *
sysprt_NFSRPCREAD(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="cread   ";
        val2valstr(sstat->nfs.server.rpcread,
                   buf+6, sizeof buf-6, 6, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFSRPCREAD = {"NFSRPCREAD", sysprt_NFSRPCREAD};
/*******************************************************************/
char *
sysprt_NFSRPCWRITE(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="cwrit   ";
        val2valstr(sstat->nfs.server.rpcwrite,
                   buf+6, sizeof buf-6, 6, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFSRPCWRITE = {"NFSRPCWRITE", sysprt_NFSRPCWRITE};
/*******************************************************************/
char *
sysprt_NFSBADFMT(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="badfmt   ";
        val2valstr(sstat->nfs.server.rpcbadfmt,
                   buf+7, sizeof buf-7, 5, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFSBADFMT = {"NFSBADFMT", sysprt_NFSBADFMT};
/*******************************************************************/
char *
sysprt_NFSBADAUT(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="badaut   ";
        val2valstr(sstat->nfs.server.rpcbadaut,
                   buf+7, sizeof buf-7, 5, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFSBADAUT = {"NFSBADAUT", sysprt_NFSBADAUT};
/*******************************************************************/
char *
sysprt_NFSBADCLN(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="badcln   ";
        val2valstr(sstat->nfs.server.rpcbadcln,
                   buf+7, sizeof buf-7, 5, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFSBADCLN = {"NFSBADCLN", sysprt_NFSBADCLN};
/*******************************************************************/
char *
sysprt_NFSNETTCP(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="nettcp   ";
        val2valstr(sstat->nfs.server.nettcpcnt,
                   buf+7, sizeof buf-7, 5, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFSNETTCP = {"NFSNETTCP", sysprt_NFSNETTCP};
/*******************************************************************/
char *
sysprt_NFSNETUDP(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="netudp   ";
        val2valstr(sstat->nfs.server.netudpcnt,
                   buf+7, sizeof buf-7, 5, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFSNETUDP = {"NFSNETUDP", sysprt_NFSNETUDP};
/*******************************************************************/
char *
sysprt_NFSNRBYTES(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam	*as=q;
        static char	buf[32]="MBcr/s ";

        snprintf(buf+7, sizeof buf-7, "%5.1lf",
		sstat->nfs.server.nrbytes / 1024.0 / 1024.0 / as->nsecs);

        return buf;
}

sys_printdef syspdef_NFSNRBYTES = {"NFSNRBYTES", sysprt_NFSNRBYTES};
/*******************************************************************/
char *
sysprt_NFSNWBYTES(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam	*as=q;
        static char	buf[32]="MBcw/s ";

        snprintf(buf+7, sizeof buf-7, "%5.1lf",
		sstat->nfs.server.nwbytes / 1024.0 / 1024.0 / as->nsecs);

        return buf;
}

sys_printdef syspdef_NFSNWBYTES = {"NFSNWBYTES", sysprt_NFSNWBYTES};
/*******************************************************************/
char *
sysprt_NFSRCHITS(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="rchits   ";
        val2valstr(sstat->nfs.server.rchits,
                   buf+8, sizeof buf-8, 4, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFSRCHITS = {"NFSRCHITS", sysprt_NFSRCHITS};
/*******************************************************************/
char *
sysprt_NFSRCMISS(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="rcmiss   ";
        val2valstr(sstat->nfs.server.rcmiss,
                   buf+8, sizeof buf-8, 4, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFSRCMISS = {"NFSRCMISS", sysprt_NFSRCMISS};
/*******************************************************************/
char *
sysprt_NFSRCNOCA(void *p, void *q, int badness, int *color) 
{
        struct sstat *sstat=p;
        extraparam *as=q;
        static char buf[16]="rcnoca   ";
        val2valstr(sstat->nfs.server.rcnoca,
                   buf+8, sizeof buf-8, 4, as->avgval, as->nsecs);
        return buf;
}

sys_printdef syspdef_NFSRCNOCA = {"NFSRCNOCA", sysprt_NFSRCNOCA};
/*******************************************************************/
char *
sysprt_BLANKBOX(void *p, void *notused, int badness, int *color) 
{
        return "            ";
}

sys_printdef syspdef_BLANKBOX = {"BLANKBOX", sysprt_BLANKBOX};
