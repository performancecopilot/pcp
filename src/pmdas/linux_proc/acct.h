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

#include <sys/stat.h>
#include "pmapi.h"
#include "pmda.h"
#include "libpcp.h"
#include "indom.h"

typedef uint16_t    comp_t;
typedef uint32_t    comp2_t;

#define ACCT_COMM    16

struct acct_header {
    char        ac_flag;            /* Flags */
    char        ac_version;         /* Always set to ACCT_VERSION */
};

struct acct_v3 {
    char        ac_flag;            /* Flags */
    char        ac_version;         /* Always set to ACCT_VERSION */
    uint16_t    ac_tty;             /* Control Terminal */
    uint32_t    ac_exitcode;        /* Exitcode */
    uint32_t    ac_uid;             /* Real User ID */
    uint32_t    ac_gid;             /* Real Group ID */
    uint32_t    ac_pid;             /* Process ID */
    uint32_t    ac_ppid;            /* Parent Process ID */
    uint32_t    ac_btime;           /* Process Creation Time */
    float       ac_etime;           /* Elapsed Time */
    comp_t      ac_utime;           /* User Time */
    comp_t      ac_stime;           /* System Time */
    comp_t      ac_mem;             /* Average Memory Usage */
    comp_t      ac_io;              /* Chars Transferred */
    comp_t      ac_rw;              /* Blocks Read or Written */
    comp_t      ac_minflt;          /* Minor Pagefaults */
    comp_t      ac_majflt;          /* Major Pagefaults */
    comp_t      ac_swaps;           /* Number of Swaps */
    char        ac_comm[ACCT_COMM]; /* Command Name */
};

enum {          /* Bits that may be set in ac_flag field */
    AFORK = 0x01,           /* Has executed fork, but no exec */
    ASU   = 0x02,           /* Used superuser privileges */
    ACORE = 0x08,           /* Dumped core */
    AXSIG = 0x10,           /* Killed by a signal */
};


typedef struct {
    __pmHashCtl	accthash;	/* hash table for acct */
    pmdaIndom	*indom;		/* instance domain table */
    time_t	now;		/* timestamp of this sample */
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
    ACCT_TTYNAME  = 16,
    ACCT_UIDNAME  = 17,
    ACCT_GIDNAME  = 18,

    ACCTFLAG_FORK = 19,
    ACCTFLAG_SU   = 20,
    ACCTFLAG_CORE = 21,
    ACCTFLAG_XSIG = 22,

    CONTROL_OPEN_RETRY_INTERVAL = 23,
    CONTROL_CHECK_ACCT_INTERVAL = 24,
    CONTROL_FILE_SIZE_THRESHOLD = 25,
    CONTROL_ACCT_LIFETIME       = 26,
    CONTROL_ACCT_TIMER_INTERVAL = 27,
    CONTROL_ACCT_ENABLE         = 28,
    CONTROL_ACCT_STATE          = 29,
};

extern void acct_init(proc_acct_t *);
extern void refresh_acct(proc_acct_t *);
extern int acct_fetchCallBack(int i_inst, int item, proc_acct_t *proc_acct, pmAtomValue *atom);
extern int acct_store(pmResult *result, pmdaExt *pmda, pmValueSet *vsp);
