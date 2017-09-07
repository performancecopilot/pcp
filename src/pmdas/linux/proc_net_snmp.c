/*
 * Copyright (c) 2013-2016 Red Hat.
 * Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
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
#include "linux.h"
#include "proc_net_snmp.h"

extern proc_net_snmp_t	_pm_proc_net_snmp;
extern pmdaInstid _pm_proc_net_snmp_indom_id[];
static char *proc_net_snmp_icmpmsg_names;

typedef struct {
    const char		*field;
    __uint64_t		*offset;
} snmp_fields_t;

snmp_fields_t ip_fields[] = {
    { .field = "Forwarding",
     .offset = &_pm_proc_net_snmp.ip[_PM_SNMP_IP_FORWARDING] },
    { .field = "DefaultTTL",
     .offset = &_pm_proc_net_snmp.ip[_PM_SNMP_IP_DEFAULTTTL] },
    { .field = "InReceives",
     .offset = &_pm_proc_net_snmp.ip[_PM_SNMP_IP_INRECEIVES] },
    { .field = "InHdrErrors",
     .offset = &_pm_proc_net_snmp.ip[_PM_SNMP_IP_INHDRERRORS] },
    { .field = "InAddrErrors",
     .offset = &_pm_proc_net_snmp.ip[_PM_SNMP_IP_INADDRERRORS] },
    { .field = "ForwDatagrams",
     .offset = &_pm_proc_net_snmp.ip[_PM_SNMP_IP_FORWDATAGRAMS] },
    { .field = "InUnknownProtos",
     .offset = &_pm_proc_net_snmp.ip[_PM_SNMP_IP_INUNKNOWNPROTOS] },
    { .field = "InDiscards",
     .offset = &_pm_proc_net_snmp.ip[_PM_SNMP_IP_INDISCARDS] },
    { .field = "InDelivers",
     .offset = &_pm_proc_net_snmp.ip[_PM_SNMP_IP_INDELIVERS] },
    { .field = "OutRequests",
     .offset = &_pm_proc_net_snmp.ip[_PM_SNMP_IP_OUTREQUESTS] },
    { .field = "OutDiscards",
     .offset = &_pm_proc_net_snmp.ip[_PM_SNMP_IP_OUTDISCARDS] },
    { .field = "OutNoRoutes",
     .offset = &_pm_proc_net_snmp.ip[_PM_SNMP_IP_OUTNOROUTES] },
    { .field = "ReasmTimeout",
     .offset = &_pm_proc_net_snmp.ip[_PM_SNMP_IP_REASMTIMEOUT] },
    { .field = "ReasmReqds",
     .offset = &_pm_proc_net_snmp.ip[_PM_SNMP_IP_REASMREQDS] },
    { .field = "ReasmOKs",
     .offset = &_pm_proc_net_snmp.ip[_PM_SNMP_IP_REASMOKS] },
    { .field = "ReasmFails",
     .offset = &_pm_proc_net_snmp.ip[_PM_SNMP_IP_REASMFAILS] },
    { .field = "FragOKs",
     .offset = &_pm_proc_net_snmp.ip[_PM_SNMP_IP_FRAGOKS] },
    { .field = "FragFails",
     .offset = &_pm_proc_net_snmp.ip[_PM_SNMP_IP_FRAGFAILS] },
    { .field = "FragCreates",
     .offset = &_pm_proc_net_snmp.ip[_PM_SNMP_IP_FRAGCREATES] },
    { .field = NULL, .offset = NULL }
};

snmp_fields_t icmp_fields[] = {
    { .field = "InMsgs",
     .offset = &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_INMSGS] },
    { .field = "InErrors",
     .offset = &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_INERRORS] },
    { .field = "InCsumErrors",
     .offset = &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_INCSUMERRORS] },
    { .field = "InDestUnreachs",
     .offset = &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_INDESTUNREACHS] },
    { .field = "InTimeExcds",
     .offset = &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_INTIMEEXCDS] },
    { .field = "InParmProbs",
     .offset = &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_INPARMPROBS] },
    { .field = "InSrcQuenchs",
     .offset = &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_INSRCQUENCHS] },
    { .field = "InRedirects",
     .offset = &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_INREDIRECTS] },
    { .field = "InEchos",
     .offset = &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_INECHOS] },
    { .field = "InEchoReps",
     .offset = &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_INECHOREPS] },
    { .field = "InTimestamps",
     .offset = &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_INTIMESTAMPS] },
    { .field = "InTimestampReps",
     .offset = &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_INTIMESTAMPREPS] },
    { .field = "InAddrMasks",
     .offset = &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_INADDRMASKS] },
    { .field = "InAddrMaskReps",
     .offset = &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_INADDRMASKREPS] },
    { .field = "OutMsgs",
     .offset = &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_OUTMSGS] },
    { .field = "OutErrors",
     .offset = &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_OUTERRORS] },
    { .field = "OutDestUnreachs",
     .offset = &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_OUTDESTUNREACHS] },
    { .field = "OutTimeExcds",
     .offset = &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_OUTTIMEEXCDS] },
    { .field = "OutParmProbs",
     .offset = &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_OUTPARMPROBS] },
    { .field = "OutSrcQuenchs",
     .offset = &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_OUTSRCQUENCHS] },
    { .field = "OutRedirects",
     .offset = &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_OUTREDIRECTS] },
    { .field = "OutEchos",
     .offset = &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_OUTECHOS] },
    { .field = "OutEchoReps",
     .offset = &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_OUTECHOREPS] },
    { .field = "OutTimestamps",
     .offset = &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_OUTTIMESTAMPS] },
    { .field = "OutTimestampReps",
     .offset = &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_OUTTIMESTAMPREPS] },
    { .field = "OutAddrMasks",
     .offset = &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_OUTADDRMASKS] },
    { .field = "OutAddrMaskReps",
     .offset = &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_OUTADDRMASKREPS] },
    { .field = NULL, .offset = NULL }
};

snmp_fields_t icmpmsg_fields[] = {
    { .field = "InType%u",
     .offset = &_pm_proc_net_snmp.icmpmsg[_PM_SNMP_ICMPMSG_INTYPE] },
    { .field = "OutType%u",
     .offset = &_pm_proc_net_snmp.icmpmsg[_PM_SNMP_ICMPMSG_OUTTYPE] },
    { .field = NULL, .offset = NULL }
};

snmp_fields_t tcp_fields[] = {
    { .field = "RtoAlgorithm",
     .offset = &_pm_proc_net_snmp.tcp[_PM_SNMP_TCP_RTOALGORITHM] },
    { .field = "RtoMin",
     .offset = &_pm_proc_net_snmp.tcp[_PM_SNMP_TCP_RTOMIN] },
    { .field = "RtoMax",
     .offset = &_pm_proc_net_snmp.tcp[_PM_SNMP_TCP_RTOMAX] },
    { .field = "MaxConn",
     .offset = &_pm_proc_net_snmp.tcp[_PM_SNMP_TCP_MAXCONN] },
    { .field = "ActiveOpens",
     .offset = &_pm_proc_net_snmp.tcp[_PM_SNMP_TCP_ACTIVEOPENS] },
    { .field = "PassiveOpens",
     .offset = &_pm_proc_net_snmp.tcp[_PM_SNMP_TCP_PASSIVEOPENS] },
    { .field = "AttemptFails",
     .offset = &_pm_proc_net_snmp.tcp[_PM_SNMP_TCP_ATTEMPTFAILS] },
    { .field = "EstabResets",
     .offset = &_pm_proc_net_snmp.tcp[_PM_SNMP_TCP_ESTABRESETS] },
    { .field = "CurrEstab",
     .offset = &_pm_proc_net_snmp.tcp[_PM_SNMP_TCP_CURRESTAB] },
    { .field = "InSegs",
     .offset = &_pm_proc_net_snmp.tcp[_PM_SNMP_TCP_INSEGS] },
    { .field = "OutSegs",
     .offset = &_pm_proc_net_snmp.tcp[_PM_SNMP_TCP_OUTSEGS] },
    { .field = "RetransSegs",
     .offset = &_pm_proc_net_snmp.tcp[_PM_SNMP_TCP_RETRANSSEGS] },
    { .field = "InErrs",
     .offset = &_pm_proc_net_snmp.tcp[_PM_SNMP_TCP_INERRS] },
    { .field = "OutRsts",
     .offset = &_pm_proc_net_snmp.tcp[_PM_SNMP_TCP_OUTRSTS] },
    { .field = "InCsumErrors",
     .offset = &_pm_proc_net_snmp.tcp[_PM_SNMP_TCP_INCSUMERRORS] },
    { .field = NULL, .offset = NULL }
};

snmp_fields_t udp_fields[] = {
    { .field = "InDatagrams",
     .offset = &_pm_proc_net_snmp.udp[_PM_SNMP_UDP_INDATAGRAMS] },
    { .field = "NoPorts",
     .offset = &_pm_proc_net_snmp.udp[_PM_SNMP_UDP_NOPORTS] },
    { .field = "InErrors",
     .offset = &_pm_proc_net_snmp.udp[_PM_SNMP_UDP_INERRORS] },
    { .field = "OutDatagrams",
     .offset = &_pm_proc_net_snmp.udp[_PM_SNMP_UDP_OUTDATAGRAMS] },
    { .field = "RcvbufErrors",
     .offset = &_pm_proc_net_snmp.udp[_PM_SNMP_UDP_RECVBUFERRORS] },
    { .field = "SndbufErrors",
     .offset = &_pm_proc_net_snmp.udp[_PM_SNMP_UDP_SNDBUFERRORS] },
    { .field = "InCsumErrors",
     .offset = &_pm_proc_net_snmp.udp[_PM_SNMP_UDP_INCSUMERRORS] },
    { .field = NULL, .offset = NULL }
};

snmp_fields_t udplite_fields[] = {
    { .field = "InDatagrams",
     .offset = &_pm_proc_net_snmp.udplite[_PM_SNMP_UDPLITE_INDATAGRAMS] },
    { .field = "NoPorts",
     .offset = &_pm_proc_net_snmp.udplite[_PM_SNMP_UDPLITE_NOPORTS] },
    { .field = "InErrors",
     .offset = &_pm_proc_net_snmp.udplite[_PM_SNMP_UDPLITE_INERRORS] },
    { .field = "OutDatagrams",
     .offset = &_pm_proc_net_snmp.udplite[_PM_SNMP_UDPLITE_OUTDATAGRAMS] },
    { .field = "RcvbufErrors",
     .offset = &_pm_proc_net_snmp.udplite[_PM_SNMP_UDPLITE_RECVBUFERRORS] },
    { .field = "SndbufErrors",
     .offset = &_pm_proc_net_snmp.udplite[_PM_SNMP_UDPLITE_SNDBUFERRORS] },
    { .field = "InCsumErrors",
     .offset = &_pm_proc_net_snmp.udplite[_PM_SNMP_UDPLITE_INCSUMERRORS] },
    { .field = NULL, .offset = NULL }
};

static void
get_fields(snmp_fields_t *fields, char *header, char *buffer)
{
    int i, j, count;
    char *p, *indices[SNMP_MAX_COLUMNS];

    /* first get pointers to each of the column headings */
    strtok(header, " ");
    for (i = 0; i < SNMP_MAX_COLUMNS; i++) {
	if ((p = strtok(NULL, " \n")) == NULL)
	    break;
	indices[i] = p;
    }
    count = i;

    /*
     * Extract values via back-referencing column headings.
     * "i" is the last found index, which we use for a bit
     * of optimisation for the (common) in-order maps case
     * (where "in order" means in the order defined by the
     * passed in "fields" table which typically matches the
     * kernel - but may be out-of-order for older kernels).
     */
    strtok(buffer, " ");
    for (i = j = 0; j < count && fields[i].field; j++, i++) {
        if ((p = strtok(NULL, " \n")) == NULL)
            break;
        if (strcmp(fields[i].field, indices[j]) == 0) {
            *fields[i].offset = strtoull(p, NULL, 10);
        } else {
            for (i = 0; fields[i].field; i++) {
                if (strcmp(fields[i].field, indices[j]) != 0)
                    continue;
                *fields[i].offset = strtoull(p, NULL, 10);
                break;
            }
	    if (fields[i].field == NULL) /* not found, ignore */
		i = 0;
	}
    }
}

static void
get_ordinal_fields(snmp_fields_t *fields, char *header, char *buffer,
                   unsigned limit)
{
    int i, j, count;
    unsigned int inst;
    char *p, *indices[SNMP_MAX_COLUMNS];

    strtok(header, " ");
    for (i = 0; i < SNMP_MAX_COLUMNS; i++) {
	if ((p = strtok(NULL, " \n")) == NULL)
	    break;
	indices[i] = p;
    }
    count = i;

    strtok(buffer, " ");
    for (j = 0; j < count; j++) {
        if ((p = strtok(NULL, " \n")) == NULL)
            break;
        for (i = 0; fields[i].field; i++) {
            if (sscanf(indices[j], fields[i].field, &inst) != 1)
                continue;
            if (inst >= limit)
                continue;
            *(fields[i].offset + inst) = strtoull(p, NULL, 10);
            break;
	}
    }
}

#define SNMP_IP_OFFSET(ii, pp) (int64_t *)((char *)pp + \
    (__psint_t)ip_fields[ii].offset - (__psint_t)&_pm_proc_net_snmp.ip)
#define SNMP_ICMP_OFFSET(ii, pp) (int64_t *)((char *)pp + \
    (__psint_t)icmp_fields[ii].offset - (__psint_t)&_pm_proc_net_snmp.icmp)
#define SNMP_ICMPMSG_OFFSET(ii, nn, pp) (int64_t *)((char *)pp + \
    (__psint_t)(icmpmsg_fields[ii].offset + nn) - (__psint_t)&_pm_proc_net_snmp.icmpmsg)
#define SNMP_TCP_OFFSET(ii, pp) (int64_t *)((char *)pp + \
    (__psint_t)tcp_fields[ii].offset - (__psint_t)&_pm_proc_net_snmp.tcp)
#define SNMP_UDP_OFFSET(ii, pp) (int64_t *)((char *)pp + \
    (__psint_t)udp_fields[ii].offset - (__psint_t)&_pm_proc_net_snmp.udp)
#define SNMP_UDPLITE_OFFSET(ii, pp) (int64_t *)((char *)pp + \
    (__psint_t)udplite_fields[ii].offset - (__psint_t)&_pm_proc_net_snmp.udplite)

static void
init_refresh_proc_net_snmp(proc_net_snmp_t *snmp)
{
    pmdaIndom	*idp;
    char	*s;
    int		i, n;

    /* initially, all marked as "no value available" */
    for (i = 0; ip_fields[i].field != NULL; i++)
	*(SNMP_IP_OFFSET(i, snmp->ip)) = -1;
    for (i = 0; icmp_fields[i].field != NULL; i++)
	*(SNMP_ICMP_OFFSET(i, snmp->icmp)) = -1;
    for (i = 0; tcp_fields[i].field != NULL; i++)
	*(SNMP_TCP_OFFSET(i, snmp->tcp)) = -1;
    for (i = 0; udp_fields[i].field != NULL; i++)
	*(SNMP_UDP_OFFSET(i, snmp->udp)) = -1;
    for (i = 0; udplite_fields[i].field != NULL; i++)
	*(SNMP_UDPLITE_OFFSET(i, snmp->udplite)) = -1;
    for (i = 0; icmpmsg_fields[i].field != NULL; i++)
	for (n = 0; n < NR_ICMPMSG_COUNTERS; n++)
	    *(SNMP_ICMPMSG_OFFSET(i, n, snmp->icmpmsg)) = -1;

    /* only need to allocate and setup the names once */
    if (proc_net_snmp_icmpmsg_names)
	return;
    s = calloc(NR_ICMPMSG_COUNTERS, SNMP_MAX_ICMPMSG_TYPESTR);
    if (!s)
	return;
    proc_net_snmp_icmpmsg_names = s;
    for (n = 0; n < NR_ICMPMSG_COUNTERS; n++) {
	pmsprintf(s, SNMP_MAX_ICMPMSG_TYPESTR, "Type%u", n);
	_pm_proc_net_snmp_indom_id[n].i_name = s;
	_pm_proc_net_snmp_indom_id[n].i_inst = n;
	s += SNMP_MAX_ICMPMSG_TYPESTR;
    }
    idp = PMDAINDOM(ICMPMSG_INDOM);
    idp->it_numinst = NR_ICMPMSG_COUNTERS;
    idp->it_set = _pm_proc_net_snmp_indom_id;
}

int
refresh_proc_net_snmp(proc_net_snmp_t *snmp)
{
    char	buf[MAXPATHLEN];
    char	header[1024];
    FILE	*fp;

    init_refresh_proc_net_snmp(snmp);
    if ((fp = linux_statsfile("/proc/net/snmp", buf, sizeof(buf))) == NULL)
	return -oserror();
    while (fgets(header, sizeof(header), fp) != NULL) {
	if (fgets(buf, sizeof(buf), fp) != NULL) {
	    if (strncmp(buf, "Ip:", 3) == 0)
		get_fields(ip_fields, header, buf);
	    else if (strncmp(buf, "Icmp:", 5) == 0)
		get_fields(icmp_fields, header, buf);
	    else if (strncmp(buf, "IcmpMsg:", 8) == 0)
		get_ordinal_fields(icmpmsg_fields, header, buf,
                                   NR_ICMPMSG_COUNTERS);
	    else if (strncmp(buf, "Tcp:", 4) == 0)
		get_fields(tcp_fields, header, buf);
	    else if (strncmp(buf, "Udp:", 4) == 0)
		get_fields(udp_fields, header, buf);
	    else if (strncmp(buf, "UdpLite:", 8) == 0)
		get_fields(udplite_fields, header, buf);
	    else
	    	fprintf(stderr, "Error: unrecognised snmp row: %s", buf);
	}
    }
    fclose(fp);
    return 0;
}
