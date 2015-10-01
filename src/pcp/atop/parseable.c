/*
** ATOP - System & Process Monitor
**
** The program 'atop' offers the possibility to view the activity of
** the system on system-level as well as process-level.
**
** Copyright (C) 2007-2010 Gerlof Langeveld
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

#include "atop.h"
#include "photosyst.h"
#include "photoproc.h"
#include "parseable.h"

void 	print_CPU();
void 	print_cpu();
void 	print_CPL();
void 	print_MEM();
void 	print_SWP();
void 	print_PAG();
void 	print_LVM();
void 	print_MDD();
void 	print_DSK();
void	print_NFM();
void	print_NFC();
void	print_NFS();
void 	print_NET();

void 	print_PRG();
void 	print_PRC();
void 	print_PRM();
void 	print_PRD();
void 	print_PRN();

/*
** table with possible labels and the corresponding
** print-function for parseable output.
*/
struct labeldef {
	char	*label;
	int	valid;
	void	(*prifunc)(char *, struct sstat *, struct tstat *, int);
};

static struct labeldef	labeldef[] = {
	{ "CPU",	0,	print_CPU },
	{ "cpu",	0,	print_cpu },
	{ "CPL",	0,	print_CPL },
	{ "MEM",	0,	print_MEM },
	{ "SWP",	0,	print_SWP },
	{ "PAG",	0,	print_PAG },
	{ "LVM",	0,	print_LVM },
	{ "MDD",	0,	print_MDD },
	{ "DSK",	0,	print_DSK },
	{ "NFM",	0,	print_NFM },
	{ "NFC",	0,	print_NFC },
	{ "NFS",	0,	print_NFS },
	{ "NET",	0,	print_NET },

	{ "PRG",	0,	print_PRG },
	{ "PRC",	0,	print_PRC },
	{ "PRM",	0,	print_PRM },
	{ "PRD",	0,	print_PRD },
	{ "PRN",	0,	print_PRN },
};

static int	numlabels = sizeof labeldef/sizeof(struct labeldef);

/*
** analyse the parse-definition string that has been
** passed as argument with the flag -P
*/
int
parsedef(char *pd)
{
	register int	i;
	char		*p, *ep = pd + strlen(pd);

	/*
	** check if string passed bahind -P is not another flag
	*/
	if (*pd == '-')
	{
		fprintf(stderr, "flag -P should be followed by label list\n");
		return 0;
	}

	/*
	** check list of comma-separated labels 
	*/
	while (pd < ep)
	{
		/*
		** exchange comma by null-byte
		*/
		if ( (p = strchr(pd, ',')) )
			*p = 0;	
		else
			p  = ep-1;

		/*
		** check if the next label exists
		*/
		for (i=0; i < numlabels; i++)
		{
			if ( strcmp(labeldef[i].label, pd) == 0)
			{
				labeldef[i].valid = 1;
				break;
			}
		}

		/*
		** non-existing label has been used
		*/
		if (i == numlabels)
		{
			/*
			** check if special label 'ALL' has been used
			*/
			if ( strcmp("ALL", pd) == 0)
			{
				for (i=0; i < numlabels; i++)
					labeldef[i].valid = 1;
				break;
			}
			else
			{
				fprintf(stderr, "label %s not found\n", pd);
				return 0;
			}
		}

		pd = p+1;
	}

	setbuf(stdout, (char *)0);

	return 1;
}

/*
** produce parseable output for an interval
*/
char
parseout(double timed, double delta,
	 struct sstat *ss, struct tstat *ts, struct tstat **proclist,
	 int ndeviat, int ntask, int nactproc,
         int totproc, int totrun, int totslpi, int totslpu, int totzomb,
	 int nexit, unsigned int noverflow, char flag)
{
	register int	i;
	char		datestr[32], timestr[32], header[256];

	/*
	** print reset-label for sample-values since boot
	*/
	if (flag&RRBOOT)
		printf("RESET\n");

	/*
	** search all labels which are selected before
	*/
	for (i=0; i < numlabels; i++)
	{
		if (labeldef[i].valid)
		{
			/*
			** prepare generic columns
			*/
			convdate(timed, datestr, sizeof(datestr)-1);
			convtime(timed, timestr, sizeof(timestr)-1);

			snprintf(header, sizeof header, "%s %s %ld %s %s %d",
				labeldef[i].label,
				sysname.nodename,
				(long)timed,
				datestr, timestr,
				(int)delta);

			/*
			** call a selected print-function
			*/
			(labeldef[i].prifunc)(header, ss, ts, ndeviat);
		}
	}

	/*
	** print separator
	*/
	printf("SEP\n");

	return '\0';
}

/*
** print functions for system-level statistics
*/
void
calc_freqscale(count_t maxfreq, count_t cnt, count_t ticks, 
               count_t *freq, int *freqperc)
{
        // if ticks != 0, do full calcs
        if (maxfreq && ticks) 
        {
            *freq=cnt/ticks;
            *freqperc=100* *freq / maxfreq;
        } 
        else if (maxfreq)   // max frequency is known so % can be calculated
        {
            *freq=cnt;
            *freqperc=100*cnt/maxfreq;
        }
        else if (cnt)       // no max known, set % to 100
        {
            *freq=cnt;
            *freqperc=100;
        }
        else                // nothing is known: set freq to 0, % to 100
        {
            *freq=0;
            *freqperc=100;
        }
}


void
print_CPU(char *hp, struct sstat *ss, struct tstat *ps, int nact)
{
        count_t maxfreq=0;
        count_t cnt=0;
        count_t ticks=0;
        count_t freq;
        int freqperc;
        int i;

        // calculate average clock frequency
	for (i=0; i < ss->cpu.nrcpu; i++)
        {
                cnt    += ss->cpu.cpu[i].freqcnt.cnt;
                ticks  += ss->cpu.cpu[i].freqcnt.ticks;
        }
        maxfreq = ss->cpu.cpu[0].freqcnt.maxfreq;
        calc_freqscale(maxfreq, cnt, ticks, &freq, &freqperc);

	printf("%s %u %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %d\n",
			hp,
			hertz,
	        	ss->cpu.nrcpu,
	        	ss->cpu.all.stime,
        		ss->cpu.all.utime,
        		ss->cpu.all.ntime,
        		ss->cpu.all.itime,
        		ss->cpu.all.wtime,
        		ss->cpu.all.Itime,
        		ss->cpu.all.Stime,
        		ss->cpu.all.steal,
        		ss->cpu.all.guest,
                        freq,
                        freqperc
                        );
}

void
print_cpu(char *hp, struct sstat *ss, struct tstat *ps, int nact)
{
	register int i;
        count_t maxfreq=0;
        count_t cnt=0;
        count_t ticks=0;
        count_t freq;
        int freqperc;

	for (i=0; i < ss->cpu.nrcpu; i++)
	{
                cnt    = ss->cpu.cpu[i].freqcnt.cnt;
                ticks  = ss->cpu.cpu[i].freqcnt.ticks;
                maxfreq= ss->cpu.cpu[0].freqcnt.maxfreq;

                calc_freqscale(maxfreq, cnt, ticks, &freq, &freqperc);

		printf("%s %u %d %lld %lld %lld "
		       "%lld %lld %lld %lld %lld %lld %lld %d\n",
			hp, hertz, i,
	        	ss->cpu.cpu[i].stime,
        		ss->cpu.cpu[i].utime,
        		ss->cpu.cpu[i].ntime,
        		ss->cpu.cpu[i].itime,
        		ss->cpu.cpu[i].wtime,
        		ss->cpu.cpu[i].Itime,
        		ss->cpu.cpu[i].Stime,
        		ss->cpu.cpu[i].steal,
        		ss->cpu.cpu[i].guest,
                        freq,
                        freqperc);
	}
}

void
print_CPL(char *hp, struct sstat *ss, struct tstat *ps, int nact)
{
	printf("%s %lld %.2f %.2f %.2f %lld %lld\n",
			hp,
	        	ss->cpu.nrcpu,
	        	ss->cpu.lavg1,
	        	ss->cpu.lavg5,
	        	ss->cpu.lavg15,
	        	ss->cpu.csw,
	        	ss->cpu.devint);
}

void
print_MEM(char *hp, struct sstat *ss, struct tstat *ps, int nact)
{
	printf(	"%s %u %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld\n",
			hp,
			pagesize,
			ss->mem.physmem,
			ss->mem.freemem,
			ss->mem.cachemem,
			ss->mem.buffermem,
			ss->mem.slabmem,
			ss->mem.cachedrt,
			ss->mem.slabreclaim,
        		ss->mem.vmwballoon,
        		ss->mem.shmem,
        		ss->mem.shmrss,
        		ss->mem.shmswp,
        		ss->mem.hugepagesz,
        		ss->mem.tothugepage,
        		ss->mem.freehugepage);
}

void
print_SWP(char *hp, struct sstat *ss, struct tstat *ps, int nact)
{
	printf(	"%s %u %lld %lld %lld %lld %lld\n",
			hp,
			pagesize,
			ss->mem.totswap,
			ss->mem.freeswap,
			(long long)0,
			ss->mem.committed,
			ss->mem.commitlim);
}

void
print_PAG(char *hp, struct sstat *ss, struct tstat *ps, int nact)
{
	printf(	"%s %u %lld %lld %lld %lld %lld\n",
			hp,
			pagesize,
			ss->mem.pgscans,
			ss->mem.allocstall,
			(long long)0,
			ss->mem.swins,
			ss->mem.swouts);
}

void
print_LVM(char *hp, struct sstat *ss, struct tstat *ps, int nact)
{
	register int	i;

        for (i=0; ss->dsk.lvm[i].name[0]; i++)
	{
		printf(	"%s %s %lld %lld %lld %lld %lld\n",
			hp,
			ss->dsk.lvm[i].name,
			ss->dsk.lvm[i].io_ms,
			ss->dsk.lvm[i].nread,
			ss->dsk.lvm[i].nrsect,
			ss->dsk.lvm[i].nwrite,
			ss->dsk.lvm[i].nwsect);
	}
}

void
print_MDD(char *hp, struct sstat *ss, struct tstat *ps, int nact)
{
	register int	i;

        for (i=0; ss->dsk.mdd[i].name[0]; i++)
	{
		printf(	"%s %s %lld %lld %lld %lld %lld\n",
			hp,
			ss->dsk.mdd[i].name,
			ss->dsk.mdd[i].io_ms,
			ss->dsk.mdd[i].nread,
			ss->dsk.mdd[i].nrsect,
			ss->dsk.mdd[i].nwrite,
			ss->dsk.mdd[i].nwsect);
	}
}

void
print_NFM(char *hp, struct sstat *ss, struct tstat *ps, int nact)
{
	register int	i;

        for (i=0; i < ss->nfs.nrmounts; i++)
	{
		printf("%s %s %lld %lld %lld %lld %lld %lld %lld %lld\n",
			hp,
			ss->nfs.nfsmnt[i].mountdev,
			ss->nfs.nfsmnt[i].bytestotread,
			ss->nfs.nfsmnt[i].bytestotread,
			ss->nfs.nfsmnt[i].bytesread,
			ss->nfs.nfsmnt[i].byteswrite,
			ss->nfs.nfsmnt[i].bytesdread,
			ss->nfs.nfsmnt[i].bytesdwrite,
			ss->nfs.nfsmnt[i].pagesmread,
			ss->nfs.nfsmnt[i].pagesmwrite);
	}
}

void
print_NFC(char *hp, struct sstat *ss, struct tstat *ps, int nact)
{
	printf(	"%s %lld %lld %lld %lld %lld\n",
			hp,
			ss->nfs.client.rpccnt,
			ss->nfs.client.rpcread,
			ss->nfs.client.rpcwrite,
			ss->nfs.client.rpcretrans,
			ss->nfs.client.rpcautrefresh);
}

void
print_NFS(char *hp, struct sstat *ss, struct tstat *ps, int nact)
{
	printf(	"%s %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld "
	        "%lld %lld %lld %lld %lld\n",
			hp,
			ss->nfs.server.rpccnt,
			ss->nfs.client.rpcread,
			ss->nfs.client.rpcwrite,
			ss->nfs.server.nrbytes,
			ss->nfs.server.nwbytes,
			ss->nfs.server.rpcbadfmt,
			ss->nfs.server.rpcbadaut,
			ss->nfs.server.rpcbadcln,
			ss->nfs.server.netcnt,
			ss->nfs.server.nettcpcnt,
			ss->nfs.server.netudpcnt,
			ss->nfs.server.nettcpcon,
			ss->nfs.server.rchits,
			ss->nfs.server.rcmiss,
			ss->nfs.server.rcnoca);
}

void
print_DSK(char *hp, struct sstat *ss, struct tstat *ps, int nact)
{
	register int	i;

        for (i=0; ss->dsk.dsk[i].name[0]; i++)
	{
		printf(	"%s %s %lld %lld %lld %lld %lld\n",
			hp,
			ss->dsk.dsk[i].name,
			ss->dsk.dsk[i].io_ms,
			ss->dsk.dsk[i].nread,
			ss->dsk.dsk[i].nrsect,
			ss->dsk.dsk[i].nwrite,
			ss->dsk.dsk[i].nwsect);
	}
}

void
print_NET(char *hp, struct sstat *ss, struct tstat *ps, int nact)
{
	register int 	i;

	printf(	"%s %s %lld %lld %lld %lld %lld %lld %lld %lld\n",
			hp,
			"upper",
        		ss->net.tcp.InSegs,
   		     	ss->net.tcp.OutSegs,
       		 	ss->net.udpv4.InDatagrams +
				ss->net.udpv6.Udp6InDatagrams,
       		 	ss->net.udpv4.OutDatagrams +
				ss->net.udpv6.Udp6OutDatagrams,
       		 	ss->net.ipv4.InReceives  +
				ss->net.ipv6.Ip6InReceives,
       		 	ss->net.ipv4.OutRequests +
				ss->net.ipv6.Ip6OutRequests,
       		 	ss->net.ipv4.InDelivers +
       		 		ss->net.ipv6.Ip6InDelivers,
       		 	ss->net.ipv4.ForwDatagrams +
       		 		ss->net.ipv6.Ip6OutForwDatagrams);

	for (i=0; ss->intf.intf[i].name[0]; i++)
	{
		printf(	"%s %s %lld %lld %lld %lld %ld %d\n",
			hp,
			ss->intf.intf[i].name,
			ss->intf.intf[i].rpack,
			ss->intf.intf[i].rbyte,
			ss->intf.intf[i].spack,
			ss->intf.intf[i].sbyte,
			ss->intf.intf[i].speed,
			ss->intf.intf[i].duplex);
	}
}

/*
** print functions for process-level statistics
*/
void
print_PRG(char *hp, struct sstat *ss, struct tstat *ps, int nact)
{
	register int i;

	for (i=0; i < nact; i++, ps++)
	{
		printf("%s %d (%s) %c %d %d %d %d %d %ld (%s) %d %d %d %d "
 		       "%d %d %d %d %d %d %ld %c %d %d\n",
				hp,
				ps->gen.pid,
				ps->gen.name,
				ps->gen.state,
				ps->gen.ruid,
				ps->gen.rgid,
				ps->gen.tgid,
				ps->gen.nthr,
				ps->gen.excode,
				ps->gen.btime,
				ps->gen.cmdline,
				ps->gen.ppid,
				ps->gen.nthrrun,
				ps->gen.nthrslpi,
				ps->gen.nthrslpu,
				ps->gen.euid,
				ps->gen.egid,
				ps->gen.suid,
				ps->gen.sgid,
				ps->gen.fsuid,
				ps->gen.fsgid,
				ps->gen.elaps,
				ps->gen.isproc ? 'y':'n',
				ps->gen.vpid,
				ps->gen.ctid);
	}
}

void
print_PRC(char *hp, struct sstat *ss, struct tstat *ps, int nact)
{
	register int i;

	for (i=0; i < nact; i++, ps++)
	{
		printf("%s %d (%s) %c %u %lld %lld %d %d %d %d %d %d %d %c\n",
				hp,
				ps->gen.pid,
				ps->gen.name,
				ps->gen.state,
				hertz,
				ps->cpu.utime,
				ps->cpu.stime,
				ps->cpu.nice,
				ps->cpu.prio,
				ps->cpu.rtprio,
				ps->cpu.policy,
				ps->cpu.curcpu,
				0, /* ps->cpu.sleepavg - not in kernel now */
				ps->gen.tgid,
				ps->gen.isproc ? 'y':'n');
	}
}

void
print_PRM(char *hp, struct sstat *ss, struct tstat *ps, int nact)
{
	register int i;

	for (i=0; i < nact; i++, ps++)
	{
		printf("%s %d (%s) %c %u %lld %lld %lld %lld %lld %lld "
		       "%lld %lld %lld %lld %lld %d %c %lld\n",
				hp,
				ps->gen.pid,
				ps->gen.name,
				ps->gen.state,
				pagesize,
				ps->mem.vmem,
				ps->mem.rmem,
				ps->mem.vexec,
				ps->mem.vgrow,
				ps->mem.rgrow,
				ps->mem.minflt,
				ps->mem.majflt,
				ps->mem.vlibs,
				ps->mem.vdata,
				ps->mem.vstack,
				ps->mem.vswap,
				ps->gen.tgid,
				ps->gen.isproc ? 'y':'n',
				ps->mem.pmem == (unsigned long long)-1LL ?
								0:ps->mem.pmem);
	}
}

void
print_PRD(char *hp, struct sstat *ss, struct tstat *ps, int nact)
{
	register int i;

	for (i=0; i < nact; i++, ps++)
	{
		printf("%s %d (%s) %c %c %c %lld %lld %lld %lld %lld %d n %c\n",
				hp,
				ps->gen.pid,
				ps->gen.name,
				ps->gen.state,
				'n',
				supportflags & IOSTAT ? 'y' : 'n',
				ps->dsk.rio, ps->dsk.rsz,
				ps->dsk.wio, ps->dsk.wsz, ps->dsk.cwsz,
				ps->gen.tgid,
				ps->gen.isproc ? 'y':'n');
	}
}

void
print_PRN(char *hp, struct sstat *ss, struct tstat *ps, int nact)
{
	register int i;

	for (i=0; i < nact; i++, ps++)
	{
		printf("%s %d (%s) %c %c %lld %lld %lld %lld %lld %lld "
		       "%lld %lld %d %d %d %c\n",
				hp,
				ps->gen.pid,
				ps->gen.name,
				ps->gen.state,
				supportflags & NETATOP ? 'y' : 'n',
				ps->net.tcpsnd, ps->net.tcpssz,
				ps->net.tcprcv, ps->net.tcprsz,
				ps->net.udpsnd, ps->net.udpssz,
				ps->net.udprcv, ps->net.udprsz,
				0,              0,
				ps->gen.tgid,   ps->gen.isproc ? 'y':'n');
	}
}
