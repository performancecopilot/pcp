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

#include <sys/wait.h>
#include <sys/sysmacros.h>
#include <sys/vfs.h>
#include "acct.h"
#include "getinfo.h"

#define MAX_ACCT_RECORD_SIZE_BYTES 128
#define RINGBUF_SIZE               5000
#define ACCT_LIFE_TIME             60
#define OPEN_RETRY_INTERVAL        60
#define CHECK_ACCOUNTING_INTERVAL  600
#define ACCT_TIMER_INTERVAL        600
#define ACCT_FILE_SIZE_THRESHOLD   10485760

static char pacct_system_file[1024];
static char pacct_private_file[1024];

static int      acct_enable_private_acct       = 0;
static uint32_t acct_lifetime                  = ACCT_LIFE_TIME;
static uint32_t acct_open_retry_interval       = OPEN_RETRY_INTERVAL;
static uint32_t acct_check_accounting_interval = CHECK_ACCOUNTING_INTERVAL;
static uint64_t acct_file_size_threshold       = ACCT_FILE_SIZE_THRESHOLD;

/*
 * state of PMDA's accounting activity
 */
#define MYSTATE_INACTIVE	0
#define MYSTATE_SYSTEM		1
#define MYSTATE_PRIVATE		2
static int32_t acct_state = MYSTATE_INACTIVE;

struct timeval acct_update_interval = {
    .tv_sec = ACCT_TIMER_INTERVAL,
};
static int acct_timer_id = -1;
static int is_child = 0;

static struct {
    const char *path;
    int fd;
    unsigned long long prev_size;
    int acct_enabled;
    int version;
    int record_size;
    time_t last_fail_open;
    time_t last_check_accounting;
} acct_file;

static struct {
    int    (*get_pid)(void *);
    char * (*get_comm)(void *);
    time_t (*get_end_time)(void *);
    int    (*fetchCallBack)(int, void *, pmAtomValue *);
} acct_ops;

typedef struct {
    time_t time;
    struct pmdaInstid instid;
} acct_ringbuf_entry_t;

static struct {
    acct_ringbuf_entry_t *buf;
    int next_index;
} acct_ringbuf;

static int
get_pid_v3(void *entry)
{
    return ((struct acct_v3 *)entry)->ac_pid;
}

static char *
get_comm_v3(void *entry)
{
    return ((struct acct_v3 *)entry)->ac_comm;
}

static time_t
get_end_time_v3(void *entry)
{
    return ((struct acct_v3 *)entry)->ac_btime +
	   (int)(((struct acct_v3 *)entry)->ac_etime / _pm_hertz);
}

static unsigned long long
decode_comp_t(comp_t c)
{
    int exp;
    unsigned long long val;

    exp = (c >> 13) & 0x7;
    val = c & 0x1fff;

    while (exp-- > 0)
	val <<= 3;

    return val;
}

static int
acct_fetchCallBack_v3(int item, void *p, pmAtomValue *atom)
{
    struct acct_v3 *acctp = (struct acct_v3 *)p;
    switch (item) {
    case ACCT_TTY:
	atom->ul = acctp->ac_tty;
	break;
    case ACCT_TTYNAME:
	atom->cp = get_ttyname_info(acctp->ac_tty);
	break;
    case ACCT_PID:
	atom->ul = acctp->ac_pid;
	break;
    case ACCT_PPID:
	atom->ul = acctp->ac_ppid;
	break;
    case ACCT_BTIME:
	atom->ul = acctp->ac_btime;
	break;
    case ACCT_ETIME:
	atom->f = acctp->ac_etime / _pm_hertz;
	break;
    case ACCT_UTIME:
	atom->f = decode_comp_t(acctp->ac_utime) * 1.0 / _pm_hertz;
	break;
    case ACCT_STIME:
	atom->f = decode_comp_t(acctp->ac_stime) * 1.0 / _pm_hertz;
	break;
    case ACCT_MEM:
	atom->ull = decode_comp_t(acctp->ac_mem);
	break;
    case ACCT_IO:
	atom->ull = decode_comp_t(acctp->ac_io);
	break;
    case ACCT_RW:
	atom->ull = decode_comp_t(acctp->ac_rw);
	break;
    case ACCT_MINFLT:
	atom->ull = decode_comp_t(acctp->ac_minflt);
	break;
    case ACCT_MAJFLT:
	atom->ull = decode_comp_t(acctp->ac_majflt);
	break;
    case ACCT_SWAPS:
	atom->ull = decode_comp_t(acctp->ac_swaps);
	break;
    case ACCT_EXITCODE:
	atom->ul = acctp->ac_exitcode;
	break;

    case ACCT_UID:
	atom->ul = acctp->ac_uid;
	break;
    case ACCT_UIDNAME:
	atom->cp = proc_uidname_lookup(acctp->ac_uid);
	break;
    case ACCT_GID:
	atom->ul = acctp->ac_gid;
	break;
    case ACCT_GIDNAME:
	atom->cp = proc_gidname_lookup(acctp->ac_gid);
	break;

    case ACCTFLAG_FORK:
	atom->ul = (acctp->ac_flag & AFORK) != 0;
	break;
    case ACCTFLAG_SU:
	atom->ul = (acctp->ac_flag & ASU) != 0;
	break;
    case ACCTFLAG_CORE:
	atom->ul = (acctp->ac_flag & ACORE) != 0;
	break;
    case ACCTFLAG_XSIG:
	atom->ul = (acctp->ac_flag & AXSIG) != 0;
	break;

    default:
	return 0;
    }
    return 1;
}

static int
set_record_size(int fd)
{
    struct acct_header tmprec;
    int sts;

    if ((sts = read(fd, &tmprec, sizeof(tmprec))) < sizeof(tmprec)) {
	if (pmDebugOptions.appl3)
	    pmNotifyErr(LOG_WARNING, "acct: bad read fd=%d len=%d (not %d), so no process accounting available\n", fd, sts, (int)sizeof(tmprec));
	return 0;
    }

    if ((tmprec.ac_version & 0x0f) == 3) {
	acct_file.version = 3;
	acct_file.record_size = sizeof(struct acct_v3);

	acct_ops.get_pid       = get_pid_v3;
	acct_ops.get_comm      = get_comm_v3;
	acct_ops.get_end_time  = get_end_time_v3;
	acct_ops.fetchCallBack = acct_fetchCallBack_v3;
	return 1;
    }

    if (pmDebugOptions.appl3)
	pmNotifyErr(LOG_WARNING, "acct: fd=%d version=%d (not 3), so no process accounting available\n", fd, tmprec.ac_version & 0x0f);
    return 0;
}

static int
check_accounting(int fd, const char *name)
{
    struct stat before, after;
    char	errmsg[PM_MAXERRMSGLEN];
    int		sts;
    static int	onetrip = 1;

    if (fstat(fd, &before) < 0) {
	if (pmDebugOptions.appl3)
	    pmNotifyErr(LOG_WARNING, "acct: before fstat(fd=%d, name=%s) failed: %s\n", fd, name, pmErrStr_r(-oserror(), errmsg, sizeof(errmsg)));
	return 0;
    }
    if (fork() == 0) {
	is_child = 1;
	_exit(0);
    }
    wait(0);
    if (fstat(fd, &after) < 0) {
	if (pmDebugOptions.appl3)
	    pmNotifyErr(LOG_WARNING, "acct: after fstat(fd=%d, name=%s) failed: %s\n", fd, name, pmErrStr_r(-oserror(), errmsg, sizeof(errmsg)));
	return 0;
    }
    sts = after.st_size > before.st_size;
    if (sts <= 0) {
	if (onetrip) {
	    pmNotifyErr(LOG_WARNING, "acct: existing pacct file did not grow as expected: system level process accounting disabled or file system full?");
	    if (pmDebugOptions.appl3) {
		struct timeval	now;
		struct statfs	fstat;
		fprintf(stderr, "acct: pacct growth test failed\n");
		fprintf(stderr, "    name: %s\n", name);
		fprintf(stderr, "    size: %" FMT_UINT64 "\n", (uint64_t)after.st_size);
		fprintf(stderr, "    mtime: %s", ctime(&after.st_mtime));
		fprintf(stderr, "    ctime: %s", ctime(&after.st_ctime));
		gettimeofday(&now, NULL);
		fprintf(stderr, "    nowtime: %s", ctime(&now.tv_sec));
		fprintf(stderr, "    dev: %d/%d\n", major(after.st_dev), minor(after.st_dev));
		fstatfs(fd, &fstat);
		fprintf(stderr, "    filesystem (1KB blocks): size=%" FMT_UINT64 " avail=%" FMT_UINT64 " used=%d%%\n",
			fstat.f_bsize*(uint64_t)fstat.f_blocks/1024,
			fstat.f_bsize*(uint64_t)fstat.f_bavail/1024,
			(int)(100*(fstat.f_blocks-fstat.f_bavail)/fstat.f_blocks));
	    }
	    else
		pmNotifyErr(LOG_INFO, "acct: enable -Dappl3 for more detailed logging");
	    onetrip = 0;
	}
    }

    return sts;
}

static void
init_acct_file_info(void)
{
    memset(&acct_file, 0, sizeof(acct_file));
    acct_file.fd = -1;
}

static void
close_pacct_file(void)
{
    if (pmDebugOptions.appl3)
	pmNotifyErr(LOG_DEBUG, "acct: close file=%s fd=%d acct_enabled=%d\n", acct_file.path, acct_file.fd, acct_file.acct_enabled);

    if (acct_file.fd >= 0) {
	close(acct_file.fd);
	if (acct_file.acct_enabled) {
	    acct(0);
	    unlink(acct_file.path);
	}
    }
    init_acct_file_info();
}

static void
acct_cleanup(void)
{
    if (!is_child)
	close_pacct_file();
}

static int
open_and_acct(const char *path, int do_acct)
{
    struct stat file_stat;
    char	errmsg[PM_MAXERRMSGLEN];

    if (acct_file.fd != -1)
	return 0;

    if (path == NULL || path[0] == '\0') {
	/* no path, no play */
	return 0;
    }

    if (do_acct)
	acct_file.fd = open(path, O_TRUNC|O_CREAT, S_IRUSR);
    else
	acct_file.fd = open(path, O_RDONLY);

    if (acct_file.fd < 0) {
	if (pmDebugOptions.appl3)
	    pmNotifyErr(LOG_DEBUG, "acct: open(\"%s\", ...) do_acct=%d failed: %s\n", path, do_acct, pmErrStr_r(-oserror(), errmsg, sizeof(errmsg)));

	goto err1;
    }

    if (fstat(acct_file.fd, &file_stat) < 0) {
	if (pmDebugOptions.appl3)
	    pmNotifyErr(LOG_DEBUG, "acct: fstat \"%s\" failed: %s\n", path, pmErrStr_r(-oserror(), errmsg, sizeof(errmsg)));
	goto err2;
    }

    if (do_acct && acct(path) < 0) {
	if (pmDebugOptions.appl3)
	    pmNotifyErr(LOG_DEBUG, "acct: acct(\"%s\") failed: %s\n", path, pmErrStr_r(-oserror(), errmsg, sizeof(errmsg)));
	goto err2;
    }

    if (!check_accounting(acct_file.fd, path))
	goto err3;

    if (!set_record_size(acct_file.fd))
	goto err3;

    if (lseek(acct_file.fd, file_stat.st_size, SEEK_SET) < 0) {
	if (pmDebugOptions.appl3)
	    pmNotifyErr(LOG_DEBUG, "acct: lseek \"%s\",%lld failed: %s\n", path, (long long)file_stat.st_size, pmErrStr_r(-oserror(), errmsg, sizeof(errmsg)));
	goto err3;
    }

    acct_file.prev_size = file_stat.st_size;
    acct_file.path = path;

    if (pmDebugOptions.appl3)
	pmNotifyErr(LOG_DEBUG, "acct: open file=%s acct=%d version=%d\n", path, do_acct, acct_file.version);
    return 1;

err3:
    if (do_acct)
	acct(0);

err2:
    close(acct_file.fd);

err1:
    init_acct_file_info();

    return 0;
}

static int
open_pacct_file(void)
{
    int ret;

    if (pmDebugOptions.appl3)
	pmNotifyErr(LOG_DEBUG, "acct: open enable_private=%d timer_id=%d\n", acct_enable_private_acct, acct_timer_id);

    ret = open_and_acct(pacct_system_file, 0);
    if (ret) {
	acct_file.acct_enabled = 0;
	acct_state = MYSTATE_SYSTEM;
	return 1;
    }

    if (!acct_enable_private_acct || acct_timer_id == -1) {
	acct_state = MYSTATE_INACTIVE;
	return 0;
    }

    ret = open_and_acct(pacct_private_file, 1);
    if (ret) {
	acct_file.acct_enabled = 1;
	acct_state = MYSTATE_PRIVATE;
	return 1;
    }

    acct_file.last_fail_open = time(NULL);
    acct_state = MYSTATE_INACTIVE;
    return 0;
}

static void
reopen_pacct_file(void)
{
    close_pacct_file();
    open_pacct_file();
}

static void
free_entry(__pmHashCtl *hp, int i_inst)
{
    __pmHashNode *node = __pmHashSearch(i_inst, hp);
    if (node && node->data) {
	__pmHashDel(i_inst, (void *)node->data, hp);
	free(node->data);
    }
}

static int
free_ringbuf_entry(__pmHashCtl *hp, int index)
{
    if (!acct_ringbuf.buf[index].instid.i_inst)
	return 0;
    free_entry(hp, acct_ringbuf.buf[index].instid.i_inst);
    memset(&acct_ringbuf.buf[index], 0, sizeof(acct_ringbuf_entry_t));
    return 1;
}

static int
next_ringbuf_index(int index)
{
    return (index + 1) % RINGBUF_SIZE;
}

static void
acct_ringbuf_add(__pmHashCtl *hp, acct_ringbuf_entry_t *entry)
{
    free_ringbuf_entry(hp, acct_ringbuf.next_index);
    acct_ringbuf.buf[acct_ringbuf.next_index] = *entry;
    acct_ringbuf.next_index = next_ringbuf_index(acct_ringbuf.next_index);
}

static int
ringbuf_entry_is_valid(time_t t, int index)
{
    return (t - acct_ringbuf.buf[index].time) <= acct_lifetime;
}

static int
acct_gc(__pmHashCtl *hp, time_t t)
{
    int need_update = 0;
    int i, index = acct_ringbuf.next_index;
    for (i = 0; i < RINGBUF_SIZE; i++) {
	if (ringbuf_entry_is_valid(t, index))
	    break;
	need_update += free_ringbuf_entry(hp, index);
	index = next_ringbuf_index(index);
    }
    return need_update;
}

static void
copy_ringbuf_to_indom(pmdaIndom *indomp, time_t t)
{
    int i, index;
    for (i = 0; i < RINGBUF_SIZE; i++) {
	index = (acct_ringbuf.next_index - 1 - i + RINGBUF_SIZE) % RINGBUF_SIZE;
	if (!ringbuf_entry_is_valid(t, index))
	    break;
	indomp->it_set[i] = acct_ringbuf.buf[index].instid;
    }
    indomp->it_numinst = i;
}

static long long
get_file_size(void)
{
    struct stat statbuf;
    if (acct_file.fd < 0)
	return -1;
    if (fstat(acct_file.fd, &statbuf) < 0)
	return -1;
    return statbuf.st_size;
}

static int
exists_hash_entry(int i_inst, proc_acct_t *proc_acct)
{
    __pmHashNode *node = __pmHashSearch(i_inst, &proc_acct->accthash);
    return node && node->data ? 1 : 0;
}

static void
acct_timer(int sig, void *ptr)
{
    if (pmDebugOptions.appl3)
	pmNotifyErr(LOG_DEBUG, "acct: timer called\n");
    if (acct_file.fd >= 0 && acct_file.acct_enabled && get_file_size() > acct_file_size_threshold)
	reopen_pacct_file();
}

static void
reset_acct_timer(void)
{
    int sts;

    if (acct_timer_id != -1) {
	__pmAFunregister(acct_timer_id);
	acct_timer_id = -1;
    }
    sts = __pmAFregister(&acct_update_interval, NULL, acct_timer);
    if (sts < 0) {
	close_pacct_file();
	if (pmDebugOptions.appl3)
	    pmNotifyErr(LOG_DEBUG, "acct: error registering timer: %s\n", pmErrStr(sts));
	return;
    }
    acct_timer_id = sts;
    reopen_pacct_file();
}

/*
 * Probe for system process accounting file ... looking in:
 * - $PCP_PACCT_SYSTEM_PATH, else
 * - /var/log/account/pacct, else
 * - /var/account/pacct, else
 * - ...
 */
static char *pacct_path[] = {
    "",				/* try $PCP_PACCT_SYSTEM_PATH */
    "/var/log/account/pacct",
    "/var/account/pacct",
    NULL
};
static void
init_pacct_system_file(void)
{
    char **path;
    char *tmppath;

    pacct_system_file[0] = '\0';
    for (path = pacct_path; *path != NULL; path++) {
	if ((*path)[0] == '\0') {
	    tmppath = pmGetOptionalConfig("PCP_PACCT_SYSTEM_PATH");
	    if (tmppath == NULL)
		continue;
	}
	else
	    tmppath = *path;
	if (access(tmppath, F_OK) == 0) {
	    /* file exists, pick me! */
	    strncpy(pacct_system_file, tmppath, sizeof(pacct_system_file)-1);
	    break;
	}
    }

    if (pmDebugOptions.appl3) {
	if (pacct_system_file[0] == '\0')
	    pmNotifyErr(LOG_DEBUG, "acct: no valid pacct_system_file path found\n");
	else
	    pmNotifyErr(LOG_DEBUG, "acct: initialize pacct_system_file path to %s\n", pacct_system_file);
    }
}

static void
init_pacct_private_file(void)
{
    char *pacctdir;

    pacct_private_file[0] = '\0';
    if ((pacctdir = pmGetOptionalConfig("PCP_VAR_DIR")) == NULL) {
	pacct_private_file[0] = '\0';
    } else {
	pmsprintf(pacct_private_file, sizeof(pacct_private_file), "%s/pmcd/pacct", pacctdir);
    }

    if (pmDebugOptions.appl3) {
	if (pacct_private_file[0] == '\0')
	    pmNotifyErr(LOG_DEBUG, "acct: cannot initialize pacct_private_file path\n");
	else
	    pmNotifyErr(LOG_DEBUG, "acct: initialize pacct_private_file path to %s\n", pacct_private_file);
    }
}

void
acct_init(proc_acct_t *proc_acct)
{
    init_pacct_system_file();
    init_pacct_private_file();

    init_acct_file_info();
    reset_acct_timer();

    acct_ringbuf.next_index = 0;
    acct_ringbuf.buf = calloc(RINGBUF_SIZE, sizeof(acct_ringbuf_entry_t));

    proc_acct->indom->it_numinst = 0;
    proc_acct->indom->it_set = calloc(RINGBUF_SIZE, sizeof(pmdaInstid));

    atexit(acct_cleanup);
}

void
refresh_acct(proc_acct_t *proc_acct)
{
    char tmprec[MAX_ACCT_RECORD_SIZE_BYTES];
    void *acctp;
    long long acct_file_size;
    int i, records, i_inst, need_update = 0;
    time_t process_end_time;
    acct_ringbuf_entry_t ringbuf_entry;

    proc_acct->now = time(NULL);	/* timestamp for current sample */

    if (acct_file.fd < 0) {
	if ((proc_acct->now - acct_file.last_fail_open) > acct_open_retry_interval)
	    open_pacct_file();
	else if (pmDebugOptions.appl3)
	    pmNotifyErr(LOG_DEBUG, "acct: open skipped: retry=%d < limit=%d\n", (int)(proc_acct->now - acct_file.last_fail_open), acct_open_retry_interval);
	return;
    }

    if (acct_file.record_size <= 0 || MAX_ACCT_RECORD_SIZE_BYTES < acct_file.record_size)
	return;

    if ((proc_acct->now - acct_file.last_check_accounting) > acct_check_accounting_interval) {
	if (pmDebugOptions.appl3)
	    pmNotifyErr(LOG_DEBUG, "acct: check accounting\n");
	if (!check_accounting(acct_file.fd, acct_file.path)) {
	    reopen_pacct_file();
	    return;
	}
	acct_file.last_check_accounting = proc_acct->now;
    }
    need_update = acct_gc(&proc_acct->accthash, proc_acct->now);

    if (need_update) {
	if (pmDebugOptions.appl3)
	    pmNotifyErr(LOG_DEBUG, "acct: acct_gc n=%d\n", need_update);
    }

    acct_file_size = get_file_size();
    if (acct_file_size < 0) {
	reopen_pacct_file();
	return;
    }

    records = (acct_file_size - acct_file.prev_size) / acct_file.record_size;
    for (i = 0; i < records; i++) {
	if (read(acct_file.fd, tmprec, acct_file.record_size) < acct_file.record_size) {
	    reopen_pacct_file();
	    return;
	}

	if (((struct acct_header *)tmprec)->ac_version != acct_file.version) {
	    reopen_pacct_file();
	    return;
	}

	i_inst = acct_ops.get_pid(tmprec);

	if (!i_inst)
	    continue;

	if (exists_hash_entry(i_inst, proc_acct))
	    continue;

	process_end_time = acct_ops.get_end_time(tmprec);
	if (proc_acct->now - process_end_time > acct_lifetime)
	    continue;

	acctp = malloc(acct_file.record_size);
	memcpy(acctp, tmprec, acct_file.record_size);

	ringbuf_entry.time = process_end_time;
	ringbuf_entry.instid.i_inst = i_inst;
	ringbuf_entry.instid.i_name = acct_ops.get_comm(acctp);

	if (pmDebugOptions.appl3)
	    pmNotifyErr(LOG_DEBUG, "acct: hash add pid=%d comm=%s\n", i_inst, acct_ops.get_comm(acctp));

	acct_ringbuf_add(&proc_acct->accthash, &ringbuf_entry);
	__pmHashAdd(i_inst, acctp, &proc_acct->accthash);
	need_update++;
    }

    if (need_update) {
	copy_ringbuf_to_indom(proc_acct->indom, proc_acct->now);
	if (pmDebugOptions.appl3)
	    pmNotifyErr(LOG_DEBUG, "acct: update indom it_numinst=%d\n", proc_acct->indom->it_numinst);
    }
    acct_file.prev_size = acct_file_size;
}

int
acct_fetchCallBack(int i_inst, int item, proc_acct_t *proc_acct, pmAtomValue *atom)
{
    __pmHashNode *node;

    switch (item) {
    case CONTROL_OPEN_RETRY_INTERVAL:
	atom->ul = acct_open_retry_interval;
	return 1;
    case CONTROL_CHECK_ACCT_INTERVAL:
	atom->ul = acct_check_accounting_interval;
	return 1;
    case CONTROL_FILE_SIZE_THRESHOLD:
	atom->ull = acct_file_size_threshold;
	return 1;
    case CONTROL_ACCT_LIFETIME:
	atom->ul = acct_lifetime;
	return 1;
    case CONTROL_ACCT_TIMER_INTERVAL:
	atom->ul = acct_update_interval.tv_sec;
	return 1;
    case CONTROL_ACCT_ENABLE:
	atom->ul = acct_enable_private_acct;
	return 1;
    case CONTROL_ACCT_STATE:
	atom->l = acct_state;
	return 1;
    }

    if (acct_file.fd < 0)
	return 0;

    node = __pmHashSearch(i_inst, &proc_acct->accthash);
    if (!node || !node->data)
	return 0;

    if (proc_acct->now - acct_ops.get_end_time(node->data) > acct_lifetime)
	return 0;

    return acct_ops.fetchCallBack(item, node->data, atom);
}

int
acct_store(pmResult *result, pmdaExt *pmda, pmValueSet *vsp)
{
    int sts = 0;
    pmAtomValue av;
    switch (pmID_item(vsp->pmid)) {
    case CONTROL_OPEN_RETRY_INTERVAL: /* acct.control.open_retry_interval */
	if ((sts = pmExtractValue(vsp->valfmt, &vsp->vlist[0],
			PM_TYPE_U32, &av, PM_TYPE_U32)) >= 0) {
	    acct_open_retry_interval = av.ul;
	}
	break;
    case CONTROL_CHECK_ACCT_INTERVAL: /* acct.control.check_acct_interval */
	if ((sts = pmExtractValue(vsp->valfmt, &vsp->vlist[0],
			PM_TYPE_U32, &av, PM_TYPE_U32)) >= 0) {
	    acct_check_accounting_interval = av.ul;
	}
	break;
    case CONTROL_FILE_SIZE_THRESHOLD: /* acct.control.file_size_threshold */
	if ((sts = pmExtractValue(vsp->valfmt, &vsp->vlist[0],
			PM_TYPE_U64, &av, PM_TYPE_U64)) >= 0) {
	    acct_file_size_threshold = av.ul;
	}
	break;
    case CONTROL_ACCT_LIFETIME: /* acct.control.lifetime */
	if ((sts = pmExtractValue(vsp->valfmt, &vsp->vlist[0],
			PM_TYPE_U32, &av, PM_TYPE_U32)) >= 0) {
	    acct_lifetime = av.ul;
	}
	break;
    case CONTROL_ACCT_TIMER_INTERVAL: /* acct.control.refresh */
	if ((sts = pmExtractValue(vsp->valfmt, &vsp->vlist[0],
			PM_TYPE_U32, &av, PM_TYPE_U32)) >= 0) {
	    if (av.ul > 0) {
		acct_update_interval.tv_sec = av.ul;
		reset_acct_timer();
	    } else {
		sts = PM_ERR_PERMISSION;
	    }
	}
	break;
    case CONTROL_ACCT_ENABLE: /* acct.control.enable_acct */
	if ((sts = pmExtractValue(vsp->valfmt, &vsp->vlist[0],
			PM_TYPE_U32, &av, PM_TYPE_U32)) >= 0) {
	    int state_changed = !acct_enable_private_acct != !av.ul;
	    if (pmDebugOptions.appl3)
		pmNotifyErr(LOG_DEBUG, "acct: store enable_acct old=%d new=%d\n", acct_enable_private_acct, av.ul);
	    acct_enable_private_acct = av.ul;
	    if (state_changed)
		reopen_pacct_file();
	}
	break;
    default:
	sts = PM_ERR_PERMISSION;
	break;
    }

    return sts;
}
