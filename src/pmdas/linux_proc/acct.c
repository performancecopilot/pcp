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
#include "acct.h"

#define MAX_ACCT_RECORD_SIZE_BYTES	128
#define RINGBUF_SIZE			5000
#define OPEN_RETRY_INTERVAL		60
#define CHECK_ACCOUNTING_INTERVAL	600
#define ACCT_FILE_SIZE_THRESHOLD	10485760
#define PACCT_SYSTEM_FILE		"/var/account/pacct"
#define PACCT_PCP_PRIVATE_FILE		"/tmp/pcp-pacct"

int acct_lifetime = 60;
struct timeval acct_update_interval = {
	.tv_sec = 600,
};
static int acct_timer_id = -1;

static struct {
	const char* path;
	int fd;
	unsigned long long prev_size;
	int acct_enabled;
	int version;
	int record_size;
	time_t last_fail_open;
	time_t last_check_accounting;
} acct_file;

static struct {
	int    (*get_pid)(void*);
	char*  (*get_comm)(void*);
	time_t (*get_end_time)(void*);
	int    (*fetchCallBack)(int, void*, pmAtomValue*);
} acct_ops;

typedef struct {
	time_t time;
	struct pmdaInstid instid;
} acct_ringbuf_entry_t;

static struct {
	acct_ringbuf_entry_t *buf;
	int next_index;
} acct_ringbuf;

static int get_pid_v3(void *entry) {
	return ((struct acct_v3*)entry)->ac_pid;
}

static char* get_comm_v3(void *entry) {
	return ((struct acct_v3*)entry)->ac_comm;
}

static time_t get_end_time_v3(void *entry) {
	return ((struct acct_v3*)entry)->ac_btime +
		(int)(((struct acct_v3*)entry)->ac_etime / hz);
}

static int acct_fetchCallBack_v3(int item, void *p, pmAtomValue* atom) {
	struct acct_v3* acctp = (struct acct_v3*)p;
	switch (item) {
	case ACCT_TTY:
		atom->ul = acctp->ac_tty;
		break;
	case ACCT_EXITCODE:
		atom->ul = acctp->ac_exitcode;
		break;
	case ACCT_UID:
		atom->ul = acctp->ac_uid;
		break;
	case ACCT_GID:
		atom->ul = acctp->ac_gid;
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
		atom->f = acctp->ac_etime * 1000 / hz;
		break;
	case ACCT_UTIME:
		atom->ul = acctp->ac_utime * 1000 / hz;
		break;
	case ACCT_STIME:
		atom->ul = acctp->ac_stime * 1000 / hz;
		break;
	case ACCT_MEM:
		atom->ul = acctp->ac_mem;
		break;
	case ACCT_IO:
		atom->ul = acctp->ac_io;
		break;
	case ACCT_RW:
		atom->ul = acctp->ac_rw;
		break;
	case ACCT_MINFLT:
		atom->ul = acctp->ac_minflt;
		break;
	case ACCT_MAJFLT:
		atom->ul = acctp->ac_majflt;
		break;
	case ACCT_SWAPS:
		atom->ul = acctp->ac_swaps;
		break;
	default:
		return 0;
	}
	return 1;
}

static int set_record_size(int fd) {
	struct acct_header tmprec;

	if (read(fd, &tmprec, sizeof(tmprec)) < sizeof(tmprec))
		return 0;

	if ((tmprec.ac_version & 0x0f) == 3) {
		acct_file.version = 3;
		acct_file.record_size = sizeof(struct acct_v3);

		acct_ops.get_pid       = get_pid_v3;
		acct_ops.get_comm      = get_comm_v3;
		acct_ops.get_end_time  = get_end_time_v3;
		acct_ops.fetchCallBack = acct_fetchCallBack_v3;
		return 1;
	}

	return 0;
}

static int check_accounting(int fd) {
	struct stat before, after;

	if (fstat(fd, &before) < 0)
		return 0;
	if (fork() == 0)
		exit(0);
	wait(0);
	if (fstat(fd, &after) < 0)
		return 0;

	return after.st_size > before.st_size;
}

static void init_acct_file_info(void) {
	memset(&acct_file, 0, sizeof(acct_file));
	acct_file.fd = -1;
}

static void close_pacct_file(void) {
	if (pmDebugOptions.libpmda && pmDebugOptions.desperate)
		pmNotifyErr(LOG_DEBUG, "acct: close file=%s\n", acct_file.path);

	if (acct_file.fd >= 0) {
		if (acct_file.acct_enabled)
			acct(0);
		close(acct_file.fd);
	}
	init_acct_file_info();
}

static int open_and_acct(const char* path, int do_acct) {
	struct stat file_stat;

	if (acct_file.fd != -1)
		return 0;

	if (do_acct && acct_timer_id == -1)
		return 0;

	if (do_acct)
		acct_file.fd = open(path, O_TRUNC|O_CREAT, S_IRUSR);
	else
		acct_file.fd = open(path, O_RDONLY);

	if (acct_file.fd < 0)
		goto err1;

	if (stat(path, &file_stat) < 0)
		goto err2;

	if (do_acct && acct(path) < 0)
		goto err2;

	if (!check_accounting(acct_file.fd))
		goto err3;

	if (!set_record_size(acct_file.fd))
		goto err3;

	if (lseek(acct_file.fd, file_stat.st_size, SEEK_SET) < 0)
		goto err3;

	acct_file.prev_size = file_stat.st_size;
	acct_file.path = path;

	if (pmDebugOptions.libpmda && pmDebugOptions.desperate)
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

static int open_pacct_file(void) {
	int ret;

	ret = open_and_acct(PACCT_SYSTEM_FILE, 0);
	if (ret) {
		acct_file.acct_enabled = 0;
		return 1;
	}

	ret = open_and_acct(PACCT_PCP_PRIVATE_FILE, 1);
	if (ret) {
		acct_file.acct_enabled = 1;
		return 1;
	}

	acct_file.last_fail_open = time(NULL);
	return 0;
}

static void reopen_pacct_file(void) {
	close_pacct_file();
	open_pacct_file();
}

static void free_entry(__pmHashCtl *hp, int i_inst) {
	__pmHashNode *node = __pmHashSearch(i_inst, hp);
	if (node && node->data) {
		__pmHashDel(i_inst, (void *)node->data, hp);
		free(node->data);
	}
}

static int free_ringbuf_entry(__pmHashCtl *hp, int index) {
	if (!acct_ringbuf.buf[index].instid.i_inst)
		return 0;
	free_entry(hp, acct_ringbuf.buf[index].instid.i_inst);
	memset(&acct_ringbuf.buf[index], 0, sizeof(acct_ringbuf_entry_t));
	return 1;
}

static int next_ringbuf_index(int index) {
	return (index + 1) % RINGBUF_SIZE;
}

static void acct_ringbuf_add(__pmHashCtl *hp, acct_ringbuf_entry_t *entry) {
	free_ringbuf_entry(hp, acct_ringbuf.next_index);
	acct_ringbuf.buf[acct_ringbuf.next_index] = *entry;
	acct_ringbuf.next_index = next_ringbuf_index(acct_ringbuf.next_index);
}

static int ringbuf_entry_is_valid(time_t t, int index) {
	return (t - acct_ringbuf.buf[index].time) <= acct_lifetime;
}

static int acct_gc(__pmHashCtl *hp, time_t t) {
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

static void copy_ringbuf_to_indom(pmdaIndom* indomp, time_t t) {
	int i, index;
	for (i = 0; i < RINGBUF_SIZE; i++) {
		index = (acct_ringbuf.next_index - 1 - i + RINGBUF_SIZE) % RINGBUF_SIZE;
		if (!ringbuf_entry_is_valid(t, index))
			break;
		indomp->it_set[i] = acct_ringbuf.buf[index].instid;
	}
	indomp->it_numinst = i;
}

static unsigned long long get_file_size(const char* path) {
	struct stat statbuf;
	if (stat(path, &statbuf) < 0)
		return -1;
	return statbuf.st_size;
}

static int exists_hash_entry(int i_inst, proc_acct_t *proc_acct) {
	__pmHashNode *node = __pmHashSearch(i_inst, &proc_acct->accthash);
	return node && node->data ? 1 : 0;
}

static void acct_timer(int sig, void *ptr) {
	if (pmDebugOptions.libpmda && pmDebugOptions.desperate)
		pmNotifyErr(LOG_DEBUG, "acct: timer called\n");
	if (acct_file.fd >= 0 && acct_file.acct_enabled && get_file_size(acct_file.path) > ACCT_FILE_SIZE_THRESHOLD)
		reopen_pacct_file();
}

static void init_acct_timer(void) {
	int sts;

	sts = __pmAFregister(&acct_update_interval, NULL, acct_timer);
	if (sts < 0) {
		if (pmDebugOptions.libpmda && pmDebugOptions.desperate)
			pmNotifyErr(LOG_DEBUG, "acct: error registering timer: %s\n", pmErrStr(sts));
		return;
	}
	acct_timer_id = sts;
}

void acct_init(proc_acct_t *proc_acct) {
	init_acct_timer();

	init_acct_file_info();
	open_pacct_file();

	acct_ringbuf.next_index = 0;
	acct_ringbuf.buf = calloc(RINGBUF_SIZE, sizeof(acct_ringbuf_entry_t));

	proc_acct->indom->it_numinst = 0;
	proc_acct->indom->it_set = calloc(RINGBUF_SIZE, sizeof(pmdaInstid));
}

void refresh_acct(proc_acct_t *proc_acct) {
	char tmprec[MAX_ACCT_RECORD_SIZE_BYTES];
	void* acctp;
	unsigned long long acct_file_size;
	int i, records, i_inst, need_update = 0;
	time_t now, process_end_time;
	acct_ringbuf_entry_t ringbuf_entry;

	now = time(NULL);
	if (acct_file.fd < 0) {
		if ((now - acct_file.last_fail_open) > OPEN_RETRY_INTERVAL)
			open_pacct_file();
		return;
	}

	if (acct_file.record_size <= 0 || MAX_ACCT_RECORD_SIZE_BYTES < acct_file.record_size)
		return;

	if ((now - acct_file.last_check_accounting) > CHECK_ACCOUNTING_INTERVAL) {
		if (pmDebugOptions.libpmda && pmDebugOptions.desperate)
			pmNotifyErr(LOG_DEBUG, "acct: check accounting\n");
		if (!check_accounting(acct_file.fd)) {
			reopen_pacct_file();
			return;
		}
		acct_file.last_check_accounting = now;
	}
	need_update = acct_gc(&proc_acct->accthash, now);

	if (need_update) {
		if (pmDebugOptions.libpmda && pmDebugOptions.desperate)
			pmNotifyErr(LOG_DEBUG, "acct: acct_gc n=%d\n", need_update);
	}

	acct_file_size = get_file_size(acct_file.path);
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

		if (((struct acct_header*)tmprec)->ac_version != acct_file.version) {
			reopen_pacct_file();
			return;
		}

		i_inst = acct_ops.get_pid(tmprec);

		if (!i_inst)
			continue;

		if (exists_hash_entry(i_inst, proc_acct))
			continue;

		process_end_time = acct_ops.get_end_time(tmprec);
		if (now - process_end_time > acct_lifetime)
			continue;

		acctp = malloc(acct_file.record_size);
		memcpy(acctp, tmprec, acct_file.record_size);

		ringbuf_entry.time = process_end_time;
		ringbuf_entry.instid.i_inst = i_inst;
		ringbuf_entry.instid.i_name = acct_ops.get_comm(acctp);

		if (pmDebugOptions.libpmda && pmDebugOptions.desperate)
			pmNotifyErr(LOG_DEBUG, "acct: hash add pid=%d comm=%s\n", i_inst, acct_ops.get_comm(acctp));

		acct_ringbuf_add(&proc_acct->accthash, &ringbuf_entry);
		__pmHashAdd(i_inst, acctp, &proc_acct->accthash);
		need_update++;
	}

	if (need_update) {
		copy_ringbuf_to_indom(proc_acct->indom, now);
		if (pmDebugOptions.libpmda && pmDebugOptions.desperate)
			pmNotifyErr(LOG_DEBUG, "acct: update indom it_numinst=%d\n", proc_acct->indom->it_numinst);
	}
	acct_file.prev_size = acct_file_size;
}

int acct_fetchCallBack(int i_inst, int item, proc_acct_t* proc_acct, pmAtomValue *atom) {
	if (acct_file.fd < 0)
		return 0;

	__pmHashNode *node = __pmHashSearch(i_inst, &proc_acct->accthash);
	if (!node || !node->data)
		return 0;

	if (time(NULL) - acct_ops.get_end_time(node->data) > acct_lifetime)
		return 0;

	return acct_ops.fetchCallBack(item, node->data, atom);
}
