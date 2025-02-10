/*
** ATOP - System & Process Monitor
**
** The program 'atop' offers the possibility to view the activity of
** the system on system-level as well as process-level.
**
** Copyright (C) 2007-2021 Gerlof Langeveld
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

void   print_CPU(char *, struct sstat *, struct tstat *, int);
void   print_cpu(char *, struct sstat *, struct tstat *, int);
void   print_CPL(char *, struct sstat *, struct tstat *, int);
void   print_GPU(char *, struct sstat *, struct tstat *, int);
void   print_MEM(char *, struct sstat *, struct tstat *, int);
void   print_SWP(char *, struct sstat *, struct tstat *, int);
void   print_PAG(char *, struct sstat *, struct tstat *, int);
void   print_PSI(char *, struct sstat *, struct tstat *, int);
void   print_LVM(char *, struct sstat *, struct tstat *, int);
void   print_MDD(char *, struct sstat *, struct tstat *, int);
void   print_DSK(char *, struct sstat *, struct tstat *, int);
void   print_NFM(char *, struct sstat *, struct tstat *, int);
void   print_NFC(char *, struct sstat *, struct tstat *, int);
void   print_NFS(char *, struct sstat *, struct tstat *, int);
void   print_NET(char *, struct sstat *, struct tstat *, int);
void   print_IFB(char *, struct sstat *, struct tstat *, int);
void   print_NUM(char *, struct sstat *, struct tstat *, int);
void   print_NUC(char *, struct sstat *, struct tstat *, int);
void   print_LLC(char *, struct sstat *, struct tstat *, int);

void   print_PRG(char *, struct sstat *, struct tstat *, int);
void   print_PRC(char *, struct sstat *, struct tstat *, int);
void   print_PRM(char *, struct sstat *, struct tstat *, int);
void   print_PRD(char *, struct sstat *, struct tstat *, int);
void   print_PRN(char *, struct sstat *, struct tstat *, int);
void   print_PRE(char *, struct sstat *, struct tstat *, int);

static void calc_freqscale(count_t, count_t, count_t, count_t *, int *);
static char *spaceformat(char *, char *);
static int  cgroupv2max(int, int);

/*
** table with possible labels and the corresponding
** print-function for parsable output
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
	{ "GPU",	0,	print_GPU },
	{ "MEM",	0,	print_MEM },
	{ "SWP",	0,	print_SWP },
	{ "PAG",	0,	print_PAG },
	{ "PSI",	0,	print_PSI },
	{ "LVM",	0,	print_LVM },
	{ "MDD",	0,	print_MDD },
	{ "DSK",	0,	print_DSK },
	{ "NFM",	0,	print_NFM },
	{ "NFC",	0,	print_NFC },
	{ "NFS",	0,	print_NFS },
	{ "NET",	0,	print_NET },
	{ "IFB",	0,	print_IFB },
	{ "NUM",	0,	print_NUM },
	{ "NUC",	0,	print_NUC },
	{ "LLC",	0,	print_LLC },

	{ "PRG",	0,	print_PRG },
	{ "PRC",	0,	print_PRC },
	{ "PRM",	0,	print_PRM },
	{ "PRD",	0,	print_PRD },
	{ "PRN",	0,	print_PRN },
	{ "PRE",	0,	print_PRE },
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
** produce parsable output for an interval
*/
char
parseout(double timed, double delta,
	 struct devtstat *devtstat, struct sstat *sstat,
	 int nexit, unsigned int noverflow, int flag)
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

			pmsprintf(header, sizeof header, "%s %s %ld %s %s %d",
				labeldef[i].label,
				sysname.nodename,
				(long)timed,
				datestr, timestr,
				(int)delta);

			/*
			** call a selected print-function
			*/
			(labeldef[i].prifunc)(header, sstat,
				devtstat->taskall, devtstat->ntaskall);
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
static void
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

	if (ss->cpu.all.instr == 1)
	{
        	ss->cpu.all.instr = 0;
        	ss->cpu.all.cycle = 0;
	}

	printf("%s %u %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %d %lld %lld\n",
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
                        freqperc,
        		ss->cpu.all.instr,
        		ss->cpu.all.cycle
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
		       "%lld %lld %lld %lld %lld %lld %lld %d %lld %lld\n",
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
                        freqperc,
        		ss->cpu.cpu[i].instr,
        		ss->cpu.cpu[i].cycle
			);
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
print_GPU(char *hp, struct sstat *ss, struct tstat *ps, int nact)
{
	int	i;

	for (i=0; i < ss->gpu.nrgpus; i++)
	{
		printf("%s %d %s %s %d %d %lld %lld %lld %lld %lld %lld\n",
			hp, i,
	        	ss->gpu.gpu[i].busid,
	        	ss->gpu.gpu[i].type,
	        	ss->gpu.gpu[i].gpupercnow,
	        	ss->gpu.gpu[i].mempercnow,
	        	ss->gpu.gpu[i].memtotnow,
	        	ss->gpu.gpu[i].memusenow,
	        	ss->gpu.gpu[i].samples,
	        	ss->gpu.gpu[i].gpuperccum,
	        	ss->gpu.gpu[i].memperccum,
	        	ss->gpu.gpu[i].memusecum);
	}
}

void
print_MEM(char *hp, struct sstat *ss, struct tstat *ps, int nact)
{
	printf(	"%s %u %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld "
   		"%lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld "
		"%lld %lld %lld\n",
			hp,
			pagesize,
			ss->mem.physmem,
			ss->mem.freemem,
			ss->mem.cachemem,
			ss->mem.buffermem,
			ss->mem.slabmem,
			ss->mem.cachedrt,
			ss->mem.slabreclaim,
        		ss->mem.vmwballoon != -1 ? ss->mem.vmwballoon : 0,
        		ss->mem.shmem,
        		ss->mem.shmrss,
        		ss->mem.shmswp,
        		ss->mem.shugepagesz,
        		ss->mem.stothugepage,
        		ss->mem.sfreehugepage,
        		ss->mem.zfsarcsize != -1 ? ss->mem.zfsarcsize : 0,
        		ss->mem.ksmsharing != -1 ? ss->mem.ksmsharing : 0,
        		ss->mem.ksmshared  != -1 ? ss->mem.ksmshared  : 0,
			ss->mem.tcpsock,
			ss->mem.udpsock,
   			ss->mem.pagetables,
        		ss->mem.lhugepagesz,
        		ss->mem.ltothugepage,
        		ss->mem.lfreehugepage,
			ss->mem.availablemem,
			ss->mem.anonhugepage);
}

void
print_SWP(char *hp, struct sstat *ss, struct tstat *ps, int nact)
{
	printf(	"%s %u %lld %lld %lld %lld %lld %lld %lld %lld\n",
			hp,
			pagesize,
			ss->mem.totswap,
			ss->mem.freeswap,
			ss->mem.swapcached,
			ss->mem.committed,
			ss->mem.commitlim,
        		ss->mem.swapcached,
        		ss->mem.zswapped,
        		ss->mem.zswap);
}

void
print_PAG(char *hp, struct sstat *ss, struct tstat *ps, int nact)
{
	printf("%s %u %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld "
	       "%lld %lld\n",
			hp,
			pagesize,
			ss->mem.pgscans,
			ss->mem.allocstall,
			(long long)0,
			ss->mem.swins,
			ss->mem.swouts,
			ss->mem.oomkills,
			ss->mem.compactstall,
			ss->mem.pgmigrate,
			ss->mem.numamigrate,
			ss->mem.pgins,
			ss->mem.pgouts,
			ss->mem.zswins,
			ss->mem.zswouts);
}

void
print_PSI(char *hp, struct sstat *ss, struct tstat *ps, int nact)
{
	printf("%s %c %.1f %.1f %.1f %llu %.1f %.1f %.1f %llu "
	       "%.1f %.1f %.1f %llu %.1f %.1f %.1f %llu %.1f %.1f %.1f %llu\n",
		hp, ss->psi.present ? 'y' : 'n',
                ss->psi.cpusome.avg10, ss->psi.cpusome.avg60,
                ss->psi.cpusome.avg300, ss->psi.cpusome.total,
                ss->psi.memsome.avg10, ss->psi.memsome.avg60,
                ss->psi.memsome.avg300, ss->psi.memsome.total,
                ss->psi.memfull.avg10, ss->psi.memfull.avg60,
                ss->psi.memfull.avg300, ss->psi.memfull.total,
                ss->psi.iosome.avg10, ss->psi.iosome.avg60,
                ss->psi.iosome.avg300, ss->psi.iosome.total,
                ss->psi.iofull.avg10, ss->psi.iofull.avg60,
                ss->psi.iofull.avg300, ss->psi.iofull.total);
}

void
print_LVM(char *hp, struct sstat *ss, struct tstat *ps, int nact)
{
	register int	i;

        for (i=0; ss->dsk.lvm[i].name[0]; i++)
	{
		printf(	"%s %s %lld %lld %lld %lld %lld %lld %lld %lld %.2f\n",
			hp,
			ss->dsk.lvm[i].name,
			ss->dsk.lvm[i].io_ms,
			ss->dsk.lvm[i].nread,
			ss->dsk.lvm[i].nrsect,
			ss->dsk.lvm[i].nwrite,
			ss->dsk.lvm[i].nwsect,
			ss->dsk.lvm[i].ndisc,
			ss->dsk.lvm[i].ndsect,
			ss->dsk.lvm[i].inflight,
			ss->dsk.lvm[i].io_ms > 0 ?
			  (double)ss->dsk.lvm[i].avque/ss->dsk.lvm[i].io_ms :
			  0.0);
	}
}

void
print_MDD(char *hp, struct sstat *ss, struct tstat *ps, int nact)
{
	register int	i;

        for (i=0; ss->dsk.mdd[i].name[0]; i++)
	{
		printf(	"%s %s %lld %lld %lld %lld %lld %lld %lld %lld %.2f\n",
			hp,
			ss->dsk.mdd[i].name,
			ss->dsk.mdd[i].io_ms,
			ss->dsk.mdd[i].nread,
			ss->dsk.mdd[i].nrsect,
			ss->dsk.mdd[i].nwrite,
			ss->dsk.mdd[i].nwsect,
			ss->dsk.mdd[i].ndisc,
			ss->dsk.mdd[i].ndsect,
			ss->dsk.mdd[i].inflight,
			ss->dsk.mdd[i].io_ms > 0 ?
			  (double)ss->dsk.mdd[i].avque/ss->dsk.mdd[i].io_ms :
			  0.0);
	}
}

void
print_DSK(char *hp, struct sstat *ss, struct tstat *ps, int nact)
{
	register int	i;

        for (i=0; ss->dsk.dsk[i].name[0]; i++)
	{
		printf(	"%s %s %lld %lld %lld %lld %lld %lld %lld %lld %.2f\n",
			hp,
			ss->dsk.dsk[i].name,
			ss->dsk.dsk[i].io_ms,
			ss->dsk.dsk[i].nread,
			ss->dsk.dsk[i].nrsect,
			ss->dsk.dsk[i].nwrite,
			ss->dsk.dsk[i].nwsect,
			ss->dsk.dsk[i].ndisc,
			ss->dsk.dsk[i].ndsect,
			ss->dsk.dsk[i].inflight,
			ss->dsk.dsk[i].io_ms > 0 ?
			  (double)ss->dsk.dsk[i].avque/ss->dsk.dsk[i].io_ms :
			  0.0);
	}
}

void
print_NFM(char *hp, struct sstat *ss, struct tstat *ps, int nact)
{
	register int	i;

        for (i=0; i < ss->nfs.nfsmounts.nrmounts; i++)
	{
		printf("%s %s %lld %lld %lld %lld %lld %lld %lld %lld\n",
			hp,
			ss->nfs.nfsmounts.nfsmnt[i].mountdev,
			ss->nfs.nfsmounts.nfsmnt[i].bytestotread,
			ss->nfs.nfsmounts.nfsmnt[i].bytestotwrite,
			ss->nfs.nfsmounts.nfsmnt[i].bytesread,
			ss->nfs.nfsmounts.nfsmnt[i].byteswrite,
			ss->nfs.nfsmounts.nfsmnt[i].bytesdread,
			ss->nfs.nfsmounts.nfsmnt[i].bytesdwrite,
			ss->nfs.nfsmounts.nfsmnt[i].pagesmread,
			ss->nfs.nfsmounts.nfsmnt[i].pagesmwrite);
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
			ss->nfs.server.rpcread,
			ss->nfs.server.rpcwrite,
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
print_NET(char *hp, struct sstat *ss, struct tstat *ps, int nact)
{
	register int 	i;

	printf(	"%s %s %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld\n",
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
       		 		ss->net.ipv6.Ip6OutForwDatagrams,
       		 	ss->net.udpv4.InErrors +
				ss->net.udpv6.Udp6InErrors,
       		 	ss->net.udpv4.NoPorts +
				ss->net.udpv6.Udp6NoPorts,
			ss->net.tcp.ActiveOpens,
			ss->net.tcp.PassiveOpens,
			ss->net.tcp.CurrEstab,
			ss->net.tcp.RetransSegs,
			ss->net.tcp.InErrs,
			ss->net.tcp.OutRsts,
			ss->net.tcp.InCsumErrors);

	for (i=0; ss->intf.intf[i].name[0]; i++)
	{
		printf(	"%s %s %lld %lld %lld %lld %lld %d\n",
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

void
print_IFB(char *hp, struct sstat *ss, struct tstat *ps, int nact)
{
	register int 	i;

	for (i=0; i < ss->ifb.nrports; i++)
	{
		printf(	"%s %s %hd %hd %lld %lld %lld %lld %lld\n",
			hp,
			ss->ifb.ifb[i].ibname,
			ss->ifb.ifb[i].portnr,
			ss->ifb.ifb[i].lanes,
			ss->ifb.ifb[i].rate,
			ss->ifb.ifb[i].rcvb,
			ss->ifb.ifb[i].sndb,
			ss->ifb.ifb[i].rcvp,
			ss->ifb.ifb[i].sndp);
	}
}

void
print_NUM(char *hp, struct sstat *ss, struct tstat *ps, int nact)
{
	register int 	i;

	for (i=0; i < ss->memnuma.nrnuma; i++)
	{
		printf(	"%s %d %u %.0f %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld\n",
			hp, ss->memnuma.numa[i].numanr,
			pagesize,
			ss->memnuma.numa[i].frag * 100.0,
			ss->memnuma.numa[i].totmem,
			ss->memnuma.numa[i].freemem,
			ss->memnuma.numa[i].active,
			ss->memnuma.numa[i].inactive,
			ss->memnuma.numa[i].filepage,
			ss->memnuma.numa[i].dirtymem,
			ss->memnuma.numa[i].slabmem,
			ss->memnuma.numa[i].slabreclaim,
			ss->memnuma.numa[i].shmem,
			ss->memnuma.numa[i].tothp,
			ss->memnuma.numa[i].freehp);
	}
}

void
print_NUC(char *hp, struct sstat *ss, struct tstat *ps, int nact)
{
	register int 	i;

	for (i=0; i < ss->cpunuma.nrnuma; i++)
	{
		printf(	"%s %d %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld\n",
			hp, ss->cpunuma.numa[i].numanr,
	        	ss->cpunuma.numa[i].nrcpu,
	        	ss->cpunuma.numa[i].stime,
        		ss->cpunuma.numa[i].utime,
        		ss->cpunuma.numa[i].ntime,
        		ss->cpunuma.numa[i].itime,
        		ss->cpunuma.numa[i].wtime,
        		ss->cpunuma.numa[i].Itime,
        		ss->cpunuma.numa[i].Stime,
        		ss->cpunuma.numa[i].steal,
        		ss->cpunuma.numa[i].guest);
	}
}

void
print_LLC(char *hp, struct sstat *ss, struct tstat *ps, int nact)
{
	register int 	i;

	for (i=0; i < ss->llc.nrllcs; i++)
	{
	        printf( "%s LLC%02d %3.1f%% %lld %lld\n",
			hp,
			ss->llc.perllc[i].id,
			ss->llc.perllc[i].occupancy * 100,
			ss->llc.perllc[i].mbm_total,
			ss->llc.perllc[i].mbm_local);
	}
}

/*
** print functions for process-level statistics
*/
void
print_PRG(char *hp, struct sstat *ss, struct tstat *ps, int nact)
{
	register int	i, exitcode;
	char		namout[PNAMLEN+1+2], cmdout[CMDLEN+1+2],
			pathout[CGRLEN+1];

	for (i=0; i < nact; i++, ps++)
	{
		if (ps->gen.excode & 0xff) // killed by signal?
			exitcode = (ps->gen.excode & 0x7f) + 256;
		else
			exitcode = (ps->gen.excode >>   8) & 0xff;

		printf("%s %d %s %c %d %d %d %d %d %ld %s %d %d %d %d "
 		       "%d %d %d %d %d %d %ld %c %d %d %s %c %s %ld %d\n",
			hp,
			ps->gen.pid,
			spaceformat(ps->gen.name, namout),
			ps->gen.state,
			ps->gen.ruid,
			ps->gen.rgid,
			ps->gen.tgid,
			ps->gen.nthr,
			exitcode,
			(long)ps->gen.btime,
			spaceformat(ps->gen.cmdline, cmdout),
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
			(long)ps->gen.elaps,
			ps->gen.isproc ? 'y':'n',
			ps->gen.vpid,
			ps->gen.ctid,
			ps->gen.utsname[0] ? ps->gen.utsname:"-",
        		ps->gen.excode & ~(INT_MAX) ? 'N' : '-',
			spaceformat(ps->gen.cgpath, pathout),
			ps->gen.state == 'E' ?
			    ps->gen.btime + ps->gen.elaps/hertz : 0,
			ps->gen.nthridle);
	}
}

void
print_PRC(char *hp, struct sstat *ss, struct tstat *ps, int nact)
{
	register int	i;
	char		namout[PNAMLEN+1+2], wchanout[20];

	for (i=0; i < nact; i++, ps++)
	{
		printf("%s %d %s %c %u %lld %lld %d %d %d %d %d %d %d %c "
		       "%llu %s %llu %d %d %llu %llu\n",
			hp,
			ps->gen.pid,
			spaceformat(ps->gen.name, namout),
			ps->gen.state,
			hertz,
			ps->cpu.utime,
			ps->cpu.stime,
			ps->cpu.nice,
			ps->cpu.prio,
			ps->cpu.rtprio,
			ps->cpu.policy,
			ps->cpu.curcpu,
			ps->cpu.sleepavg,
			ps->gen.tgid,
			ps->gen.isproc ? 'y':'n',
			ps->cpu.rundelay,
			spaceformat(ps->cpu.wchan, wchanout),
			ps->cpu.blkdelay,
			cgroupv2max(ps->gen.isproc, ps->cpu.cgcpumax),
			cgroupv2max(ps->gen.isproc, ps->cpu.cgcpumaxr),
			ps->cpu.nvcsw,
			ps->cpu.nivcsw);
	}
}

void
print_PRM(char *hp, struct sstat *ss, struct tstat *ps, int nact)
{
	register int 	i;
	char		namout[PNAMLEN+1+2];

	for (i=0; i < nact; i++, ps++)
	{
		printf("%s %d %s %c %u %lld %lld %lld %lld %lld %lld "
		       "%lld %lld %lld %lld %lld %d %c %lld %lld %d %d %d %d\n",
			hp,
			ps->gen.pid,
			spaceformat(ps->gen.name, namout),
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
							0:ps->mem.pmem,
			ps->mem.vlock,
			cgroupv2max(ps->gen.isproc, ps->mem.cgmemmax),
			cgroupv2max(ps->gen.isproc, ps->mem.cgmemmaxr),
			cgroupv2max(ps->gen.isproc, ps->mem.cgswpmax),
			cgroupv2max(ps->gen.isproc, ps->mem.cgswpmaxr));
	}
}

void
print_PRD(char *hp, struct sstat *ss, struct tstat *ps, int nact)
{
	register int	i;
	char		namout[PNAMLEN+1+2];

	for (i=0; i < nact; i++, ps++)
	{
		printf("%s %d %s %c %c %c %lld %lld %lld %lld %lld %d n %c\n",
			hp,
			ps->gen.pid,
			spaceformat(ps->gen.name, namout),
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
	register int	i;
	char		namout[PNAMLEN+1+2];

	for (i=0; i < nact; i++, ps++)
	{
		printf("%s %d %s %c %c %lld %lld %lld %lld %lld %lld "
		       "%lld %lld %d %d %d %c\n",
			hp,
			ps->gen.pid,
			spaceformat(ps->gen.name, namout),
			ps->gen.state,
			supportflags & NETATOP || supportflags & NETATOP ? 'y' : 'n',
			ps->net.tcpsnd, ps->net.tcpssz,
			ps->net.tcprcv, ps->net.tcprsz,
			ps->net.udpsnd, ps->net.udpssz,
			ps->net.udprcv, ps->net.udprsz,
			0,              0,
			ps->gen.tgid,   ps->gen.isproc ? 'y':'n');
	}
}

void
print_PRE(char *hp, struct sstat *ss, struct tstat *ps, int nact)
{
	register int	i;
	char		namout[PNAMLEN+1+2];

	for (i=0; i < nact; i++, ps++)
	{
		printf("%s %d %s %c %c %d %x %d %d %lld %lld %lld\n",
			hp,
			ps->gen.pid,
			spaceformat(ps->gen.name, namout),
			ps->gen.state,
			ps->gpu.state == '\0' ? 'N':ps->gpu.state,
			ps->gpu.nrgpus,
			ps->gpu.gpulist,
			ps->gpu.gpubusy,
			ps->gpu.membusy,
			ps->gpu.memnow,
			ps->gpu.memcum,
			ps->gpu.sample);
	}
}


/*
** Strings, like command name, might contain spaces
** and will be represented for that reason surrounded
** by parenthesis. However, this is difficult to parse
** by other tools, so the option -Z might have been
** specified to exchange all spaces by underscores while
** omitting the parenthesis in that case.
**
** This function formats the input string (istr) in the
** required format to the output string (ostr).
** Take care that the buffer pointed to by ostr is at least
** two bytes larger than the input string (for the parenthesis).
** The pointer ostr is also returned.
*/
static char *
spaceformat(char *istr, char *ostr)
{
	// formatting with spaces and parenthesis required?
	if (!rmspaces)
	{
		*ostr = '(';
		strcpy(ostr+1, istr);
		strcat(ostr, ")");
	}
	// formatting with underscores without parenthesis required
	else
	{
		register char *pi = istr, *po = ostr;

		while (*pi)
		{
			if (*pi == ' ')
			{
				*po++ = '_';
				pi++;
			}
			else
			{
				*po++ = *pi++;
			}
		}

		if (po == ostr)		// empty string: still return underscore
			*po++ = '_';

		*po = '\0';		// terminate output string
	}

	return ostr;
}

/*
** return proper integer for cgroup v2 maximum values
*/
static int
cgroupv2max(int isproc, int max)
{
	if (! (supportflags&CGROUPV2))
		return -3;

	if (! isproc)
		return -2;

	return max;
}
