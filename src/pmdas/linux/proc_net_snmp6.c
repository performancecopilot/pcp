/*
 * IPv6 counters from procfs (/proc/net/snmp6)
 *
 * Copyright (c) 2015 Red Hat.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#include <dirent.h>
#include <sys/stat.h>
#include <string.h>

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "indom.h"
#include "linux_table.h"
#include "proc_net_snmp6.h"

/* procfs file for IPv6 stats */
proc_net_snmp6_t _pm_proc_net_snmp6[] = {
    [_PM_IP6_INRECEIVES]		{ field: "Ip6InReceives" },
    [_PM_IP6_INHDRERRORS]		{ field: "Ip6InHdrErrors" },
    [_PM_IP6_INTOOBIGERRORS]		{ field: "Ip6InTooBigErrors" },
    [_PM_IP6_INNOROUTES]		{ field: "Ip6InNoRoutes" },
    [_PM_IP6_INADDRERRORS]		{ field: "Ip6InAddrErrors" },
    [_PM_IP6_INUNKNOWNPROTOS]		{ field: "Ip6InUnknownProtos" },
    [_PM_IP6_INTRUNCATEDPKTS]		{ field: "Ip6InTruncatedPkts" },
    [_PM_IP6_INDISCARDS]		{ field: "Ip6InDiscards" },
    [_PM_IP6_INDELIVERS]		{ field: "Ip6InDelivers" },
    [_PM_IP6_OUTFORWDATAGRAMS]		{ field: "Ip6OutForwDatagrams" },
    [_PM_IP6_OUTREQUESTS]		{ field: "Ip6OutRequests" },
    [_PM_IP6_OUTDISCARDS]		{ field: "Ip6OutDiscards" },
    [_PM_IP6_OUTNOROUTES]		{ field: "Ip6OutNoRoutes" },
    [_PM_IP6_REASMTIMEOUT]		{ field: "Ip6ReasmTimeout" },
    [_PM_IP6_REASMREQDS]		{ field: "Ip6ReasmReqds" },
    [_PM_IP6_REASMOKS]			{ field: "Ip6ReasmOKs" },
    [_PM_IP6_REASMFAILS]		{ field: "Ip6ReasmFails" },
    [_PM_IP6_FRAGOKS]			{ field: "Ip6FragOKs" },
    [_PM_IP6_FRAGFAILS]			{ field: "Ip6FragFails" },
    [_PM_IP6_FRAGCREATES]		{ field: "Ip6FragCreates" },
    [_PM_IP6_INMCASTPKTS]		{ field: "Ip6InMcastPkts" },
    [_PM_IP6_OUTMCASTPKTS]		{ field: "Ip6OutMcastPkts" },
    [_PM_IP6_INOCTETS]			{ field: "Ip6InOctets" },
    [_PM_IP6_OUTOCTETS]			{ field: "Ip6OutOctets" },
    [_PM_IP6_INMCASTOCTETS]		{ field: "Ip6InMcastOctets" },
    [_PM_IP6_OUTMCASTOCTETS]		{ field: "Ip6OutMcastOctets" },
    [_PM_IP6_INBCASTOCTETS]		{ field: "Ip6InBcastOctets" },
    [_PM_IP6_OUTBCASTOCTETS]		{ field: "Ip6OutBcastOctets" },
    [_PM_IP6_INNOECTPKTS]		{ field: "Ip6InNoECTPkts" },
    [_PM_IP6_INECT1PKTS]		{ field: "Ip6InECT1Pkts" },
    [_PM_IP6_INECT0PKTS]		{ field: "Ip6InECT0Pkts" },
    [_PM_IP6_INCEPKTS]			{ field: "Ip6InCEPkts" },

    [_PM_ICMP6_INMSGS]			{ field: "Icmp6InMsgs" },
    [_PM_ICMP6_INERRORS]		{ field: "Icmp6InErrors" },
    [_PM_ICMP6_OUTMSGS]			{ field: "Icmp6OutMsgs" },
    [_PM_ICMP6_OUTERRORS]		{ field: "Icmp6OutErrors" },
    [_PM_ICMP6_INCSUMERRORS]		{ field: "Icmp6InCsumErrors" },
    [_PM_ICMP6_INDESTUNREACHS]		{ field: "Icmp6InDestUnreachs" },
    [_PM_ICMP6_INPKTTOOBIGS]		{ field: "Icmp6InPktTooBigs" },
    [_PM_ICMP6_INTIMEEXCDS]		{ field: "Icmp6InTimeExcds" },
    [_PM_ICMP6_INPARMPROBLEMS]		{ field: "Icmp6InParmProblems" },
    [_PM_ICMP6_INECHOS]			{ field: "Icmp6InEchos" },
    [_PM_ICMP6_INECHOREPLIES]		{ field: "Icmp6InEchoReplies" },
    [_PM_ICMP6_INGROUPMEMBQUERIES]	{ field: "Icmp6InGroupMembQueries" },
    [_PM_ICMP6_INGROUPMEMBRESPONSES]	{ field: "Icmp6InGroupMembResponses" },
    [_PM_ICMP6_INGROUPMEMBREDUCTIONS]	{ field: "Icmp6InGroupMembReductions" },
    [_PM_ICMP6_INROUTERSOLICITS]	{ field: "Icmp6InRouterSolicits" },
    [_PM_ICMP6_INROUTERADVERTISEMENTS] { field: "Icmp6InRouterAdvertisements" },
    [_PM_ICMP6_INNEIGHBORSOLICITS]	{ field: "Icmp6InNeighborSolicits" },
    [_PM_ICMP6_INNEIGHBORADVERTISEMENTS] { field: "Icmp6InNeighborAdvertisements" },
    [_PM_ICMP6_INREDIRECTS]		{ field: "Icmp6InRedirects" },
    [_PM_ICMP6_INMLDV2REPORTS]		{ field: "Icmp6InMLDv2Reports" },

    [_PM_ICMP6_OUTDESTUNREACHS]		{ field: "Icmp6OutDestUnreachs" },
    [_PM_ICMP6_OUTPKTTOOBIGS]		{ field: "Icmp6OutPktTooBigs" },
    [_PM_ICMP6_OUTMLDV2REPORTS]		{ field: "Icmp6OutTimeExcds" },
    [_PM_ICMP6_OUTTIMEEXCDS]		{ field: "Icmp6OutParmProblems" },
    [_PM_ICMP6_OUTPARMPROBLEMS]		{ field: "Icmp6OutEchos" },
    [_PM_ICMP6_OUTECHOS]		{ field: "Icmp6OutEchoReplies" },
    [_PM_ICMP6_OUTECHOREPLIES]		{ field: "Icmp6OutGroupMembQueries" },
    [_PM_ICMP6_OUTGROUPMEMBQUERIES]	{ field: "Icmp6OutGroupMembResponses" },
    [_PM_ICMP6_OUTGROUPMEMBRESPONSES] { field: "Icmp6OutGroupMembReductions" },
    [_PM_ICMP6_OUTGROUPMEMBREDUCTIONS]	{ field: "Icmp6OutRouterSolicits" },
    [_PM_ICMP6_OUTROUTERSOLICITS] { field: "Icmp6OutRouterAdvertisements" },
    [_PM_ICMP6_OUTROUTERADVERTISEMENTS]	{ field: "Icmp6OutNeighborSolicits" },
    [_PM_ICMP6_OUTNEIGHBORSOLICITS] { field: "Icmp6OutNeighborAdvertisements" },
    [_PM_ICMP6_OUTNEIGHBORADVERTISEMENTS] { field: "Icmp6OutRedirects" },
    [_PM_ICMP6_OUTREDIRECTS]		{ field: "Icmp6OutMLDv2Reports" },

    [_PM_UDP6_INDATAGRAMS]		{ field: "Udp6InDatagrams" },
    [_PM_UDP6_NOPORTS]			{ field: "Udp6NoPorts" },
    [_PM_UDP6_INERRORS]			{ field: "Udp6InErrors" },
    [_PM_UDP6_OUTDATAGRAMS]		{ field: "Udp6OutDatagrams" },
    [_PM_UDP6_RCVBUFERRORS]		{ field: "Udp6RcvbufErrors" },
    [_PM_UDP6_SNDBUFERRORS]		{ field: "Udp6SndbufErrors" },
    [_PM_UDP6_INCSUMERRORS]		{ field: "Udp6InCsumErrors" },
    [_PM_UDP6_IGNOREDMULTI]		{ field: "Udp6IgnoredMulti" },

    [_PM_UDPLITE6_INDATAGRAMS]		{ field: "UdpLite6InDatagrams" },
    [_PM_UDPLITE6_NOPORTS]		{ field: "UdpLite6NoPorts" },
    [_PM_UDPLITE6_INERRORS]		{ field: "UdpLite6InErrors" },
    [_PM_UDPLITE6_OUTDATAGRAMS]		{ field: "UdpLite6OutDatagrams" },
    [_PM_UDPLITE6_RCVBUFERRORS]		{ field: "UdpLite6RcvbufErrors" },
    [_PM_UDPLITE6_SNDBUFERRORS]		{ field: "UdpLite6SndbufErrors" },
    [_PM_UDPLITE6_INCSUMERRORS]		{ field: "UdpLite6InCsumErrors" },

    [_PM_SNMP6_METRIC_COUNT]		{ field: NULL }
};

int refresh_proc_net_snmp6(proc_net_snmp6_t *proc_net_snmp6)
{
    FILE *fp;
    proc_net_snmp6_t *t;
    char buf[MAXPATHLEN];
    static int first = 1;

    if (first) {
	for (t = proc_net_snmp6; t && t->field; t++)
	    t->field_len = strlen(t->field);
	first = 0;
    }

    if ((fp = linux_statsfile("/proc/net/snmp6", buf, sizeof(buf))) == NULL)
	return -oserror();

    linux_table_scan(fp, _pm_proc_net_snmp6);
    fclose(fp);
    return 0;
}
