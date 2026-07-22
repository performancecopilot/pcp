/*
 * Copyright (c) 2026 Red Hat.
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
 *
 * Handlers for collectl subsystems missing from the original collectl2pcp:
 *   sock   - /proc/net/sockstat  -> network.sockstat.*
 *   fs-ds  - /proc/sys/fs/dentry-state -> vfs.dentry.*
 *   fs-is  - /proc/sys/fs/inode-nr     -> vfs.inodes.*
 *   fs-fnr - /proc/sys/fs/file-nr      -> vfs.files.*
 *   buddy  - /proc/buddyinfo            -> mem.buddyinfo.*
 *   nfsc-  - /proc/net/rpc/nfs         -> nfs.client.*, nfs3.client.*, nfs4.client.*
 *   nfss-  - /proc/net/rpc/nfsd        -> nfs.server.*, nfs3.server.*, nfs4.server.*
 *   numai  - /sys/devices/system/node/nodeN/meminfo -> mem.numa.util.*
 */

#include "metrics.h"

/*
 * sock handler - /proc/net/sockstat lines prefixed with "sock"
 *
 * sock sockets: used 664
 * sock TCP: inuse 24 orphan 0 tw 0 alloc 32 mem 0
 * sock UDP: inuse 6 mem 512
 * sock UDPLITE: inuse 0
 * sock RAW: inuse 0
 * sock FRAG: inuse 0 memory 0
 */
int
sock_handler(handler_t *h, fields_t *f)
{
    if (f->nfields < 3)
	return 0;

    if (strcmp(f->fields[1], "TCP:") == 0 && f->nfields >= 12) {
	/* TCP: inuse N orphan N tw N alloc N mem N */
	put_str_value("network.sockstat.tcp.inuse",  PM_INDOM_NULL, NULL, f->fields[3]);
	put_str_value("network.sockstat.tcp.orphan", PM_INDOM_NULL, NULL, f->fields[5]);
	put_str_value("network.sockstat.tcp.tw",     PM_INDOM_NULL, NULL, f->fields[7]);
	put_str_value("network.sockstat.tcp.alloc",  PM_INDOM_NULL, NULL, f->fields[9]);
	put_str_value("network.sockstat.tcp.mem",    PM_INDOM_NULL, NULL, f->fields[11]);
    }
    else if (strcmp(f->fields[1], "UDP:") == 0 && f->nfields >= 5) {
	/* UDP: inuse N mem N */
	put_str_value("network.sockstat.udp.inuse", PM_INDOM_NULL, NULL, f->fields[3]);
	if (f->nfields >= 7)
	    put_str_value("network.sockstat.udp.mem", PM_INDOM_NULL, NULL, f->fields[5]);
    }
    else if (strcmp(f->fields[1], "UDPLITE:") == 0 && f->nfields >= 3) {
	put_str_value("network.sockstat.udplite.inuse", PM_INDOM_NULL, NULL, f->fields[3]);
    }
    else if (strcmp(f->fields[1], "RAW:") == 0 && f->nfields >= 3) {
	put_str_value("network.sockstat.raw.inuse", PM_INDOM_NULL, NULL, f->fields[3]);
    }
    else if (strcmp(f->fields[1], "FRAG:") == 0 && f->nfields >= 3) {
	put_str_value("network.sockstat.frag.inuse",   PM_INDOM_NULL, NULL, f->fields[3]);
	if (f->nfields >= 5)
	    put_str_value("network.sockstat.frag.memory", PM_INDOM_NULL, NULL, f->fields[5]);
    }
    return 0;
}

/*
 * fs-ds handler - /proc/sys/fs/dentry-state
 * fs-ds 265818 232899 45 0 139870 0
 *        count   free  age want_pages negative 0
 */
int
dentry_handler(handler_t *h, fields_t *f)
{
    if (f->nfields < 2)
	return 0;
    put_str_value("vfs.dentry.count", PM_INDOM_NULL, NULL, f->fields[1]);
    return 0;
}

/*
 * fs-is handler - /proc/sys/fs/inode-nr
 * fs-is 126559 3234
 *        count  free
 */
int
inode_handler(handler_t *h, fields_t *f)
{
    if (f->nfields < 2)
	return 0;
    put_str_value("vfs.inodes.count", PM_INDOM_NULL, NULL, f->fields[1]);
    return 0;
}

/*
 * fs-fnr handler - /proc/sys/fs/file-nr
 * fs-fnr 6493 0 9223372036854775807
 *         count 0 max
 */
int
filenr_handler(handler_t *h, fields_t *f)
{
    if (f->nfields < 2)
	return 0;
    put_str_value("vfs.files.count", PM_INDOM_NULL, NULL, f->fields[1]);
    if (f->nfields >= 4)
	put_str_value("vfs.files.max", PM_INDOM_NULL, NULL, f->fields[3]);
    return 0;
}

/*
 * buddy handler - /proc/buddyinfo lines
 * buddy Node 0, zone      DMA  4  3  3  3  3  2  1  1  0  1  3
 *
 * Instance name: "Node N ZoneName" e.g. "Node 0 DMA"
 * Fields [5..15] are per-order free page counts.
 * We write mem.buddyinfo.total per zone (sum of all order counts * 2^order pages).
 */
int
buddy_handler(handler_t *h, fields_t *f)
{
    char inst[64];
    char node[16];
    pmInDom indom = pmInDom_build(LINUX_DOMAIN, BUDDYINFO_INDOM);
    unsigned long long total = 0;
    char total_str[32];
    int i;

    /* buddy Node N, zone ZONENAME val0 val1 ... */
    /* fields: [0]=buddy [1]=Node [2]=N, [3]=zone [4]=ZONENAME [5..]=counts */
    if (f->nfields < 6 || strcmp(f->fields[3], "zone") != 0)
	return 0;

    /* strip trailing comma from node number */
    pmstrncpy(node, sizeof(node), f->fields[2]);
    {
	char *comma = strchr(node, ',');
	if (comma) *comma = '\0';
    }
    pmsprintf(inst, sizeof(inst), "Node %s %s", node, f->fields[4]);

    put_str_instance(indom, inst);

    /* sum pages weighted by order: pages = count * 2^order */
    for (i = 5; i < f->nfields; i++) {
	unsigned long long count = strtoull(f->fields[i], NULL, 10);
	total += count * (1ULL << (i - 5));
    }
    pmsprintf(total_str, sizeof(total_str), "%llu", total);
    put_str_value("mem.buddyinfo.total", indom, inst, total_str);

    return 0;
}

/*
 * NFS client handler - /proc/net/rpc/nfs lines prefixed "nfsc-"
 *
 * nfsc- net  0 0 0 0
 * nfsc- rpc  99 0 0
 * nfsc- proc2 18 <18-values>   -> nfs.client.calls, nfs.client.reqs[getattr,...]
 * nfsc- proc3 23 <23-values>   -> nfs3.client.calls, ...
 * nfsc- proc4ops 59 <59-values> -> nfs4.client.calls, ...
 */
int
nfsc_handler(handler_t *h, fields_t *f)
{
    if (f->nfields < 3)
	return 0;

    if (strcmp(f->fields[1], "rpc") == 0 && f->nfields >= 4) {
	put_str_value("nfs.client.calls",   PM_INDOM_NULL, NULL, f->fields[2]);
	put_str_value("nfs.client.retrans", PM_INDOM_NULL, NULL, f->fields[3]);
    }
    else if (strcmp(f->fields[1], "proc3") == 0 && f->nfields >= 4) {
	put_str_value("nfs3.client.calls", PM_INDOM_NULL, NULL, f->fields[2]);
    }
    else if (strcmp(f->fields[1], "proc4ops") == 0 && f->nfields >= 4) {
	put_str_value("nfs4.client.calls", PM_INDOM_NULL, NULL, f->fields[2]);
    }
    return 0;
}

/*
 * NFS server handler - /proc/net/rpc/nfsd lines prefixed "nfss-"
 */
int
nfss_handler(handler_t *h, fields_t *f)
{
    if (f->nfields < 3)
	return 0;

    if (strcmp(f->fields[1], "rpc") == 0 && f->nfields >= 4) {
	put_str_value("nfs.server.calls",   PM_INDOM_NULL, NULL, f->fields[2]);
	put_str_value("nfs.server.badcalls", PM_INDOM_NULL, NULL, f->fields[3]);
    }
    else if (strcmp(f->fields[1], "proc3") == 0 && f->nfields >= 4) {
	put_str_value("nfs3.server.calls", PM_INDOM_NULL, NULL, f->fields[2]);
    }
    else if (strcmp(f->fields[1], "proc4ops") == 0 && f->nfields >= 4) {
	put_str_value("nfs4.server.calls", PM_INDOM_NULL, NULL, f->fields[2]);
    }
    return 0;
}

/*
 * numai handler - /sys/devices/system/node/nodeN/meminfo
 * numai MemTotal:       16384 kB
 * numai MemFree:         8192 kB
 * ...
 *
 * We use the same generic1_handler-style approach but with NUMA indom.
 * Instance name is derived from the node number embedded in the header
 * written by collectl ("numai" prefix used for all nodes sequentially).
 * Since we cannot recover the node number from the line itself, we map
 * to the aggregate mem.numa.util.* metrics.
 */
int
numai_handler(handler_t *h, fields_t *f)
{
    /* numai MemKey: value kB */
    if (f->nfields < 3)
	return 0;

    if (strcmp(f->fields[1], "MemTotal:") == 0)
	put_str_value("mem.numa.util.total", PM_INDOM_NULL, NULL, f->fields[2]);
    else if (strcmp(f->fields[1], "MemFree:") == 0)
	put_str_value("mem.numa.util.free", PM_INDOM_NULL, NULL, f->fields[2]);
    else if (strcmp(f->fields[1], "MemUsed:") == 0)
	put_str_value("mem.numa.util.used", PM_INDOM_NULL, NULL, f->fields[2]);
    else if (strcmp(f->fields[1], "Active:") == 0)
	put_str_value("mem.numa.util.active", PM_INDOM_NULL, NULL, f->fields[2]);
    else if (strcmp(f->fields[1], "Inactive:") == 0)
	put_str_value("mem.numa.util.inactive", PM_INDOM_NULL, NULL, f->fields[2]);
    else if (strcmp(f->fields[1], "Dirty:") == 0)
	put_str_value("mem.numa.util.dirty", PM_INDOM_NULL, NULL, f->fields[2]);
    return 0;
}
