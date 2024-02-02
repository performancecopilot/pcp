/*
** ATOP - System & Process Monitor 
**
** The program 'atop' offers the possibility to view the activity of
** the system on system-level as well as process-level.
**
** This source-file contains the Linux-specific functions to calculate
** figures to be visualized.
** 
** Copyright (C) 2015,2018-2021 Red Hat.
** Copyright (C) 2009-2010 JC van Winkel
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

static void	make_proc_dynamicgen(void);
static int	get_perc(char *, char *);
static void	make_proc_prints(proc_printpair *, int, const char *, 
					const char *);

/*
** critical percentages for occupation-percentage;
** these defaults can be overruled via the config-file
*/
int   cpubadness = 90;        /* percentage           */
int   gpubadness = 100;       /* percentage           */
int   membadness = 90;        /* percentage           */
int   swpbadness = 80;        /* percentage           */
int   dskbadness = 90;        /* percentage           */
int   netbadness = 90;        /* percentage           */
int   pagbadness = 10;        /* number per second    */

int   almostcrit = 80;        /* percentage           */

/*
 * tables with all sys_printdefs
 */
sys_printdef *prcsyspdefs[] = {
	&syspdef_PRCSYS,
	&syspdef_PRCUSER,
	&syspdef_PRCNPROC,
        &syspdef_PRCNRUNNING,
        &syspdef_PRCNSLEEPING,
        &syspdef_PRCNDSLEEPING,
        &syspdef_PRCNIDLE,
	&syspdef_PRCNZOMBIE,
	&syspdef_PRCCLONES,
	&syspdef_PRCNNEXIT,
	&syspdef_BLANKBOX,
        0
};
sys_printdef *cpusyspdefs[] = {
	&syspdef_CPUSYS,
	&syspdef_CPUUSER,
	&syspdef_CPUIRQ,
	&syspdef_CPUIDLE,
	&syspdef_CPUWAIT,
	&syspdef_BLANKBOX,
	&syspdef_CPUIPC,
	&syspdef_CPUCYCLE,
	&syspdef_CPUFREQ,
	&syspdef_CPUSCALE,
	&syspdef_CPUSTEAL,
	&syspdef_CPUGUEST,
	&syspdef_BLANKBOX,
        0
};
sys_printdef *cpisyspdefs[] = {
	&syspdef_CPUISYS,
	&syspdef_CPUIUSER,
	&syspdef_CPUIIRQ,
	&syspdef_CPUIIDLE,
	&syspdef_CPUIWAIT,
	&syspdef_BLANKBOX,
	&syspdef_CPUIIPC,
	&syspdef_CPUICYCLE,
	&syspdef_CPUIFREQ,
	&syspdef_CPUISCALE,
	&syspdef_CPUISTEAL,
	&syspdef_CPUIGUEST,
	&syspdef_BLANKBOX,
        0
};
sys_printdef *cplsyspdefs[] = {
	&syspdef_CPLNUMCPU,
	&syspdef_CPLAVG1,
	&syspdef_CPLAVG5,
	&syspdef_CPLAVG15,
	&syspdef_CPLCSW,
	&syspdef_CPLINTR,
	&syspdef_BLANKBOX,
        0
};
sys_printdef *gpusyspdefs[] = {
	&syspdef_GPUBUS,
	&syspdef_GPUGPUPERC,
	&syspdef_GPUMEMPERC,
	&syspdef_GPUMEMOCC,
	&syspdef_GPUMEMTOT,
	&syspdef_GPUMEMUSE,
	&syspdef_GPUMEMAVG,
	&syspdef_GPUTYPE,
	&syspdef_GPUNRPROC,
	&syspdef_BLANKBOX,
        0
};
sys_printdef *memsyspdefs1[] = {
	&syspdef_MEMTOT,
	&syspdef_MEMFREE,
	&syspdef_MEMAVAIL,
	&syspdef_BLANKBOX,
	&syspdef_MEMCACHE,
	&syspdef_MEMDIRTY,
	&syspdef_MEMBUFFER,
	&syspdef_BLANKBOX,
	&syspdef_MEMSLAB,
	&syspdef_RECSLAB,
	&syspdef_BLANKBOX,
	&syspdef_HUPTOT,
	&syspdef_HUPUSE,
        0
};
sys_printdef *memsyspdefs2[] = {
	&syspdef_NUMNUMA,
	&syspdef_BLANKBOX,
	&syspdef_TCPSOCK,
	&syspdef_UDPSOCK,
	&syspdef_BLANKBOX,
	&syspdef_SHMEM,
	&syspdef_SHMRSS,
	&syspdef_SHMSWP,
	&syspdef_BLANKBOX,
	&syspdef_PAGETABS,
	&syspdef_BLANKBOX,
	&syspdef_ANONTHP,
	&syspdef_VMWBAL,
	&syspdef_BLANKBOX,
	&syspdef_ZFSARC,
        0
};
sys_printdef *swpsyspdefs[] = {
	&syspdef_SWPTOT,
	&syspdef_SWPFREE,
	&syspdef_SWPCACHE,
	&syspdef_BLANKBOX,
	&syspdef_ZSWPOOL,
	&syspdef_ZSWSTORED,
	&syspdef_KSMSHARING,
	&syspdef_KSMSHARED,
	&syspdef_BLANKBOX,
	&syspdef_SWPCOMMITTED,
	&syspdef_SWPCOMMITLIM,
	&syspdef_BLANKBOX,
        0
};
sys_printdef *pagsyspdefs[] = {
	&syspdef_PAGSCAN,
	&syspdef_PAGSTEAL,
	&syspdef_PAGSTALL,
	&syspdef_PAGCOMPACT,
	&syspdef_PGMIGRATE,
	&syspdef_NUMAMIGRATE,
	&syspdef_PAGSWIN,
	&syspdef_PAGSWOUT,
	&syspdef_PAGZSWIN,
	&syspdef_PAGZSWOUT,
	&syspdef_OOMKILLS,
	&syspdef_PAGPGIN,
	&syspdef_PAGPGOUT,
	&syspdef_BLANKBOX,
        0
};
sys_printdef *memnumasyspdefs[] = {
	&syspdef_NUMATOT,
	&syspdef_NUMAFREE,
	&syspdef_NUMAFILEPAGE,
	&syspdef_NUMANR,
	&syspdef_NUMADIRTY,
	&syspdef_NUMAACTIVE,
	&syspdef_NUMAINACTIVE,
	&syspdef_NUMASLAB,
	&syspdef_NUMASLABRECLAIM,
	&syspdef_NUMASHMEM,
	&syspdef_NUMAFRAG,
	&syspdef_NUMAHUPTOT,
	&syspdef_NUMAHUPUSE,
        0
};
sys_printdef *cpunumasyspdefs[] = {
	&syspdef_NUMACPUSYS,
	&syspdef_NUMACPUUSER,
	&syspdef_NUMACPUNICE,
	&syspdef_NUMACPUIRQ,
	&syspdef_NUMACPUSOFTIRQ,
	&syspdef_NUMACPUIDLE,
	&syspdef_NUMACPUWAIT,
	&syspdef_NUMACPUSTEAL,
	&syspdef_NUMACPUGUEST,
	&syspdef_NUMANUMCPU,
        0
};
sys_printdef *llcsyspdefs[] = {
	&syspdef_LLCMBMTOTAL,
	&syspdef_LLCMBMLOCAL,
	&syspdef_NUMLLC,
	&syspdef_BLANKBOX,
        0
};
sys_printdef *psisyspdefs[] = {
	&syspdef_PSICPUSTOT,
	&syspdef_PSIMEMSTOT,
	&syspdef_PSIMEMFTOT,
	&syspdef_PSIIOSTOT,
	&syspdef_PSIIOFTOT,
	&syspdef_PSICPUS,
	&syspdef_PSIMEMS,
	&syspdef_PSIMEMF,
	&syspdef_PSIIOS,
	&syspdef_PSIIOF,
	&syspdef_BLANKBOX,
        0
};
sys_printdef *contsyspdefs[] = {
	&syspdef_CONTNAME,
	&syspdef_CONTNPROC,
	&syspdef_CONTCPU,
	&syspdef_CONTMEM,
	&syspdef_BLANKBOX,
	0
};
sys_printdef *dsksyspdefs[] = {
	&syspdef_DSKNAME,
	&syspdef_DSKBUSY,
	&syspdef_DSKNREAD,
	&syspdef_DSKNWRITE,
	&syspdef_DSKNDISC,
	&syspdef_DSKMBPERSECWR,
	&syspdef_DSKMBPERSECRD,
	&syspdef_DSKKBPERRD,
	&syspdef_DSKKBPERWR,
	&syspdef_DSKKBPERDS,
	&syspdef_DSKINFLIGHT,
	&syspdef_DSKAVQUEUE,
	&syspdef_DSKAVIO,
	&syspdef_BLANKBOX,
        0
};
sys_printdef *nfsmntsyspdefs[] = {
	&syspdef_NFMPATH,
	&syspdef_NFMSERVER,
	&syspdef_NFMTOTREAD,
	&syspdef_NFMTOTWRITE,
	&syspdef_NFMNREAD,
	&syspdef_NFMNWRITE,
	&syspdef_NFMDREAD,
	&syspdef_NFMDWRITE,
	&syspdef_NFMMREAD,
	&syspdef_NFMMWRITE,
	&syspdef_BLANKBOX,
        0
};
sys_printdef *nfcsyspdefs[] = {
	&syspdef_NFCRPCCNT,
	&syspdef_NFCRPCREAD,
	&syspdef_NFCRPCWRITE,
	&syspdef_NFCRPCRET,
	&syspdef_NFCRPCARF,
	&syspdef_BLANKBOX,
        0
};
sys_printdef *nfssyspdefs[] = {
	&syspdef_NFSRPCCNT,
	&syspdef_NFSRPCREAD,
	&syspdef_NFSRPCWRITE,
	&syspdef_NFSNRBYTES,
	&syspdef_NFSNWBYTES,
	&syspdef_NFSNETTCP,
	&syspdef_NFSNETUDP,
	&syspdef_NFSBADFMT,
	&syspdef_NFSBADAUT,
	&syspdef_NFSBADCLN,
	&syspdef_NFSRCHITS,
	&syspdef_NFSRCMISS,
	&syspdef_NFSRCNOCA,
	&syspdef_BLANKBOX,
        0
};
sys_printdef *nettranssyspdefs[] = {
	&syspdef_NETTRANSPORT,
	&syspdef_NETTCPI,
	&syspdef_NETTCPO,
	&syspdef_NETUDPI,
	&syspdef_NETUDPO,
	&syspdef_NETTCPACTOPEN,
	&syspdef_NETTCPPASVOPEN,
	&syspdef_NETTCPRETRANS,
	&syspdef_NETTCPINERR,
	&syspdef_NETTCPORESET,
	&syspdef_NETTCPCSUMERR,
	&syspdef_NETUDPNOPORT,
	&syspdef_NETUDPINERR,
	&syspdef_BLANKBOX,
        0
};
sys_printdef *netnetsyspdefs[] = {
	&syspdef_NETNETWORK,
	&syspdef_NETIPI,
	&syspdef_NETIPO,
	&syspdef_NETIPFRW,
	&syspdef_NETIPDELIV,
	&syspdef_NETICMPIN,
	&syspdef_NETICMPOUT,
	&syspdef_BLANKBOX,
        0
};
sys_printdef *netintfsyspdefs[] = {
	&syspdef_NETNAME,
	&syspdef_NETPCKI,
	&syspdef_NETPCKO,
	&syspdef_NETSPEEDMAX,
	&syspdef_NETSPEEDIN,
	&syspdef_NETSPEEDOUT,
	&syspdef_NETCOLLIS,
	&syspdef_NETMULTICASTIN,
	&syspdef_NETRCVERR,
	&syspdef_NETSNDERR,
	&syspdef_NETRCVDROP,
	&syspdef_NETSNDDROP,
	&syspdef_BLANKBOX,
        0
};

sys_printdef *infinisyspdefs[] = {
	&syspdef_IFBNAME,
	&syspdef_IFBPCKI,
	&syspdef_IFBPCKO,
	&syspdef_IFBSPEEDMAX,
	&syspdef_IFBSPEEDIN,
	&syspdef_IFBSPEEDOUT,
	&syspdef_IFBLANES,
	&syspdef_BLANKBOX,
        0
};

/*
 * table with all proc_printdefs
 */
proc_printdef *allprocpdefs[]= 
{
	&procprt_PID,
	&procprt_TID,
	&procprt_PPID,
	&procprt_SYSCPU,
	&procprt_USRCPU,
	&procprt_RUNDELAY,
	&procprt_BLKDELAY,
	&procprt_WCHAN,
	&procprt_NVCSW,
	&procprt_NIVCSW,
	&procprt_VGROW,
	&procprt_RGROW,
	&procprt_MINFLT,
	&procprt_MAJFLT,
	&procprt_VSTEXT,
	&procprt_VSIZE,
	&procprt_RSIZE,
	&procprt_PSIZE,
	&procprt_VSLIBS,
	&procprt_VDATA,
	&procprt_VSTACK,
	&procprt_SWAPSZ,
	&procprt_LOCKSZ,
	&procprt_CMD,
	&procprt_RUID,
	&procprt_EUID,
	&procprt_SUID,
	&procprt_FSUID,
	&procprt_RGID,
	&procprt_EGID,
	&procprt_SGID,
	&procprt_FSGID,
	&procprt_CTID,
	&procprt_VPID,
	&procprt_CID,
	&procprt_STDATE,
	&procprt_STTIME,
	&procprt_ENDATE,
	&procprt_ENTIME,
	&procprt_THR,
	&procprt_TRUN,
	&procprt_TSLPI,
	&procprt_TSLPU,
	&procprt_TIDLE,
	&procprt_POLI,
	&procprt_NICE,
	&procprt_PRI,
	&procprt_RTPR,
	&procprt_CURCPU,
	&procprt_ST,
	&procprt_EXC,
	&procprt_S,
	&procprt_COMMAND_LINE,
	&procprt_NPROCS,
	&procprt_RDDSK,
	&procprt_WRDSK,
	&procprt_CWRDSK,
	&procprt_WCANCEL,
	&procprt_TCPRCV,
	&procprt_TCPRASZ,
	&procprt_TCPSND,
	&procprt_TCPSASZ,
	&procprt_UDPRCV,
	&procprt_UDPRASZ,
	&procprt_UDPSND,
	&procprt_UDPSASZ,
	&procprt_RNET,
	&procprt_SNET,
	&procprt_BANDWI,
	&procprt_BANDWO,
	&procprt_GPULIST,
	&procprt_GPUMEMNOW,
	&procprt_GPUMEMAVG,
	&procprt_GPUGPUBUSY,
	&procprt_GPUMEMBUSY,
	&procprt_CGROUP_PATH,
	&procprt_CGRCPUWGT,
	&procprt_CGRCPUMAX,
	&procprt_CGRCPUMAXR,
	&procprt_CGRMEMMAX,
	&procprt_CGRMEMMAXR,
	&procprt_CGRSWPMAX,
	&procprt_CGRSWPMAXR,
	&procprt_SORTITEM,
        0
};

/*
 * table with all proc_printdefs with PID/TID width to be initialized
 */
proc_printdef *idprocpdefs[]= 
{
	&procprt_PID,
	&procprt_TID,
	&procprt_PPID,
	&procprt_VPID,
	0
};


/***************************************************************/
/*
 * output definitions for process data
 * these should be user configurable
 */
proc_printpair userprocs[MAXITEMS];
proc_printpair memprocs[MAXITEMS];
proc_printpair schedprocs[MAXITEMS];
proc_printpair genprocs[MAXITEMS];
proc_printpair dskprocs[MAXITEMS];
proc_printpair netprocs[MAXITEMS];
proc_printpair gpuprocs[MAXITEMS];
proc_printpair varprocs[MAXITEMS];
proc_printpair cmdprocs[MAXITEMS];
proc_printpair cgrprocs[MAXITEMS];
proc_printpair ownprocs[MAXITEMS];
proc_printpair totusers[MAXITEMS];
proc_printpair totprocs[MAXITEMS];
proc_printpair totconts[MAXITEMS];


/*****************************************************************/
/*
 * output definitions for system data
 * these should be user configurable
 */
sys_printpair sysprcline[MAXITEMS];
sys_printpair allcpuline[MAXITEMS];
sys_printpair indivcpuline[MAXITEMS];
sys_printpair cplline[MAXITEMS];
sys_printpair gpuline[MAXITEMS];
sys_printpair memline1[MAXITEMS];
sys_printpair memline2[MAXITEMS];
sys_printpair swpline[MAXITEMS];
sys_printpair memnumaline[MAXITEMS];
sys_printpair cpunumaline[MAXITEMS];
sys_printpair llcline[MAXITEMS];
sys_printpair pagline[MAXITEMS];
sys_printpair psiline[MAXITEMS];
sys_printpair contline[MAXITEMS];
sys_printpair dskline[MAXITEMS];
sys_printpair nettransportline[MAXITEMS];
sys_printpair netnetline[MAXITEMS];
sys_printpair netinterfaceline[MAXITEMS];
sys_printpair infinibandline[MAXITEMS];
sys_printpair nfsmountline[MAXITEMS];
sys_printpair nfcline[MAXITEMS];
sys_printpair nfsline[MAXITEMS];

typedef struct {
        const char *name;
        int        prio;
} name_prio;

/*
** make an string,int pair array from a string.  chop based on spaces/tabs
** example: input: "ABCD:3  EFG:1   QWE:16"
**         output: { { "ABCD", 3 }, {"EFG", 1},  { "QWE", 16}, { 0, 0 }  }
*/
static void
makeargv(char *line, const char *linename, name_prio *vec) 
{
        int   i=0;
        char *p=line;
        char *name=0;
        char *prio=0;

        // find pair and scan it
        while (*p && i<MAXITEMS-1) 
        {
                // skip initial spaces
                while (*p && (*p==' ' || *p=='\t'))
                {
                        ++p;
                }
                if (! *p) 
                {
                        break;
                }
                name=p;
                // found a new word; let's chop!
                while (*p && *p !=':')
                {
                        ++p;
                }
                if (*p==':')
                {
                        *p=0;
                }
                else
                        mcleanstop(1,	"atoprc - %s: no name:prio pair for "
                        		"`%s'\n", name, linename);

                /* now get number */
                p++;
                prio=p;
                errno = 0;    /* To distinguish success/failure after call */

                long lprio=strtol(p, &p, 10);

                if (prio==p || errno == ERANGE || lprio >= INT_MAX || lprio <0)
                {
		        fprintf(stderr,
			"atoprc - %s: item `%s` has "
                        "invalid priority `", linename, name);
                        while (*prio && *prio !=' ') {
                            fputc(*prio, stderr);
                            prio++;
                        }
                        fprintf(stderr, "'\n");
                        cleanstop(1);
                }
                vec[i].name=name;
                vec[i].prio=lprio;

                ++i;
        }
                
        vec[i].name=0;
}


/*
 * make_sys_prints: make array of sys_printpairs
 * input: string, sys_printpair array, maxentries
 */
static void
make_sys_prints(sys_printpair *ar, int maxn, const char *pairs, 
                sys_printdef *permissables[], const char *linename,
		struct sstat *sstat, extraparam *extra)
{
        name_prio	items[MAXITEMS];
        int		i, a, n=strlen(pairs);
        char		str[n+1];

        strcpy(str, pairs);

        makeargv(str, linename, items);

        for(i=a=0; items[i].name && i<maxn-1; i++) 
        {
                const char *name=items[i].name;
                int j;

                for (j=0; permissables[j] != 0; ++j)
                {
                        if (strcmp(permissables[j]->configname, name) == 0)
                        {
				// call validate function to see if this 
				// counter is relevant
				//
				if (sstat != NULL && 
				    permissables[j]->dovalidate != NULL &&
				    permissables[j]->dovalidate(sstat) == 0)
					break;

                               	ar[a].f    = permissables[j];
                               	ar[a].prio = items[i].prio;
				a++;
                                break;
                        }
                }

                if (permissables[j]==0)
		{
			mcleanstop(1,
			"atoprc - own system line: item %s invalid in %s line\n",
			name, linename);
		}
        }

        ar[a].f=0;
        ar[a].prio=0;
}



/*
 * init_proc_prints: determine width of columns that are
 *                   dependent of dynamic values 
 */
static void 
init_proc_prints(count_t numcpu)
{
	char	linebuf[64];
	int	i;

	/*
	** fill number of digits for various PID/TID columns
	** and reformat header to new width
	*/
	for (i=0; idprocpdefs[i] != 0; i++)
	{
		idprocpdefs[i]->width = pidwidth;

		if ( strlen(idprocpdefs[i]->head) < pidwidth)
		{
			char *p = malloc(pidwidth+1);

			ptrverify(p, "Malloc failed for formatted header\n");

			pmsprintf(p, pidwidth+1, "%*s", pidwidth, idprocpdefs[i]->head);
			idprocpdefs[i]->head = p;
		}
	}

	/*
	** fill number of positions for the SORTITEM (percentage),
	** depending on the number of CPUs (e.g. with 10+ CPUs a process
	** can reach a CPU percentage of 1000% and with 100+ CPUs a
	** CPU percentage of 10000%).
	*/
	procprt_SORTITEM.width =
		pmsprintf(linebuf, sizeof linebuf, "%lld", numcpu*100) + 1;
}

/*
** make_proc_prints: make array of proc_printpairs
** input: string, proc_printpair array, maxentries
*/
void 
make_proc_prints(proc_printpair *ar, int maxn, const char *pairs, 
					const char *linename)
{
        name_prio items[MAXITEMS];
        int n=strlen(pairs);

        char str[n+1];
        strcpy(str, pairs);

        makeargv(str, linename, items);

        int i;
        for(i=0; items[i].name && i<maxn-1; ++i) 
        {
                const char *name=items[i].name;
                int j;
                for (j=0; allprocpdefs[j] != 0; ++j)
                {
                        if (strcmp(allprocpdefs[j]->configname, name)==0)
                        {
                                ar[i].f=allprocpdefs[j];
                                ar[i].prio=items[i].prio;
                                break;
                        }
                }
                if (allprocpdefs[j]==0)
                {
                        mcleanstop(1,
				"atoprc - ownprocline: item %s invalid!\n",
				name);
                }
        }
        ar[i].f=0;
        ar[i].prio=0;
}

/*
** calculate the total consumption on system-level for the 
** four main resources
*/
void
totalcap(struct syscap *psc, struct sstat *sstat,
                             struct tstat **proclist, int nactproc)
{
        register int    i;

        psc->nrcpu      = sstat->cpu.nrcpu;

        psc->availcpu   = sstat->cpu.all.stime +
                          sstat->cpu.all.utime +
                          sstat->cpu.all.ntime +
                          sstat->cpu.all.itime +
                          sstat->cpu.all.wtime +
                          sstat->cpu.all.Itime +
                          sstat->cpu.all.Stime +
                          sstat->cpu.all.steal;

        psc->availmem   = sstat->mem.physmem;

	/*
	** calculate total transfer issued by the active processes
	** for disk and for network
	*/
	for (psc->availnet=psc->availdsk=0, i=0; i < nactproc; i++) 
	{
		struct tstat 	*curstat = *(proclist+i);
		count_t		nett_wsz;

		if (curstat)
		{
			psc->availnet += curstat->net.tcpssz;
			psc->availnet += curstat->net.tcprsz;
			psc->availnet += curstat->net.udpssz;
			psc->availnet += curstat->net.udprsz;

			if (curstat->dsk.wsz > curstat->dsk.cwsz)
				nett_wsz = curstat->dsk.wsz -
					   curstat->dsk.cwsz;
			else
				nett_wsz = 0;

			psc->availdsk += curstat->dsk.rsz;
			psc->availdsk += nett_wsz;
		}
	}

	for (psc->availgpumem=i=0; i < sstat->gpu.nrgpus; i++)
		psc->availgpumem += sstat->gpu.gpu[i].memtotnow;

	psc->nrgpu = sstat->gpu.nrgpus;

	psc->nrmemnuma = sstat->memnuma.nrnuma;
	psc->nrcpunuma = sstat->cpunuma.nrnuma;
}

/*
** calculate cumulative system- and user-time for all active processes
** besides, initialize all counter lines on system level
*/
void
pricumproc(struct sstat *sstat, struct devtstat *devtstat,
           int nexit, unsigned int noverflow, int avgval, double delta)
{
        static int 	firsttime=1;
        int     	i;
        extraparam 	extra;

        for (i=0, extra.totut=extra.totst=0; i < devtstat->nprocactive; i++)
        {
		struct tstat *curstat = *(devtstat->procactive+i);

		if (curstat == NULL)
		    continue;
                extra.totut	+= curstat->cpu.utime;
                extra.totst 	+= curstat->cpu.stime;
        }

        extra.nproc	= devtstat->nprocall;
	extra.ntrun	= devtstat->totrun;
	extra.ntslpi	= devtstat->totslpi;
	extra.ntslpu	= devtstat->totslpu;
	extra.ntidle	= devtstat->totidle;
        extra.nzomb	= devtstat->totzombie;
        extra.nexit	= nexit;
        extra.noverflow	= noverflow;
        extra.avgval	= avgval;
        extra.nsecs	= delta;
        extra.index	= 0;

        if (firsttime)
        {
                firsttime=0;

                if (sysprcline[0].f == 0)
                {
                    make_sys_prints(sysprcline, MAXITEMS,
                        "PRCSYS:8 "
                        "PRCUSER:8 "
	                "BLANKBOX:0 "
                        "PRCNPROC:7 "
                        "PRCNRUNNING:5 "
                        "PRCNSLEEPING:5 "
                        "PRCNDSLEEPING:5 "
                        "PRCNIDLE:5 "
                        "PRCNZOMBIE:4 "
                        "PRCCLONES:3 "
	                "BLANKBOX:0 "
                        "PRCNNEXIT:6",
			prcsyspdefs, "builtin sysprcline",
			sstat, &extra);
                }

                if (allcpuline[0].f == 0)
                {
                    make_sys_prints(allcpuline, MAXITEMS,
	                "CPUSYS:9 "
	                "CPUUSER:8 "
	                "CPUIRQ:6 "
	                "BLANKBOX:0 "
	                "CPUIDLE:7 "
	                "CPUWAIT:7 "
                        "CPUSTEAL:2 "
                        "CPUGUEST:3 "
	                "BLANKBOX:0 "
                        "CPUIPC:5 "
                        "CPUCYCLE:4 "
                        "CPUFREQ:4 "
                        "CPUSCALE:4 ",
			cpusyspdefs, "builtin allcpuline",
			sstat, &extra);
                }

                if (indivcpuline[0].f == 0)
                {
                    make_sys_prints(indivcpuline, MAXITEMS,
	                "CPUISYS:9 "
                        "CPUIUSER:8 "
	                "CPUIIRQ:6 "
	                "BLANKBOX:0 "
	                "CPUIIDLE:7 "
	                "CPUIWAIT:7 "
                        "CPUISTEAL:2 "
                        "CPUIGUEST:3 "
	                "BLANKBOX:0 "
                        "CPUIIPC:5 "
                        "CPUICYCLE:4 "
                        "CPUIFREQ:4 "
                        "CPUISCALE:4 ",
			cpisyspdefs, "builtin indivcpuline",
			sstat, &extra);
                }

                if (cplline[0].f == 0)
                {
                    make_sys_prints(cplline, MAXITEMS,
	                "CPLNUMCPU:7"
	                "BLANKBOX:0 "
	                "CPLAVG1:4 "
	                "CPLAVG5:3 "
	                "CPLAVG15:2 "
	                "BLANKBOX:0 "
	                "CPLCSW:6 "
	                "CPLINTR:5 ",
			cplsyspdefs, "builtin cplline",
			sstat, &extra);
                }

                if (gpuline[0].f == 0)
                {
                    make_sys_prints(gpuline, MAXITEMS,
	                "GPUBUS:8 "
	                "GPUGPUPERC:7 "
	                "GPUMEMPERC:6 "
	                "GPUMEMOCC:5 "
	                "GPUMEMTOT:3 "
	                "GPUMEMUSE:4 "
	                "GPUMEMAVG:2 "
	                "GPUNRPROC:2 "
	                "BLANKBOX:0 "
	                "GPUTYPE:1 ",
			gpusyspdefs, "builtin gpuline",
			NULL, NULL);
                }

                if (memline1[0].f == 0)
                {
                    make_sys_prints(memline1, MAXITEMS,
	                "MEMTOT:8 "
	                "MEMFREE:9 "
	                "MEMAVAIL:7 "
	                "BLANKBOX:0 "
	                "MEMCACHE:7 "
	                "MEMDIRTY:5 "
	                "MEMBUFFER:6 "
	                "BLANKBOX:0 "
	                "MEMSLAB:6 "
	                "RECSLAB:2 "
	                "BLANKBOX:0 "
	                "HUPTOT:4 "
	                "HUPUSE:1 ",
			memsyspdefs1, "builtin memline1",
			sstat, &extra);
                }

                if (memline2[0].f == 0)
                {
                    make_sys_prints(memline2, MAXITEMS,
	                "NUMNUMA:8 "
	                "BLANKBOX:1 "
	                "SHMEM:6 "
	                "SHMRSS:4 "
	                "SHMSWP:4 "
	                "BLANKBOX:0 "
	                "TCPSOCK:3 "
	                "UDPSOCK:2 "
	                "BLANKBOX:0 "
	                "PAGETABS:5 "
	                "BLANKBOX:0 "
                        "ANONTHP:4 "
	                "VMWBAL:4 "
	                "BLANKBOX:0 "
	                "ZFSARC:5 ",
			memsyspdefs2, "builtin memline2",
			sstat, &extra);
                }

                if (swpline[0].f == 0)
                {
                    make_sys_prints(swpline, MAXITEMS,
	                "SWPTOT:5 "
	                "SWPFREE:6 "
	                "SWPCACHE:4 "
	                "BLANKBOX:0 "
	                "ZSWPOOL:3 "
	                "ZSWSTORED:3 "
	                "BLANKBOX:0 "
	                "KSMSHARED:2 "
	                "KSMSHARING:2 "
	                "BLANKBOX:0 "
	                "SWPCOMMITTED:7 "
	                "SWPCOMMITLIM:8",
 			swpsyspdefs, "builtin swpline",
			sstat, &extra);
                }

                if (memnumaline[0].f == 0)
                {
                    make_sys_prints(memnumaline, MAXITEMS,
	                "NUMATOT:8 "
	                "NUMAFREE:9 "
	                "NUMAFILEPAGE:9 "
	                "NUMANR:7 "
	                "NUMADIRTY:5 "
	                "NUMAACTIVE:5 "
	                "NUMAINACTIVE:5 "
	                "NUMASLAB:7 "
	                "NUMASLABRECLAIM:4 "
	                "NUMASHMEM:4 "
	                "NUMAFRAG:6 "
	                "NUMAHUPTOT:4 "
	                "NUMAHUPUSE:3 ",
			memnumasyspdefs, "builtin memnumaline",
			sstat, &extra);
                }

                if (cpunumaline[0].f == 0)
                {
                    make_sys_prints(cpunumaline, MAXITEMS,
	                "NUMACPUSYS:9 "
	                "NUMACPUUSER:8 "
	                "NUMACPUNICE:8 "
	                "NUMACPUIRQ:6 "
	                "NUMACPUSOFTIRQ:6 "
	                "NUMACPUIDLE:7 "
	                "NUMACPUWAIT:7 "
	                "NUMACPUSTEAL:2 "
	                "NUMACPUGUEST:3 "
	                "NUMANUMCPU:5",
			cpunumasyspdefs, "builtin cpunumaline",
			NULL, NULL);
                }

                if (llcline[0].f == 0)
                {
                    make_sys_prints(llcline, MAXITEMS,
	                "LLCMBMTOTAL:9 "
	                "LLCMBMLOCAL:8 "
	                "NUMLLC:7 "
	                "BLANKBOX:0 ",
			llcsyspdefs, "builtin llcline",
			sstat, &extra);
                }

                if (pagline[0].f == 0)
                {
                    make_sys_prints(pagline, MAXITEMS,
	                "PAGSCAN:3 "
	                "PAGSTEAL:2 "
	                "PAGSTALL:1 "
	                "PAGCOMPACT:5 "
			"NUMAMIGRATE:5"
			"PGMIGRATE:6"
	                "PAGPGIN:6 "
	                "PAGPGOUT:6 "
	                "PAGZSWIN:4 "
	                "PAGZSWOUT:7 "
			"OOMKILLS:9 ",
			pagsyspdefs, "builtin pagline",
			sstat, &extra);
                }

                if (psiline[0].f == 0)
                {
                    make_sys_prints(psiline, MAXITEMS,
	                "PSICPUSTOT:7 "
	                "PSIMEMSTOT:7 "
	                "PSIMEMFTOT:8 "
	                "PSIIOSTOT:7 "
	                "PSIIOFTOT:8 "
	                "PSICPUS:6 "
	                "PSIMEMS:5 "
	                "PSIMEMF:3 "
	                "PSIIOS:4 "
	                "PSIIOF:2 "
	                "BLANKBOX:0 ",
			psisyspdefs, "builtin psiline",
			sstat, &extra);
                }

                if (contline[0].f == 0)
                {
                    make_sys_prints(contline, MAXITEMS,
	                "CONTNAME:8 "
	                "CONTNPROC:7 "
	                "CONTCPU:6 "
	                "CONTMEM:6 "
	                "BLANKBOX:0 "
	                "BLANKBOX:0 ",
			contsyspdefs, "builtin contline",
			NULL, NULL);
                }

                if (dskline[0].f == 0)
                {
                    make_sys_prints(dskline, MAXITEMS,
	                "DSKNAME:9 "
	                "DSKBUSY:8 "
	                "DSKNREAD:7 "
	                "DSKNWRITE:7 "
	                "DSKNDISC:6 "
	                "DSKKBPERRD:5 "
	                "DSKKBPERWR:5 "
	                "DSKKBPERDS:4 "
                        "DSKMBPERSECRD:6 "
                        "DSKMBPERSECWR:6 "
	                "DSKINFLIGHT:2 "
	                "DSKAVQUEUE:1 "
	                "DSKAVIO:6",
			dsksyspdefs, "builtin dskline",
			sstat, &extra);
                }

                if (nfsmountline[0].f == 0)
                {
                    make_sys_prints(nfsmountline, MAXITEMS,
	                "NFMPATH:8 "
	                "NFMSERVER:8 "
			"NFMTOTREAD:8 "
			"NFMTOTWRITE:8 "
	                "BLANKBOX:0 "
			"NFMNREAD:7 "
			"NFMNWRITE:6 "
	                "BLANKBOX:0 "
			"NFMDREAD:5 "
			"NFMDWRITE:4 "
	                "BLANKBOX:0 "
			"NFMMREAD:3 "
			"NFMMWRITE:2 "
	                "BLANKBOX:0 "
                        "BLANKBOX:0",
			nfsmntsyspdefs, "builtin nfsmountline",
			NULL, NULL);
                }

                if (nfcline[0].f == 0)
                {
                    make_sys_prints(nfcline, MAXITEMS,
	                "NFCRPCCNT:8 "
	                "NFCRPCREAD:7 "
	                "NFCRPCWRITE:7 "
	                "NFCRPCRET:5 "
	                "NFCRPCARF:5 "
	                "BLANKBOX:0 "
	                "BLANKBOX:0 "
	                "BLANKBOX:0 "
	                "BLANKBOX:0 "
	                "BLANKBOX:0 ",
			nfcsyspdefs, "builtin nfcline",
			sstat, &extra);
		}

                if (nfsline[0].f == 0)
                {
                    make_sys_prints(nfsline, MAXITEMS,
	                "NFSRPCCNT:8 "
	                "NFSRPCREAD:6 "
	                "NFSRPCWRITE:6 "
	                "BLANKBOX:0 "
	                "NFSNRBYTES:7 "
	                "NFSNWBYTES:7 "
	                "BLANKBOX:0 "
	                "NFSNETTCP:5 "
	                "NFSNETUDP:5 "
	                "BLANKBOX:0 "
	                "NFSRCHITS:3 "
	                "NFSRCMISS:2 "
	                "NFSRCNOCA:1 "
	                "BLANKBOX:0 "
	                "NFSBADFMT:4 "
	                "NFSBADAUT:4 "
	                "NFSBADCLN:4 ",
			nfssyspdefs, "builtin nfsline",
			sstat, &extra);
		}

                if (nettransportline[0].f == 0)
                {
                    make_sys_prints(nettransportline, MAXITEMS,
	                "NETTRANSPORT:9 "
	                "NETTCPI:8 "
	                "NETTCPO:8 "
	                "NETUDPI:8 "
	                "NETUDPO:8 "
                        "NETTCPACTOPEN:6 "
                        "NETTCPPASVOPEN:5 "
                        "NETTCPRETRANS:4 "
                        "NETTCPINERR:3 "
                        "NETTCPORESET:2 "
                        "NETTCPCSUMERR:2 "
                        "NETUDPNOPORT:1 "
                        "NETUDPINERR:3",
			nettranssyspdefs, "builtin nettransportline",
			sstat, &extra);
                }

                if (netnetline[0].f == 0)
                {
                    make_sys_prints(netnetline, MAXITEMS,
                        "NETNETWORK:5 "
                        "NETIPI:4 "
                        "NETIPO:4 "
                        "NETIPFRW:4 "
                        "NETIPDELIV:4 "
	                "BLANKBOX:0 "
	                "BLANKBOX:0 "
	                "BLANKBOX:0 "
                        "NETICMPIN:1 "
                        "NETICMPOUT:1 ",
			netnetsyspdefs, "builtin netnetline",
			sstat, &extra);
                }

                if (netinterfaceline[0].f == 0)
                {
                    make_sys_prints(netinterfaceline, MAXITEMS,
	                "NETNAME:8 "
	                "BLANKBOX:0 "
	                "NETPCKI:7 "
	                "NETPCKO:7 "
	                "BLANKBOX:0 "
	                "NETSPEEDMAX:5 "
	                "NETSPEEDIN:6 "
	                "NETSPEEDOUT:6 "
	                "BLANKBOX:0 "
                        "NETCOLLIS:2 "
                        "NETMULTICASTIN:2 "
                        "NETRCVERR:4 "
                        "NETSNDERR:4 "
                        "NETRCVDROP:3 "
                        "NETSNDDROP:3",
			netintfsyspdefs, "builtin netinterfaceline",
			NULL, NULL);
                }

                if (infinibandline[0].f == 0)
                {
                    make_sys_prints(infinibandline, MAXITEMS,
	                "IFBNAME:8 "
	                "BLANKBOX:0 "
	                "IFBPCKI:7 "
	                "IFBPCKO:7 "
	                "BLANKBOX:0 "
	                "IFBSPEEDMAX:5 "
	                "IFBSPEEDIN:6 "
	                "IFBSPEEDOUT:6 "
	                "IFBLANES:4 "
	                "BLANKBOX:0 "
	                "BLANKBOX:0 "
	                "BLANKBOX:0 "
	                "BLANKBOX:0 "
	                "BLANKBOX:0 "
	                "BLANKBOX:0 ",
			infinisyspdefs, "builtin infinibandline",
			sstat, &extra);
                }
        }  // firsttime

        move(1, 0);
        showsysline(sysprcline, sstat, &extra, "PRC", 0);
}

/*
** print the header for the process list
*/
void
priphead(int curlist, int totlist, char *showtype, char *showorder,
					char autosort, count_t numcpu)
{
        static int      firsttime=1;
        static int      prev_supportflags = -1, prev_threadview = -1;

	/*
 	** determine once the layout of all per-process reports
	** except for the generic report (might change dynamically)
	*/
        if (firsttime) 
        {
		init_proc_prints(numcpu);

                make_proc_prints(memprocs, MAXITEMS, 
                        "PID:10 TID:3 MINFLT:2 MAJFLT:2 VSTEXT:4 VSLIBS:4 "
			"VDATA:4 VSTACK:4 LOCKSZ:3 VSIZE:6 RSIZE:7 PSIZE:5 "
                        "VGROW:7 RGROW:8 SWAPSZ:5 RUID:1 EUID:0 "
                        "SORTITEM:9 CMD:10", 
                        "built-in memprocs");

                make_proc_prints(schedprocs, MAXITEMS, 
                        "PID:10 TID:6 CID:4 VPID:3 CTID:3 TRUN:7 TSLPI:7 "
			"TSLPU:7 TIDLE:7 POLI:8 NICE:9 PRI:5 RTPR:9 CPUNR:8 "
			"ST:8 EXC:8 S:8 RDELAY:8 BDELAY:7 WCHAN:5 "
			"NVCSW:7 NIVCSW:7 SORTITEM:10 CMD:10",
                        "built-in schedprocs");

                make_proc_prints(dskprocs, MAXITEMS, 
                        "PID:10 TID:4 RDDSK:9 "
                        "WRDSK:9 WCANCL:8 "
                        "SORTITEM:10 CMD:10", 
                        "built-in dskprocs");

                make_proc_prints(netprocs, MAXITEMS, 
                        "PID:10 TID:6 "
			"TCPRCV:9 TCPRASZ:4 TCPSND:9 TCPSASZ:4 "
			"UDPRCV:8 UDPRASZ:3 UDPSND:8 UDPSASZ:3 "
			"BANDWI:10 BANDWO:10 "
                        "SORTITEM:10 CMD:10", 
                        "built-in netprocs");

                make_proc_prints(gpuprocs, MAXITEMS, 
                        "PID:10 TID:5 CID:4 GPULIST:8 GPUGPUBUSY:8 GPUMEMBUSY:8 "
			"GPUMEM:7 GPUMEMAVG:6 S:8 SORTITEM:10 CMD:10", 
                        "built-in gpuprocs");

                make_proc_prints(varprocs, MAXITEMS,
                        "PID:10 TID:4 PPID:9 CID:2 VPID:1 CTID:1 "
			"RUID:8 RGID:8 EUID:5 EGID:4 "
     			"SUID:3 SGID:2 FSUID:3 FSGID:2 "
                        "STDATE:7 STTIME:7 ENDATE:5 ENTIME:5 "
			"ST:6 EXC:6 S:6 SORTITEM:10 CMD:10", 
                        "built-in varprocs");

                make_proc_prints(cmdprocs, MAXITEMS,
                        "PID:10 TID:4 S:8 SORTITEM:10 COMMAND-LINE:10", 
                        "built-in cmdprocs");

                make_proc_prints(cgrprocs, MAXITEMS, 
                        "PID:10 CPUWGT:9 CPUMAX:9 CPUMAXR:9 MEMMAX:8 MMMAXR:8 "
			"SWPMAX:7 SWMAXR:7 SORTITEM:6 CMD:9 CGROUP-PATH:10", 
                        "built-in cgrprocs");

                make_proc_prints(totusers, MAXITEMS, 
                        "NPROCS:10 SYSCPU:9 USRCPU:9 VSIZE:6 "
                        "RSIZE:8 PSIZE:8 LOCKSZ:3 SWAPSZ:5 RDDSK:7 CWRDSK:7 "
			"RNET:6 SNET:6 SORTITEM:10 RUID:10", 
                        "built-in totusers");

                make_proc_prints(totprocs, MAXITEMS, 
                        "NPROCS:10 SYSCPU:9 USRCPU:9 VSIZE:6 "
                        "RSIZE:8 PSIZE:8 LOCKSZ:3 SWAPSZ:5 RDDSK:7 CWRDSK:7 "
			"RNET:6 SNET:6 SORTITEM:10 CMD:10", 
                        "built-in totprocs");

                make_proc_prints(totconts, MAXITEMS, 
                        "NPROCS:10 SYSCPU:9 USRCPU:9 RDELAY:8 BDELAY:7 VSIZE:6 "
                        "RSIZE:8 PSIZE:8 LOCKSZ:3 SWAPSZ:5 RDDSK:7 CWRDSK:7 "
			"RNET:6 SNET:6 SORTITEM:10 CID:10", 
                        "built-in totconts");
        }

	/*
 	** update the generic report if needed
	*/
	if (prev_supportflags != supportflags || prev_threadview != threadview)
	{
		make_proc_dynamicgen();

		prev_supportflags = supportflags;
		prev_threadview   = threadview;

		if (*showtype == MPROCNET && !(supportflags & NETATOP || supportflags & NETATOPBPF) )
		{
			*showtype  = MPROCGEN;
			*showorder = MSORTCPU;
		}
	}

        /*
        ** print the header line
        */
        switch (*showtype)
        {
           case MPROCGEN:
                showhdrline(genprocs, curlist, totlist, *showorder, autosort);
                break;

           case MPROCMEM:
                showhdrline(memprocs, curlist, totlist, *showorder, autosort);
                break;

           case MPROCDSK:
                showhdrline(dskprocs, curlist, totlist, *showorder, autosort);
                break;

           case MPROCNET:
                showhdrline(netprocs, curlist, totlist, *showorder, autosort);
                break;

           case MPROCGPU:
                showhdrline(gpuprocs, curlist, totlist, *showorder, autosort);
                break;

           case MPROCVAR:
                showhdrline(varprocs, curlist, totlist, *showorder, autosort);
                break;

           case MPROCARG:
                showhdrline(cmdprocs, curlist, totlist, *showorder, autosort);
                break;

           case MPROCCGR:
                showhdrline(cgrprocs, curlist, totlist, *showorder, autosort);
                break;

           case MPROCOWN:
                showhdrline(ownprocs, curlist, totlist, *showorder, autosort);
                break;

           case MPROCSCH:
                showhdrline(schedprocs, curlist, totlist, *showorder, autosort);
                break;

           case MCUMUSER:
                showhdrline(totusers, curlist, totlist, *showorder, autosort);
                break;

           case MCUMPROC:
                showhdrline(totprocs, curlist, totlist, *showorder, autosort);
                break;

           case MCUMCONT:
                showhdrline(totconts, curlist, totlist, *showorder, autosort);
                break;
        }
}

/*
** assemble the layout of the generic line,
** depending on the supported features (like
** I/O stats, network stats) and current view
*/
#define	FORMPID	"PID:10 "
#define	FORMTID	"TID:6 "
#define	FORMCID	"CID:5 "
#define	FORMCPU	"SYSCPU:9 USRCPU:9 "
#define	FORMDEL	"RDELAY:4 "
#define	FORMBDL	"BDELAY:4 "
#define FORMMEM	"VGROW:8 RGROW:8 "
#define FORMDSK	"RDDSK:7 CWRDSK:7 "
#define FORMNET	"RNET:6 SNET:6 "
#define FORMMSC	"RUID:2 EUID:1 ST:3 EXC:3 THR:3 S:3 CPUNR:3 "
#define FORMEND	"SORTITEM:10 CMD:10"

static void
make_proc_dynamicgen()
{
	char format[300], *p = format;

	memcpy(p, FORMPID, sizeof FORMPID -1);
	p += sizeof FORMPID -1;

	if (threadview)
	{
		memcpy(p, FORMTID, sizeof FORMTID -1);
		p += sizeof FORMTID -1;
	}

	if (supportflags & CONTAINERSTAT)
	{
		memcpy(p, FORMCID, sizeof FORMCID -1);
		p += sizeof FORMCID -1;
	}

	memcpy(p, FORMCPU, sizeof FORMCPU -1);
	p += sizeof FORMCPU -1;

	memcpy(p, FORMDEL, sizeof FORMDEL -1);
	p += sizeof FORMDEL -1;

	memcpy(p, FORMBDL, sizeof FORMBDL -1);
	p += sizeof FORMBDL -1;

	memcpy(p, FORMMEM, sizeof FORMMEM -1);
	p += sizeof FORMMEM -1;

	if (supportflags & IOSTAT)
	{
		memcpy(p, FORMDSK, sizeof FORMDSK -1);
		p += sizeof FORMDSK -1;
	}

	if (supportflags & NETATOP || supportflags & NETATOPBPF)
	{
		memcpy(p, FORMNET, sizeof FORMNET -1);
		p += sizeof FORMNET -1;
	}

	memcpy(p, FORMMSC, sizeof FORMMSC -1);
	p += sizeof FORMMSC -1;

	memcpy(p, FORMEND, sizeof FORMEND);
	p += sizeof FORMEND;

	make_proc_prints(genprocs, MAXITEMS, format, "built-in genprocs");
}

/*
** print the list of processes from the deviation-list
*/
int
priproc(struct tstat **proclist, int firstproc, int lastproc, int curline,
        int curlist, int totlist, char showtype, char showorder,
        struct syscap *sb, double nsecs, int avgval)
{
        register int            i;
        register struct tstat   *curstat;
        double                  perc;

        /*
        ** print info per actual process and maintain totals
        */
        for (i=firstproc; i < lastproc; i++)
        {
                curstat = *(proclist+i);

                if (curstat == NULL)
                        continue;

                if (screen && curline >= LINES) /* screen filled entirely ? */
                        break;

                /*
                ** calculate occupation-percentage of this process
                ** depending on selected resource
                */
                switch (showorder) 
                {
                   case MSORTCPU:
                        perc = 0.0;

                        if (sb->availcpu)
                        {
                                perc = (double)(curstat->cpu.stime +
                                                curstat->cpu.utime  ) * 100 /
                                                (sb->availcpu / sb->nrcpu);

                                if (perc > 100.0 * sb->nrcpu)
                                        perc = 100.0 * sb->nrcpu;

                                if (perc > 100.0 * curstat->gen.nthr)
                                        perc = 100.0 * curstat->gen.nthr;
                        }
                        break;

                   case MSORTMEM:
                        perc = 0.0;

                        if (sb->availmem)
                        {
                                perc = (double)curstat->mem.rmem *
                                               100.0 / sb->availmem;

                                if (perc > 100.0)
                                        perc = 100.0;
                        }
                        break;

                   case MSORTDSK:
                        perc = 0.0;

			if (sb->availdsk)
                        {
				count_t nett_wsz;


				if (curstat->dsk.wsz > curstat->dsk.cwsz)
					nett_wsz = curstat->dsk.wsz -
					           curstat->dsk.cwsz;
				else
					nett_wsz = 0;

				perc = (double)(curstat->dsk.rsz + nett_wsz) *
						100.0 / sb->availdsk;

				if (perc > 100.0)
					perc = 100.0;
                        }
                        break;

                   case MSORTNET:
                        perc = 0.0;

                        if (sb->availnet)
                        {
                                perc = (double)(curstat->net.tcpssz +
                                                curstat->net.tcprsz +
                                                curstat->net.udpssz +
                                                curstat->net.udprsz  ) *
                                                100.0 / sb->availnet;

                                if (perc > 100.0)
                                        perc = 100.0;
                        }
                        break;

                   case MSORTGPU:
                        perc = 0.0;

			if (!curstat->gpu.state)
				break;

                        if (curstat->gpu.gpubusy != -1)
			{
                        	perc = curstat->gpu.gpubusy;
			}
			else
			{
                        	perc = curstat->gpu.memnow*100 *
				       sb->nrgpu / sb->availgpumem;
			}
                        break;

                   default:
                        perc = 0.0;
                }

                /*
                ** now do the formatting of output
                */
                if (screen) {
                        move(curline,0);
                }

                switch (showtype)
                {
                   case MPROCGEN:
                        showprocline(genprocs, curstat, perc, nsecs, avgval);
                        break;

                   case MPROCMEM:
                        showprocline(memprocs, curstat, perc, nsecs, avgval);
                        break;

                   case MPROCDSK:
                        showprocline(dskprocs, curstat, perc, nsecs, avgval);
                        break;

                   case MPROCNET:
                        showprocline(netprocs, curstat, perc, nsecs, avgval);
                        break;

                   case MPROCGPU:
                        showprocline(gpuprocs, curstat, perc, nsecs, avgval);
                        break;

                   case MPROCVAR:
                        showprocline(varprocs, curstat, perc, nsecs, avgval);
                        break;

                   case MPROCARG:
                        showprocline(cmdprocs, curstat, perc, nsecs, avgval);
                        break;

                   case MPROCCGR:
                        showprocline(cgrprocs, curstat, perc, nsecs, avgval);
                        break;

                   case MPROCOWN:
                        showprocline(ownprocs, curstat, perc, nsecs, avgval);
                        break;

                   case MPROCSCH:
                        showprocline(schedprocs, curstat, perc, nsecs, avgval);
                        break;

                   case MCUMUSER:
                        showprocline(totusers, curstat, perc, nsecs, avgval);
                        break;

                   case MCUMPROC:
                        showprocline(totprocs, curstat, perc, nsecs, avgval);
                        break;

                   case MCUMCONT:
                        showprocline(totconts, curstat, perc, nsecs, avgval);
                        break;
                }

                curline++;
        }

        return curline;
}


/*
** print the system-wide statistics
*/
static void	pridisklike(extraparam *, struct perdsk *, char *,
		      char *, int, unsigned int *, int *, int, regex_t *);

int
prisyst(struct sstat *sstat, int curline, double nsecs, int avgval,
        int fixedhead, struct sselection *selp, char *highorderp,
        int maxcpulines, int maxgpulines, int maxdsklines, int maxmddlines,
	int maxlvmlines, int maxintlines, int maxifblines,
	int maxnfslines, int maxcontlines, int maxnumalines, int maxllclines)
{
        extraparam      extra;
        int             lin;
        count_t         busy;
        unsigned int    badness, highbadness=0;

        extra.nsecs	= nsecs;
        extra.avgval	= avgval;

        /*
        ** CPU statistics
        */
        extra.cputot = sstat->cpu.all.stime + sstat->cpu.all.utime +
                       sstat->cpu.all.ntime + sstat->cpu.all.itime +
                       sstat->cpu.all.wtime + sstat->cpu.all.Itime +
                       sstat->cpu.all.Stime + sstat->cpu.all.steal;

        busy   = (extra.cputot - sstat->cpu.all.itime - sstat->cpu.all.wtime)
                                * 100.0 / extra.cputot;

        if (cpubadness)
                badness = busy * 100 / cpubadness;
        else
                badness = 0;

        if (highbadness < badness)
        {
                highbadness = badness;
                *highorderp = MSORTCPU;
        }

        if (extra.cputot == 0)
                extra.cputot = 1;             /* avoid divide-by-zero */

        extra.percputot = extra.cputot / sstat->cpu.nrcpu;

        if (extra.percputot == 0)
                extra.percputot = 1;          /* avoid divide-by-zero */

	if (screen)
 	       move(curline, 0);

        showsysline(allcpuline, sstat, &extra, "CPU", badness);
        curline++;

        if (sstat->cpu.nrcpu > 1)
        {
                for (extra.index=lin=0;
		     extra.index < sstat->cpu.nrcpu && lin < maxcpulines;
   		     extra.index++)
                {
                        extra.percputot =  sstat->cpu.cpu[extra.index].stime +
                                     sstat->cpu.cpu[extra.index].utime +
                                     sstat->cpu.cpu[extra.index].ntime +
                                     sstat->cpu.cpu[extra.index].itime +
                                     sstat->cpu.cpu[extra.index].wtime +
                                     sstat->cpu.cpu[extra.index].Itime +
                                     sstat->cpu.cpu[extra.index].Stime +
                                     sstat->cpu.cpu[extra.index].steal;

                        if (extra.percputot ==
				(sstat->cpu.cpu[extra.index].itime +
                                 sstat->cpu.cpu[extra.index].wtime  ) &&
                                 !fixedhead                             )
                                continue;       /* inactive cpu */

                        busy   = (extra.percputot -
					sstat->cpu.cpu[extra.index].itime -
                                        sstat->cpu.cpu[extra.index].wtime)
                                                  * 100.0 / extra.percputot;

                        if (cpubadness)
                                badness = busy * 100 / cpubadness;
                        else
                                badness = 0;

                        if (highbadness < badness)
                        {
                                highbadness = badness;
                                *highorderp = MSORTCPU;
                        }

                        if (extra.percputot == 0)
                                extra.percputot = 1; /* avoid divide-by-zero */


			if (screen)
        	                move(curline, 0);

                        showsysline(indivcpuline, sstat, &extra, "cpu",
								badness);
                        curline++;
                        lin++;
                }
        }

        /*
        ** other CPU-related statistics
        */
	if (screen)
   	     move(curline, 0);

        showsysline(cplline, sstat, &extra, "CPL", 0);
        curline++;

        /*
        ** GPU statistics
        */
	if (sstat->gpu.nrgpus)
	{
        	for (extra.index=0, lin=0;
		     extra.index < sstat->gpu.nrgpus && lin < maxgpulines;
		     extra.index++)
        	{
			int	totbusy;
			count_t	avgmemuse;

			// notice that GPU percentage and memory percentage
			// are not always available; in that case both
			// values have the value -1
			//
			totbusy = sstat->gpu.gpu[extra.index].gpuperccum +
			          sstat->gpu.gpu[extra.index].memperccum;

			if (totbusy == -2)	// metrics available?
				totbusy= 0;

			if (sstat->gpu.gpu[extra.index].samples == 0)
			{
				totbusy =
					sstat->gpu.gpu[extra.index].gpupercnow +
			          	sstat->gpu.gpu[extra.index].mempercnow;

				avgmemuse =
					sstat->gpu.gpu[extra.index].memusenow;
			}
			else
			{
				totbusy = totbusy /
				     	sstat->gpu.gpu[extra.index].samples;

		   		avgmemuse =
					 sstat->gpu.gpu[extra.index].memusecum/
			                 sstat->gpu.gpu[extra.index].samples;
			}

        		if (gpubadness)
                		badness = totbusy * 100 / gpubadness;
        		else
                		badness = 0;

			if (	totbusy > 0 			||
			    	// memusage > 512 MiB (rather arbitrary)?
			    	avgmemuse > 512*1024 		||
			    	fixedhead			  )
			{
					showsysline(gpuline, sstat,
							&extra, "GPU", badness);
	 				curline++;
					lin++;
			}
		}
	}

        /*
        ** MEMORY statistics
        */
        busy   = (sstat->mem.physmem - sstat->mem.freemem
                                     - sstat->mem.cachemem
                                     - sstat->mem.buffermem
                                     - sstat->mem.slabreclaim
                                     + sstat->mem.shmem)
                                                * 100.0 / sstat->mem.physmem;

        if (membadness)
                badness = busy * 100 / membadness;
        else
                badness = 0;

        if (highbadness < badness)
        {
                highbadness = badness;
                *highorderp = MSORTMEM;
        }

	if (screen)
	        move(curline, 0);

        showsysline(memline1, sstat, &extra, "MEM", badness);
        curline++;

        showsysline(memline2, sstat, &extra, "MEM", badness);
        curline++;

        /*
        ** SWAP statistics
        */
        busy   = (sstat->mem.totswap - sstat->mem.freeswap)
                                * 100.0 / sstat->mem.totswap;

        if (swpbadness)
        {
                badness = busy * 100 / swpbadness;
        }
        else
        {
                badness = 0;
        }

        if (highbadness < badness)
        {
                highbadness = badness;
                *highorderp = MSORTMEM;
        }

	if (screen)
        	move(curline, 0);

        showsysline(swpline, sstat, &extra, "SWP", badness);
        curline++;

	/*
	** memory info related for per NUMA
	*/
	if (sstat->memnuma.nrnuma > 1)
	{
		for (extra.index=lin=0;
		     extra.index < sstat->memnuma.nrnuma && lin < maxnumalines;
		     extra.index++)
		{
			busy = (sstat->memnuma.numa[extra.index].totmem
				- sstat->memnuma.numa[extra.index].freemem
				- sstat->memnuma.numa[extra.index].filepage
				- sstat->memnuma.numa[extra.index].slabreclaim
				+ sstat->memnuma.numa[extra.index].shmem)
					* 100.0 / sstat->memnuma.numa[extra.index].totmem;

			if (membadness)
				badness = busy * 100 / membadness;
			else
				badness = 0;

			if (highbadness < badness)
			{
				highbadness = badness;
				*highorderp = MSORTMEM;
			}

			if (screen)
				move(curline, 0);

			showsysline(memnumaline, sstat, &extra, "NUM", badness);
			curline++;
			lin++;
		}
	}

	/*
	** Accumulate each cpu statistic for per NUMA
	*/
	if (sstat->cpunuma.nrnuma > 1)
	{
		for (extra.index=lin=0;
		     extra.index < sstat->cpunuma.nrnuma && lin < maxnumalines;
		     extra.index++)
		{
			extra.pernumacputot = sstat->cpunuma.numa[extra.index].stime +
					      sstat->cpunuma.numa[extra.index].utime +
					      sstat->cpunuma.numa[extra.index].ntime +
					      sstat->cpunuma.numa[extra.index].itime +
					      sstat->cpunuma.numa[extra.index].wtime +
					      sstat->cpunuma.numa[extra.index].Itime +
					      sstat->cpunuma.numa[extra.index].Stime +
					      sstat->cpunuma.numa[extra.index].steal;

			if (extra.pernumacputot ==
				(sstat->cpunuma.numa[extra.index].itime +
				 sstat->cpunuma.numa[extra.index].wtime  ) &&
				 !fixedhead                             )
				continue;       /* inactive cpu */

			if (extra.pernumacputot == 0)
				extra.pernumacputot = 1; /* avoid divide-by-zero */

			busy = (extra.pernumacputot -
					sstat->cpunuma.numa[extra.index].itime -
					sstat->cpunuma.numa[extra.index].wtime)
					* 100.0 / extra.pernumacputot;

			if (cpubadness)
				badness = busy * 100 / cpubadness;
			else
				badness = 0;

			if (highbadness < badness)
			{
				highbadness = badness;
				*highorderp = MSORTCPU;
			}

			extra.percputot = extra.pernumacputot /
						(sstat->cpu.nrcpu/sstat->cpunuma.nrnuma);
			if (extra.percputot == 0)
				extra.percputot = 1; /* avoid divide-by-zero */

			if (screen)
				move(curline, 0);

			showsysline(cpunumaline, sstat, &extra, "NUC", badness);
			curline++;
			lin++;
		}
	}

	/*
 	** LLC statistics (if supported by kernel and
	** pseudo filesystem mounted)
	*/
	for (extra.index=0, lin=0;
			extra.index < sstat->llc.nrllcs && lin < maxllclines;
			extra.index++)
	{
		if (fixedhead     	        	  	||
		    sstat->llc.perllc[extra.index].mbm_local	||
		    sstat->llc.perllc[extra.index].mbm_total	  )
		{
			if (screen)
				move(curline, 0);

			showsysline(llcline, sstat, &extra, "LLC", 0);
			curline++;
			lin++;
		}
	}

        /*
        ** PAGING statistics
        */
        if (fixedhead             	||
            sstat->mem.pgscans    	||
            sstat->mem.pgsteal    	||
            sstat->mem.allocstall 	||
            sstat->mem.compactstall 	||
            sstat->mem.pgins      	||
            sstat->mem.pgouts     	||
            sstat->mem.tcpsock     	||
            sstat->mem.udpsock     	||
            sstat->mem.swins      	||
            sstat->mem.swouts     	||
            sstat->mem.oomkills > 0   	||
            sstat->mem.pgmigrate     	||
            sstat->mem.numamigrate     	  )
        {
                busy = sstat->mem.swouts / nsecs * pagbadness;

                if (busy > 100)
                        busy = 100;

                if (membadness)
                        badness = busy * 100 / membadness;
                else
                        badness = 0;

                if (highbadness < badness)
                {
                        highbadness = badness;
                        *highorderp = MSORTMEM;
                }

                /*
                ** take care that this line is anyhow colored for
                ** 'almost critical' in case of swapouts > 1 per second
                */
                if (sstat->mem.swouts / nsecs > 0  &&
                    pagbadness && almostcrit && badness < almostcrit)
                        badness = almostcrit;

		if (screen)
                	move(curline, 0);

                showsysline(pagline, sstat, &extra, "PAG", badness);
                curline++;
        }

        /*
        ** Pressure statistics
        */
	if (sstat->psi.present)
	{
        	if (fixedhead                 ||
	            sstat->psi.cpusome.avg10  || sstat->psi.memsome.avg10  ||
	            sstat->psi.iosome.avg10   ||
	            sstat->psi.cpusome.avg60  || sstat->psi.memsome.avg60  ||
	            sstat->psi.iosome.avg60   ||
	            sstat->psi.cpusome.avg300 || sstat->psi.memsome.avg300 ||
	            sstat->psi.iosome.avg300    )
	        {
			badness = sstat->psi.cpusome.avg10 >
			          sstat->psi.cpusome.avg60 ?
			          sstat->psi.cpusome.avg10 :
			          sstat->psi.cpusome.avg60;

			if (badness < sstat->psi.cpusome.avg300)
				badness = sstat->psi.cpusome.avg300;

			if (screen)
                		move(curline, 0);

                	showsysline(psiline, sstat, &extra,"PSI", badness);
                	curline++;
		}
	}

	/*
 	** Container statistics (if any)
	*/
        for (extra.index=0, lin=0;
	     extra.index < sstat->cfs.nrcontainer && lin < maxcontlines;
             extra.index++)
	{
        	if (fixedhead             		||
	            sstat->cfs.cont[extra.index].system	||
	            sstat->cfs.cont[extra.index].user	||
	            sstat->cfs.cont[extra.index].nice	  )
		{
			if (screen)
                		move(curline, 0);

                        showsysline(contline, sstat, &extra, "CON", 0);
                        curline++;
                        lin++;
		}
	}

        /*
        ** DISK statistics
        */
        extra.mstot = extra.cputot / sstat->cpu.nrcpu; 

	pridisklike(&extra, sstat->dsk.lvm, "LVM", highorderp, maxlvmlines,
			&highbadness, &curline, fixedhead,
			selp->lvmnamesz ? &(selp->lvmregex) : (void *) 0);

	pridisklike(&extra, sstat->dsk.mdd, "MDD", highorderp, maxmddlines,
			&highbadness, &curline, fixedhead, (void *) 0);

	pridisklike(&extra, sstat->dsk.dsk, "DSK", highorderp, maxdsklines,
			&highbadness, &curline, fixedhead,
			selp->dsknamesz ? &(selp->dskregex) : (void *) 0);

        /*
        ** NFS server and client statistics
        */
	for (extra.index=0, lin=0;
	     extra.index < sstat->nfs.nfsmounts.nrmounts && lin < maxnfslines;
								extra.index++)
	{
		int i = extra.index;

		if ( (sstat->nfs.nfsmounts.nfsmnt[i].bytesread     +
		      sstat->nfs.nfsmounts.nfsmnt[i].byteswrite    +
		      sstat->nfs.nfsmounts.nfsmnt[i].bytesdread    +
		      sstat->nfs.nfsmounts.nfsmnt[i].bytesdwrite   +
		      sstat->nfs.nfsmounts.nfsmnt[i].bytestotread  +
		      sstat->nfs.nfsmounts.nfsmnt[i].bytestotwrite +
		      sstat->nfs.nfsmounts.nfsmnt[i].pagesmread    +
		      sstat->nfs.nfsmounts.nfsmnt[i].pagesmwrite    ) ||
		      sstat->nfs.nfsmounts.nfsmnt[i].age < nsecs      ||
		      fixedhead                                         )
		{
			if (screen)
                		move(curline, 0);

                        showsysline(nfsmountline, sstat, &extra, "NFM", 0);
                        curline++;
                        lin++;
		}
	}

        if (sstat->nfs.client.rpccnt || fixedhead )
        {
		if (screen)
                	move(curline, 0);

                showsysline(nfcline, sstat, &extra, "NFC", 0);
                curline++;
        }

        if (sstat->nfs.server.rpccnt || fixedhead )
        {
		if (screen)
                	move(curline, 0);

                showsysline(nfsline, sstat, &extra, "NFS", 0);
                curline++;
        }

        /*
        ** NET statistics: transport
        */
        if (sstat->net.tcp.InSegs             ||
            sstat->net.tcp.OutSegs            ||
            sstat->net.udpv4.InDatagrams      ||
            sstat->net.udpv6.Udp6InDatagrams  ||
            sstat->net.udpv4.OutDatagrams     ||
            sstat->net.udpv6.Udp6OutDatagrams ||
            fixedhead )
        {
		if (screen)
                	move(curline, 0);

                showsysline(nettransportline, sstat, &extra, "NET", 0);
                curline++;
        }

        /*
        ** NET statistics: network
        */
        if (sstat->net.ipv4.InReceives ||
            sstat->net.ipv6.Ip6InReceives ||
            sstat->net.ipv4.OutRequests ||
            sstat->net.ipv6.Ip6OutRequests ||
            fixedhead )
        {
		if (screen)
                	move(curline, 0);

                showsysline(netnetline, sstat, &extra, "NET", 0);
                curline++;
        }

        /*
        ** NET statistics: interfaces
        */
        for (extra.index=0, lin=0;
	     sstat->intf.intf[extra.index].name[0] && lin < maxintlines;
             extra.index++)
        {
                if (sstat->intf.intf[extra.index].rpack ||
                    sstat->intf.intf[extra.index].spack || fixedhead)
                {
			if (selp->itfnamesz && regexec(&(selp->itfregex),
			       sstat->intf.intf[extra.index].name, 0, NULL, 0))
				continue;	// suppress (not selected)

                        /*
                        ** calculate busy-percentage for interface
                        */

                       count_t ival, oval;

                        /*
                        ** convert byte-transfers to bit-transfers     (*    8)
                        ** convert bit-transfers  to kilobit-transfers (/ 1000)
                        ** per second
                        */
                        ival    = sstat->intf.intf[extra.index].rbyte/125/nsecs;
                        oval    = sstat->intf.intf[extra.index].sbyte/125/nsecs;

			/* speed known? */
                        if (sstat->intf.intf[extra.index].speed) 
                        {
                                if (sstat->intf.intf[extra.index].duplex)
                                       busy = (ival > oval ? ival : oval) /
					   (sstat->intf.intf[extra.index].speed
									*10);
                                else
                                       busy = (ival + oval) /
                                           (sstat->intf.intf[extra.index].speed
									*10);

                        }
                        else
                        {
                                busy = 0;
                        }

                        if (netbadness)
                                badness = busy * 100 / netbadness;
                        else
                                badness = 0;

                        if (highbadness < badness && (supportflags & NETATOP || supportflags & NETATOPBPF) )
                        {
                                highbadness = badness;
                                *highorderp = MSORTNET;
                        }

			if (screen)
                		move(curline, 0);

                        showsysline(netinterfaceline, sstat, &extra, 
                                      			"NET", badness);
                        curline++;
                        lin++;
                }
        }

        /*
        ** NET statistics: InfiniBand
        */
        for (extra.index=0, lin=0;
	     extra.index < sstat->ifb.nrports && lin < maxifblines;
             extra.index++)
        {
                if (sstat->ifb.ifb[extra.index].rcvb ||
                    sstat->ifb.ifb[extra.index].sndb || fixedhead)
                {
                        /*
                        ** calculate busy-percentage for IB port
                        */
                       count_t ival, oval;

                        /*
                        ** convert byte-transfers to bit-transfers     (*    8)
                        ** convert bit-transfers  to kilobit-transfers (/ 1000)
                        ** per second
                        */
                        ival    = sstat->ifb.ifb[extra.index].rcvb/125/nsecs;
                        oval    = sstat->ifb.ifb[extra.index].sndb/125/nsecs;

			if (sstat->ifb.ifb[extra.index].rate)
				busy = (ival > oval ? ival : oval) *
			                 sstat->ifb.ifb[extra.index].lanes /
					(sstat->ifb.ifb[extra.index].rate * 10);
			else
				busy = 0;

                        if (netbadness)
                                badness = busy * 100 / netbadness;
                        else
                                badness = 0;

                        if (highbadness < badness)
                        {
                                highbadness = badness;
                                *highorderp = MSORTNET;
                        }

			if (screen)
                		move(curline, 0);

                        showsysline(infinibandline, sstat, &extra, 
                                      			"IFB", badness);
                        curline++;
                        lin++;
                }
        }

        /*
        ** application statistics
        **
        ** WWW: notice that we cause one access ourselves by fetching
        **      the statistical counters
        */
        if (sstat->www.accesses > 1 || fixedhead )
        {
		char format1[8], format2[8], format3[8], format4[8], format5[8];

		if (screen)
               		move(curline, 0);

                printg("WWW | reqs  %s | totKB %s | byt/rq %s | iwork %s |"
                       " bwork %s |",
                        val2valstr(sstat->www.accesses,
                                format1, sizeof(format1)-1, 6, avgval, nsecs),
                        val2valstr(sstat->www.totkbytes,
                                format2, sizeof(format2)-1, 6, avgval, nsecs),
                        val2valstr(sstat->www.accesses == 0 ? 0 :
                                sstat->www.totkbytes*1024/sstat->www.accesses,
                                format3, sizeof(format3)-1, 5, 0, 0),
                        val2valstr(sstat->www.iworkers,
                                format4, sizeof(format4)-1, 6, 0, 0),
                        val2valstr(sstat->www.bworkers,
                                format5, sizeof(format5)-1, 6, 0, 0) );
                if (!screen) 
                {
                        printg("\n");
                }
                curline++;
        }

        /*
        ** if the system is hardly loaded, still CPU-ordering of
        ** processes is most interesting (instead of memory)
        */
        if (highbadness < 70 && *highorderp == MSORTMEM)
                *highorderp = MSORTCPU;

        return curline;
}

/*
** handle all instances of a specific disk-like device
*/
static void
pridisklike(extraparam *ep, struct perdsk *dp, char *lp, char *highorderp,
	int maxlines, unsigned int *highbadp, int *curlinp, int fixedhead,
	regex_t *rep)
{
	int 		lin;
        count_t         busy;
        unsigned int    badness;

        for (ep->perdsk = dp, ep->index=0, lin=0;
	     ep->perdsk[ep->index].name[0] && lin < maxlines; ep->index++)
        {
		if (rep && regexec(rep, ep->perdsk[ep->index].name, 0, NULL, 0))
			continue;	// suppress (not selected)

                ep->iotot =  ep->perdsk[ep->index].nread +
		             ep->perdsk[ep->index].nwrite;

                busy        = (double)(ep->perdsk[ep->index].io_ms *
						100.0 / ep->mstot);

                if (dskbadness)
                        badness = busy * 100 / dskbadness;
                else
                        badness = 0;

                if (*highbadp < badness && (supportflags & IOSTAT) )
                {
                        *highbadp	= badness;
                        *highorderp 	= MSORTDSK;
                }

                if (ep->iotot || fixedhead)
                {
                        move(*curlinp, 0);
                        showsysline(dskline, 0, ep, lp, badness);
                        (*curlinp)++;
                        lin++;
                }
        }
}


/*
** process-level sort functions
*/
int
compcpu(const void *a, const void *b)
{
        struct tstat	*ta = *(struct tstat **)a;
        struct tstat	*tb = *(struct tstat **)b;

        register count_t acpu;
        register count_t bcpu;

        if (ta == NULL)  return 1;
        if (tb == NULL)  return -1;

        acpu = ta->cpu.stime + ta->cpu.utime;
        bcpu = tb->cpu.stime + tb->cpu.utime;

        if (acpu < bcpu)
                return  1;

        if (acpu > bcpu)
                return -1;

        return compmem(a, b);
}

int
compdsk(const void *a, const void *b)
{
	struct tstat	*ta = *(struct tstat **)a;
	struct tstat	*tb = *(struct tstat **)b;

        count_t	adsk;
        count_t bdsk;

	if (ta == NULL)  return  1;
	if (tb == NULL)  return  -1;

	if (ta->dsk.wsz > ta->dsk.cwsz)
		adsk = ta->dsk.rio + ta->dsk.wsz - ta->dsk.cwsz;
	else
		adsk = ta->dsk.rio;

	if (tb->dsk.wsz > tb->dsk.cwsz)
		bdsk = tb->dsk.rio + tb->dsk.wsz - tb->dsk.cwsz;
	else
		bdsk = tb->dsk.rio;

        if (adsk < bdsk)
                return  1;

        if (adsk > bdsk)
                return -1;

        return compcpu(a, b);
}

int
compmem(const void *a, const void *b)
{
	struct tstat	*ta = *(struct tstat **)a;
	struct tstat	*tb = *(struct tstat **)b;

        register count_t amem;
        register count_t bmem;

	if (ta == NULL)   return 1;
	if (tb == NULL)   return -1;

        amem = ta->mem.rmem;
        bmem = tb->mem.rmem;

        if (amem < bmem)
                return  1;

        if (amem > bmem)
                return -1;

        return  0;
}

int
compgpu(const void *a, const void *b)
{
	struct tstat	*ta = *(struct tstat **)a;
	struct tstat	*tb = *(struct tstat **)b;

        register char 	 astate;
        register char 	 bstate;
        register count_t abusy;
        register count_t bbusy;
        register count_t amem;
        register count_t bmem;

	if (ta == NULL)   return 1;
	if (tb == NULL)   return -1;

        astate = ta->gpu.state;
        bstate = tb->gpu.state;
        abusy  = ta->gpu.gpubusy;
        bbusy  = tb->gpu.gpubusy;
        amem   = ta->gpu.memnow;
        bmem   = tb->gpu.memnow;

        if (!astate)		// no GPU usage?
		abusy = amem = -2; 

        if (!bstate)		// no GPU usage?
		bbusy = bmem = -2; 

	if (abusy == -1 || bbusy == -1)
	{
                if (amem < bmem)
                        return  1;

                if (amem > bmem)
                        return -1;

                return  0;
	}
	else
	{
                if (abusy < bbusy)
                        return  1;

                if (abusy > bbusy)
                        return -1;

       		return  0;
	}
}

int
compnet(const void *a, const void *b)
{
	struct tstat	*ta = *(struct tstat **)a;
	struct tstat	*tb = *(struct tstat **)b;

        register count_t anet;
        register count_t bnet;

	if (ta == NULL)   return 1;
	if (tb == NULL)   return -1;

        anet = ta->net.tcpssz + ta->net.tcprsz + ta->net.udpssz + ta->net.udprsz;
        bnet = tb->net.tcpssz + tb->net.tcprsz + tb->net.udpssz + tb->net.udprsz;

        if (anet < bnet)
                return  1;

        if (anet > bnet)
                return -1;

	return compcpu(a, b);
}

int
compusr(const void *a, const void *b)
{
	struct tstat	*ta = *(struct tstat **)a;
	struct tstat	*tb = *(struct tstat **)b;

        register int     uida;
        register int     uidb;

	if (ta == tb)     return 0;
	if (ta == NULL)   return 1;
	if (tb == NULL)   return -1;

        uida = ta->gen.ruid;
        uidb = tb->gen.ruid;

        if (uida > uidb)
                return  1;

        if (uida < uidb)
                return -1;

        return  0;
}

int
compnam(const void *a, const void *b)
{
	struct tstat	*ta = *(struct tstat **)a;
	struct tstat	*tb = *(struct tstat **)b;

        register char *nama;
        register char *namb;

	if (ta == tb)     return 0;
	if (ta == NULL)   return 1;
	if (tb == NULL)   return -1;

        nama = ta->gen.name;
        namb = tb->gen.name;

        return strcmp(nama, namb);
}

int
compcon(const void *a, const void *b)
{
	struct tstat	*ta = *(struct tstat **)a;
	struct tstat	*tb = *(struct tstat **)b;

        register char *utsa;
        register char *utsb;

	if (ta == tb)     return 0;
	if (ta == NULL)   return 1;
	if (tb == NULL)   return -1;

        utsa = ta->gen.utsname;
        utsb = tb->gen.utsname;

       return strcmp(utsa, utsb);
}

/*
** system-level sort functions
*/
int
cpucompar(const void *a, const void *b)
{
        register count_t aidle = ((struct percpu *)a)->itime +
                                 ((struct percpu *)a)->wtime;
        register count_t bidle = ((struct percpu *)b)->itime +
                                 ((struct percpu *)b)->wtime;

        if (aidle < bidle)
                return -1;

        if (aidle > bidle)
                return  1;

	return  0;
}

int
gpucompar(const void *a, const void *b)
{
        register count_t agpuperc  = ((struct pergpu *)a)->gpuperccum;
        register count_t bgpuperc  = ((struct pergpu *)b)->gpuperccum;
        register count_t amemuse   = ((struct pergpu *)a)->memusenow;
        register count_t bmemuse   = ((struct pergpu *)b)->memusenow;

	if (agpuperc == -1 || bgpuperc == -1)
	{
                if (amemuse < bmemuse)
                        return  1;

                if (amemuse > bmemuse)
                        return -1;

		return  0;
	}
	else
	{
                if (agpuperc < bgpuperc)
                        return  1;

                if (agpuperc > bgpuperc)
                        return -1;

		return  0;
	}
}

int
diskcompar(const void *a, const void *b)
{
        register count_t amsio = ((struct perdsk *)a)->io_ms;
        register count_t bmsio = ((struct perdsk *)b)->io_ms;

        if (amsio < bmsio)
                return  1;

        if (amsio > bmsio)
                return -1;

        return  0;
}

int
intfcompar(const void *a, const void *b)
{
        register count_t afactor=0, bfactor=0;

        count_t aspeed         = ((struct perintf *)a)->speed;
        count_t bspeed         = ((struct perintf *)b)->speed;
        char    aduplex        = ((struct perintf *)a)->duplex;
        char    bduplex        = ((struct perintf *)b)->duplex;
        count_t arbyte         = ((struct perintf *)a)->rbyte;
        count_t brbyte         = ((struct perintf *)b)->rbyte;
        count_t asbyte         = ((struct perintf *)a)->sbyte;
        count_t bsbyte         = ((struct perintf *)b)->sbyte;

        /*
        ** if speed of first interface known, calculate busy factor
        */
        if (aspeed)
        {
                if (aduplex)
                        afactor = (arbyte > asbyte ? arbyte : asbyte) 
                                                                * 10 / aspeed;
                else
                        afactor = (arbyte + asbyte)             * 10 / aspeed;
        }

        /*
        ** if speed of second interface known, calculate busy factor
        */
        if (bspeed)
        {
                if (bduplex)
                        bfactor = (brbyte > bsbyte ? brbyte : bsbyte)
                                                                * 10 / bspeed;
                else
                        bfactor = (brbyte + bsbyte)             * 10 / bspeed;
        }

        /*
        ** compare interfaces
        */
        if (aspeed && bspeed)
        {
                if (afactor < bfactor)
                        return  1;

                if (afactor > bfactor)
                        return -1;

                return  0;
        }

        if (!aspeed && !bspeed)
        {
                if ((arbyte + asbyte) < (brbyte + bsbyte))
                        return  1;

                if ((arbyte + asbyte) > (brbyte + bsbyte))
                        return -1;

                return  0;
        }

        if (aspeed)
                return -1;
        else
                return  1;
}


int
ifbcompar(const void *a, const void *b)
{
        count_t atransfer  = ((struct perifb *)a)->rcvb +
                             ((struct perifb *)a)->sndb;
        count_t btransfer  = ((struct perifb *)b)->rcvb +
                             ((struct perifb *)b)->sndb;

	if (atransfer < btransfer)
                return  1;

	if (atransfer > btransfer)
                return -1;

	return  0;
}


int
nfsmcompar(const void *a, const void *b)
{
        const struct pernfsmount *na = a;
        const struct pernfsmount *nb = b;

        register count_t aused = na->bytesread    + na->byteswrite    + 
	                         na->bytesdread   + na->bytesdwrite   +
	                         na->bytestotread + na->bytestotwrite +
                                 na->pagesmread   + na->pagesmwrite;
        register count_t bused = nb->bytesread    + nb->byteswrite    + 
	                         nb->bytesdread   + nb->bytesdwrite   +
	                         nb->bytestotread + nb->bytestotwrite +
                                 nb->pagesmread   + nb->pagesmwrite;

        if (aused < bused)
                return  1;

        if (aused > bused)
                return -1;

        return  0;
}

int
contcompar(const void *a, const void *b)
{
        const struct percontainer *ca = a;
        const struct percontainer *cb = b;

        register count_t aused = ca->system + ca->user + ca->nice;
        register count_t bused = cb->system + cb->user + cb->nice;

        if (aused < bused) 
                return  1;

        if (aused > bused)
                return -1;

        return  0;
}

int
memnumacompar(const void *a, const void *b)
{
        register count_t aused = ((struct mempernuma *)a)->totmem -
                                 ((struct mempernuma *)a)->freemem;
        register count_t bused = ((struct mempernuma *)b)->totmem -
                                 ((struct mempernuma *)b)->freemem;

        if (aused < bused)
                return  1;

        if (aused > bused)
                return -1;

        return  0;
}

int
cpunumacompar(const void *a, const void *b)
{
        register count_t aidle = ((struct cpupernuma *)a)->itime +
                                 ((struct cpupernuma *)a)->wtime;
        register count_t bidle = ((struct cpupernuma *)b)->itime +
                                 ((struct cpupernuma *)b)->wtime;

        if (aidle < bidle)
                return -1;

        if (aidle > bidle)
                return  1;

	return  0;
}

int
llccompar(const void *a, const void *b)
{
        const struct perllc *ca = a;
        const struct perllc *cb = b;

        register count_t aused = ca->mbm_local + ca->mbm_total;
        register count_t bused = cb->mbm_local + cb->mbm_total;

        if (aused < bused)
                return  1;

        if (aused > bused)
                return -1;

        return  0;
}

/*
** handle modifications from the /etc/atoprc and ~/.atoprc file
*/
int
get_posval(char *name, char *val)
{
        int     value = atoi(val);

        if ( !numeric(val))
        {
                fprintf(stderr, "atoprc: %s value %s not a (positive) numeric\n", 
                name, val);
                exit(1);
        }

        if (value < 0)
        {
                fprintf(stderr,
                        "atoprc: %s value %d not positive\n", 
                        name, value);
                exit(1);
        }

        return value;
}

static int
get_perc(char *name, char *val)
{
        int     value = get_posval(name, val);

        if (value < 0 || value > 100)
        {
                fprintf(stderr, "atoprc: %s value %d not in range 0-100\n", 
                        name, value);
                exit(1);
        }

        return value;
}

void
do_cpucritperc(char *name, char *val)
{
        cpubadness = get_perc(name, val);
}

void
do_gpucritperc(char *name, char *val)
{
        gpubadness = get_perc(name, val);
}

void
do_memcritperc(char *name, char *val)
{
        membadness = get_perc(name, val);
}

void
do_swpcritperc(char *name, char *val)
{
        swpbadness = get_perc(name, val);
}

void
do_dskcritperc(char *name, char *val)
{
        dskbadness = get_perc(name, val);
}

void
do_netcritperc(char *name, char *val)
{
        netbadness = get_perc(name, val);
}

void
do_swoutcritsec(char *name, char *val)
{
        pagbadness = get_posval(name, val);
}

void
do_almostcrit(char *name, char *val)
{
        almostcrit = get_perc(name, val);
}

void
do_ownsysprcline(char *name, char *val)
{
        make_sys_prints(sysprcline, MAXITEMS, val, prcsyspdefs, name,
					NULL, NULL);
}

void
do_ownallcpuline(char *name, char *val)
{
        make_sys_prints(allcpuline, MAXITEMS, val, cpusyspdefs, name,
					NULL, NULL);
}

void
do_ownindivcpuline(char *name, char *val)
{
        make_sys_prints(indivcpuline, MAXITEMS, val, cpisyspdefs, name,
					NULL, NULL);
}

void
do_owncplline(char *name, char *val)
{
        make_sys_prints(cplline, MAXITEMS, val, cplsyspdefs, name,
					NULL, NULL);
}

void
do_owngpuline(char *name, char *val)
{
        make_sys_prints(gpuline, MAXITEMS, val, gpusyspdefs, name,
					NULL, NULL);
}

void
do_ownmemline(char *name, char *val)
{
        make_sys_prints(memline1, MAXITEMS, val, memsyspdefs1, name,
					NULL, NULL);
}

void
do_ownswpline(char *name, char *val)
{
        make_sys_prints(swpline, MAXITEMS, val, swpsyspdefs, name,
					NULL, NULL);
}

void
do_ownpagline(char *name, char *val)
{
        make_sys_prints(pagline, MAXITEMS, val, pagsyspdefs, name,
					NULL, NULL);
}

void
do_ownmemnumaline(char *name, char *val)
{
        make_sys_prints(memnumaline, MAXITEMS, val, memnumasyspdefs, name,
					NULL, NULL);
}

void
do_owncpunumaline(char *name, char *val)
{
        make_sys_prints(cpunumaline, MAXITEMS, val, cpunumasyspdefs, name,
					NULL, NULL);
}

void
do_ownllcline(char *name, char *val)
{
        make_sys_prints(llcline, MAXITEMS, val, llcsyspdefs, name,
					NULL, NULL);
}

void
do_owndskline(char *name, char *val)
{
        make_sys_prints(dskline, MAXITEMS, val, dsksyspdefs, name,
					NULL, NULL);
}

void
do_ownnettransportline(char *name, char *val)
{
        make_sys_prints(nettransportline, MAXITEMS, val, nettranssyspdefs, name,
					NULL, NULL);
}

void
do_ownnetnetline(char *name, char *val)
{
        make_sys_prints(netnetline, MAXITEMS, val, netnetsyspdefs, name,
					NULL, NULL);
}

void
do_ownnetinterfaceline(char *name, char *val)
{
        make_sys_prints(netinterfaceline, MAXITEMS, val, netintfsyspdefs, name,
					NULL, NULL);
}

void
do_owninfinibandline(char *name, char *val)
{
        make_sys_prints(infinibandline, MAXITEMS, val, infinisyspdefs, name,
					NULL, NULL);
}

void
do_ownprocline(char *name, char *val)
{
	make_proc_prints(ownprocs, MAXITEMS, val, name);
}
