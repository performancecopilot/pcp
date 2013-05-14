/*
 * Copyright (c) 2013 Red Hat Inc.
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
 * Mark Goodwin <mgoodwin@redhat.com> May 2013.
 */

#include <ctype.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/pmda.h>
#include <pcp/import.h>

/* domains from stdpmid */
#define LINUX_DOMAIN 60
#define PROC_DOMAIN 3

enum { /* from indom.h in the Linux PMDA */
        CPU_INDOM = 0,          /* 0 - percpu */
        DISK_INDOM,             /* 1 - disks */
        LOADAVG_INDOM,          /* 2 - 1, 5, 15 minute load averages */
        NET_DEV_INDOM,          /* 3 - network interfaces */
        PROC_INTERRUPTS_INDOM,  /* 4 - interrupt lines -> proc PMDA */
        FILESYS_INDOM,          /* 5 - mounted bdev filesystems */
        SWAPDEV_INDOM,          /* 6 - swap devices */
        NFS_INDOM,              /* 7 - nfs operations */
        NFS3_INDOM,             /* 8 - nfs v3 operations */
        PROC_PROC_INDOM,        /* 9 - processes */
        PARTITIONS_INDOM,       /* 10 - disk partitions */
        SCSI_INDOM,             /* 11 - scsi devices */
        SLAB_INDOM,             /* 12 - kernel slabs */
        IB_INDOM,               /* 13 - deprecated: do not re-use */
        NFS4_CLI_INDOM,         /* 14 - nfs v4 client operations */
        NFS4_SVR_INDOM,         /* 15 - nfs n4 server operations */
        QUOTA_PRJ_INDOM,        /* 16 - project quota */
        NET_INET_INDOM,         /* 17 - inet addresses */
        TMPFS_INDOM,            /* 18 - tmpfs mounts */
        NODE_INDOM,             /* 19 - NUMA nodes */
        PROC_CGROUP_SUBSYS_INDOM,       /* 20 - control group subsystems -> proc PMDA */
        PROC_CGROUP_MOUNTS_INDOM,       /* 21 - control group mounts -> proc PMDA */

        NUM_INDOMS              /* one more than highest numbered cluster */
};

typedef struct {
	char *name;
	pmDesc desc;
} metric_t;

typedef struct {
	char *pattern;
	int (*handler)(char *buf);
} handler_t;

/* metric table, see metrics.c (generated from pmdesc) */
extern metric_t metrics[];

/* indom count table */
extern int indom_cnt[NUM_INDOMS];

/* metric value handler table */
extern handler_t handlers[];

/* handlers */
extern int timestamp_flush(void);
extern int timestamp_handler(char *buf);
extern int cpu_handler(char *buf);
extern int disk_handler(char *buf);
extern int net_handler(char *buf);
extern int load_handler(char *buf);

/* various helpers, see util.c */
extern char *strfield_r(char *p, int f, char *r);
extern metric_t *find_metric(char *name);
extern handler_t *find_handler(char *buf);
extern int put_int_value(char *name, int indom, char *instance, int val);
extern int put_str_value(char *name, int indom, char *instance, char *val);
