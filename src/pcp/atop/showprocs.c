/*
** ATOP - System & Process Monitor 
**
** The program 'atop' offers the possibility to view the activity of
** the system on system-level as well as process-level.
**
** This source-file contains the Linux-specific functions to calculate
** figures to be visualized.
**
** Copyright (C) 2015,2019,2021 Red Hat.
** Copyright (C) 2009 JC van Winkel
** Copyright (C) 2000-2010 Gerlof Langeveld
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

#include "atop.h"
#include "photoproc.h"
#include "photosyst.h"
#include "showgeneric.h"
#include "showlinux.h"

static void	format_bandw(char *, size_t, count_t);
static void	gettotwidth(proc_printpair *, int *, int *, int *);
static int 	*getspacings(proc_printpair *);

char *procprt_TID_ae(struct tstat *, int, double);
char *procprt_PID_a(struct tstat *, int, double);
char *procprt_PID_e(struct tstat *, int, double);
char *procprt_PPID_a(struct tstat *, int, double);
char *procprt_PPID_e(struct tstat *, int, double);
char *procprt_VPID_a(struct tstat *, int, double);
char *procprt_VPID_e(struct tstat *, int, double);
char *procprt_CTID_a(struct tstat *, int, double);
char *procprt_CTID_e(struct tstat *, int, double);
char *procprt_CID_a(struct tstat *, int, double);
char *procprt_CID_e(struct tstat *, int, double);
char *procprt_SYSCPU_ae(struct tstat *, int, double);
char *procprt_USRCPU_ae(struct tstat *, int, double);
char *procprt_VGROW_a(struct tstat *, int, double);
char *procprt_VGROW_e(struct tstat *, int, double);
char *procprt_RGROW_a(struct tstat *, int, double);
char *procprt_RGROW_e(struct tstat *, int, double);
char *procprt_MINFLT_ae(struct tstat *, int, double);
char *procprt_MAJFLT_ae(struct tstat *, int, double);
char *procprt_VSTEXT_a(struct tstat *, int, double);
char *procprt_VSTEXT_e(struct tstat *, int, double);
char *procprt_VSIZE_a(struct tstat *, int, double);
char *procprt_VSIZE_e(struct tstat *, int, double);
char *procprt_RSIZE_a(struct tstat *, int, double);
char *procprt_RSIZE_e(struct tstat *, int, double);
char *procprt_PSIZE_a(struct tstat *, int, double);
char *procprt_PSIZE_e(struct tstat *, int, double);
char *procprt_VSLIBS_a(struct tstat *, int, double);
char *procprt_VSLIBS_e(struct tstat *, int, double);
char *procprt_VDATA_a(struct tstat *, int, double);
char *procprt_VDATA_e(struct tstat *, int, double);
char *procprt_VSTACK_a(struct tstat *, int, double);
char *procprt_VSTACK_e(struct tstat *, int, double);
char *procprt_SWAPSZ_a(struct tstat *, int, double);
char *procprt_SWAPSZ_e(struct tstat *, int, double);
char *procprt_LOCKSZ_a(struct tstat *, int, double);
char *procprt_LOCKSZ_e(struct tstat *, int, double);
char *procprt_CMD_a(struct tstat *, int, double);
char *procprt_CMD_e(struct tstat *, int, double);
char *procprt_RUID_ae(struct tstat *, int, double);
char *procprt_EUID_a(struct tstat *, int, double);
char *procprt_EUID_e(struct tstat *, int, double);
char *procprt_SUID_a(struct tstat *, int, double);
char *procprt_SUID_e(struct tstat *, int, double);
char *procprt_FSUID_a(struct tstat *, int, double);
char *procprt_FSUID_e(struct tstat *, int, double);
char *procprt_RGID_ae(struct tstat *, int, double);
char *procprt_EGID_a(struct tstat *, int, double);
char *procprt_EGID_e(struct tstat *, int, double);
char *procprt_SGID_a(struct tstat *, int, double);
char *procprt_SGID_e(struct tstat *, int, double);
char *procprt_FSGID_a(struct tstat *, int, double);
char *procprt_FSGID_e(struct tstat *, int, double);
char *procprt_STDATE_ae(struct tstat *, int, double);
char *procprt_STTIME_ae(struct tstat *, int, double);
char *procprt_ENDATE_a(struct tstat *, int, double);
char *procprt_ENDATE_e(struct tstat *, int, double);
char *procprt_ENTIME_a(struct tstat *, int, double);
char *procprt_ENTIME_e(struct tstat *, int, double);
char *procprt_THR_a(struct tstat *, int, double);
char *procprt_THR_e(struct tstat *, int, double);
char *procprt_TRUN_a(struct tstat *, int, double);
char *procprt_TRUN_e(struct tstat *, int, double);
char *procprt_TSLPI_a(struct tstat *, int, double);
char *procprt_TSLPI_e(struct tstat *, int, double);
char *procprt_TSLPU_a(struct tstat *, int, double);
char *procprt_TSLPU_e(struct tstat *, int, double);
char *procprt_TIDLE_a(struct tstat *, int, double);
char *procprt_TIDLE_e(struct tstat *, int, double);
char *procprt_POLI_a(struct tstat *, int, double);
char *procprt_POLI_e(struct tstat *, int, double);
char *procprt_NICE_a(struct tstat *, int, double);
char *procprt_NICE_e(struct tstat *, int, double);
char *procprt_PRI_a(struct tstat *, int, double);
char *procprt_PRI_e(struct tstat *, int, double);
char *procprt_RTPR_a(struct tstat *, int, double);
char *procprt_RTPR_e(struct tstat *, int, double);
char *procprt_CURCPU_a(struct tstat *, int, double);
char *procprt_CURCPU_e(struct tstat *, int, double);
char *procprt_ST_a(struct tstat *, int, double);
char *procprt_ST_e(struct tstat *, int, double);
char *procprt_EXC_a(struct tstat *, int, double);
char *procprt_EXC_e(struct tstat *, int, double);
char *procprt_S_a(struct tstat *, int, double);
char *procprt_S_e(struct tstat *, int, double);
char *procprt_COMMAND_LINE_ae(struct tstat *, int, double);
char *procprt_NPROCS_ae(struct tstat *, int, double);
char *procprt_RDDSK_a(struct tstat *, int, double);
char *procprt_RDDSK_e(struct tstat *, int, double);
char *procprt_WRDSK_a(struct tstat *, int, double);
char *procprt_WRDSK_e(struct tstat *, int, double);
char *procprt_CWRDSK_a(struct tstat *, int, double);
char *procprt_WCANCEL_a(struct tstat *, int, double);
char *procprt_WCANCEL_e(struct tstat *, int, double);
char *procprt_BANDWI_a(struct tstat *, int, double);
char *procprt_BANDWI_e(struct tstat *, int, double);
char *procprt_BANDWO_a(struct tstat *, int, double);
char *procprt_BANDWO_e(struct tstat *, int, double);
char *procprt_GPULIST_ae(struct tstat *, int, double);
char *procprt_GPUMEMNOW_ae(struct tstat *, int, double);
char *procprt_GPUMEMAVG_ae(struct tstat *, int, double);
char *procprt_GPUGPUBUSY_ae(struct tstat *, int, double);
char *procprt_GPUMEMBUSY_ae(struct tstat *, int, double);
char *procprt_WCHAN_a(struct tstat *, int, double);
char *procprt_WCHAN_e(struct tstat *, int, double);
char *procprt_RUNDELAY_a(struct tstat *, int, double);
char *procprt_RUNDELAY_e(struct tstat *, int, double);
char *procprt_BLKDELAY_a(struct tstat *, int, double);
char *procprt_BLKDELAY_e(struct tstat *, int, double);
char *procprt_NVCSW_a(struct tstat *, int, double);
char *procprt_NVCSW_e(struct tstat *, int, double);
char *procprt_NIVCSW_a(struct tstat *, int, double);
char *procprt_NIVCSW_e(struct tstat *, int, double);
char *procprt_CGROUP_PATH_a(struct tstat *, int, double);
char *procprt_CGROUP_PATH_e(struct tstat *, int, double);
char *procprt_CGRCPUWGT_a(struct tstat *, int, double);
char *procprt_CGRCPUWGT_e(struct tstat *, int, double);
char *procprt_CGRCPUMAX_a(struct tstat *, int, double);
char *procprt_CGRCPUMAX_e(struct tstat *, int, double);
char *procprt_CGRCPUMAXR_a(struct tstat *, int, double);
char *procprt_CGRCPUMAXR_e(struct tstat *, int, double);
char *procprt_CGRMEMMAX_a(struct tstat *, int, double);
char *procprt_CGRMEMMAX_e(struct tstat *, int, double);
char *procprt_CGRMEMMAXR_a(struct tstat *, int, double);
char *procprt_CGRMEMMAXR_e(struct tstat *, int, double);
char *procprt_CGRSWPMAX_a(struct tstat *, int, double);
char *procprt_CGRSWPMAX_e(struct tstat *, int, double);
char *procprt_CGRSWPMAXR_a(struct tstat *, int, double);
char *procprt_CGRSWPMAXR_e(struct tstat *, int, double);
char *procprt_SORTITEM_ae(struct tstat *, int, double);


static char     *columnhead[] = {
	[MSORTCPU]= "CPU", [MSORTMEM]= "MEM",
	[MSORTDSK]= "DSK", [MSORTNET]= "NET",
	[MSORTGPU]= "GPU",
};


/***************************************************************/
static int *colspacings;     // ugly static var, 
                             // but saves a lot of recomputations
                             // points to table with intercolumn 
                             // spacings
static proc_printpair newelems[MAXITEMS];
                             // ugly static var,
                             // but saves a lot of recomputations
                             // contains the actual list of items to
                             // be printed
                             //
                             //
/***************************************************************/
/*
 * gettotwidth: calculate the sum of widths and number of columns
 * Also copys the printpair elements to the static array newelems
 * for later removal of lower priority elements.
 * Params:
 * elemptr: the array of what to print
 * nitems: (ref) returns the number of printitems in the array
 * sumwidth: (ref) returns the total width of the printitems in the array
 * varwidth: (ref) returns the number of variable width items in the array
 */
static void
gettotwidth(proc_printpair* elemptr, int *nitems, int *sumwidth, int* varwidth) 
{
        int i;
        int col;
        int varw=0;

        for (i=0, col=0; elemptr[i].f!=0; ++i) 
        {
                col += (elemptr[i].f->varwidth ? 0 : elemptr[i].f->width);
                varw += elemptr[i].f->varwidth;
                newelems[i]=elemptr[i];    // copy element
        }
        newelems[i].f=0;
        *nitems=i;
        *sumwidth=col;
        *varwidth=varw;
}



/***************************************************************/
/*
 * getspacings: determine how much extra space there is for
 * inter-column space.
 * returns an int array this number of spaces to add after each column
 * also removes items from the newelems array if the available width
 * is lower than what is needed.  The lowest priority columns are
 * removed first.
 *
 * Note: this function is only to be called when screen is true.
 */
static int *
getspacings(proc_printpair* elemptr) 
{
        static int spacings[MAXITEMS];

        int col=0;
        int nitems;
        int varwidth=0;
        int j;
        int maxw=screen ? COLS : linelen;    // for non screen: 80 columns max

        // get width etc; copy elemptr array to static newelms
        gettotwidth(elemptr, &nitems, &col, &varwidth);

        /* cases:
         *   1) nitems==1: just one column, no spacing needed.  Done
         *
         *   2) total width is more than COLS: remove low prio columns
         *      2a)  a varwidth column: no spacing needed
         *      2b)  total width is less than COLS: compute inter spacing
         */

        if (nitems==1)          // no inter column spacing if 1 column
        {
                spacings[0]=0;
                return spacings;
        }

        // Check if available width is less than required.
        // If so, delete columns to make things fit
        // space required:
        // width + (nitems-1) * 1 space + 12 for a varwidth column.
        while (col + nitems-1+ 12*varwidth > maxw)  
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
                col -= newelems[lowestprio_index].f->width;
                varwidth -= newelems[lowestprio_index].f->varwidth;
                memmove(newelems+lowestprio_index, 
                        newelems+lowestprio_index+1, 
                        (nitems-lowestprio_index)* sizeof(proc_printpair));   
                       // also copies final 0 entry
                nitems--;
        }


        /* if there is a var width column, handle that separately */
        if (varwidth) 
        {
                for (j=0; j<nitems; ++j) 
                {
                        spacings[j]=1;
                        if (elemptr[j].f->varwidth)
                        {
                                elemptr[j].f->width=maxw-col-(nitems-1);
                                // only nitems-1 in-between spaces
                                // needed
                        }

                }
                return spacings;
        }

        // avoid division by 0
        if (nitems==1)
        {
                spacings[0]=0;
                return spacings;
        }

        /* fixed columns, spread whitespace over columns */
        double over=(0.0+maxw-col)/(nitems-1);
        double todo=over;

        for (j=0; j<nitems-1; ++j)   // last column gets no space appended
        {
                spacings[j]=(int)todo+0.5;
                todo-=spacings[j];
                todo+=over;
        }
        spacings[j]=0;
        return spacings;
}


/*
 * showhdrline: show header line for processes.
 * if in interactive mode, also add a page numer
 * if in interactive mode, columns are aligned to fill out rows
 */
void
showhdrline(proc_printpair* elemptr, int curlist, int totlist, 
                  char showorder, char autosort) 
{
        proc_printpair curelem;
	size_t buflen;
        char *chead="";
        char *autoindic="";
        int order=showorder;
        int col=0;
        int allign;
        char pagindic[10];
        int pagindiclen;
        int n=0;
        int bufsz;
        int maxw=screen ? COLS : linelen;    // for non screen: 80 columns max

        colspacings=getspacings(elemptr);
        bufsz=maxw+1;

        elemptr=newelems;     // point to adjusted array
        char buf[bufsz+2];    // long live dynamically sized auto arrays...
        
        if (!screen) 
        {
                printg("\n");
        }

        while ((curelem=*elemptr).f!=0) 
        {
		int widen = 0;

                if (curelem.f->head==0)     // empty header==special: SORTITEM
                {
                        chead     = columnhead[order];
                        autoindic = autosort ? "A" : " ";
			widen     = procprt_SORTITEM.width-3;
                } 
                else 
                {
                        chead=curelem.f->head;
                        autoindic="";
                }

		buflen = sizeof buf - col - 1;
                if (screen)
                {
                        col += pmsprintf(buf+col, buflen, "%*s%s%*s",
				widen, autoindic, chead, colspacings[n], "");
                }
                else
                {
                        col += pmsprintf(buf+col, buflen, "%s%s ", autoindic, chead);
                }
                              
                elemptr++;
                n++;
        }

        if (screen)   // add page number, eat from last header if needed...
        {
                pagindiclen = pmsprintf(pagindic, sizeof pagindic, "%d/%d", curlist, totlist);
                allign=COLS-col-pagindiclen;    // extra spaces needed
            
                if (allign >= 0)     // allign by adding spaces
                {
			buflen = sizeof buf - col - 1;
                        pmsprintf(buf+col, buflen, "%*s", allign+pagindiclen, pagindic);
                }
                else if (col+allign >= 0)
                {    // allign by removing from the right
			buflen = sizeof buf - col - allign - 1;
                        pmsprintf(buf+col+allign, buflen, "%s", pagindic);
                }
        }

        printg("%s", buf);

        if (!screen) 
        {
                printg("\n");
        }
}



/***************************************************************/
/*
 * showprocline: show line for processes.
 * if in interactive mode, columns are aligned to fill out rows
 * params:
 *     elemptr: pointer to array of print definition structs ptrs
 *     curstat: the process to print
 *     perc: the sort order used
 *     nsecs: number of seconds elapsed between previous and this sample
 *     avgval: is averaging out per second needed?
 */
void
showprocline(proc_printpair* elemptr, struct tstat *curstat, 
                            double perc, double nsecs, int avgval) 
{
        proc_printpair curelem;
        
        elemptr=newelems;      // point to static array
        int n=0;

	if (screen && threadview)
	{
		if (usecolors && !curstat->gen.isproc)
		{
			attron(COLOR_PAIR(FGCOLORTHR));
		}
		else
		{
			if (!usecolors && curstat->gen.isproc)
				attron(A_BOLD);
		}
	}

        while ((curelem=*elemptr).f!=0) 
        {
                // what to print?  SORTITEM, or active process or
                // exited process?

                if (curelem.f->head==0)                // empty string=sortitem
                {
                        printg("%*.0lf%%", procprt_SORTITEM.width-1, perc);
                }
                else if (curstat->gen.state != 'E')  // active process
                {
                        printg("%s", curelem.f->doactiveconvert(curstat, 
                                                        avgval, nsecs));
                }
                else                                 // exited process
                {
                        printg("%s", curelem.f->doexitconvert(curstat, 
                                                        avgval, nsecs));
                }

                if (screen)
                {
                        printg("%*s",colspacings[n], "");
                }
                else
                {
                        printg(" ");
                }

                elemptr++;
                n++;
        }

	if (screen && threadview)
	{
		if (usecolors && !curstat->gen.isproc)
		{
			attroff(COLOR_PAIR(FGCOLORTHR));
		}
		else
		{
			if (!usecolors && curstat->gen.isproc)
				attroff(A_BOLD);
		}
	}

        if (!screen) 
        {
                printg("\n");
        }
}


/*******************************************************************/
/* PROCESS PRINT FUNCTIONS */
/***************************************************************/
char *
procprt_NOTAVAIL_4(struct tstat *curstat, int avgval, double nsecs)
{
        return "   ?";
}

char *
procprt_NOTAVAIL_5(struct tstat *curstat, int avgval, double nsecs)
{
        return "    ?";
}

char *
procprt_NOTAVAIL_6(struct tstat *curstat, int avgval, double nsecs)
{
        return "     ?";
}

char *
procprt_NOTAVAIL_7(struct tstat *curstat, int avgval, double nsecs)
{
        return "      ?";
}
/***************************************************************/
char *
procprt_TID_ae(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[64];

	if (curstat->gen.isproc)
        	pmsprintf(buf, sizeof buf, "%*s", procprt_TID.width, "-");
	else
        	pmsprintf(buf, sizeof buf, "%*d", procprt_TID.width, curstat->gen.pid);
        return buf;
}

proc_printdef procprt_TID = 
   { "TID", "TID", procprt_TID_ae, procprt_TID_ae, 5}; //DYNAMIC WIDTH!
/***************************************************************/
char *
procprt_PID_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[64];

        pmsprintf(buf, sizeof buf, "%*d", procprt_PID.width, curstat->gen.tgid);
        return buf;
}

char *
procprt_PID_e(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[64];

        if (curstat->gen.pid == 0)
        	pmsprintf(buf, sizeof buf, "%*s", procprt_PID.width, "?");
	else
        	pmsprintf(buf, sizeof buf, "%*d", procprt_PID.width, curstat->gen.tgid);
        return buf;
}

proc_printdef procprt_PID = 
   { "PID", "PID", procprt_PID_a, procprt_PID_e, 5}; //DYNAMIC WIDTH!
/***************************************************************/
char *
procprt_PPID_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[64];

        pmsprintf(buf, sizeof buf, "%*d", procprt_PPID.width, curstat->gen.ppid);
        return buf;
}

char *
procprt_PPID_e(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[64];

	if (curstat->gen.ppid)
        	pmsprintf(buf, sizeof buf, "%*d", procprt_PPID.width, curstat->gen.ppid);
	else
		pmsprintf(buf, sizeof buf, "%*s", procprt_PPID.width, "-");
        return buf;
}

proc_printdef procprt_PPID = 
   { "PPID", "PPID", procprt_PPID_a, procprt_PPID_e, 5 }; //DYNAMIC WIDTH!
/***************************************************************/
char *
procprt_VPID_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[64];

        pmsprintf(buf, sizeof buf, "%*d", procprt_VPID.width, curstat->gen.vpid);
        return buf;
}

char *
procprt_VPID_e(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[64];

	pmsprintf(buf, sizeof buf, "%*s", procprt_VPID.width, "-");
        return buf;
}

proc_printdef procprt_VPID = 
   { "VPID", "VPID", procprt_VPID_a, procprt_VPID_e, 5 }; //DYNAMIC WIDTH!
/***************************************************************/
char *
procprt_CTID_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[32];

        pmsprintf(buf, sizeof buf, "%5d", curstat->gen.ctid);
        return buf;
}

char *
procprt_CTID_e(struct tstat *curstat, int avgval, double nsecs)
{
        return "    -";
}

proc_printdef procprt_CTID = 
   { " CTID", "CTID", procprt_CTID_a, procprt_CTID_e, 5 };
/***************************************************************/
char *
procprt_CID_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[64];

	if (curstat->gen.utsname[0])
        	pmsprintf(buf, sizeof buf, "%-15s", curstat->gen.utsname);
	else
        	pmsprintf(buf, sizeof buf, "%-15s", "host-----------");

        return buf;
}

char *
procprt_CID_e(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[64];

	if (curstat->gen.utsname[0])
        	pmsprintf(buf, sizeof buf, "%-15s", curstat->gen.utsname);
	else
        	pmsprintf(buf, sizeof buf, "%-15s", "?");

        return buf;
}

proc_printdef procprt_CID = 
   { "CID/POD     ", "CID", procprt_CID_a, procprt_CID_e, 15};
/***************************************************************/
char *
procprt_SYSCPU_ae(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[10];

        val2cpustr(curstat->cpu.stime, buf, sizeof buf);
        return buf;
}

proc_printdef procprt_SYSCPU = 
   { "SYSCPU", "SYSCPU", procprt_SYSCPU_ae, procprt_SYSCPU_ae, 6 };
/***************************************************************/
char *
procprt_USRCPU_ae(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[10];

        val2cpustr(curstat->cpu.utime, buf, sizeof buf);
        return buf;
}

proc_printdef procprt_USRCPU = 
   { "USRCPU", "USRCPU", procprt_USRCPU_ae, procprt_USRCPU_ae, 6 };
/***************************************************************/
char *
procprt_VGROW_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[10];

        val2memstr(curstat->mem.vgrow*1024, buf, sizeof buf, BFORMAT, 0, 0);
        return buf;
}

char *
procprt_VGROW_e(struct tstat *curstat, int avgval, double nsecs)
{
        return "    0K";
}

proc_printdef procprt_VGROW = 
   { " VGROW", "VGROW", procprt_VGROW_a, procprt_VGROW_e, 6 };
/***************************************************************/
char *
procprt_RGROW_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[10];

        val2memstr(curstat->mem.rgrow*1024, buf, sizeof buf, BFORMAT, 0, 0);
        return buf;
}

char *
procprt_RGROW_e(struct tstat *curstat, int avgval, double nsecs)
{
        return "    0K";
}

proc_printdef procprt_RGROW = 
   { " RGROW", "RGROW", procprt_RGROW_a, procprt_RGROW_e, 6 };
/***************************************************************/
char *
procprt_MINFLT_ae(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[10];

        val2valstr(curstat->mem.minflt, buf, sizeof buf, 6, avgval, nsecs);
        return buf;
}

proc_printdef procprt_MINFLT = 
   { "MINFLT", "MINFLT", procprt_MINFLT_ae, procprt_MINFLT_ae, 6 };
/***************************************************************/
char *
procprt_MAJFLT_ae(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[10];

        val2valstr(curstat->mem.majflt, buf, sizeof buf, 6, avgval, nsecs);
        return buf;
}

proc_printdef procprt_MAJFLT = 
   { "MAJFLT", "MAJFLT", procprt_MAJFLT_ae, procprt_MAJFLT_ae, 6 };
/***************************************************************/
char *
procprt_VSTEXT_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[10];

        val2memstr(curstat->mem.vexec*1024, buf, sizeof buf, BFORMAT, 0, 0);
        return buf;
}

char *
procprt_VSTEXT_e(struct tstat *curstat, int avgval, double nsecs)
{
        return "    0K";
}

proc_printdef procprt_VSTEXT = 
   { "VSTEXT", "VSTEXT", procprt_VSTEXT_a, procprt_VSTEXT_e, 6 };
/***************************************************************/
char *
procprt_VSIZE_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[10];

        val2memstr(curstat->mem.vmem*1024, buf, sizeof buf, BFORMAT, 0, 0);
        return buf;
}

char *
procprt_VSIZE_e(struct tstat *curstat, int avgval, double nsecs)
{
        return "    0K";
}

proc_printdef procprt_VSIZE = 
   { " VSIZE", "VSIZE", procprt_VSIZE_a, procprt_VSIZE_e, 6 };
/***************************************************************/
char *
procprt_RSIZE_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[10];

        val2memstr(curstat->mem.rmem*1024, buf, sizeof buf, BFORMAT, 0, 0);
        return buf;
}

char *
procprt_RSIZE_e(struct tstat *curstat, int avgval, double nsecs)
{
        return "    0K";
}

proc_printdef procprt_RSIZE = 
   { " RSIZE", "RSIZE", procprt_RSIZE_a, procprt_RSIZE_e, 6 };
/***************************************************************/
char *
procprt_PSIZE_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[10];

	if (curstat->mem.pmem == (unsigned long long)-1LL)	
        	return "    ?K";

       	val2memstr(curstat->mem.pmem*1024, buf, sizeof buf, BFORMAT, 0, 0);
        return buf;
}

char *
procprt_PSIZE_e(struct tstat *curstat, int avgval, double nsecs)
{
        return "    0K";
}

proc_printdef procprt_PSIZE = 
   { " PSIZE", "PSIZE", procprt_PSIZE_a, procprt_PSIZE_e, 6 };
/***************************************************************/
char *
procprt_VSLIBS_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[10];

        val2memstr(curstat->mem.vlibs*1024, buf, sizeof buf, BFORMAT, 0, 0);
        return buf;
}

char *
procprt_VSLIBS_e(struct tstat *curstat, int avgval, double nsecs)
{
        return "    0K";
}

proc_printdef procprt_VSLIBS = 
   { "VSLIBS", "VSLIBS", procprt_VSLIBS_a, procprt_VSLIBS_e, 6 };
/***************************************************************/
char *
procprt_VDATA_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[10];

        val2memstr(curstat->mem.vdata*1024, buf, sizeof buf, BFORMAT, 0, 0);
        return buf;
}

char *
procprt_VDATA_e(struct tstat *curstat, int avgval, double nsecs)
{
        return "    0K";
}

proc_printdef procprt_VDATA = 
   { " VDATA", "VDATA", procprt_VDATA_a, procprt_VDATA_e, 6 };
/***************************************************************/
char *
procprt_VSTACK_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[10];

        val2memstr(curstat->mem.vstack*1024, buf, sizeof buf, BFORMAT, 0, 0);
        return buf;
}

char *
procprt_VSTACK_e(struct tstat *curstat, int avgval, double nsecs)
{
        return "    0K";
}

proc_printdef procprt_VSTACK = 
   { "VSTACK", "VSTACK", procprt_VSTACK_a, procprt_VSTACK_e, 6 };
/***************************************************************/
char *
procprt_SWAPSZ_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[10];

        val2memstr(curstat->mem.vswap*1024, buf, sizeof buf, BFORMAT, 0, 0);
        return buf;
}

char *
procprt_SWAPSZ_e(struct tstat *curstat, int avgval, double nsecs)
{
        return "    0K";
}

proc_printdef procprt_SWAPSZ = 
   { "SWAPSZ", "SWAPSZ", procprt_SWAPSZ_a, procprt_SWAPSZ_e, 6 };
/***************************************************************/
char *
procprt_LOCKSZ_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[10];

        val2memstr(curstat->mem.vlock*1024, buf, sizeof buf, KBFORMAT, 0, 0);
        return buf;
}

char *
procprt_LOCKSZ_e(struct tstat *curstat, int avgval, double nsecs)
{
        return "    0K";
}

proc_printdef procprt_LOCKSZ = 
   { "LOCKSZ", "LOCKSZ", procprt_LOCKSZ_a, procprt_LOCKSZ_e, 6 };
/***************************************************************/
char *
procprt_CMD_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[16];

        pmsprintf(buf, sizeof buf, "%-14.14s", curstat->gen.name);
        return buf;
}

char *
procprt_CMD_e(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[16]="<";
        char        helpbuf[16];

        pmsprintf(helpbuf, sizeof helpbuf, "<%.12s>",  curstat->gen.name);
        pmsprintf(buf,     sizeof buf, "%-14.14s", helpbuf);
        return buf;
}

proc_printdef procprt_CMD = 
   { "CMD           ", "CMD", procprt_CMD_a, procprt_CMD_e, 14 };
/***************************************************************/
char *
procprt_RUID_ae(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[9];
        char *username;

        if ( (username = get_username(curstat->gen.ruid)) )
        {
                pmsprintf(buf, sizeof buf, "%-8.8s", username);
        }
        else
        {
                pmsprintf(buf, sizeof buf, "%-8d", curstat->gen.ruid);
        }
        return buf;
}

proc_printdef procprt_RUID = 
   { "RUID    ", "RUID", procprt_RUID_ae, procprt_RUID_ae, 8 };
/***************************************************************/
char *
procprt_EUID_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[9];
        char *username;

        if ( (username = get_username(curstat->gen.euid)) )
        {
                pmsprintf(buf, sizeof buf, "%-8.8s", username);
        }
        else
        {
                pmsprintf(buf, sizeof buf, "%-8d", curstat->gen.euid);
        }
        return buf;
}

char *
procprt_EUID_e(struct tstat *curstat, int avgval, double nsecs)
{
	return "-       ";
}

proc_printdef procprt_EUID = 
   { "EUID    ", "EUID", procprt_EUID_a, procprt_EUID_e, 8 };
/***************************************************************/
char *
procprt_SUID_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[9];
        char *username;

        if ( (username = get_username(curstat->gen.suid)) )
        {
                pmsprintf(buf, sizeof buf, "%-8.8s", username);
        } 
        else
        {
                pmsprintf(buf, sizeof buf, "%-8d", curstat->gen.suid);
        }
        return buf;
}

char *
procprt_SUID_e(struct tstat *curstat, int avgval, double nsecs)
{
	return "-       ";
}

proc_printdef procprt_SUID = 
   { "SUID    ", "SUID", procprt_SUID_a, procprt_SUID_e, 8 };
/***************************************************************/
char *
procprt_FSUID_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[9];
        char *username;

        if ( (username = get_username(curstat->gen.fsuid)) )
        {
                pmsprintf(buf, sizeof buf, "%-8.8s", username);
        }
        else
        {
                pmsprintf(buf, sizeof buf, "%-8d", curstat->gen.fsuid);
        }
        return buf;
}

char *
procprt_FSUID_e(struct tstat *curstat, int avgval, double nsecs)
{
	return "-       ";
}

proc_printdef procprt_FSUID = 
   { "FSUID   ", "FSUID", procprt_FSUID_a, procprt_FSUID_e, 8 };
/***************************************************************/
char *
procprt_RGID_ae(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[10];
        char *groupname;
        char grname[16];

        if ( (groupname = get_groupname(curstat->gen.rgid)) == NULL )
        {
                pmsprintf(grname, sizeof grname, "%d",curstat->gen.rgid);
                groupname = grname;
        }

        pmsprintf(buf, sizeof buf, "%-8.8s", groupname);
        return buf;
}

proc_printdef procprt_RGID = 
   { "RGID    ", "RGID", procprt_RGID_ae, procprt_RGID_ae, 8 };
/***************************************************************/
char *
procprt_EGID_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[10];
        char *groupname;
        char grname[16];

        if ( (groupname = get_groupname(curstat->gen.egid)) == NULL )
        {
                pmsprintf(grname, sizeof grname, "%d",curstat->gen.egid);
                groupname = grname;
        }

        pmsprintf(buf, sizeof buf, "%-8.8s", groupname);
        return buf;
}

char *
procprt_EGID_e(struct tstat *curstat, int avgval, double nsecs)
{
	return "-       ";
}

proc_printdef procprt_EGID = 
   { "EGID    ", "EGID", procprt_EGID_a, procprt_EGID_e, 8 };
/***************************************************************/
char *
procprt_SGID_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[10];
        char *groupname;
        char grname[16];

        if ( (groupname = get_groupname(curstat->gen.sgid)) == NULL )
        {
                pmsprintf(grname, sizeof grname, "%d",curstat->gen.sgid);
                groupname = grname;
        }

        pmsprintf(buf, sizeof buf, "%-8.8s", groupname);
        return buf;
}

char *
procprt_SGID_e(struct tstat *curstat, int avgval, double nsecs)
{
	return "-       ";
}

proc_printdef procprt_SGID = 
   { "SGID    ", "SGID", procprt_SGID_a, procprt_SGID_e, 8 };
/***************************************************************/
char *
procprt_FSGID_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[10];
        char *groupname;
        char grname[16];

        if ( (groupname = get_groupname(curstat->gen.fsgid)) == NULL )
        {
                pmsprintf(grname, sizeof grname,"%d",curstat->gen.fsgid);
                groupname = grname;
        }

        pmsprintf(buf, sizeof buf, "%-8.8s", groupname);
        return buf;
}

char *
procprt_FSGID_e(struct tstat *curstat, int avgval, double nsecs)
{
	return "-       ";
}

proc_printdef procprt_FSGID = 
   { "FSGID   ", "FSGID", procprt_FSGID_a, procprt_FSGID_e, 8 };
/***************************************************************/
char *
procprt_STDATE_ae(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[11];

        if (system_boottime)
                convdate(system_boottime + curstat->gen.btime / 1000, buf, sizeof buf);
        else
                strcpy(buf, "----/--/--");

        return buf;
}

proc_printdef procprt_STDATE = 
   { "  STDATE  ", "STDATE", procprt_STDATE_ae, procprt_STDATE_ae, 10 };
/***************************************************************/
char *
procprt_STTIME_ae(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[9];

        if (system_boottime)
                convtime(system_boottime + curstat->gen.btime / 1000, buf, sizeof buf);
        else
                strcpy(buf, "--:--:--");

        return buf;
}

proc_printdef procprt_STTIME = 
   { " STTIME ", "STTIME", procprt_STTIME_ae, procprt_STTIME_ae, 8 };
/***************************************************************/
char *
procprt_ENDATE_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[11];

	strcpy(buf, "  active  ");

        return buf;
}

char *
procprt_ENDATE_e(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[11];

        if (system_boottime)
                convdate(system_boottime + curstat->gen.btime / 1000 + curstat->gen.elaps, buf, sizeof buf);
        else
                strcpy(buf, "----/--/--");

        return buf;
}

proc_printdef procprt_ENDATE = 
   { "  ENDATE  ", "ENDATE", procprt_ENDATE_a, procprt_ENDATE_e, 10 };
/***************************************************************/
char *
procprt_ENTIME_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[9];

	strcpy(buf, " active ");

        return buf;
}

char *
procprt_ENTIME_e(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[9];

        if (system_boottime)
                convtime(system_boottime + curstat->gen.btime / 1000 + curstat->gen.elaps, buf, sizeof buf);
        else
                strcpy(buf, "--:--:--");

        return buf;
}

proc_printdef procprt_ENTIME = 
   { " ENTIME ", "ENTIME", procprt_ENTIME_a, procprt_ENTIME_e, 8 };
/***************************************************************/
char *
procprt_THR_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[15];

        pmsprintf(buf, sizeof buf, "%4d", curstat->gen.nthr);
        return buf;
}

char *
procprt_THR_e(struct tstat *curstat, int avgval, double nsecs)
{
        return "   0";
}

proc_printdef procprt_THR = 
   { " THR", "THR", procprt_THR_a, procprt_THR_e, 4 };
/***************************************************************/
char *
procprt_TRUN_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[15];

        pmsprintf(buf, sizeof buf, "%4d", curstat->gen.nthrrun);
        return buf;
}

char *
procprt_TRUN_e(struct tstat *curstat, int avgval, double nsecs)
{
        return "   0";
}

proc_printdef procprt_TRUN = 
   { "TRUN", "TRUN", procprt_TRUN_a, procprt_TRUN_e, 4 };
/***************************************************************/
char *
procprt_TSLPI_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[15];

        pmsprintf(buf, sizeof buf, "%5d", curstat->gen.nthrslpi);
        return buf;
}

char *
procprt_TSLPI_e(struct tstat *curstat, int avgval, double nsecs)
{
        return "    0";
}

proc_printdef procprt_TSLPI = 
   { "TSLPI", "TSLPI", procprt_TSLPI_a, procprt_TSLPI_e, 5 };
/***************************************************************/
char *
procprt_TSLPU_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[15];

        pmsprintf(buf, sizeof buf, "%5d", curstat->gen.nthrslpu);
        return buf;
}

char *
procprt_TSLPU_e(struct tstat *curstat, int avgval, double nsecs)
{
        return "    0";
}

proc_printdef procprt_TSLPU = 
   { "TSLPU", "TSLPU", procprt_TSLPU_a, procprt_TSLPU_e, 5 };
/***************************************************************/
char *
procprt_TIDLE_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[15];

        sprintf(buf, "%5d", curstat->gen.nthridle);
        return buf;
}

char *
procprt_TIDLE_e(struct tstat *curstat, int avgval, double nsecs)
{
        return "    0";
}

proc_printdef procprt_TIDLE = 
   { "TIDLE", "TIDLE", procprt_TIDLE_a, procprt_TIDLE_e, 5 };
/***************************************************************/
#define SCHED_NORMAL	0
#define SCHED_FIFO	1
#define SCHED_RR	2
#define SCHED_BATCH	3
#define SCHED_ISO	4
#define SCHED_IDLE	5
#define SCHED_DEADLINE	6

char *
procprt_POLI_a(struct tstat *curstat, int avgval, double nsecs)
{
        switch (curstat->cpu.policy)
        {
                case SCHED_NORMAL:
                        return "norm";
                case SCHED_FIFO:
                        return "fifo";
                case SCHED_RR:
                        return "rr  ";
                case SCHED_BATCH:
                        return "btch";
                case SCHED_ISO:
                        return "iso ";
                case SCHED_IDLE:
                        return "idle";
                case SCHED_DEADLINE:
                        return "dead";
        }
        return "?   ";
}

char *
procprt_POLI_e(struct tstat *curstat, int avgval, double nsecs)
{
        return "-   ";
}

proc_printdef procprt_POLI = 
   { "POLI", "POLI", procprt_POLI_a, procprt_POLI_e, 4 };
/***************************************************************/
char *
procprt_NICE_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[15];

        pmsprintf(buf, sizeof buf, "%4d", curstat->cpu.nice);
        return buf;
}

char *
procprt_NICE_e(struct tstat *curstat, int avgval, double nsecs)
{
        return "   -";
}

proc_printdef procprt_NICE = 
   { "NICE", "NICE", procprt_NICE_a, procprt_NICE_e, 4 };
/***************************************************************/
char *
procprt_PRI_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[15];

        pmsprintf(buf, sizeof buf, "%3d", curstat->cpu.prio);
        return buf;
}

char *
procprt_PRI_e(struct tstat *curstat, int avgval, double nsecs)
{
        return "  -";
}

proc_printdef procprt_PRI = 
   { "PRI", "PRI", procprt_PRI_a, procprt_PRI_e, 3 };
/***************************************************************/
char *
procprt_RTPR_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[15];

        pmsprintf(buf, sizeof buf, "%4d", curstat->cpu.rtprio);
        return buf;
}

char *
procprt_RTPR_e(struct tstat *curstat, int avgval, double nsecs)
{
        return "   -";
}

proc_printdef procprt_RTPR = 
   { "RTPR", "RTPR", procprt_RTPR_a, procprt_RTPR_e, 4 };
/***************************************************************/
char *
procprt_CURCPU_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[15];

        pmsprintf(buf, sizeof buf, "%5d", curstat->cpu.curcpu);
        return buf;
}

char *
procprt_CURCPU_e(struct tstat *curstat, int avgval, double nsecs)
{
        return "    -";
}

proc_printdef procprt_CURCPU = 
   { "CPUNR", "CPUNR", procprt_CURCPU_a, procprt_CURCPU_e, 5 };
/***************************************************************/
char *
procprt_ST_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[3]="--";
        if (curstat->gen.excode & ~(INT_MAX))
        {
                buf[0]='N';
        } 
        else
        { 
                buf[0]='-';
        }
        return buf;
}

char *
procprt_ST_e(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[3];
        if (curstat->gen.excode & ~(INT_MAX))
        {
                buf[0]='N';
        } 
        else 
        { 
                buf[0]='-';
        }
        if (curstat->gen.excode & 0xff) 
        {
                if (curstat->gen.excode & 0x80)
                        buf[1] = 'C';
                else
                        buf[1] = 'S';
        } 
        else 
        {
                buf[1] = 'E';
        }
        return buf;
}

proc_printdef procprt_ST = 
   { "ST", "ST", procprt_ST_a, procprt_ST_e, 2 };
/***************************************************************/
char *
procprt_EXC_a(struct tstat *curstat, int avgval, double nsecs)
{
        return "  -";
}

char *
procprt_EXC_e(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[4];

        pmsprintf(buf, sizeof buf, "%3d", 
                 curstat->gen.excode & 0xff ?
                          curstat->gen.excode & 0x7f : 
                          (curstat->gen.excode>>8) & 0xff);
        return buf;
}


proc_printdef procprt_EXC = 
   { "EXC", "EXC", procprt_EXC_a, procprt_EXC_e, 3 };
/***************************************************************/
char *
procprt_S_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[2];

	if (!curstat->gen.state)
                buf[0]='-';
	else
        	buf[0]=curstat->gen.state;
        return buf;
}

char *
procprt_S_e(struct tstat *curstat, int avgval, double nsecs)
{
        return "E";
}

proc_printdef procprt_S = 
   { "S", "S", procprt_S_a, procprt_S_e, 1 };

/***************************************************************/
char *
procprt_COMMAND_LINE_ae(struct tstat *curstat, int avgval, double nsecs)
{
        extern proc_printdef procprt_COMMAND_LINE;
        extern int	startoffset;	// influenced by -> and <- keys

        static char	buf[CMDLEN+1];

	char	*pline     = curstat->gen.cmdline[0] ?
		                curstat->gen.cmdline : curstat->gen.name;

        int 	curwidth   = procprt_COMMAND_LINE.width <= CMDLEN ?
				procprt_COMMAND_LINE.width : CMDLEN;

        int 	cmdlen     = strlen(pline);
        int 	curoffset  = startoffset <= cmdlen ? startoffset : cmdlen;

        if (screen) 
                pmsprintf(buf, sizeof buf, "%-*.*s", curwidth, curwidth, pline+curoffset);
        else
                pmsprintf(buf, sizeof buf, "%.*s", CMDLEN, pline+curoffset);

        return buf;
}

proc_printdef procprt_COMMAND_LINE = 
       { "COMMAND-LINE (horizontal scroll with <- and -> keys)",
	"COMMAND-LINE", 
        procprt_COMMAND_LINE_ae, procprt_COMMAND_LINE_ae, 0, 1 };
/***************************************************************/
char *
procprt_NPROCS_ae(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[10];

        val2valstr(curstat->gen.pid, buf, sizeof buf, 6, 0, 0); // pid abused as proc counter
        return buf;
}

proc_printdef procprt_NPROCS = 
   { "NPROCS", "NPROCS", procprt_NPROCS_ae, procprt_NPROCS_ae, 6 };
/***************************************************************/
char *
procprt_RDDSK_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[10];
        val2memstr(curstat->dsk.rsz*512, buf, sizeof buf, BFORMAT, avgval, nsecs);

        return buf;
}

char *
procprt_RDDSK_e(struct tstat *curstat, int avgval, double nsecs)
{
        return "     -";
}

proc_printdef procprt_RDDSK = 
   { " RDDSK", "RDDSK", procprt_RDDSK_a, procprt_RDDSK_e, 6 };
/***************************************************************/
char *
procprt_WRDSK_a(struct tstat *curstat, int avgval, double nsecs) 
{
        static char buf[10];

        val2memstr(curstat->dsk.wsz*512, buf, sizeof buf, BFORMAT, avgval, nsecs);

        return buf;
}

char *
procprt_WRDSK_e(struct tstat *curstat, int avgval, double nsecs)
{
        return "     -";
}

proc_printdef procprt_WRDSK = 
   { " WRDSK", "WRDSK", procprt_WRDSK_a, procprt_WRDSK_e, 6 };
/***************************************************************/
char *
procprt_CWRDSK_a(struct tstat *curstat, int avgval, double nsecs) 
{
	count_t nett_wsz;
        static char buf[10];

	if (curstat->dsk.wsz > curstat->dsk.cwsz)
		nett_wsz = curstat->dsk.wsz - curstat->dsk.cwsz;
	else
		nett_wsz = 0;

        val2memstr(nett_wsz*512, buf, sizeof buf, BFORMAT, avgval, nsecs);

        return buf;
}

proc_printdef procprt_CWRDSK = 
   {" WRDSK", "CWRDSK", procprt_CWRDSK_a, procprt_WRDSK_e, 6 };
/***************************************************************/
char *
procprt_WCANCEL_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[10];
        val2memstr(curstat->dsk.cwsz*512, buf, sizeof buf, KBFORMAT, avgval, nsecs);

        return buf;
}

char *
procprt_WCANCEL_e(struct tstat *curstat, int avgval, double nsecs)
{
        return "     -";
}

proc_printdef procprt_WCANCEL = 
   {"WCANCL", "WCANCL", procprt_WCANCEL_a, procprt_WCANCEL_e, 6};
/***************************************************************/
char *
procprt_TCPRCV_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[10];
        
        val2valstr(curstat->net.tcprcv, buf, sizeof buf, 6, avgval, nsecs);

        return buf;
}

char *
procprt_TCPRCV_e(struct tstat *curstat, int avgval, double nsecs) 
{      
	if (supportflags & NETATOPD || supportflags & NETATOPBPF )
	{
        	static char buf[10];
        
        	val2valstr(curstat->net.tcprcv, buf, sizeof buf, 6, avgval, nsecs);

        	return buf;
	}
	else
        	return "     -";
}


proc_printdef procprt_TCPRCV = 
   { "TCPRCV", "TCPRCV", procprt_TCPRCV_a, procprt_TCPRCV_e, 6 };
/***************************************************************/
char *
procprt_TCPRASZ_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[10];
        
        int avgtcpr = curstat->net.tcprcv ?
                                  curstat->net.tcprsz / curstat->net.tcprcv : 0;

        val2valstr(avgtcpr, buf, sizeof buf, 7, 0, 0);
        return buf;
}

char *
procprt_TCPRASZ_e(struct tstat *curstat, int avgval, double nsecs)
{
	if (supportflags & NETATOPD || supportflags & NETATOPBPF )
	{
        	static char buf[10];
        
        	int avgtcpr = curstat->net.tcprcv ?
                                  curstat->net.tcprsz / curstat->net.tcprcv : 0;

        	val2valstr(avgtcpr, buf, sizeof buf, 7, 0, 0);
	        return buf;
	}
	else
        	return "      -";
}

proc_printdef procprt_TCPRASZ = 
   { "TCPRASZ", "TCPRASZ", procprt_TCPRASZ_a, procprt_TCPRASZ_e, 7 };
/***************************************************************/
char *
procprt_TCPSND_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[10];
        
        val2valstr(curstat->net.tcpsnd, buf, sizeof buf, 6, avgval, nsecs);

        return buf;
}

char *
procprt_TCPSND_e(struct tstat *curstat, int avgval, double nsecs)
{
	if (supportflags & NETATOPD || supportflags & NETATOPBPF )
	{
        	static char buf[10];
        
        	val2valstr(curstat->net.tcpsnd, buf, sizeof buf, 6, avgval, nsecs);

        	return buf;
	}
	else
        	return "     -";
}

proc_printdef procprt_TCPSND = 
   { "TCPSND", "TCPSND", procprt_TCPSND_a, procprt_TCPSND_e, 6 };
/***************************************************************/
char *
procprt_TCPSASZ_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[10];
        
        int avgtcps = curstat->net.tcpsnd ?
                                  curstat->net.tcpssz / curstat->net.tcpsnd : 0;

        val2valstr(avgtcps, buf, sizeof buf, 7, 0, 0);
        return buf;
}

char *
procprt_TCPSASZ_e(struct tstat *curstat, int avgval, double nsecs)
{
	if (supportflags & NETATOPD || supportflags & NETATOPBPF )
	{
        	static char buf[10];
        
        	int avgtcps = curstat->net.tcpsnd ?
                                  curstat->net.tcpssz / curstat->net.tcpsnd : 0;

        	val2valstr(avgtcps, buf, sizeof buf, 7, 0, 0);
        	return buf;
	}
	else
        	return "      -";
}

proc_printdef procprt_TCPSASZ = 
   { "TCPSASZ", "TCPSASZ", procprt_TCPSASZ_a, procprt_TCPSASZ_e, 7 };
/***************************************************************/
char *
procprt_UDPRCV_a(struct tstat *curstat, int avgval, double nsecs)        
{
        static char buf[10];
        
        val2valstr(curstat->net.udprcv, buf, sizeof buf, 6, avgval, nsecs);

        return buf;
}

char *
procprt_UDPRCV_e(struct tstat *curstat, int avgval, double nsecs) 
{
	if (supportflags & NETATOPD || supportflags & NETATOPBPF )
	{
        	static char buf[10];
        
        	val2valstr(curstat->net.udprcv, buf, sizeof buf, 6, avgval, nsecs);

        	return buf;
	}
	else
        	return "     -";
}


proc_printdef procprt_UDPRCV = 
   { "UDPRCV", "UDPRCV", procprt_UDPRCV_a, procprt_UDPRCV_e, 6 };
/***************************************************************/
char *
procprt_UDPRASZ_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[10];
        
        int avgudpr = curstat->net.udprcv ?
                          curstat->net.udprsz / curstat->net.udprcv : 0;

        val2valstr(avgudpr, buf, sizeof buf, 7, 0, 0);
        return buf;
}

char *
procprt_UDPRASZ_e(struct tstat *curstat, int avgval, double nsecs)
{
	if (supportflags & NETATOPD || supportflags & NETATOPBPF )
	{
        	static char buf[10];
        
        	int avgudpr = curstat->net.udprcv ?
                          curstat->net.udprsz / curstat->net.udprcv : 0;

        	val2valstr(avgudpr, buf, sizeof buf, 7, 0, 0);
        	return buf;
	}
	else
        	return "      -";
}


proc_printdef procprt_UDPRASZ = 
   { "UDPRASZ", "UDPRASZ", procprt_UDPRASZ_a, procprt_UDPRASZ_e, 7 };
/***************************************************************/
char *
procprt_UDPSND_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[10];
        
        val2valstr(curstat->net.udpsnd, buf, sizeof buf, 6, avgval, nsecs);

        return buf;
}

char *
procprt_UDPSND_e(struct tstat *curstat, int avgval, double nsecs)
{
	if (supportflags & NETATOPD || supportflags & NETATOPBPF )
	{
        	static char buf[10];
        
        	val2valstr(curstat->net.udpsnd, buf, sizeof buf, 6, avgval, nsecs);

        	return buf;
	}
	else
        	return "     -";
}

proc_printdef procprt_UDPSND = 
   { "UDPSND", "UDPSND", procprt_UDPSND_a, procprt_UDPSND_e, 6 };
/***************************************************************/
char *
procprt_UDPSASZ_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[10];
        
        int avgudps = curstat->net.udpsnd ?
                                  curstat->net.udpssz / curstat->net.udpsnd : 0;

        val2valstr(avgudps, buf, sizeof buf, 7, 0, 0);
        return buf;
}

char *
procprt_UDPSASZ_e(struct tstat *curstat, int avgval, double nsecs)
{
	if (supportflags & NETATOPD || supportflags & NETATOPBPF )
	{
        	static char buf[10];
        
        	int avgudps = curstat->net.udpsnd ?
                                  curstat->net.udpssz / curstat->net.udpsnd : 0;

        	val2valstr(avgudps, buf, sizeof buf, 7, 0, 0);
        	return buf;
	}
	else
        	return "      -";
}


proc_printdef procprt_UDPSASZ = 
   { "UDPSASZ", "UDPSASZ", procprt_UDPSASZ_a, procprt_UDPSASZ_e, 7 };
/***************************************************************/
char *
procprt_RNET_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[10];
        
        val2valstr(curstat->net.tcprcv + curstat->net.udprcv ,
					buf, sizeof buf, 5, avgval, nsecs);

        return buf;
}

char *
procprt_RNET_e(struct tstat *curstat, int avgval, double nsecs)
{
	if (supportflags & NETATOPD || supportflags & NETATOPBPF )
	{
        	static char buf[10];
 
	        val2valstr(curstat->net.tcprcv + curstat->net.udprcv ,
					buf, sizeof buf, 5, avgval, nsecs);

       		return buf;
	}
	else
        	return "    -";
}

proc_printdef procprt_RNET = 
   { " RNET", "RNET", procprt_RNET_a, procprt_RNET_e, 5 };
/***************************************************************/
char *
procprt_SNET_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[10];
        
        val2valstr(curstat->net.tcpsnd + curstat->net.udpsnd,
                           		buf, sizeof buf, 5, avgval, nsecs);
        return buf;
}

char *
procprt_SNET_e(struct tstat *curstat, int avgval, double nsecs)
{
	if (supportflags & NETATOPD || supportflags & NETATOPBPF )
	{
	        static char buf[10];
        
       		val2valstr(curstat->net.tcpsnd + curstat->net.udpsnd,
                           		buf, sizeof buf, 5, avgval, nsecs);
	        return buf;
	}
	else
        	return "    -";
}

proc_printdef procprt_SNET = 
   { " SNET", "SNET", procprt_SNET_a, procprt_SNET_e, 5 };
/***************************************************************/
char *
procprt_BANDWI_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[16];
	count_t     rkbps = (curstat->net.tcprsz+curstat->net.udprsz)/125/nsecs;

	format_bandw(buf, sizeof buf, rkbps);
        return buf;
}

char *
procprt_BANDWI_e(struct tstat *curstat, int avgval, double nsecs)
{
	if (supportflags & NETATOPD || supportflags & NETATOPBPF )
	{
        	static char buf[16];
		count_t     rkbps = (curstat->net.tcprsz + curstat->net.udprsz)
								/125/nsecs;

		format_bandw(buf, sizeof buf, rkbps);
        	return buf;
	}
	else
        	return "        -";
}

proc_printdef procprt_BANDWI = 
   { "   BANDWI", "BANDWI", procprt_BANDWI_a, procprt_BANDWI_e, 9};
/***************************************************************/
char *
procprt_BANDWO_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[16];
	count_t     skbps = (curstat->net.tcpssz+curstat->net.udpssz)/125/nsecs;

	format_bandw(buf, sizeof buf, skbps);
        return buf;
}

char *
procprt_BANDWO_e(struct tstat *curstat, int avgval, double nsecs)
{
	if (supportflags & NETATOPD || supportflags & NETATOPBPF )
	{
        	static char buf[16];
		count_t     skbps = (curstat->net.tcpssz + curstat->net.udpssz)
								/125/nsecs;

		format_bandw(buf, sizeof buf, skbps);
       		return buf;
	}
	else
        	return "        -";
}

proc_printdef procprt_BANDWO = 
   { "   BANDWO", "BANDWO", procprt_BANDWO_a, procprt_BANDWO_e, 9};
/***************************************************************/
static void
format_bandw(char *buf, size_t buflen, count_t kbps)
{
	char        c;

	if (kbps < 10000)
	{
                c='K';
        }
        else if (kbps < (count_t)10000 * 1000)
        {
                kbps/=1000;
                c = 'M';
        }
        else if (kbps < (count_t)10000 * 1000 * 1000)
        {
                kbps/=1000 * 1000;
                c = 'G';
        }
        else
        {
                kbps = kbps / 1000 / 1000 / 1000;
                c = 'T';
        }

        pmsprintf(buf, buflen-1, "%4lld %cbps", kbps, c);
}
/***************************************************************/
char *
procprt_GPULIST_ae(struct tstat *curstat, int avgval, double nsecs)
{
        static char	buf[64];
        char		tmp[64], *p=tmp;
	int		i;

	if (!curstat->gpu.state)
		return "       -";

	if (!curstat->gpu.gpulist)
		return "       -";

	memset(tmp, 0, sizeof tmp);
	for (i=0; i < hinv_nrgpus; i++)
	{
		if (curstat->gpu.gpulist & 1<<i)
		{
			if (tmp == p)	// first?
				p += pmsprintf(p, sizeof tmp, "%d", i);
			else
				p += pmsprintf(p, sizeof tmp - (p-tmp), ",%d", i);

			if (p - tmp > 8)
			{
				pmsprintf(tmp, sizeof tmp, "0x%06x",
						curstat->gpu.gpulist);
				break;
			}
		}
	}

	pmsprintf(buf, sizeof buf, "%8.8s", tmp);
        return buf;
}

proc_printdef procprt_GPULIST = 
   { " GPUNUMS", "GPULIST", procprt_GPULIST_ae, procprt_GPULIST_ae, 8};
/***************************************************************/
char *
procprt_GPUMEMNOW_ae(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[10];

	if (!curstat->gpu.state)
		return "     -";

        val2memstr(curstat->gpu.memnow*1024, buf, sizeof buf, BFORMAT, 0, 0);
        return buf;
}

proc_printdef procprt_GPUMEMNOW = 
   { "MEMNOW", "GPUMEM", procprt_GPUMEMNOW_ae, procprt_GPUMEMNOW_ae, 6};
/***************************************************************/
char *
procprt_GPUMEMAVG_ae(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[10];

	if (!curstat->gpu.state)
		return "     -";

	if (curstat->gpu.sample == 0)
		return("    0K");

       	val2memstr(curstat->gpu.nrgpus * curstat->gpu.memcum /
	           curstat->gpu.sample*1024, buf, sizeof buf, BFORMAT, 0, 0);
       	return buf;
}

proc_printdef procprt_GPUMEMAVG = 
   { "MEMAVG", "GPUMEMAVG", procprt_GPUMEMAVG_ae, procprt_GPUMEMAVG_ae, 6};
/***************************************************************/
char *
procprt_GPUGPUBUSY_ae(struct tstat *curstat, int avgval, double nsecs)
{
        static char 	buf[16];

	if (!curstat->gpu.state)
		return "      -";

	if (curstat->gpu.gpubusy == -1)
		return "    N/A";

       	pmsprintf(buf, sizeof buf, "%6d%%", curstat->gpu.gpubusy);
       	return buf;
}

proc_printdef procprt_GPUGPUBUSY = 
   { "GPUBUSY", "GPUGPUBUSY", procprt_GPUGPUBUSY_ae, procprt_GPUGPUBUSY_ae, 7};
/***************************************************************/
char *
procprt_GPUMEMBUSY_ae(struct tstat *curstat, int avgval, double nsecs)
{
        static char 	buf[16];

	if (!curstat->gpu.state)
		return "      -";

	if (curstat->gpu.membusy == -1)
		return "    N/A";

        pmsprintf(buf, sizeof buf, "%6d%%", curstat->gpu.membusy);
        return buf;
}

proc_printdef procprt_GPUMEMBUSY = 
   { "MEMBUSY", "GPUMEMBUSY", procprt_GPUMEMBUSY_ae, procprt_GPUMEMBUSY_ae, 7};
/***************************************************************/
char *
procprt_WCHAN_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[32];

        if (curstat->gen.state != 'R')
		pmsprintf(buf, sizeof buf, "%-15.15s", curstat->cpu.wchan);
	else
		pmsprintf(buf, sizeof buf, "%-15.15s", " ");

        return buf;
}

char *
procprt_WCHAN_e(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[32];

	pmsprintf(buf, sizeof buf, "%-15.15s", " ");
        return buf;
}

proc_printdef procprt_WCHAN =
   { "WCHAN          ", "WCHAN", procprt_WCHAN_a, procprt_WCHAN_e, 15};
/***************************************************************/
char *
procprt_RUNDELAY_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[10];

        val2cpustr(curstat->cpu.rundelay/1000000, buf, sizeof buf);
        return buf;
}

char *
procprt_RUNDELAY_e(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[10];

        pmsprintf(buf, sizeof buf, "     -");
        return buf;
}

proc_printdef procprt_RUNDELAY =
   { "RDELAY", "RDELAY", procprt_RUNDELAY_a, procprt_RUNDELAY_e, 6};
/***************************************************************/
char *
procprt_BLKDELAY_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[10];

        val2cpustr(curstat->cpu.blkdelay*1000, buf, sizeof buf);
        return buf;
}

char *
procprt_BLKDELAY_e(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[10];

        pmsprintf(buf, sizeof buf, "     -");
        return buf;
}

proc_printdef procprt_BLKDELAY =
   { "BDELAY", "BDELAY", procprt_BLKDELAY_a, procprt_BLKDELAY_e, 6};
/***************************************************************/
char *
procprt_NVCSW_a(struct tstat *curstat, int avgval, double nsecs)
{
	static char buf[64];

        val2valstr(curstat->cpu.nvcsw, buf, sizeof buf, 6, avgval, nsecs);
	return buf;
}

char *
procprt_NVCSW_e(struct tstat *curstat, int avgval, double nsecs)
{
	return "     -";
}

proc_printdef procprt_NVCSW =
   { " NVCSW", "NVCSW", procprt_NVCSW_a, procprt_NVCSW_e, 6 };
/***************************************************************/
char *
procprt_NIVCSW_a(struct tstat *curstat, int avgval, double nsecs)
{
	static char buf[64];

        val2valstr(curstat->cpu.nivcsw, buf, sizeof buf, 6, avgval, nsecs);
	return buf;
}

char *
procprt_NIVCSW_e(struct tstat *curstat, int avgval, double nsecs)
{
	return "     -";
}

proc_printdef procprt_NIVCSW =
   { "NIVCSW", "NIVCSW", procprt_NIVCSW_a, procprt_NIVCSW_e, 6 };
/***************************************************************/
char *
procprt_CGROUP_PATH_a(struct tstat *curstat, int avgval, double nsecs)
{
        extern proc_printdef procprt_CGROUP_PATH;
        extern int	startoffset;	// influenced by -> and <- keys
        static char	buf[CMDLEN+1];

	char	*pline     = curstat->gen.cgpath[0] ? curstat->gen.cgpath:"?";

        int 	curwidth   = procprt_CGROUP_PATH.width <= CGRLEN ?
				procprt_CGROUP_PATH.width : CGRLEN;

        int 	pathlen    = strlen(pline);
        int 	curoffset  = startoffset <= pathlen ? startoffset : pathlen;

	if (! curstat->gen.isproc)
		return "";

        if (screen) 
                pmsprintf(buf, sizeof buf, "%-*.*s", curwidth, curwidth, pline+curoffset);
        else
                pmsprintf(buf, sizeof buf, "%.*s", CGRLEN, pline+curoffset);

        return buf;
}

char *
procprt_CGROUP_PATH_e(struct tstat *curstat, int avgval, double nsecs)
{
	return "-";
}

proc_printdef procprt_CGROUP_PATH = 
       {"CGROUP (horizontal scroll: <- and ->)",
	"CGROUP-PATH", 
        procprt_CGROUP_PATH_a, procprt_CGROUP_PATH_e, 42, 0 };
/***************************************************************/
char *
procprt_CGRCPUWGT_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[16];

	if (! curstat->gen.isproc)
		return "      ";

	if (curstat->gen.cgpath[0])
	{
		switch (curstat->cpu.cgcpuweight)
		{
		   case -2:
        		return "     -";
		   default:
			pmsprintf(buf, sizeof buf, "%6d", curstat->cpu.cgcpuweight);
        		return buf;
		}
	}
	else
        	return "     -";
}

char *
procprt_CGRCPUWGT_e(struct tstat *curstat, int avgval, double nsecs)
{
        return "     -";
}

proc_printdef procprt_CGRCPUWGT =
   { "CPUWGT", "CPUWGT", procprt_CGRCPUWGT_a, procprt_CGRCPUWGT_e, 6};
/***************************************************************/
char *
procprt_CGRCPUMAX_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[16];

	if (! curstat->gen.isproc)
		return "      ";

	if (curstat->gen.cgpath[0])
	{
		switch (curstat->cpu.cgcpumax)
		{
		   case -1:
        		return "   max";
		   case -2:
        		return "     -";
		   default:
			pmsprintf(buf, sizeof buf, "%5d%%", curstat->cpu.cgcpumax);
        		return buf;
		}
	}
	else
        	return "     -";
}

char *
procprt_CGRCPUMAX_e(struct tstat *curstat, int avgval, double nsecs)
{
        return "     -";
}

proc_printdef procprt_CGRCPUMAX =
   { "CPUMAX", "CPUMAX", procprt_CGRCPUMAX_a, procprt_CGRCPUMAX_e, 6};
/***************************************************************/
char *
procprt_CGRCPUMAXR_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[16];

	if (! curstat->gen.isproc)
		return "       ";

	if (curstat->gen.cgpath[0])
	{
		switch (curstat->cpu.cgcpumaxr)
		{
		   case -1:
        		return "    max";
		   case -2:
        		return "      -";
		   default:
			pmsprintf(buf, sizeof buf, "%6d%%", curstat->cpu.cgcpumaxr);
        		return buf;
		}
	}
	else
        	return "      -";
}

char *
procprt_CGRCPUMAXR_e(struct tstat *curstat, int avgval, double nsecs)
{
        return "      -";
}

proc_printdef procprt_CGRCPUMAXR =
   { "CPUMAXR", "CPUMAXR", procprt_CGRCPUMAXR_a, procprt_CGRCPUMAXR_e, 7};
/***************************************************************/
char *
procprt_CGRMEMMAX_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[16];

	if (! curstat->gen.isproc)
		return "      ";

	if (curstat->gen.cgpath[0])
	{
		switch (curstat->mem.cgmemmax)
		{
		   case -1:
        		return "   max";
		   case -2:
        		return "     -";
		   default:
        		val2memstr(curstat->mem.cgmemmax*1024, buf, sizeof buf, BFORMAT, 0, 0);
       		 	return buf;
		}
	}
	else
        	return "     -";
}

char *
procprt_CGRMEMMAX_e(struct tstat *curstat, int avgval, double nsecs)
{
        return "     -";
}

proc_printdef procprt_CGRMEMMAX =
   { "MEMMAX", "MEMMAX", procprt_CGRMEMMAX_a, procprt_CGRMEMMAX_e, 6};
/***************************************************************/
char *
procprt_CGRMEMMAXR_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[16];

	if (! curstat->gen.isproc)
		return "      ";

	if (curstat->gen.cgpath[0])
	{
		switch (curstat->mem.cgmemmaxr)
		{
		   case -1:
        		return "   max";
		   case -2:
        		return "     -";
		   default:
        		val2memstr(curstat->mem.cgmemmaxr*1024, buf, sizeof buf, BFORMAT, 0, 0);
       		 	return buf;
		}
	}
	else
        	return "     -";
}

char *
procprt_CGRMEMMAXR_e(struct tstat *curstat, int avgval, double nsecs)
{
        return "     -";
}

proc_printdef procprt_CGRMEMMAXR =
   { "MMMAXR", "MMMAXR", procprt_CGRMEMMAXR_a, procprt_CGRMEMMAXR_e, 6};
/***************************************************************/
char *
procprt_CGRSWPMAX_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[16];

	if (! curstat->gen.isproc)
		return "      ";

	if (curstat->gen.cgpath[0])
	{
		switch (curstat->mem.cgswpmax)
		{
		   case -1:
        		return "   max";
		   case -2:
        		return "     -";
		   default:
        		val2memstr(curstat->mem.cgswpmax*1024, buf, sizeof buf, BFORMAT, 0, 0);
        		return buf;
		}
	}
	else
        	return "     -";
}

char *
procprt_CGRSWPMAX_e(struct tstat *curstat, int avgval, double nsecs)
{
        return "     -";
}

proc_printdef procprt_CGRSWPMAX =
   { "SWPMAX", "SWPMAX", procprt_CGRSWPMAX_a, procprt_CGRSWPMAX_e, 6};
/***************************************************************/
char *
procprt_CGRSWPMAXR_a(struct tstat *curstat, int avgval, double nsecs)
{
        static char buf[16];

	if (! curstat->gen.isproc)
		return "      ";

	if (curstat->gen.cgpath[0])
	{
		switch (curstat->mem.cgswpmaxr)
		{
		   case -1:
        		return "   max";
		   case -2:
        		return "     -";
		   default:
        		val2memstr(curstat->mem.cgswpmaxr*1024, buf, sizeof buf, BFORMAT, 0, 0);
        		return buf;
		}
	}
	else
        	return "     -";
}

char *
procprt_CGRSWPMAXR_e(struct tstat *curstat, int avgval, double nsecs)
{
        return "     -";
}

proc_printdef procprt_CGRSWPMAXR =
   { "SWMAXR", "SWMAXR", procprt_CGRSWPMAXR_a, procprt_CGRSWPMAXR_e, 6};
/***************************************************************/
char *
procprt_SORTITEM_ae(struct tstat *curstat, int avgval, double nsecs)
{
        return "";   // dummy function
}

proc_printdef procprt_SORTITEM =   // width is dynamically defined!
   { 0, "SORTITEM", procprt_SORTITEM_ae, procprt_SORTITEM_ae, 4};
