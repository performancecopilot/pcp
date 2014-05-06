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
 * Handler for per-process metrics
 *
 * proc:28896 stat 28896 (bash) S 3574 28896 28896 34844 28896 4202496 2286 23473 1 21 4 2 486 129 20 0 1 0 125421198 119214080 85 18446744073709551615 4194304 5080360 140737040162832 140737040157848 212270400064 0 0 3686404 1266761467 18446744071582594345 0 0 17 2 0 0 0 0 0 9310744 9344396 14004224
 *
 */

#include "metrics.h"

#define ticks_to_msec(ticks) (1000ULL * strtoull(ticks, NULL, 0) / kernel_all_hz)

/* /proc/PID/stat fields (starting at fields[2]) */
#define PROC_PID_STAT_PID                0
#define PROC_PID_STAT_CMD                1
#define PROC_PID_STAT_STATE              2
#define PROC_PID_STAT_PPID               3
#define PROC_PID_STAT_PGRP               4
#define PROC_PID_STAT_SESSION            5
#define PROC_PID_STAT_TTY                6
#define PROC_PID_STAT_TTY_PGRP           7
#define PROC_PID_STAT_FLAGS              8
#define PROC_PID_STAT_MINFLT             9
#define PROC_PID_STAT_CMIN_FLT           10
#define PROC_PID_STAT_MAJ_FLT            11
#define PROC_PID_STAT_CMAJ_FLT           12
#define PROC_PID_STAT_UTIME              13
#define PROC_PID_STAT_STIME              14
#define PROC_PID_STAT_CUTIME             15
#define PROC_PID_STAT_CSTIME             16
#define PROC_PID_STAT_PRIORITY           17
#define PROC_PID_STAT_NICE               18
#define PROC_PID_STAT_REMOVED            19
#define PROC_PID_STAT_IT_REAL_VALUE      20
#define PROC_PID_STAT_START_TIME         21
#define PROC_PID_STAT_VSIZE              22
#define PROC_PID_STAT_RSS                23
#define PROC_PID_STAT_RSS_RLIM           24
#define PROC_PID_STAT_START_CODE         25
#define PROC_PID_STAT_END_CODE           26
#define PROC_PID_STAT_START_STACK        27
#define PROC_PID_STAT_ESP                28
#define PROC_PID_STAT_EIP                29
#define PROC_PID_STAT_SIGNAL             30
#define PROC_PID_STAT_BLOCKED            31
#define PROC_PID_STAT_SIGIGNORE          32
#define PROC_PID_STAT_SIGCATCH           33
#define PROC_PID_STAT_WCHAN              34
#define PROC_PID_STAT_NSWAP              35
#define PROC_PID_STAT_CNSWAP             36
#define PROC_PID_STAT_EXIT_SIGNAL        37
#define PROC_PID_STAT_PROCESSOR      	 38
#define PROC_PID_STAT_TTYNAME        	 39
#define PROC_PID_STAT_WCHAN_SYMBOL   	 40
#define PROC_PID_STAT_PSARGS         	 41

static char *inst;
static fields_t *proc_stat;

static int
find_command_start(const char *buf, size_t len)
{
    int i;

    /* skip over (minimal) leading "proc:N cmd " */
    for (i = 7; i < len - 4; i++)
	if (strncmp(&buf[i], "cmd", 4) == 0)
	    return i + 4;
    return -1;	/* wha?  cannot find the "cmd" component */
}

static void
inst_command_clean(char *command, size_t size)
{
    int i;

    /* command contains nulls - replace 'em */
    for (i = 0; i < size; i++) {
	if (!isprint(command[i]))
	    command[i] = ' ';
    }
    /* and trailing whitespace - clean that */
    while (--size) {
	if (isspace(command[size]))
	    command[size] = '\0';
	else
	    break;
    }
}

void
base_command_name(const char *command, char *base, size_t size)
{
    char *p, *start, *end;
    int kernel = (command[0] == '(');	/* kernel daemons heuristic */

    /* moral equivalent of basename, dealing with args stripping too */
    for (p = end = start = (char *)command; *p; end = ++p) {
	if (kernel)
	    continue;
	else if (*p == '/')
	    start = end = p+1;
	else if (isspace(*p))
	    break;
    }
    size--;	/* allow for a null */
    if (size > (end - start))
	size = (end - start);
    memcpy(base, start, size);
    base[size] = '\0';
}

int
proc_handler(handler_t *h, fields_t *f)
{
    int pid, off, bytes;
    char *command;
    size_t size;

    if (f->nfields < 2 || f->fieldlen[0] < 6)
    	return 0;

    if (strcmp(f->fields[1], "cmd") == 0) {
	/*
	 * e.g. :
	 * proc:27041 cmd /bin/sh /usr/prod/mts/common/bin/dblogin_gateway_reader
	 */
	if ((off = find_command_start(f->buf, f->len)) < 0)
	    return 0;
	size = f->len - off + 16;	/* +16 for the "%06d " pid */
	if ((inst = (char *)malloc(size)) == NULL)
	    return 0;
	sscanf(f->buf, "proc:%d", &pid);
	bytes = snprintf(inst, size, "%06d ", pid);

	/* f->buf contains nulls - so memcpy it then replace 'em */
	size = f->len - off - 1;
	command = inst + bytes;
	memcpy(command, f->buf + off, size);
	command[size] = '\0';
	inst_command_clean(command, size);
    }

    if (inst == NULL && strcmp(f->fields[1], "stat") == 0) {
	/* no instance yet, so stash it for later */
	proc_stat = fields_dup(f);
	return 0;
    }

    if (inst) {
	pmInDom indom = pmInDom_build(PROC_DOMAIN, PROC_PROC_INDOM);

	if ((command = strchr(inst, ' ')) != NULL) {
	    char cmdname[MAXPATHLEN];

	    command++;
	    base_command_name(command, &cmdname[0], sizeof(cmdname));
	    put_str_value("proc.psinfo.cmd", indom, inst, cmdname);
	    put_str_value("proc.psinfo.psargs", indom, inst, command);
	}

	/* emit the stashed proc_stat fields */
	put_ull_value("proc.psinfo.utime", indom, inst, ticks_to_msec(proc_stat->fields[PROC_PID_STAT_UTIME+2]));
	put_ull_value("proc.psinfo.stime", indom, inst, ticks_to_msec(proc_stat->fields[PROC_PID_STAT_STIME+2]));
	put_str_value("proc.psinfo.processor", indom, inst, proc_stat->fields[PROC_PID_STAT_PROCESSOR+2]);
	put_str_value("proc.psinfo.rss", indom, inst, proc_stat->fields[PROC_PID_STAT_RSS+2]);
	put_str_value("proc.psinfo.vsize", indom, inst, proc_stat->fields[PROC_PID_STAT_VSIZE+2]);
	put_str_value("proc.psinfo.sname", indom, inst, proc_stat->fields[PROC_PID_STAT_STATE+2]);
	/* and the rest .. */
	fields_free(proc_stat);
	proc_stat = NULL;

	/* TODO emit other stashed stuff .. */

	free(inst);
	inst = NULL;
    }

    return 0;
}
