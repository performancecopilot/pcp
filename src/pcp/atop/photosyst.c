/*
** Copyright (C) 2015 Red Hat.
** Copyright (C) 2000-2012 Gerlof Langeveld.
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
#include <pcp/impl.h>

#include "atop.h"
#include "photosyst.h"
#include "systmetrics.h"

static void
update_disk(struct perdsk *dsk, int id, char *name, pmResult *rp, pmDesc *dp)
{
	strncpy(dsk->name, name, sizeof(dsk->name));
	dsk->name[sizeof(dsk->name)-1] = '\0';

	dsk->nread = extract_count_t_inst(rp, dp, PERDISK_NREAD, id);
	dsk->nrsect = extract_count_t_inst(rp, dp, PERDISK_NRSECT, id);
	dsk->nwrite = extract_count_t_inst(rp, dp, PERDISK_NWRITE, id);
	dsk->nwsect = extract_count_t_inst(rp, dp, PERDISK_NWSECT, id);
	dsk->io_ms = extract_count_t_inst(rp, dp, PERDISK_IO_MS, id);
	dsk->avque = extract_count_t_inst(rp, dp, PERDISK_AVEQ, id);
}

static void
update_interface(struct perintf *in, int id, char *name, pmResult *rp, pmDesc *dp)
{
	strncpy(in->name, name, sizeof(in->name));
	in->name[sizeof(in->name)-1] = '\0';

	/* /proc/net/dev */
	in->rbyte = extract_count_t_inst(rp, dp, PERINTF_RBYTE, id);
	in->rpack = extract_count_t_inst(rp, dp, PERINTF_RPACK, id);
	in->rerrs = extract_count_t_inst(rp, dp, PERINTF_RERRS, id);
	in->rdrop = extract_count_t_inst(rp, dp, PERINTF_RDROP, id);
	in->rfifo = extract_count_t_inst(rp, dp, PERINTF_RFIFO, id);
	in->rframe = extract_count_t_inst(rp, dp, PERINTF_RFRAME, id);
	in->rcompr = extract_count_t_inst(rp, dp, PERINTF_RCOMPR, id);
	in->rmultic = extract_count_t_inst(rp, dp, PERINTF_RMULTIC, id);
	in->sbyte = extract_count_t_inst(rp, dp, PERINTF_SBYTE, id);
	in->spack = extract_count_t_inst(rp, dp, PERINTF_SPACK, id);
	in->serrs = extract_count_t_inst(rp, dp, PERINTF_SERRS, id);
	in->sdrop = extract_count_t_inst(rp, dp, PERINTF_SDROP, id);
	in->sfifo = extract_count_t_inst(rp, dp, PERINTF_SFIFO, id);
	in->scollis = extract_count_t_inst(rp, dp, PERINTF_SCOLLIS, id);
	in->scarrier = extract_count_t_inst(rp, dp, PERINTF_SCARRIER, id);
	in->scompr = extract_count_t_inst(rp, dp, PERINTF_SCOMPR, id);
}

static void
update_processor(struct percpu *cpu, int id, pmResult *result, pmDesc *descs)
{
	cpu->cpunr = id;

	cpu->stime = extract_count_t_inst(result, descs, PERCPU_STIME, id);
	cpu->utime = extract_count_t_inst(result, descs, PERCPU_UTIME, id);
	cpu->ntime = extract_count_t_inst(result, descs, PERCPU_NTIME, id);
	cpu->itime = extract_count_t_inst(result, descs, PERCPU_ITIME, id);
	cpu->wtime = extract_count_t_inst(result, descs, PERCPU_WTIME, id);
	cpu->Itime = extract_count_t_inst(result, descs, PERCPU_HARDIRQ, id);
	cpu->Stime = extract_count_t_inst(result, descs, PERCPU_SOFTIRQ, id);
	cpu->steal = extract_count_t_inst(result, descs, PERCPU_STEAL, id);
	cpu->guest = extract_count_t_inst(result, descs, PERCPU_GUEST, id);

	memset(&cpu->freqcnt, 0, sizeof(struct freqcnt));
	cpu->freqcnt.cnt = extract_count_t_inst(result, descs, PERCPU_FREQCNT_CNT, id);
}

void
photosyst(struct sstat *si)
{
	static int	setup;
	count_t		count;
	unsigned int	nrcpu, nrdisk, nrintf;
	unsigned int	nrlvm = 0, nrmdd = 0;
	static pmID	pmids[SYST_NMETRICS];
	static pmDesc	descs[SYST_NMETRICS];
	pmResult	*result;
	size_t		size;
	char		**insts;
	int		*ids, sts, i;

	if (!setup)
	{
		setup_metrics(systmetrics, pmids, descs, SYST_NMETRICS);
		setup = 1;
	}

	if ((sts = pmFetch(SYST_NMETRICS, pmids, &result)) < 0)
	{
		fprintf(stderr, "%s: pmFetch system metrics: %s\n",
			pmProgname, pmErrStr(sts));
		cleanstop(1);
	}

	memset(si, 0, sizeof(struct sstat));
	si->stamp = result->timestamp;

	/* /proc/stat */
	si->cpu.csw = extract_count_t(result, descs, CPU_CSW);
	si->cpu.devint = extract_count_t(result, descs, CPU_DEVINT);
	si->cpu.nprocs = extract_count_t(result, descs, CPU_NPROCS);
	si->cpu.lavg1 = extract_float_inst(result, descs, CPU_LOAD, 1);
	si->cpu.lavg5 = extract_float_inst(result, descs, CPU_LOAD, 5);
	si->cpu.lavg15 = extract_float_inst(result, descs, CPU_LOAD, 15);
	si->cpu.all.utime = extract_count_t(result, descs, CPU_UTIME);
	si->cpu.all.ntime = extract_count_t(result, descs, CPU_NTIME);
	si->cpu.all.stime = extract_count_t(result, descs, CPU_STIME);
	si->cpu.all.itime = extract_count_t(result, descs, CPU_ITIME);
	si->cpu.all.wtime = extract_count_t(result, descs, CPU_WTIME);
	si->cpu.all.Itime = extract_count_t(result, descs, CPU_HARDIRQ);
	si->cpu.all.Stime = extract_count_t(result, descs, CPU_SOFTIRQ);
	si->cpu.all.steal = extract_count_t(result, descs, CPU_STEAL);
	si->cpu.all.guest = extract_count_t(result, descs, CPU_GUEST);

	if ((sts = pmGetInDom(descs[PERCPU_UTIME].indom, &ids, &insts)) < 0)
	{
		fprintf(stderr, "%s: pmGetInDom processors: %s\n",
			pmProgname, pmErrStr(sts));
		cleanstop(1);
	}
	if ((nrcpu = sts) > si->cpu.nrcpu)
	{
		size = nrcpu * sizeof(struct percpu);
		si->cpu.cpu = (struct percpu *)realloc(si->cpu.cpu, size);
		if (!si->cpu.cpu)
			__pmNoMem("photosyst cpus", size, PM_FATAL_ERR);
	}
	for (i=0; i < nrcpu; i++)
	{
		if (pmDebug & DBG_TRACE_APPL0)
			fprintf(stderr, "%s: updating processor %d: %s\n",
				pmProgname, ids[i], insts[i]);
		update_processor(&si->cpu.cpu[i], ids[i], result, descs);
	}
	si->cpu.nrcpu = nrcpu;
	free(insts);
	free(ids);

	/* /proc/vmstat */
	si->mem.swins = extract_count_t(result, descs, MEM_SWINS);
	si->mem.swouts = extract_count_t(result, descs, MEM_SWOUTS);
	count = 0;
	count += extract_count_t(result, descs, MEM_SCAN_DDMA);
	count += extract_count_t(result, descs, MEM_SCAN_DDMA32);
	count += extract_count_t(result, descs, MEM_SCAN_DHIGH);
	count += extract_count_t(result, descs, MEM_SCAN_DMOVABLE);
	count += extract_count_t(result, descs, MEM_SCAN_DNORMAL);
	count += extract_count_t(result, descs, MEM_SCAN_KDMA);
	count += extract_count_t(result, descs, MEM_SCAN_KDMA32);
	count += extract_count_t(result, descs, MEM_SCAN_KHIGH);
	count += extract_count_t(result, descs, MEM_SCAN_KMOVABLE);
	count += extract_count_t(result, descs, MEM_SCAN_KNORMAL);
	si->mem.pgscans = count;
	count = 0;
	count += extract_count_t(result, descs, MEM_STEAL_DMA);
	count += extract_count_t(result, descs, MEM_STEAL_DMA32);
	count += extract_count_t(result, descs, MEM_STEAL_HIGH);
	count += extract_count_t(result, descs, MEM_STEAL_MOVABLE);
	count += extract_count_t(result, descs, MEM_STEAL_NORMAL);
	si->mem.pgsteal = count;
	si->mem.allocstall = extract_count_t(result, descs, MEM_ALLOCSTALL);

	/* /proc/meminfo */
	si->mem.cachemem = extract_count_t(result, descs, MEM_CACHEMEM);
	si->mem.cachedrt = extract_count_t(result, descs, MEM_CACHEDRT);
	si->mem.physmem = extract_count_t(result, descs, MEM_PHYSMEM);
	si->mem.freemem = extract_count_t(result, descs, MEM_FREEMEM);
	si->mem.buffermem = extract_count_t(result, descs, MEM_BUFFERMEM);
	si->mem.shmem = extract_count_t(result, descs, MEM_SHMEM);
	si->mem.totswap = extract_count_t(result, descs, MEM_TOTSWAP);
	si->mem.freeswap = extract_count_t(result, descs, MEM_FREESWAP);
	si->mem.slabmem  = extract_count_t(result, descs, MEM_SLABMEM);
	si->mem.slabreclaim = extract_count_t(result, descs, MEM_SLABRECLAIM);
	si->mem.committed = extract_count_t(result, descs, MEM_COMMITTED);
	si->mem.commitlim = extract_count_t(result, descs, MEM_COMMITLIM);
	si->mem.tothugepage = extract_count_t(result, descs, MEM_TOTHUGEPAGE);
	si->mem.freehugepage = extract_count_t(result, descs, MEM_FREEHUGEPAGE);
	si->mem.hugepagesz = extract_count_t(result, descs, HUGEPAGESZ);

	/* shmctl(2) */
	si->mem.shmrss = extract_count_t(result, descs, MEM_SHMRSS);
	si->mem.shmswp = extract_count_t(result, descs, MEM_SHMSWP);

	/* /proc/net/dev */
	if ((sts = pmGetInDom(descs[PERINTF_RBYTE].indom, &ids, &insts)) < 0)
	{
		fprintf(stderr, "%s: pmGetInDom interfaces: %s\n",
			pmProgname, pmErrStr(sts));
		cleanstop(1);
	}
	if ((nrintf = sts) > si->intf.nrintf)
	{
		size = (nrintf + 1) * sizeof(struct perintf);
		si->intf.intf = (struct perintf *)realloc(si->intf.intf, size);
		if (!si->intf.intf)
			__pmNoMem("photosyst intf", size, PM_FATAL_ERR);
	}
	for (i=0; i < nrintf; i++)
	{
		if (pmDebug & DBG_TRACE_APPL0)
			fprintf(stderr, "%s: updating interface %d: %s\n",
				pmProgname, ids[i], insts[i]);
		update_interface(&si->intf.intf[i], ids[i], insts[i], result, descs);
	}
	si->intf.intf[nrintf].name[0] = '\0';
	si->intf.nrintf = nrintf;
	free(insts);
	free(ids);

	/* /proc/net/snmp */
	si->net.ipv4.Forwarding = extract_count_t(result, descs, IPV4_Forwarding);
	si->net.ipv4.DefaultTTL = extract_count_t(result, descs, IPV4_DefaultTTL);
	si->net.ipv4.InReceives = extract_count_t(result, descs, IPV4_InReceives);
	si->net.ipv4.InHdrErrors = extract_count_t(result, descs, IPV4_InHdrErrors);
	si->net.ipv4.InAddrErrors = extract_count_t(result, descs, IPV4_InAddrErrors);
	si->net.ipv4.ForwDatagrams = extract_count_t(result, descs, IPV4_ForwDatagrams);
	si->net.ipv4.InUnknownProtos = extract_count_t(result, descs, IPV4_InUnknownProtos);
	si->net.ipv4.InDiscards = extract_count_t(result, descs, IPV4_InDiscards);
	si->net.ipv4.InDelivers = extract_count_t(result, descs, IPV4_InDelivers);
	si->net.ipv4.OutRequests = extract_count_t(result, descs, IPV4_OutRequests);
	si->net.ipv4.OutDiscards = extract_count_t(result, descs, IPV4_OutDiscards);
	si->net.ipv4.OutNoRoutes = extract_count_t(result, descs, IPV4_OutNoRoutes);
	si->net.ipv4.ReasmTimeout = extract_count_t(result, descs, IPV4_ReasmTimeout);
	si->net.ipv4.ReasmReqds = extract_count_t(result, descs, IPV4_ReasmReqds);
	si->net.ipv4.ReasmOKs = extract_count_t(result, descs, IPV4_ReasmOKs);
	si->net.ipv4.ReasmFails = extract_count_t(result, descs, IPV4_ReasmFails);
	si->net.ipv4.FragOKs = extract_count_t(result, descs, IPV4_FragOKs);
	si->net.ipv4.FragFails = extract_count_t(result, descs, IPV4_FragFails);
	si->net.ipv4.FragCreates = extract_count_t(result, descs, IPV4_FragCreates);
	si->net.icmpv4.InMsgs = extract_count_t(result, descs, ICMP4_InMsgs);
	si->net.icmpv4.InErrors = extract_count_t(result, descs, ICMP4_InErrors);
	si->net.icmpv4.InDestUnreachs = extract_count_t(result, descs, ICMP4_InDestUnreachs);
	si->net.icmpv4.InTimeExcds = extract_count_t(result, descs, ICMP4_InTimeExcds);
	si->net.icmpv4.InParmProbs = extract_count_t(result, descs, ICMP4_InParmProbs);
	si->net.icmpv4.InSrcQuenchs = extract_count_t(result, descs, ICMP4_InSrcQuenchs);
	si->net.icmpv4.InRedirects = extract_count_t(result, descs, ICMP4_InRedirects);
	si->net.icmpv4.InEchos = extract_count_t(result, descs, ICMP4_InEchos);
	si->net.icmpv4.InEchoReps = extract_count_t(result, descs, ICMP4_InEchoReps);
	si->net.icmpv4.InTimestamps = extract_count_t(result, descs, ICMP4_InTimestamps);
	si->net.icmpv4.InTimestampReps = extract_count_t(result, descs, ICMP4_InTimestampReps);
	si->net.icmpv4.InAddrMasks = extract_count_t(result, descs, ICMP4_InAddrMasks);
	si->net.icmpv4.InAddrMaskReps = extract_count_t(result, descs, ICMP4_InAddrMaskReps);
	si->net.icmpv4.OutMsgs = extract_count_t(result, descs, ICMP4_OutMsgs);
	si->net.icmpv4.OutErrors = extract_count_t(result, descs, ICMP4_OutErrors);
	si->net.icmpv4.OutDestUnreachs = extract_count_t(result, descs, ICMP4_OutDestUnreachs);
	si->net.icmpv4.OutTimeExcds = extract_count_t(result, descs, ICMP4_OutTimeExcds);
	si->net.icmpv4.OutParmProbs = extract_count_t(result, descs, ICMP4_OutParmProbs);
	si->net.icmpv4.OutSrcQuenchs = extract_count_t(result, descs, ICMP4_OutSrcQuenchs);
	si->net.icmpv4.OutRedirects = extract_count_t(result, descs, ICMP4_OutRedirects);
	si->net.icmpv4.OutEchos = extract_count_t(result, descs, ICMP4_OutEchos);
	si->net.icmpv4.OutEchoReps = extract_count_t(result, descs, ICMP4_OutEchoReps);
	si->net.icmpv4.OutTimestamps = extract_count_t(result, descs, ICMP4_OutTimestamps);
	si->net.icmpv4.OutTimestampReps = extract_count_t(result, descs, ICMP4_OutTimestampReps);
	si->net.icmpv4.OutAddrMasks = extract_count_t(result, descs, ICMP4_OutAddrMasks);
	si->net.icmpv4.OutAddrMaskReps = extract_count_t(result, descs, ICMP4_OutAddrMaskReps);
	si->net.tcp.RtoAlgorithm = extract_count_t(result, descs, TCP_RtoAlgorithm);
	si->net.tcp.RtoMin = extract_count_t(result, descs, TCP_RtoMin);
	si->net.tcp.RtoMax = extract_count_t(result, descs, TCP_RtoMax);
	si->net.tcp.MaxConn = extract_count_t(result, descs, TCP_MaxConn);
	si->net.tcp.ActiveOpens = extract_count_t(result, descs, TCP_ActiveOpens);
	si->net.tcp.PassiveOpens = extract_count_t(result, descs, TCP_PassiveOpens);
	si->net.tcp.AttemptFails = extract_count_t(result, descs, TCP_AttemptFails);
	si->net.tcp.EstabResets = extract_count_t(result, descs, TCP_EstabResets);
	si->net.tcp.CurrEstab = extract_count_t(result, descs, TCP_CurrEstab);
	si->net.tcp.InSegs = extract_count_t(result, descs, TCP_InSegs);
	si->net.tcp.OutSegs = extract_count_t(result, descs, TCP_OutSegs);
	si->net.tcp.RetransSegs = extract_count_t(result, descs, TCP_RetransSegs);
	si->net.tcp.InErrs = extract_count_t(result, descs, TCP_InErrs);
	si->net.tcp.OutRsts = extract_count_t(result, descs, TCP_OutRsts);
	si->net.udpv4.InDatagrams = extract_count_t(result, descs, UDPV4_InDatagrams);
	si->net.udpv4.NoPorts = extract_count_t(result, descs, UDPV4_NoPorts);
	si->net.udpv4.InErrors = extract_count_t(result, descs, UDPV4_InErrors);
	si->net.udpv4.OutDatagrams = extract_count_t(result, descs, UDPV4_OutDatagrams);

	/* /proc/net/snmp6 */
	si->net.ipv6.Ip6InReceives = extract_count_t(result, descs, IPV6_InReceives);
	si->net.ipv6.Ip6InHdrErrors = extract_count_t(result, descs, IPV6_InHdrErrors);
	si->net.ipv6.Ip6InTooBigErrors = extract_count_t(result, descs, IPV6_InTooBigErrors);
	si->net.ipv6.Ip6InNoRoutes = extract_count_t(result, descs, IPV6_InNoRoutes);
	si->net.ipv6.Ip6InAddrErrors = extract_count_t(result, descs, IPV6_InAddrErrors);
	si->net.ipv6.Ip6InUnknownProtos = extract_count_t(result, descs, IPV6_InUnknownProtos);
	si->net.ipv6.Ip6InTruncatedPkts = extract_count_t(result, descs, IPV6_InTruncatedPkts);
	si->net.ipv6.Ip6InDiscards = extract_count_t(result, descs, IPV6_InDiscards);
	si->net.ipv6.Ip6InDelivers = extract_count_t(result, descs, IPV6_InDelivers);
	si->net.ipv6.Ip6OutForwDatagrams = extract_count_t(result, descs, IPV6_OutForwDatagrams);
	si->net.ipv6.Ip6OutRequests = extract_count_t(result, descs, IPV6_OutRequests);
	si->net.ipv6.Ip6OutDiscards = extract_count_t(result, descs, IPV6_OutDiscards);
	si->net.ipv6.Ip6OutNoRoutes = extract_count_t(result, descs, IPV6_OutNoRoutes);
	si->net.ipv6.Ip6ReasmTimeout = extract_count_t(result, descs, IPV6_ReasmTimeout);
	si->net.ipv6.Ip6ReasmReqds = extract_count_t(result, descs, IPV6_ReasmReqds);
	si->net.ipv6.Ip6ReasmOKs = extract_count_t(result, descs, IPV6_ReasmOKs);
	si->net.ipv6.Ip6ReasmFails = extract_count_t(result, descs, IPV6_ReasmFails);
	si->net.ipv6.Ip6FragOKs = extract_count_t(result, descs, IPV6_FragOKs);
	si->net.ipv6.Ip6FragFails = extract_count_t(result, descs, IPV6_FragFails);
	si->net.ipv6.Ip6FragCreates = extract_count_t(result, descs, IPV6_FragCreates);
	si->net.ipv6.Ip6InMcastPkts = extract_count_t(result, descs, IPV6_InMcastPkts);
	si->net.ipv6.Ip6OutMcastPkts = extract_count_t(result, descs, IPV6_OutMcastPkts);
	si->net.icmpv6.Icmp6InMsgs = extract_count_t(result, descs, ICMPV6_InMsgs);
	si->net.icmpv6.Icmp6InErrors = extract_count_t(result, descs, ICMPV6_InErrors);
	si->net.icmpv6.Icmp6InDestUnreachs = extract_count_t(result, descs, ICMPV6_InDestUnreachs);
	si->net.icmpv6.Icmp6InPktTooBigs = extract_count_t(result, descs, ICMPV6_InPktTooBigs);
	si->net.icmpv6.Icmp6InTimeExcds = extract_count_t(result, descs, ICMPV6_InTimeExcds);
	si->net.icmpv6.Icmp6InParmProblems = extract_count_t(result, descs, ICMPV6_InParmProblems);
	si->net.icmpv6.Icmp6InEchos = extract_count_t(result, descs, ICMPV6_InEchos);
	si->net.icmpv6.Icmp6InEchoReplies = extract_count_t(result, descs, ICMPV6_InEchoReplies);
	si->net.icmpv6.Icmp6InGroupMembQueries = extract_count_t(result, descs, ICMPV6_InGroupMembQueries);
	si->net.icmpv6.Icmp6InGroupMembResponses = extract_count_t(result, descs, ICMPV6_InGroupMembResponses);
	si->net.icmpv6.Icmp6InGroupMembReductions = extract_count_t(result, descs, ICMPV6_InGroupMembReductions);
	si->net.icmpv6.Icmp6InRouterSolicits = extract_count_t(result, descs, ICMPV6_InRouterSolicits);
	si->net.icmpv6.Icmp6InRouterAdvertisements = extract_count_t(result, descs, ICMPV6_InRouterAdvertisements);
	si->net.icmpv6.Icmp6InNeighborSolicits = extract_count_t(result, descs, ICMPV6_InNeighborSolicits);
	si->net.icmpv6.Icmp6InNeighborAdvertisements = extract_count_t(result, descs, ICMPV6_InNeighborAdvertisements);
	si->net.icmpv6.Icmp6InRedirects = extract_count_t(result, descs, ICMPV6_InRedirects);
	si->net.icmpv6.Icmp6OutMsgs = extract_count_t(result, descs, ICMPV6_OutMsgs);
	si->net.icmpv6.Icmp6OutDestUnreachs = extract_count_t(result, descs, ICMPV6_OutDestUnreachs);
	si->net.icmpv6.Icmp6OutPktTooBigs = extract_count_t(result, descs, ICMPV6_OutPktTooBigs);
	si->net.icmpv6.Icmp6OutTimeExcds = extract_count_t(result, descs, ICMPV6_OutTimeExcds);
	si->net.icmpv6.Icmp6OutParmProblems = extract_count_t(result, descs, ICMPV6_OutParmProblems);
	si->net.icmpv6.Icmp6OutEchoReplies = extract_count_t(result, descs, ICMPV6_OutEchoReplies);
	si->net.icmpv6.Icmp6OutRouterSolicits = extract_count_t(result, descs, ICMPV6_OutRouterSolicits);
	si->net.icmpv6.Icmp6OutNeighborSolicits = extract_count_t(result, descs, ICMPV6_OutNeighborSolicits);
	si->net.icmpv6.Icmp6OutNeighborAdvertisements = extract_count_t(result, descs, ICMPV6_OutNeighborAdvertisements);
	si->net.icmpv6.Icmp6OutRedirects = extract_count_t(result, descs, ICMPV6_OutRedirects);
	si->net.icmpv6.Icmp6OutGroupMembResponses = extract_count_t(result, descs, ICMPV6_OutGroupMembResponses);
	si->net.icmpv6.Icmp6OutGroupMembReductions = extract_count_t(result, descs, ICMPV6_OutGroupMembReductions);
	si->net.udpv6.Udp6InDatagrams = extract_count_t(result, descs, UDPV6_InDatagrams);
	si->net.udpv6.Udp6NoPorts = extract_count_t(result, descs, UDPV6_NoPorts);
	si->net.udpv6.Udp6InErrors = extract_count_t(result, descs, UDPV6_InErrors);
	si->net.udpv6.Udp6OutDatagrams = extract_count_t(result, descs, UDPV6_OutDatagrams);

	/* /proc/diskstats or /proc/partitions */
	if ((sts = pmGetInDom(descs[PERDISK_NREAD].indom, &ids, &insts)) < 0)
	{
		fprintf(stderr, "%s: pmGetInDom disks: %s\n",
			pmProgname, pmErrStr(sts));
		cleanstop(1);
	}
	if ((nrdisk = sts) > si->dsk.ndsk || !si->dsk.ndsk)
	{
		size = (nrdisk + 1) * sizeof(struct perdsk);
		si->dsk.dsk = (struct perdsk *)realloc(si->dsk.dsk, size);
		if (!si->dsk.dsk)
			__pmNoMem("photosyst disk", size, PM_FATAL_ERR);
	}
	for (i=0; i < nrdisk; i++)
	{
		if (pmDebug & DBG_TRACE_APPL0)
			fprintf(stderr, "%s: updating disk %d: %s\n",
				pmProgname, ids[i], insts[i]);
		update_disk(&si->dsk.dsk[i], ids[i], insts[i], result, descs);
	}
	si->dsk.dsk[nrdisk].name[0] = '\0';
	si->dsk.ndsk = nrdisk;
	free(insts);
	free(ids);

	/* TODO: work out number of LVM devices */
	if (nrlvm > si->dsk.nlvm || !si->dsk.nlvm)
	{
		size = (nrlvm + 1) * sizeof(struct perdsk);
		si->dsk.lvm = (struct perdsk *)realloc(si->dsk.lvm, size);
		if (!si->dsk.lvm)
			__pmNoMem("photosyst lvm", size, PM_FATAL_ERR);
	}
	si->dsk.lvm[si->dsk.nlvm].name[0] = '\0'; 

	/* TODO: work out number of MD devices */
	if (nrmdd > si->dsk.nmdd || !si->dsk.nmdd)
	{
		size = (nrmdd + 1) * sizeof(struct perdsk);
		si->dsk.mdd = (struct perdsk *)realloc(si->dsk.mdd, size);
		if (!si->dsk.mdd)
			__pmNoMem("photosyst mdd", size, PM_FATAL_ERR);
	}
	si->dsk.mdd[si->dsk.nmdd].name[0] = '\0';

	/* Apache status */
	si->www.accesses  = extract_count_t(result, descs, WWW_ACCESSES);
	si->www.totkbytes = extract_count_t(result, descs, WWW_TOTKBYTES);
	si->www.uptime    = extract_count_t(result, descs, WWW_UPTIME);
	si->www.bworkers  = extract_integer(result, descs, WWW_BWORKERS);
	si->www.iworkers  = extract_integer(result, descs, WWW_IWORKERS);

	pmFreeResult(result);
}

#if 0
#include <sys/stat.h>
#include <regex.h>
#define	MAXCNT	64

/* return value of isdisk() */
#define	NONTYPE	0
#define	DSKTYPE	1
#define	MDDTYPE	2
#define	LVMTYPE	3

static int	isdisk(unsigned int, unsigned int,
			char *, struct perdsk *, int);

	/*
	** check if this line concerns the entire disk
	** or just one of the partitions of a disk (to be
	** skipped)
	*/
	if (nr == 9)	/* full stats-line ? */
	{
		switch ( isdisk(major, minor, diskname,
					 &tmpdsk, MAXDKNAM) )
		{
		   case NONTYPE:
		       continue;

		   case DSKTYPE:
			if (si->dsk.ndsk < MAXDSK-1)
			  si->dsk.dsk[si->dsk.ndsk++] = tmpdsk;
			break;

		   case MDDTYPE:
			if (si->dsk.nmdd < MAXMDD-1)
			  si->dsk.mdd[si->dsk.nmdd++] = tmpdsk;
			break;

		   case LVMTYPE:
			if (si->dsk.nlvm < MAXLVM-1)
			  si->dsk.lvm[si->dsk.nlvm++] = tmpdsk;
			break;
		}
	}
...


/*
** set of subroutines to determine which disks should be monitored
** and to translate name strings into (shorter) name strings
*/
static void
nullmodname(unsigned int major, unsigned int minor,
		char *curname, struct perdsk *px, int maxlen)
{
	strncpy(px->name, curname, maxlen-1);
	*(px->name+maxlen-1) = 0;
}

static void
abbrevname1(unsigned int major, unsigned int minor,
		char *curname, struct perdsk *px, int maxlen)
{
	char	cutype[128];
	int	hostnum, busnum, targetnum, lunnum;

	sscanf(curname, "%[^/]/host%d/bus%d/target%d/lun%d",
			cutype, &hostnum, &busnum, &targetnum, &lunnum);

	snprintf(px->name, maxlen, "%c-h%db%dt%d", 
			cutype[0], hostnum, busnum, targetnum);
}

/*
** recognize LVM logical volumes
*/
#define	NUMDMHASH	64
#define	DMHASH(x,y)	(((x)+(y))%NUMDMHASH)	
#define	MAPDIR		"/dev/mapper"

struct devmap {
	unsigned int	major;
	unsigned int	minor;
	char		name[MAXDKNAM];
	struct devmap	*next;
};

static void
lvmmapname(unsigned int major, unsigned int minor,
		char *curname, struct perdsk *px, int maxlen)
{
	static int		firstcall = 1;
	static struct devmap	*devmaps[NUMDMHASH], *dmp;
	int			hashix;

	/*
 	** setup a list of major-minor numbers of dm-devices with their
	** corresponding name
	*/
	if (firstcall)
	{
		DIR		*dirp;
		struct dirent	*dentry;
		struct stat	statbuf;
		char		path[64];

		if ( (dirp = opendir(MAPDIR)) )
		{
			/*
	 		** read every directory-entry and search for
			** block devices
			*/
			while ( (dentry = readdir(dirp)) )
			{
				snprintf(path, sizeof path, "%s/%s", 
						MAPDIR, dentry->d_name);

				if ( stat(path, &statbuf) == -1 )
					continue;

				if ( ! S_ISBLK(statbuf.st_mode) )
					continue;
				/*
 				** allocate struct to store name
				*/
				if ( !(dmp = malloc(sizeof (struct devmap))))
					continue;

				/*
 				** store info in hash list
				*/
				strncpy(dmp->name, dentry->d_name, MAXDKNAM);
				dmp->name[MAXDKNAM-1] = 0;
				dmp->major 	= major(statbuf.st_rdev);
				dmp->minor 	= minor(statbuf.st_rdev);

				hashix = DMHASH(dmp->major, dmp->minor);

				dmp->next	= devmaps[hashix];

				devmaps[hashix]	= dmp;
			}

			closedir(dirp);
		}

		firstcall = 0;
	}

	/*
 	** find info in hash list
	*/
	hashix  = DMHASH(major, minor);
	dmp	= devmaps[hashix];

	while (dmp)
	{
		if (dmp->major == major && dmp->minor == minor)
		{
			/*
		 	** info found in hash list; fill proper name
			*/
			strncpy(px->name, dmp->name, maxlen-1);
			*(px->name+maxlen-1) = 0;
			return;
		}

		dmp = dmp->next;
	}

	/*
	** info not found in hash list; fill original name
	*/
	strncpy(px->name, curname, maxlen-1);
	*(px->name+maxlen-1) = 0;
}

/*
** this table is used in the function isdisk()
**
** table contains the names (in regexp format) of disks
** to be recognized, together with a function to modify
** the name-strings (i.e. to abbreviate long strings);
** some frequently found names (like 'loop' and 'ram')
** are also recognized to skip them as fast as possible
*/
static struct {
	char 	*regexp;
	regex_t	compreg;
	void	(*modname)(unsigned int, unsigned int,
				char *, struct perdsk *, int);
	int	retval;
} validdisk[] = {
	{ "^ram[0-9][0-9]*$",			{0},  (void *)0,   NONTYPE, },
	{ "^loop[0-9][0-9]*$",			{0},  (void *)0,   NONTYPE, },
	{ "^sd[a-z][a-z]*$",			{0},  nullmodname, DSKTYPE, },
	{ "^dm-[0-9][0-9]*$",			{0},  lvmmapname,  LVMTYPE, },
	{ "^md[0-9][0-9]*$",			{0},  nullmodname, MDDTYPE, },
	{ "^vd[a-z][a-z]*$",                    {0},  nullmodname, DSKTYPE, },
	{ "^hd[a-z]$",				{0},  nullmodname, DSKTYPE, },
	{ "^rd/c[0-9][0-9]*d[0-9][0-9]*$",	{0},  nullmodname, DSKTYPE, },
	{ "^cciss/c[0-9][0-9]*d[0-9][0-9]*$",	{0},  nullmodname, DSKTYPE, },
	{ "^fio[a-z][a-z]*$",			{0},  nullmodname, DSKTYPE, },
	{ "/host.*/bus.*/target.*/lun.*/disc",	{0},  abbrevname1, DSKTYPE, },
	{ "^xvd[a-z][a-z]*$",			{0},  nullmodname, DSKTYPE, },
	{ "^dasd[a-z][a-z]*$",			{0},  nullmodname, DSKTYPE, },
	{ "^mmcblk[0-9][0-9]*$",		{0},  nullmodname, DSKTYPE, },
	{ "^emcpower[a-z][a-z]*$",		{0},  nullmodname, DSKTYPE, },
};

static int
isdisk(unsigned int major, unsigned int minor,
           char *curname, struct perdsk *px, int maxlen)
{
	static int	firstcall = 1;
	register int	i;

	if (firstcall)		/* compile the regular expressions */
	{
		for (i=0; i < sizeof validdisk/sizeof validdisk[0]; i++)
			regcomp(&validdisk[i].compreg, validdisk[i].regexp,
								REG_NOSUB);
		firstcall = 0;
	}

	/*
	** try to recognize one of the compiled regular expressions
	*/
	for (i=0; i < sizeof validdisk/sizeof validdisk[0]; i++)
	{
		if (regexec(&validdisk[i].compreg, curname, 0, NULL, 0) == 0)
		{
			/*
			** name-string recognized; modify name-string
			*/
			if (validdisk[i].retval != NONTYPE)
				(*validdisk[i].modname)(major, minor,
						curname, px, maxlen);

			return validdisk[i].retval;
		}
	}

	return NONTYPE;
}
#endif
