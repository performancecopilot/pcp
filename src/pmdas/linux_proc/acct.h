/*
 * Linux acct metrics cluster
 *
 * Copyright (c) 2020 Fujitsu.
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

#include <linux/types.h>
#include <sys/stat.h>
#include <pcp/pmapi.h>
#include <pcp/pmda.h>
#include "libpcp.h"

typedef __u16	comp_t;
typedef __u32	comp2_t;

#define ACCT_COMM	16

struct acct_v3 {
	char		ac_flag;		/* Flags */
	char		ac_version;		/* Always set to ACCT_VERSION */
	__u16		ac_tty;			/* Control Terminal */
	__u32		ac_exitcode;		/* Exitcode */
	__u32		ac_uid;			/* Real User ID */
	__u32		ac_gid;			/* Real Group ID */
	__u32		ac_pid;			/* Process ID */
	__u32		ac_ppid;		/* Parent Process ID */
	__u32		ac_btime;		/* Process Creation Time */
	float		ac_etime;		/* Elapsed Time */
	comp_t		ac_utime;		/* User Time */
	comp_t		ac_stime;		/* System Time */
	comp_t		ac_mem;			/* Average Memory Usage */
	comp_t		ac_io;			/* Chars Transferred */
	comp_t		ac_rw;			/* Blocks Read or Written */
	comp_t		ac_minflt;		/* Minor Pagefaults */
	comp_t		ac_majflt;		/* Major Pagefaults */
	comp_t		ac_swaps;		/* Number of Swaps */
	char		ac_comm[ACCT_COMM];	/* Command Name */
};

typedef struct {
	__pmHashCtl	accthash;		/* hash table for acct */
	pmdaIndom	*indom;			/* instance domain table */
} proc_acct_t;

enum {
	ACCT_TTY      = 0,
	ACCT_EXITCODE = 1,
	ACCT_UID      = 2,
	ACCT_GID      = 3,
	ACCT_PID      = 4,
	ACCT_PPID     = 5,
	ACCT_BTIME    = 6,
	ACCT_ETIME    = 7,
	ACCT_UTIME    = 8,
	ACCT_STIME    = 9,
	ACCT_MEM      = 10,
	ACCT_IO       = 11,
	ACCT_RW       = 12,
	ACCT_MINFLT   = 13,
	ACCT_MAJFLT   = 14,
	ACCT_SWAPS    = 15,
};

extern void acct_init(proc_acct_t *);
extern void refresh_acct(proc_acct_t *);
extern int acct_fetchCallBack(int i_inst, int item, proc_acct_t* proc_acct, pmAtomValue *atom);
