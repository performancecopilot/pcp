/*
 * Copyright (c) 2013 Red Hat.
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
 */

#include "metrics.h"

#define ticks_to_msec(ticks) (1000ULL * strtoull(ticks, NULL, 0) / kernel_all_hz)

/* /proc/PID/stat fields */
#define PROC_PID_STAT_PID                2
#define PROC_PID_STAT_CMD                3
#define PROC_PID_STAT_STATE              4
#define PROC_PID_STAT_PPID               5
#define PROC_PID_STAT_PGRP               6
#define PROC_PID_STAT_SESSION            7
#define PROC_PID_STAT_TTY                8
#define PROC_PID_STAT_TTY_PGRP           9
#define PROC_PID_STAT_FLAGS              10
#define PROC_PID_STAT_MINFLT             11
#define PROC_PID_STAT_CMIN_FLT           12
#define PROC_PID_STAT_MAJ_FLT            13
#define PROC_PID_STAT_CMAJ_FLT           14
#define PROC_PID_STAT_UTIME              15
#define PROC_PID_STAT_STIME              16
#define PROC_PID_STAT_CUTIME             17
#define PROC_PID_STAT_CSTIME             18
#define PROC_PID_STAT_PRIORITY           19
#define PROC_PID_STAT_NICE               20
#define PROC_PID_STAT_REMOVED            21
#define PROC_PID_STAT_IT_REAL_VALUE      22
#define PROC_PID_STAT_START_TIME         23
#define PROC_PID_STAT_VSIZE              24
#define PROC_PID_STAT_RSS                25
#define PROC_PID_STAT_RSS_RLIM           26
#define PROC_PID_STAT_START_CODE         27
#define PROC_PID_STAT_END_CODE           28
#define PROC_PID_STAT_START_STACK        29
#define PROC_PID_STAT_ESP                30
#define PROC_PID_STAT_EIP                31
#define PROC_PID_STAT_SIGNAL             32
#define PROC_PID_STAT_BLOCKED            33
#define PROC_PID_STAT_SIGIGNORE          34
#define PROC_PID_STAT_SIGCATCH           35
#define PROC_PID_STAT_WCHAN              36
#define PROC_PID_STAT_NSWAP              37
#define PROC_PID_STAT_CNSWAP             38
#define PROC_PID_STAT_EXIT_SIGNAL        39
#define PROC_PID_STAT_PROCESSOR      	 40
#define PROC_PID_STAT_TTYNAME        	 41
#define PROC_PID_STAT_WCHAN_SYMBOL   	 42
#define PROC_PID_STAT_PSARGS         	 43

static void
inst_command_clean(char *command, size_t size)
{
    int i;

    /* if command contains non printable chars - replace 'em */
    for (i = 0; i < size; i++) {
	if (!isprint((int)command[i]))
	    command[i] = ' ';
    }
    /* and trailing whitespace - clean that */
    while (--size) {
	if (isspace((int)command[size]))
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
	else if (isspace((int)*p))
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
    int pid;
    int i;
    char *command;
    pmInDom indom = pmInDom_build(PROC_DOMAIN, PROC_PROC_INDOM);
    static char *inst = NULL;
    static fields_t *stashed_stat = NULL;
    static fields_t *stashed_vmpeak = NULL;
    static fields_t *stashed_vmsize = NULL;
    static fields_t *stashed_vmlock = NULL;
    static fields_t *stashed_vmhwn = NULL;
    static fields_t *stashed_vmrss = NULL;
    static fields_t *stashed_vmdata = NULL;
    static fields_t *stashed_vmstack = NULL;
    static fields_t *stashed_vmexe = NULL;
    static fields_t *stashed_vmlib = NULL;
    static fields_t *stashed_vmpte = NULL;
    static fields_t *stashed_vmswap = NULL;

    if (f->nfields < 2 || f->fieldlen[0] < 6)
    	return 0;

    if (sscanf(f->fields[0], "proc:%d", &pid) != 1)
    	return 0;

    if (strcmp(f->fields[1], "cmd") == 0) {
	/*
	 * The command name (without args) is the external instance name.
	 * Unfortunately, the proc stat entry occurs before proc cmd, so we
	 * have to stash the proc stat entry until after we know the external
	 * instance name, at which time it can be emitted.
	 */

	/* The new external instance name is the 6 digit pid + fields[2] */
	if (inst)
	    free(inst);
	inst = (char *)malloc(8 + f->fieldlen[2]);
	pmsprintf(inst, 8 + f->fieldlen[2], "%06d %s", pid, f->fields[2]);

	/* now we know the instance name, we can emit the stashed proc stat metrics */
	if (stashed_stat) {
	    /* 
	     * proc:28896 stat 28896 (bash) S 3574 28896 28896 34844 28896 4202496 2286 23473 1 21 4 2 486 129 20 0 1 0 125421198 119214080 85 18446744073709551615 4194304 5080360 140737040162832 140737040157848 212270400064 0 0 3686404 1266761467 18446744071582594345 0 0 17 2 0 0 0 0 0 9310744 9344396 14004224
	     *
	     * proc:23688 stat 23688 (MpxTestDaemon  ) S 2 0 0 0 -1 6332480 0 0 0 0 0 0 0 0 20 0 1 0 5730 0 0 18446744073709551615 0 0 0 0 0 0 2147483647 4096 0 18446744072101884088 0 0 17 47 0 0 0 0 0
	     */
	    put_ull_value("proc.psinfo.pid", indom, inst, pid);
	    put_str_value("proc.psinfo.sname", indom, inst, stashed_stat->fields[PROC_PID_STAT_STATE]);
	    put_str_value("proc.psinfo.ppid", indom, inst, stashed_stat->fields[PROC_PID_STAT_PPID]);

	    put_ull_value("proc.psinfo.utime", indom, inst, ticks_to_msec(stashed_stat->fields[PROC_PID_STAT_UTIME]));
	    put_ull_value("proc.psinfo.stime", indom, inst, ticks_to_msec(stashed_stat->fields[PROC_PID_STAT_STIME]));
	    put_ull_value("proc.psinfo.cutime", indom, inst, ticks_to_msec(stashed_stat->fields[PROC_PID_STAT_CUTIME]));
	    put_ull_value("proc.psinfo.cstime", indom, inst, ticks_to_msec(stashed_stat->fields[PROC_PID_STAT_CSTIME]));
	    put_str_value("proc.psinfo.priority", indom, inst, stashed_stat->fields[PROC_PID_STAT_PRIORITY]);
	    put_str_value("proc.psinfo.nice", indom, inst, stashed_stat->fields[PROC_PID_STAT_NICE]);

	    put_ull_value("proc.psinfo.start_time", indom, inst, ticks_to_msec(stashed_stat->fields[PROC_PID_STAT_START_TIME]));

	    put_str_value("proc.psinfo.vsize", indom, inst, stashed_stat->fields[PROC_PID_STAT_VSIZE]);
	    put_str_value("proc.psinfo.rss", indom, inst, stashed_stat->fields[PROC_PID_STAT_RSS]);

	    put_str_value("proc.psinfo.blocked", indom, inst, stashed_stat->fields[PROC_PID_STAT_BLOCKED]);
	    put_str_value("proc.psinfo.wchan_s", indom, inst, stashed_stat->fields[PROC_PID_STAT_WCHAN_SYMBOL]);

	    put_str_value("proc.psinfo.processor", indom, inst, stashed_stat->fields[PROC_PID_STAT_PROCESSOR]);

	    fields_free(stashed_stat);
	    stashed_stat = NULL;
	}

	if (stashed_vmpeak) {
	    put_str_value("proc.memory.vmpeak", indom, inst, stashed_vmpeak->fields[2]);
	    fields_free(stashed_vmpeak);
	    stashed_vmpeak = NULL;
	}
	if (stashed_vmsize) {
	    put_str_value("proc.memory.vmsize", indom, inst, stashed_vmsize->fields[2]);
	    fields_free(stashed_vmsize);
	    stashed_vmsize = NULL;
	}
	if (stashed_vmlock) {
	    put_str_value("proc.memory.vmlock", indom, inst, stashed_vmlock->fields[2]);
	    fields_free(stashed_vmlock);
	    stashed_vmlock = NULL;
	}
	if (stashed_vmhwn) {
	    put_str_value("proc.memory.vmhwn", indom, inst, stashed_vmhwn->fields[2]);
	    fields_free(stashed_vmhwn);
	    stashed_vmhwn = NULL;
	}
	if (stashed_vmrss) {
	    put_str_value("proc.memory.vmrss", indom, inst, stashed_vmrss->fields[2]);
	    fields_free(stashed_vmrss);
	    stashed_vmrss = NULL;
	}
	if (stashed_vmdata) {
	    put_str_value("proc.memory.vmdata", indom, inst, stashed_vmdata->fields[2]);
	    fields_free(stashed_vmdata);
	    stashed_vmdata = NULL;
	}
	if (stashed_vmstack) {
	    put_str_value("proc.memory.vmstack", indom, inst, stashed_vmstack->fields[2]);
	    fields_free(stashed_vmstack);
	    stashed_vmstack = NULL;
	}
	if (stashed_vmexe) {
	    put_str_value("proc.memory.vmexe", indom, inst, stashed_vmexe->fields[2]);
	    fields_free(stashed_vmexe);
	    stashed_vmexe = NULL;
	}
	if (stashed_vmlib) {
	    put_str_value("proc.memory.vmlib", indom, inst, stashed_vmlib->fields[2]);
	    fields_free(stashed_vmlib);
	}
	if (stashed_vmpte) {
	    put_str_value("proc.memory.vmpte", indom, inst, stashed_vmpte->fields[2]);
	    fields_free(stashed_vmpte);
	    stashed_vmpte = NULL;
	}
	if (stashed_vmswap) {
	    put_str_value("proc.memory.vmswap", indom, inst, stashed_vmswap->fields[2]);
	    fields_free(stashed_vmswap);
	    stashed_vmswap = NULL;
	}

	/*
	 * e.g. :
	 * proc:27041 cmd /bin/sh /usr/prod/mts/common/bin/dblogin_gateway_reader
	 */
	command = (char *)malloc(f->len);
	memset(command, 0, f->len);
	strcpy(command, f->fields[2]);
	for (i=3; i < f->nfields; i++) {
	    strcat(command, " ");
	    strcat(command, f->fields[i]);
	}
	inst_command_clean(command, f->len);
	put_str_value("proc.psinfo.cmd", indom, inst, f->fields[2]);
	put_str_value("proc.psinfo.psargs", indom, inst, command);
	free(command);
    }
    else if (strcmp(f->fields[1], "stat") == 0) {
	/* stashed until proc cmd is seen, at which time we know the instance name */
    	stashed_stat = fields_dup(f);
    }
    else if (strcmp(f->fields[1], "VmPeak:") == 0)
	stashed_vmpeak = fields_dup(f);
    else if (strcmp(f->fields[1], "VmSize:") == 0)
	stashed_vmsize = fields_dup(f);
    else if (strcmp(f->fields[1], "VmLck:") == 0)
	stashed_vmlock = fields_dup(f);
    else if (strcmp(f->fields[1], "VmHWM:") == 0)
	stashed_vmhwn = fields_dup(f);
    else if (strcmp(f->fields[1], "VmRSS:") == 0)
	stashed_vmrss = fields_dup(f);
    else if (strcmp(f->fields[1], "VmData:") == 0)
	stashed_vmdata = fields_dup(f);
    else if (strcmp(f->fields[1], "VmStk:") == 0)
	stashed_vmstack = fields_dup(f);
    else if (strcmp(f->fields[1], "VmExe:") == 0)
	stashed_vmexe = fields_dup(f);
    else if (strcmp(f->fields[1], "VmLib:") == 0)
	stashed_vmlib = fields_dup(f);
    else if (strcmp(f->fields[1], "VmPTE:") == 0)
	stashed_vmpte = fields_dup(f);
    else if (strcmp(f->fields[1], "VmSwap:") == 0)
	stashed_vmswap = fields_dup(f);

    if (strcmp(f->fields[1], "io") == 0) {
	if (strcmp(f->fields[2], "syscr:") == 0)
	    put_str_value("proc.io.syscr", indom, inst, f->fields[3]);
	else if (strcmp(f->fields[2], "syscw:") == 0)
	    put_str_value("proc.io.syscw", indom, inst, f->fields[3]);
	else if (strcmp(f->fields[2], "read_bytes:") == 0)
	    put_str_value("proc.io.read_bytes", indom, inst, f->fields[3]);
	else if (strcmp(f->fields[2], "write_bytes:") == 0)
	    put_str_value("proc.io.write_bytes", indom, inst, f->fields[3]);
	else if (strcmp(f->fields[2], "cancelled_write_bytes:") == 0)
	    put_str_value("proc.io.cancelled_write_bytes", indom, inst, f->fields[3]);
    }

    return 0;
}
