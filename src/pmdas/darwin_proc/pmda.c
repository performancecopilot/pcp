/*
 * Darwin proc PMDA (macOS per-process metrics)
 *
 * Copyright (c) 2025 Red Hat.
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
 * PMDA Debug Flags
 * -Dauth	authorization operations
 */

#include "pmapi.h"
#include "libpcp.h"
#include "pmda.h"
#include "domain.h"
#include "contexts.h"

#include <ctype.h>
#include <sys/syslog.h>
#include <unistd.h>
#include <sys/stat.h>
#include <utmp.h>
#include <pwd.h>
#include <grp.h>

#include "kinfo_proc.h"

/* globals */
static int			_isDSO = 1;	/* =0 I am a daemon */
static darwin_procs_t		procs;		/* all processes hash */
static darwin_runq_t		run_queue;	/* scheduler metrics */
static int			all_access;	/* =1 no access checks */
static int			have_access;	/* =1 recvd uid/gid */
static int			autogroup = -1;	/* =1 autogroup enabled */
static unsigned int		threads;	/* control.all.threads */

static char *proc_gidname_lookup(int);
static char *proc_uidname_lookup(int);
static char *proc_ttyname_lookup(dev_t);

#define INDOM(i) proc_indom(i)

/*
 * The proc instance domain table is direct lookup and sparse.
 * It is initialized in proc_init(), see below.
 */
enum {
    PROC_INDOM,
    NUM_INDOMS
};
static pmdaIndom indomtab[NUM_INDOMS];

/* "internal" indoms */
enum {
    STRINGS_INDOM = NUM_INDOMS, /* string values */
    UIDNAME_INDOM,		/* uid:name hash */
    GIDNAME_INDOM,		/* gid:name hash */
    TTYNAME_INDOM,		/* tty:name hash */
    NUM_INTERNAL_INDOMS
};

/*
 * all metrics supported in this PMDA - one table entry for each
 */
enum {
    CLUSTER_CONTROL = 0,
    CLUSTER_PROCS,
    CLUSTER_PROC_RUNQ,
    CLUSTER_PROC_ID,
    CLUSTER_PROC_MEM,
    CLUSTER_PROC_IO,
    CLUSTER_PROC_FD,
    NUM_CLUSTERS
};
static pmdaMetric metrictab[] = {
/* proc.nprocs */
  { NULL, { PMDA_PMID(CLUSTER_PROCS, 0), PM_TYPE_U32, PM_INDOM_NULL,
    PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) } },
/* proc.psinfo.pid */
  { NULL, { PMDA_PMID(CLUSTER_PROCS, 1), PM_TYPE_U32, PROC_INDOM,
    PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) } },
/* proc.psinfo.ppid */
  { NULL, { PMDA_PMID(CLUSTER_PROCS, 2), PM_TYPE_U32, PROC_INDOM,
    PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) } },
/* proc.psinfo.pgid */
  { NULL, { PMDA_PMID(CLUSTER_PROCS, 3), PM_TYPE_U32, PROC_INDOM,
    PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) } },
/* proc.psinfo.tpgid */
  { NULL, { PMDA_PMID(CLUSTER_PROCS, 4), PM_TYPE_U32, PROC_INDOM,
    PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) } },
/* proc.psinfo.cwd */
  { NULL, { PMDA_PMID(CLUSTER_PROCS, 5), PM_TYPE_STRING, PROC_INDOM,
    PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) } },
/* proc.psinfo.cmd */
  { NULL, { PMDA_PMID(CLUSTER_PROCS, 6), PM_TYPE_STRING, PROC_INDOM,
    PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) } },
/* proc.psinfo.exe */
  { NULL, { PMDA_PMID(CLUSTER_PROCS, 7), PM_TYPE_STRING, PROC_INDOM,
    PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) } },
/* proc.psinfo.nice */
  { NULL, { PMDA_PMID(CLUSTER_PROCS, 8), PM_TYPE_32, PROC_INDOM,
    PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) } },
/* proc.psinfo.majflt */
  { NULL, { PMDA_PMID(CLUSTER_PROCS, 9), PM_TYPE_U32, PROC_INDOM,
    PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
/* proc.psinfo.priority */
  { NULL, { PMDA_PMID(CLUSTER_PROCS, 10), PM_TYPE_32, PROC_INDOM,
    PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) } },
/* proc.psinfo.psargs */
  { NULL, { PMDA_PMID(CLUSTER_PROCS, 11), PM_TYPE_STRING, PROC_INDOM,
    PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0)}},
/* proc.psinfo.sname */
  { NULL, { PMDA_PMID(CLUSTER_PROCS, 12), PM_TYPE_STRING, PROC_INDOM,
    PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0)}},
/* proc.psinfo.start_time */
  { NULL, { PMDA_PMID(CLUSTER_PROCS, 13), PM_TYPE_U64, PROC_INDOM,
    PM_SEM_DISCRETE, PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) } },
/* proc.psinfo.utime */
  { NULL, { PMDA_PMID(CLUSTER_PROCS, 14), PM_TYPE_U64, PROC_INDOM,
    PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) } },
/* proc.psinfo.stime */
  { NULL, { PMDA_PMID(CLUSTER_PROCS, 15), PM_TYPE_U64, PROC_INDOM,
    PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) } },
/* proc.psinfo.threads */
  { NULL, { PMDA_PMID(CLUSTER_PROCS, 16), PM_TYPE_U32, PROC_INDOM,
    PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0)}},
/* proc.psinfo.translated */
  { NULL, { PMDA_PMID(CLUSTER_PROCS, 17), PM_TYPE_U32, PROC_INDOM,
    PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0)}},
/* proc.psinfo.tty */
  { NULL, { PMDA_PMID(CLUSTER_PROCS, 18), PM_TYPE_U32, PROC_INDOM,
    PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) } },
/* proc.psinfo.ttyname */
  { NULL, { PMDA_PMID(CLUSTER_PROCS, 19), PM_TYPE_STRING, PROC_INDOM,
    PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) } },
/* proc.psinfo.wchan */
  { NULL, { PMDA_PMID(CLUSTER_PROCS, 20), PM_TYPE_U64, PROC_INDOM,
    PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) } },
/* proc.psinfo.wchan_s */
  { NULL, { PMDA_PMID(CLUSTER_PROCS, 21), PM_TYPE_STRING, PROC_INDOM,
    PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0)}},
/* proc.psinfo.wmesg_s */
  { NULL, { PMDA_PMID(CLUSTER_PROCS, 22), PM_TYPE_STRING, PROC_INDOM,
    PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0)}},
/* proc.psinfo.pswitch */
  { NULL, { PMDA_PMID(CLUSTER_PROCS, 23), PM_TYPE_U64, PROC_INDOM,
    PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
/* proc.psinfo.usrpri */
  { NULL, { PMDA_PMID(CLUSTER_PROCS, 24), PM_TYPE_32, PROC_INDOM,
    PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.runq.runnable */
  { &run_queue.runnable,
    { PMDA_PMID(CLUSTER_PROC_RUNQ, 0), PM_TYPE_32, PM_INDOM_NULL,
    PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
/* proc.runq.blocked */
  { &run_queue.blocked,
    { PMDA_PMID(CLUSTER_PROC_RUNQ, 1), PM_TYPE_32, PM_INDOM_NULL,
    PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
/* proc.runq.sleeping */
  { &run_queue.sleeping,
    { PMDA_PMID(CLUSTER_PROC_RUNQ, 2), PM_TYPE_32, PM_INDOM_NULL,
    PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
/* proc.runq.stopped */
  { &run_queue.stopped,
    { PMDA_PMID(CLUSTER_PROC_RUNQ, 3), PM_TYPE_32, PM_INDOM_NULL,
    PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
/* proc.runq.swapped */
  { &run_queue.swapped,
    { PMDA_PMID(CLUSTER_PROC_RUNQ, 4), PM_TYPE_32, PM_INDOM_NULL,
    PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
/* proc.runq.defunct */
  { &run_queue.defunct,
    { PMDA_PMID(CLUSTER_PROC_RUNQ, 5), PM_TYPE_32, PM_INDOM_NULL,
    PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
/* proc.runq.unknown */
  { &run_queue.unknown,
    { PMDA_PMID(CLUSTER_PROC_RUNQ, 6), PM_TYPE_32, PM_INDOM_NULL,
    PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
/* proc.runq.kernel */
  { &run_queue.kernel,
    { PMDA_PMID(CLUSTER_PROC_RUNQ, 7), PM_TYPE_32, PM_INDOM_NULL,
    PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* proc.id.uid */
  { NULL, { PMDA_PMID(CLUSTER_PROC_ID, 0), PM_TYPE_U32, PROC_INDOM,
    PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0)}},
/* proc.id.euid */
  { NULL, { PMDA_PMID(CLUSTER_PROC_ID, 1), PM_TYPE_U32, PROC_INDOM,
    PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0)}},
/* proc.id.suid */
  { NULL, { PMDA_PMID(CLUSTER_PROC_ID, 2), PM_TYPE_U32, PROC_INDOM,
    PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0)}},
/* proc.id.gid */
  { NULL, { PMDA_PMID(CLUSTER_PROC_ID, 3), PM_TYPE_U32, PROC_INDOM,
    PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0)}},
/* proc.id.uid_nm */
  { NULL, { PMDA_PMID(CLUSTER_PROC_ID, 4), PM_TYPE_STRING, PROC_INDOM,
    PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0)}},
/* proc.id.euid_nm */
  { NULL, { PMDA_PMID(CLUSTER_PROC_ID, 5), PM_TYPE_STRING, PROC_INDOM,
    PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0)}},
/* proc.id.suid_nm */
  { NULL, { PMDA_PMID(CLUSTER_PROC_ID, 6), PM_TYPE_STRING, PROC_INDOM,
    PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0)}},
/* proc.id.gid_nm */
  { NULL, { PMDA_PMID(CLUSTER_PROC_ID, 7), PM_TYPE_STRING, PROC_INDOM,
    PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0)}},
/* proc.id.ngid */
  { NULL, { PMDA_PMID(CLUSTER_PROC_ID, 8), PM_TYPE_U32, PROC_INDOM,
    PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.memory.size */
  { NULL, { PMDA_PMID(CLUSTER_PROC_MEM, 0), PM_TYPE_U64, PROC_INDOM,
    PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },
/* proc.memory.rss */
  { NULL, { PMDA_PMID(CLUSTER_PROC_MEM, 1), PM_TYPE_U64, PROC_INDOM,
    PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },
/* proc.memory.footprint */
  { NULL, { PMDA_PMID(CLUSTER_PROC_MEM, 2), PM_TYPE_U64, PROC_INDOM,
    PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },

/* proc.io.read_bytes */
  { NULL, { PMDA_PMID(CLUSTER_PROC_IO, 0), PM_TYPE_U64, PROC_INDOM,
    PM_SEM_COUNTER, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },
/* proc.io.write_bytes */
  { NULL, { PMDA_PMID(CLUSTER_PROC_IO, 1), PM_TYPE_U64, PROC_INDOM,
    PM_SEM_COUNTER, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },
/* proc.io.logical_writes */
  { NULL, { PMDA_PMID(CLUSTER_PROC_IO, 2), PM_TYPE_U64, PROC_INDOM,
    PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* proc.fd.count */
  { NULL, { PMDA_PMID(CLUSTER_PROC_FD, 0), PM_TYPE_U32, PROC_INDOM,
    PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) } },

/*
 * Metrics control cluster
 */

/* proc.control.all.threads */
  { &threads,
    { PMDA_PMID(CLUSTER_CONTROL, 0), PM_TYPE_U32, PM_INDOM_NULL,
    PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) } },
/* proc.control.perclient.threads */
  { NULL, { PMDA_PMID(CLUSTER_CONTROL, 1), PM_TYPE_U32, PM_INDOM_NULL,
    PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) } },
};

pmInDom
proc_indom(int serial)
{
    return indomtab[serial].it_indom;
}

static int
proc_refresh(pmdaExt *pmda, int *need_refresh)
{
    if (need_refresh[CLUSTER_PROCS] ||
	need_refresh[CLUSTER_PROC_RUNQ] ||
	need_refresh[CLUSTER_PROC_ID] ||
	need_refresh[CLUSTER_PROC_MEM] ||
	need_refresh[CLUSTER_PROC_IO] ||
	need_refresh[CLUSTER_PROC_FD]) {
	darwin_refresh_processes(&indomtab[PROC_INDOM], &procs, &run_queue,
			proc_ctx_threads(pmda->e_context, threads));
    }
    return 0;
}

static int
proc_instance(pmInDom indom, int inst, char *name, pmInResult **result, pmdaExt *pmda)
{
    unsigned int	serial = pmInDom_serial(indom);
    int			need_refresh[NUM_CLUSTERS] = { 0 };
    char		newname[16];		/* see Note below */
    int			sts;

    switch (serial) {
    case PROC_INDOM:
        need_refresh[CLUSTER_PROCS]++;
	need_refresh[CLUSTER_PROC_RUNQ]++;
	need_refresh[CLUSTER_PROC_ID]++;
	need_refresh[CLUSTER_PROC_MEM]++;
	need_refresh[CLUSTER_PROC_IO]++;
	need_refresh[CLUSTER_PROC_FD]++;
        break;
    }

    if (serial == PROC_INDOM && inst == PM_IN_NULL && name != NULL) {
    	/*
	 * For the proc indoms if the name is a pid (as a string), and it
	 * contains only digits (i.e. it's not a full instance name) then
	 * reformat it to be exactly six digits, with leading zeros.
	 *
	 * Note that although format %06d is used here and kinfo_proc.c,
	 *      the PID could be longer than this (in which case there
	 *      are no leading zeroes.  The size of newname[] is chosen
	 *      to comfortably accommodate a 32-bit PID or maximum value
	 *      of 4294967295 (10 digits)
	 */
	char *p;
	for (p = name; *p != '\0'; p++) {
	    if (!isdigit((int)*p))
	    	break;
	}
	if (*p == '\0') {
	    pmsprintf(newname, sizeof(newname), "%06d", atoi(name));
	    name = newname;
	}
    }

    sts = PM_ERR_PERMISSION;
    have_access = all_access || proc_ctx_access(pmda->e_context);
    if (pmDebugOptions.auth)
	fprintf(stderr, "%s: start access have=%d all=%d proc_ctx_access=%d\n",
		__FUNCTION__, have_access, all_access,
		proc_ctx_access(pmda->e_context));

    if (have_access || serial != PROC_INDOM) {
	if ((sts = proc_refresh(pmda, need_refresh)) == 0)
	    sts = pmdaInstance(indom, inst, name, result, pmda);
    }

    have_access = all_access || proc_ctx_revert(pmda->e_context);
    if (pmDebugOptions.auth)
	fprintf(stderr, "%s: final access have=%d all=%d proc_ctx_revert=%d\n",
		__FUNCTION__, have_access, all_access,
		proc_ctx_revert(pmda->e_context));

    return sts;
}

static darwin_proc_t *
darwin_proc_lookup(unsigned int inst)
{
    __pmHashNode	*node;

    if ((node = __pmHashSearch(inst, &procs)))
	return (darwin_proc_t *)node->data;
    return NULL;
}

/*
 * callback provided to pmdaFetch
 */

static int
proc_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    unsigned int	cluster = pmID_cluster(mdesc->m_desc.pmid);
    unsigned int	item = pmID_item(mdesc->m_desc.pmid);
    darwin_proc_t	*proc;
    static char		psbuf[PROC_CMD_MAXLEN];
    char		*cp;

    if (mdesc->m_user != NULL) {
	/* 
	 * The metric value is extracted directly via the address specified
	 * in metrictab.  Note: not many metrics support this - those that
	 * don't have NULL for the m_user field in their metrictab slot.
	 */
	switch (mdesc->m_desc.type) {
	case PM_TYPE_32:
	    atom->l = *(__int32_t *)mdesc->m_user;
	    break;
	case PM_TYPE_U32:
	    atom->ul = *(__uint32_t *)mdesc->m_user;
	    break;
	case PM_TYPE_64:
	    atom->ll = *(__int64_t *)mdesc->m_user;
	    break;
	case PM_TYPE_U64:
	    atom->ull = *(__uint64_t *)mdesc->m_user;
	    break;
	case PM_TYPE_FLOAT:
	    atom->f = *(float *)mdesc->m_user;
	    break;
	case PM_TYPE_DOUBLE:
	    atom->d = *(double *)mdesc->m_user;
	    break;
	case PM_TYPE_STRING:
	    cp = *(char **)mdesc->m_user;
	    atom->cp = (char *)(cp ? cp : "");
	    break;
	default:
	    return 0;
	}
    }
    else
    switch (cluster) {
    case CLUSTER_PROCS:
	if (item == 0) { /* proc.nprocs */
	    atom->ul = procs.nodes;
	    break;
	}
	if (!have_access)
	    return PM_ERR_PERMISSION;
	if ((proc = darwin_proc_lookup(inst)) == NULL)
	    return 0;

	switch (item) {
	case 1: /* proc.psinfo.pid */
	    atom->ul = proc->id;
	    break;
	case 2: /* proc.psinfo.ppid */
	    atom->ul = proc->ppid;
	    break;
	case 3: /* proc.psinfo.pgid */
	    atom->ul = proc->pgid;
	    break;
	case 4: /* proc.psinfo.tpgid */
	    atom->ul = proc->tpgid;
	    break;
	case 5: /* proc.psinfo.cwd */
	    if ((atom->cp = proc_strings_lookup(proc->cwd_id)) == NULL)
		atom->cp = "";
	    break;
	case 6: /* proc.psinfo.cmd */
	    if ((atom->cp = proc_strings_lookup(proc->cmd_id)) == NULL)
		atom->cp = "";
	    break;
	case 7: /* proc.psinfo.exe */
	    if ((atom->cp = proc_strings_lookup(proc->exe_id)) == NULL)
		atom->cp = "";
	    break;
	case 8: /* proc.psinfo.nice */
	    atom->l = proc->nice;
	    break;
	case 9: /* proc.psinfo.majflt */
	    atom->ul = proc->majflt;
	    break;
	case 10: /* proc.psinfo.priority */
	    atom->l = proc->priority;
	    break;
	case 11: /* proc.psinfo.psargs */
	    if ((cp = proc_strings_lookup(proc->cmd_id)) != NULL) {
		if (*cp == '/') { /* full path + args, use this one directly */
		    atom->cp = cp;
		    break;
		}
		/* relative path + args, try to find the full path elsewhere */
		if ((cp = proc_strings_lookup(proc->exe_id)) == NULL)
		    cp = proc->comm;
		if (proc->psargs == NULL) {
		    atom->cp = cp;
		} else {
		    pmsprintf(psbuf, sizeof(psbuf), "%s %s", cp, proc->psargs);
		    atom->cp = psbuf;
		}
	    } else {
		if ((cp = proc_strings_lookup(proc->exe_id)) == NULL)
		    cp = proc->comm;
		atom->cp = cp;
	    }
	    break;
	case 12: /* proc.psinfo.sname */
	    atom->cp = proc->state;
	    break;
	case 13: /* proc.psinfo.start_time */
	    atom->ull = proc->start_time;
	    break;
	case 14: /* proc.psinfo.utime */
	    atom->ull = proc->utime;
	    break;
	case 15: /* proc.psinfo.stime */
	    atom->ull = proc->stime;
	    break;
	case 16: /* proc.psinfo.threads */
	    atom->ul = proc->threads;
	    break;
	case 17: /* proc.psinfo.translated */
	    atom->ul = proc->translated;
	    break;
	case 18: /* proc.psinfo.tty */
	    atom->ul = proc->tty;
	    break;
	case 19: /* proc.psinfo.ttyname */
	    atom->cp = proc_ttyname_lookup(proc->tty);
	    break;
	case 20: /* proc.psinfo.wchan */
	    atom->ull = proc->wchan_addr;
	    break;
	case 21: /* proc.psinfo.wchan_s */
	    atom->cp = proc->wchan;
	    break;
	case 22: /* proc.psinfo.wmesg_s */
	    if ((atom->cp = proc_strings_lookup(proc->msg_id)) == NULL)
		atom->cp = "";
	    break;
	case 23: /* proc.psinfo.pswitch */
	    atom->ull = proc->pswitch;
	    break;
	case 24: /* proc.psinfo.usrpri */
	    atom->l = proc->usrpri;
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    case CLUSTER_PROC_ID:
	if (!have_access)
	    return PM_ERR_PERMISSION;
	if ((proc = darwin_proc_lookup(inst)) == NULL)
	    return 0;

	switch (item) {
	case 0: /* proc.id.uid */
	    atom->ul = proc->uid;
	    break;
	case 1: /* proc.id.euid */
	    atom->ul = proc->euid;
	    break;
	case 2: /* proc.id.suid */
	    atom->ul = proc->suid;
	    break;
	case 3: /* proc.id.gid */
	    atom->ul = proc->uid;
	    break;
	case 4: /* proc.id.uid_nm */
	    atom->cp = proc_uidname_lookup(proc->uid);
	    break;
	case 5: /* proc.id.euid_nm */
	    atom->cp = proc_uidname_lookup(proc->euid);
	    break;
	case 6: /* proc.id.suid_nm */
	    atom->cp = proc_uidname_lookup(proc->suid);
	    break;
	case 7: /* proc.id.gid_nm */
	    atom->cp = proc_gidname_lookup(proc->gid);
	    break;
	case 8: /* proc.psinfo.ngid */
	    atom->ul = proc->ngid;
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    case CLUSTER_PROC_MEM:
	if (!have_access)
	    return PM_ERR_PERMISSION;
	if ((proc = darwin_proc_lookup(inst)) == NULL)
	    return 0;
	switch (item) {
	case 0: /* proc.memory.size */
	    atom->ull = proc->size;
	    break;
	case 1: /* proc.memory.rss */
	    atom->ull = proc->rss;
	    break;
	case 2: /* proc.memory.footprint */
	    atom->ull = proc->phys_footprint;
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    case CLUSTER_PROC_IO:
	if (!have_access)
	    return PM_ERR_PERMISSION;
	if ((proc = darwin_proc_lookup(inst)) == NULL)
	    return 0;
	switch (item) {
	case 0: /* proc.io.read_bytes */
	    atom->ull = proc->read_bytes;
	    break;
	case 1: /* proc.io.write_bytes */
	    atom->ull = proc->write_bytes;
	    break;
	case 2: /* proc.io.logical_writes */
	    atom->ull = proc->logical_writes;
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    case CLUSTER_PROC_FD:
	if (!have_access)
	    return PM_ERR_PERMISSION;
	if ((proc = darwin_proc_lookup(inst)) == NULL)
	    return 0;
	switch (item) {
	case 0: /* proc.fd.count */
	    atom->ul = proc->fd_count;
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    case CLUSTER_CONTROL:
	switch (item) {
	/* case 0: not reached -- proc.control.all.threads is direct */
	case 1:	/* proc.control.perclient.threads */
	    atom->ul = proc_ctx_threads(pmdaGetContext(), threads);
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    default: /* unknown cluster */
	return PM_ERR_PMID;
    }

    return PMDA_FETCH_STATIC;
}

static int
proc_fetch(int numpmid, pmID pmidlist[], pmdaResult **resp, pmdaExt *pmda)
{
    int			i, sts, need_refresh[NUM_CLUSTERS] = { 0 };

    for (i = 0; i < numpmid; i++) {
	unsigned int	cluster = pmID_cluster(pmidlist[i]);

	if (cluster < NUM_CLUSTERS)
	    need_refresh[cluster]++;
    }
    autogroup = -1;	/* reset, state not known for this fetch */

    have_access = all_access || proc_ctx_access(pmda->e_context);
    if (pmDebugOptions.auth)
	fprintf(stderr, "%s: start access have=%d all=%d proc_ctx_access=%d\n",
		__FUNCTION__, have_access, all_access,
		proc_ctx_access(pmda->e_context));

    if ((sts = proc_refresh(pmda, need_refresh)) == 0)
	sts = pmdaFetch(numpmid, pmidlist, resp, pmda);

    have_access = all_access || proc_ctx_revert(pmda->e_context);
    if (pmDebugOptions.auth)
	fprintf(stderr, "%s: final access have=%d all=%d proc_ctx_revert=%d\n",
		__FUNCTION__, have_access, all_access,
		proc_ctx_revert(pmda->e_context));

    return sts;
}

static int
proc_store(pmdaResult *result, pmdaExt *pmda)
{
    int			i, sts = 0;

    have_access = all_access || proc_ctx_access(pmda->e_context);

    for (i = 0; i < result->numpmid; i++) {
	pmValueSet *vsp = result->vset[i];
	pmAtomValue av;

	switch (pmID_cluster(vsp->pmid)) {
	case CLUSTER_CONTROL:
	    if (vsp->numval != 1)
		sts = PM_ERR_INST;
	    else switch (pmID_item(vsp->pmid)) {
	    case 0: /* proc.control.all.threads */
		if (!have_access)
		    sts = PM_ERR_PERMISSION;
		else if ((sts = pmExtractValue(vsp->valfmt, &vsp->vlist[0],
				PM_TYPE_U32, &av, PM_TYPE_U32)) >= 0) {
		    if (av.ul > 1)	/* only zero or one allowed */
			sts = PM_ERR_BADSTORE;
		    else
			threads = av.ul;
		}
		break;
	    case 1: /* proc.control.perclient.threads */
		if ((sts = pmExtractValue(vsp->valfmt, &vsp->vlist[0],
				PM_TYPE_U32, &av, PM_TYPE_U32)) >= 0) {
		    sts = proc_ctx_set_threads(pmda->e_context, av.ul);
		}
		break;
	    default:
		sts = PM_ERR_PERMISSION;
		break;
	    }
	    break;
	}

	if (sts < 0)
	    break;
    }

    have_access = all_access || proc_ctx_revert(pmda->e_context);
    return sts;
}

static int
proc_labelCallBack(pmInDom indom, unsigned int inst, pmLabelSet **lp)
{
    switch (pmInDom_serial(indom)) {
    case PROC_INDOM:
	return pmdaAddLabels(lp, "{\"pid\":%u}", inst);
    default:
	break;
    }
    return 0;
}


/*
 * Helper routines for accessing a generic static string dictionary,
 * a uid -> username dictionary, a gid -> groupname dictionary and a
 * dev_t -> ttyname dictionary.
 */

char *
proc_strings_lookup(int index)
{
    char		*value;

    if (pmdaCacheLookup(INDOM(STRINGS_INDOM), index,
			&value, NULL) == PMDA_CACHE_ACTIVE)
	return value;
    return "";
}

int
proc_strings_insert(const char *buf)
{
    return pmdaCacheStore(INDOM(STRINGS_INDOM), PMDA_CACHE_ADD, buf, NULL);
}

static char *
proc_uidname_lookup(int uid)
{
    struct passwd	*pwe;
    pmInDom		indom = INDOM(UIDNAME_INDOM);
    char		*name;

    if (pmdaCacheLookupKey(indom, NULL, sizeof(uid), &uid,
			   &name, NULL, NULL) == PMDA_CACHE_ACTIVE)
	return name;
    if ((pwe = getpwuid(uid)) != NULL)
	name = pwe->pw_name;
    else
	name = "";
    pmdaCacheStoreKey(indom, PMDA_CACHE_ADD, name, sizeof(uid), &uid, NULL);
    if (pmdaCacheLookupKey(indom, NULL, sizeof(uid), &uid,
			   &name, NULL, NULL) == PMDA_CACHE_ACTIVE)
	return name;
    return "";
}

static char *
proc_gidname_lookup(int gid)
{
    struct group	*gre;
    pmInDom		indom = INDOM(GIDNAME_INDOM);
    char		*name;

    if (pmdaCacheLookupKey(indom, NULL, sizeof(gid), &gid,
			   &name, NULL, NULL) == PMDA_CACHE_ACTIVE)
	return name;
    if ((gre = getgrgid(gid)) != NULL)
	name = gre->gr_name;
    else
	name = "";
    pmdaCacheStoreKey(indom, PMDA_CACHE_ADD, name, sizeof(gid), &gid, NULL);
    if (pmdaCacheLookupKey(indom, NULL, sizeof(gid), &gid,
			   &name, NULL, NULL) == PMDA_CACHE_ACTIVE)
	return name;
    return "";
}

static char *
proc_ttyname_lookup(dev_t tty)
{
    pmInDom		indom = INDOM(TTYNAME_INDOM);
    char		path[sizeof("/dev/") + MAXNAMLEN];
    char		*name;

    if (tty == NODEV)
	return "?";
    if (pmdaCacheLookupKey(indom, NULL, sizeof(tty), &tty,
			   &name, NULL, NULL) == PMDA_CACHE_ACTIVE)
	return name;
    if ((name = devname_r(tty, S_IFCHR, path, MAXNAMLEN)) == NULL)
	name = "?";
    pmdaCacheStoreKey(indom, PMDA_CACHE_ADD, name, sizeof(tty), &tty, NULL);
    if (pmdaCacheLookupKey(indom, NULL, sizeof(tty), &tty,
			   &name, NULL, NULL) == PMDA_CACHE_ACTIVE)
	return name;
    return "?";
}

/*
 * Initialise the agent (both daemon and DSO).
 */

void 
__PMDA_INIT_CALL
proc_init(pmdaInterface *dp)
{
    int		nindoms = sizeof(indomtab)/sizeof(indomtab[0]);
    int		nmetrics = sizeof(metrictab)/sizeof(metrictab[0]);

    if (_isDSO) {
	char helppath[MAXPATHLEN];
	int sep = pmPathSeparator();
	pmsprintf(helppath, sizeof(helppath), "%s%c" "proc" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
	pmdaDSO(dp, PMDA_INTERFACE_7, "proc DSO", helppath);
    }

    if (dp->status != 0)
	return;
    pmdaSetCommFlags(dp, PMDA_FLAG_AUTHORIZE);

    dp->version.seven.fetch = proc_fetch;
    dp->version.seven.store = proc_store;
    dp->version.seven.instance = proc_instance;
    dp->version.seven.attribute = proc_ctx_attrs;
    pmdaSetLabelCallBack(dp, proc_labelCallBack);
    pmdaSetEndContextCallBack(dp, proc_ctx_end);
    pmdaSetFetchCallBack(dp, proc_fetchCallBack);

    /*
     * Initialize the instance domain table.
     */
    indomtab[STRINGS_INDOM].it_indom = STRINGS_INDOM;
    indomtab[UIDNAME_INDOM].it_indom = UIDNAME_INDOM;
    indomtab[GIDNAME_INDOM].it_indom = GIDNAME_INDOM;
    indomtab[TTYNAME_INDOM].it_indom = TTYNAME_INDOM;
    indomtab[PROC_INDOM].it_indom = PROC_INDOM;

    proc_context_init();

    pmdaSetFlags(dp, PMDA_EXT_FLAG_HASHED);
    pmdaInit(dp, indomtab, nindoms, metrictab, nmetrics);

    /* string metrics use the pmdaCache API for value indexing */
    pmdaCacheOp(INDOM(STRINGS_INDOM), PMDA_CACHE_STRINGS);
    pmdaCacheOp(INDOM(UIDNAME_INDOM), PMDA_CACHE_STRINGS);
    pmdaCacheOp(INDOM(GIDNAME_INDOM), PMDA_CACHE_STRINGS);
    pmdaCacheOp(INDOM(TTYNAME_INDOM), PMDA_CACHE_STRINGS);
}

pmLongOptions	longopts[] = {
    PMDA_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    { "no-access-checks", 0, 'A', 0, "no access checks will be performed (insecure, beware!)" },
    PMDAOPT_DOMAIN,
    PMDAOPT_LOGFILE,
    { "with-threads", 0, 'L', 0, "include threads in the all-processes instance domain" },
    PMDAOPT_USERNAME,
    PMOPT_HELP,
    PMDA_OPTIONS_END
};

pmdaOptions	opts = {
    .short_options = "AD:d:l:Lr:U:?",
    .long_options = longopts,
};

int
main(int argc, char **argv)
{
    int			c, sep = pmPathSeparator();
    pmdaInterface	dispatch;
    char		helppath[MAXPATHLEN];
    char		*username = "root";

    _isDSO = 0;
    pmSetProgname(argv[0]);
    pmsprintf(helppath, sizeof(helppath), "%s%c" "proc" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_7, pmGetProgname(), PROC, "proc.log", helppath);

    while ((c = pmdaGetOptions(argc, argv, &opts, &dispatch)) != EOF) {
	switch (c) {
	case 'A':
	    all_access = 1;
	    break;
	case 'L':
	    threads = 1;
	    break;
	}
    }

    if (opts.errors) {
	pmdaUsageMessage(&opts);
	exit(1);
    }
    if (opts.username)
	username = opts.username;

    pmdaOpenLog(&dispatch);
    pmSetProcessIdentity(username);

    proc_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);
    exit(0);
}
