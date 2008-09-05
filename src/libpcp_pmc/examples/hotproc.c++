/*
 * Copyright (c) 1997,2004 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#include <iostream.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <signal.h>

extern "C" {
#include <curses.h>
}

#include <pcp/pmc/Group.h>
#include <pcp/pmc/Metric.h>

// This may be putenv, so make it static
static PMC_String	tzEnv = "TZ=";

// Are we using curses?
PMC_Bool		cFlag = PMC_false;

// The curses window
WINDOW*			win = NULL;

struct HotProc {
    float	cpuburn;
    uint_t	index;

    static int compare(const void* lhs, const void* rhs) {
	HotProc* l = (HotProc*)lhs;
	HotProc* r = (HotProc*)rhs;
	if (l->cpuburn < r->cpuburn)
	    return 1;
	else if (l->cpuburn > r->cpuburn)
	    return -1;
	return 0;
    }
};

typedef PMC_Vector<HotProc> ProcVector;

void
usage()
{
    pmprintf("Usage: %s [options]\n\nOptions:\n", pmProgname);
    pmprintf("  -c             continuous display using curses\n");
    pmprintf("  -h host        metrics source is PMCD on host\n");
    pmprintf("  -n pmnsFile    use an alternative PMNS\n");
    pmprintf("  -t interval    sample interval [default 2.0 seconds]\n");
    pmprintf("  -Z timeZone    set reporting timeZone\n");
    pmprintf("  -z             set reporting timeZone to local time of metrics source\n");
}

#if defined(IRIX5_3)
static void
interrupt(...)
#else
/*ARGSUSED*/
static void
interrupt(int)
#endif
{
    if (cFlag == 2) {
        endwin();
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	cerr << "interrupt detected - exiting..." << endl;
#endif

    printf("\n");
    exit(0);
}

static
int prnt(const char *fmt, ...)
{
    va_list arg;
    va_start(arg, fmt);
    return (cFlag ? vwprintw(stdscr, (char *)fmt, arg) : 
	    vfprintf(stdout, fmt, arg));
}

void
dumpHeader(PMC_String const& uname, 
	   PMC_Metric const* load, 
	   char const* timeStr,
	   int active,
	   int total)
{
    if (cFlag)
	move(0,0);
    else
	printf("\n");

    prnt("%s Load[%4.2f,%4.2f,%4.2f] %8s  %d of %d procs\n",
	 uname.ptr(), load->value(0), load->value(1), load->value(2),
	 timeStr + 11, active, total);

    if (cFlag) {
	move(1,0);
	attron(A_REVERSE);
    }

    if (cFlag || active)
	prnt("    user   pid  pgrp   %%cpu proc  pri  size   rss     time  command            \n");

    if (cFlag)
	attroff(A_REVERSE);
}

int
main(int argc, char* argv[])
{
    int			sts = 0;
    int			c;
    uint_t		i, j, l;
    int			errflag = 0;
    char*		msg;
    char		buf[32];
    pmUnits		units;
    PMC_String		host;
    PMC_String		pmnsFile;
    PMC_String		timeZone;
    PMC_String		tzLabel;
    PMC_String		tzString;
    PMC_String		userName;
    PMC_String		processor;
    PMC_String		unameStr;
    PMC_String		procName;
    PMC_String		valueStr;
    PMC_Bool		zFlag = PMC_false;
    struct timeval	interval;
    struct timeval	position;
    struct timeval	curPos;
    double		diff;
    double		pgsz;
    int			minutes;
    int			seconds;
    int			active;
    ProcVector		procList;

    pmProgname = basename(argv[0]);

    interval.tv_sec = 2;
    interval.tv_usec = 0;

    signal(SIGINT, interrupt);
    fclose(stdin);

    while ((c = getopt(argc, argv, "cD:h:n:t:Z:z?")) != EOF) {
	switch (c) {
	case 'c':
	    cFlag = PMC_true;
	    break;

	case 'D':
	    sts = __pmParseDebug(optarg);
            if (sts < 0) {
		pmprintf("%s: unrecognized debug flag specification (%s)\n",
			 pmProgname, optarg);
                errflag++;
            }
            else
                pmDebug |= sts;
            break;

        case 'h':       // contact PMCD on this hostname
	    if (host.length() > 0) {
		pmprintf("%s: only one host (-h) may be specifiedn\n",
			 pmProgname);
		errflag++;
	    }
	    else
		host = optarg;
	    break;

        case 'n':       // alternative namespace
	    if (pmnsFile.length() != 0) {
		pmprintf("%s: at most one -n option allowed\n", pmProgname);
		errflag++;
	    }
	    else
		pmnsFile = optarg;
	    break;

        case 't':       // sampling interval
            if (pmParseInterval(optarg, &interval, &msg) < 0) {
		pmprintf("%s\n", msg);
                free(msg);
                errflag++;
            }
            break;

        case 'z':       // timeZone from host
            if (timeZone.length()) {
                pmprintf("%s: -z and -Z may not be used together\n",
			 pmProgname);
                errflag++;
            }
            zFlag = PMC_true;
            break;

        case 'Z':       // $TZ timeZone
            if (zFlag) {
                pmprintf("%s: -z and -Z may not be used together\n",
			 pmProgname);
                errflag++;
            }
            timeZone = optarg;
            break;

	case '?':
	    usage();
	    pmflush();
	    exit(0);
	    /*NOTREACHED*/

	default:
	    errflag++;
	    break;
	}
    }

    if (errflag) {
	usage();
	pmflush();
	exit(1);
        /*NOTREACHED*/
    }

    PMC_Group setup;	// Fetch once at the start group
    PMC_Group group;	// The real fetch group
 
    // Get local namespace is requested before opening any contexts
    //
    if (pmnsFile.length()) {
	sts = pmLoadNameSpace(pmnsFile.ptr());
	if (sts < 0) {
	    pmprintf("%s: %s\n", pmProgname, pmErrStr(sts));
	    pmflush();
	    exit(1);
	    /*NOTREACHED*/
	}
    }

    // Add default host to both groups
    //
    if (host.length()) {
	sts = setup.use(PM_CONTEXT_HOST, host.ptr());
	if (sts < 0) {
	    pmprintf("%s: %s\n", pmProgname, pmErrStr(sts));
	    pmflush();
	    exit(1);
	    /*NOTREACHED*/
	}
	sts = group.use(PM_CONTEXT_HOST, host.ptr());
	if (sts < 0) {
	    pmprintf("%s: %s\n", pmProgname, pmErrStr(sts));
	    pmflush();
	    exit(1);
	    /*NOTREACHED*/
	}
    }

    setup.useDefault();

    // Get system information
    //
    PMC_Metric* uname = setup.addMetric("pmda.uname");
    if (uname->status() < 0) {
	pmflush();
	exit(1);
	/*NOTREACHED*/
    }
    PMC_Metric* pagesize = setup.addMetric("hinv.pagesize");
    if (pagesize->status() < 0) {
	pmflush();
	exit(1);
	/*NOTREACHED*/
    }

    setup.fetch();
    if (uname->error(0) < 0) {
	pmprintf("%s: Error %s: %s\n",
		 pmProgname, uname->spec(PMC_true).ptr(), 
		 pmErrStr(uname->error(0)));
	pmflush();
	exit(1);
	/*NOTREACHED*/
    }
    if (pagesize->error(0) < 0) {
	pmprintf("%s: Error %s: %s\n",
		 pmProgname, uname->spec(PMC_true).ptr(), 
		 pmErrStr(uname->error(0)));
	pmflush();
	exit(1);
	/*NOTREACHED*/
    }

    if (uname->strValue(0).length() > 30)
	unameStr = uname->strValue(0).substr(0, 30);
    else
	unameStr = uname->strValue(0);

    pgsz = (pagesize->value(0) / 1024.0);

    // Start curses if required
    //
    if (cFlag) {
	win = initscr();
	clear();
	refresh();
    }

    group.useDefault();

    // Setup timeZones
    if (zFlag)
	group.useTZ();
    else if (timeZone.length()) {
	sts = group.useTZ(timeZone);
        if ((sts = pmNewZone(timeZone.ptr())) < 0) {
	    pmprintf("%s: cannot set timeZone to \"%s\": %s\n",
		     pmProgname, timeZone.ptr(), pmErrStr(sts));
	    pmflush();
	    exit(1);
	    /*NOTREACHED*/
        }
    }

    group.defaultTZ(tzLabel, tzString);
    
    PMC_Metric* load = group.addMetric("kernel.all.load[1,5,15]");
    PMC_Metric* nprocs = group.addMetric("hotproc.nprocs");
    PMC_Metric* totprocs = group.addMetric("proc.nprocs");
    PMC_Metric* user = group.addMetric("hotproc.psinfo.uname", 0.0, PMC_true);
    PMC_Metric* pid = group.addMetric("hotproc.psinfo.pid", 0.0, PMC_true);
    PMC_Metric* pgrp = group.addMetric("hotproc.psinfo.pgrp", 0.0, PMC_true);
    PMC_Metric* size = group.addMetric("hotproc.psinfo.size", 0.0, PMC_true);
    PMC_Metric* rss = group.addMetric("hotproc.psinfo.rssize", 0.0, PMC_true);
    PMC_Metric* cpu = group.addMetric("hotproc.psinfo.time", 0.01, PMC_true);
    PMC_Metric* proc = group.addMetric("hotproc.psinfo.sonproc", 0.0, PMC_true);
    PMC_Metric* pri = group.addMetric("hotproc.psinfo.pri", 0.0, PMC_true);
    PMC_Metric* start = group.addMetric("hotproc.psinfo.start", 0.0, PMC_true);
    PMC_Metric* name = group.addMetric("hotproc.psinfo.fname", 0.0, PMC_true);

    if (load->status() < 0 ||
	nprocs->status() < 0 ||
	totprocs->status() < 0 ||
	user->status() < 0 ||
	pid->status() < 0 ||
	pgrp->status() < 0 ||
	size->status() < 0 ||
	rss->status() < 0 ||
	cpu->status() < 0 ||
	proc->status() < 0 ||
	pri->status() < 0 ||
	start->status() < 0 ||
	name->status() < 0) {

	if (cFlag)
	    endwin();

	pmflush();
	exit(1);
	/*NOTREACHED*/
    }

    units.dimTime = 1;
    units.scaleTime = PM_TIME_SEC;
    units.dimSpace = units.dimCount = units.scaleSpace = units.scaleCount = 0;
    cpu->setScaleUnits(units);

    gettimeofday(&position, NULL);

    // Main Loop
    //
    for(;;) {
	group.fetch();

	pmCtime(&(position.tv_sec), buf);
	buf[19]='\0';

	if (nprocs->value(0) > 0) {

	    active = 0;
	    for (c = 0; c < name->numInst(); c++)
		if (name->error(c) >= 0)
		    active++;

	    dumpHeader(unameStr, load, buf, active, (int)totprocs->value(0));

	    // Sort the instances in terms of cpu burn
	    if (procList.length() < active)
		procList.resize(active);
	    for (c = 0, i = 0; c < name->numInst(); c++) {
		if (name->error(c) >= 0) {
		    procList[i].cpuburn = cpu->value(c);
		    procList[i].index = c;
		    i++;
		}
	    }

	    qsort(procList.ptr(), active, sizeof(HotProc), HotProc::compare);
	    
	    for (c = 0, i = 0, l = 2; c < name->numInst(); c++) {

		if (cFlag && l >= LINES)
		    break;

		if (name->error(c) < 0) {
#ifdef PCP_DEBUG
		    if (pmDebug & DBG_TRACE_APPL0)
			cerr << "Proc " << cpu->instName(c) << ": "
			     << pmErrStr(cpu->error(c)) << endl;
#endif
		    continue;
		}

		j = procList[i++].index;

		if (user->strValue(j).length() > 8)
		    userName = user->strValue(j).substr(0, 8);
		else
		    userName = user->strValue(j);

		if (name->strValue(j).length() > 20)
		    procName = name->strValue(j).substr(0, 20);
		else
		    procName = name->strValue(j);

		if (proc->value(j) < 0)
		    processor = "*";
		else {
		    processor = "";
		    processor.appendInt((int)proc->value(j));
		}

		minutes = (int)((position.tv_sec - (int)start->value(j))) / 60;
		seconds = (int)((position.tv_sec - (int)start->value(j))) % 60;

		if (cpu->error(j))
		    valueStr="??.??";
		else {
		    valueStr = "";
		    valueStr.appendReal(cpu->value(j), 2);
		}

		if (cFlag)
		    move(l, 0);

		prnt("%8s %5d %5d %6s %4s %4d %5d %5d %5d:%02d  %s\n",
		     userName.ptr(), (int)pid->value(j), (int)pgrp->value(j),
		     valueStr.ptr(), processor.ptr(), (int)pri->value(j),
		     (int)(size->value(j) / pgsz),
		     (int)(rss->value(j) / pgsz), minutes, seconds,
		     procName.ptr());

		l++;
	    }

#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_APPL2) {
		    fflush(stdout);
		    cpu->dump(cerr);
		    cerr << endl;
		    cerr.flush();
		}
#endif

	    if (cFlag)
		move(l,0);

	    // If the indom changed then update it
	    //
	    if (name->indom()->changed()) {

#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_APPL0)
		    cerr << "Indom has changed, had " << name->numInst()
			 << " instances" << endl;
#endif

		user->updateIndom();
		pid->updateIndom();
		pgrp->updateIndom();
		size->updateIndom();
		rss->updateIndom();
		cpu->updateIndom();
		proc->updateIndom();
		pri->updateIndom();
		start->updateIndom();
		name->updateIndom();

#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_APPL0)
		    cerr << "Indom updated, have " << name->numInst()
			 << " instances (nprocs = " << nprocs->value(0)
			 << ')' << endl;
#endif
		
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_APPL0) {
		    cerr << "Indom updated and fetched, got "
			 << nprocs->value(0) << " procs" << endl;
		    name->dump(cerr);
		    name->indom()->dump(cerr);
		}
#endif
	    }
	}
	else {
	    dumpHeader(unameStr, load, buf, 0, (int)totprocs->value(0));

	    if (cFlag)
		move(2,0);
	}

	if (cFlag) {
	    clrtobot();
	    move(LINES-2,0);
	    refresh();
	}

	// Update our position
	//
	position.tv_sec += interval.tv_sec;
	position.tv_usec += interval.tv_usec;

	gettimeofday(&curPos, NULL);
	diff = __pmtimevalSub(&position, &curPos);

	if (diff < 0.0) {	// We missed an update
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0) {
		cerr << "Need to slip " << -diff << " seconds" << endl;
	    }
#endif
	    do {
		position.tv_sec += interval.tv_sec;
		position.tv_usec += interval.tv_usec;
		diff = __pmtimevalSub(&position, &curPos);
	    } while (diff < 0.0);
	}

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1) {
	    cerr << "Napping for " << diff << " seconds" << endl;
	}
#endif

	sginap((long)(diff * (double)CLK_TCK));
    }
}
