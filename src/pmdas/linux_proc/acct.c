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

#include "acct.h"

#define RINGBUF_SIZE			5000
unsigned long hertz;

static struct {
	const char* path;
	int fd;
	int prev_size;
	int acct_enabled;
	int version;
	int record_size;
	time_t last_fail_open;
	time_t last_check_accounting;
} acct_file;

typedef struct {
	time_t time;
	struct pmdaInstid instid;
} acct_ringbuf_entry_t;

static struct {
	acct_ringbuf_entry_t *buf;
	int next_index;
} acct_ringbuf;

static void init_acct_file_info(void) {
	memset(&acct_file, 0, sizeof(acct_file));
	acct_file.fd = -1;
}

void acct_init(proc_acct_t *proc_acct) {
	init_acct_file_info();

	acct_ringbuf.next_index = 0;
	acct_ringbuf.buf = calloc(RINGBUF_SIZE, sizeof(acct_ringbuf_entry_t));

	proc_acct->indom->it_numinst = 0;
	proc_acct->indom->it_set = calloc(RINGBUF_SIZE, sizeof(pmdaInstid));

	hertz = sysconf(_SC_CLK_TCK);
}

void refresh_acct(proc_acct_t *proc_acct) {
}

int acct_fetchCallBack(int i_inst, int item, proc_acct_t* proc_acct, pmAtomValue *atom) {
	return 0;
}
