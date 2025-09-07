/*
 * Copyright (c) 2025 Red Hat.
 * Portions Copyright (c) 2015 Hisham H. Muhammad.
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

#include <assert.h>

#include "pmapi.h"
#include "libpcp.h"
#include "pmda.h"

#include "kinfo_proc.h"

typedef struct {
    short int major, minor, patch;
} darwin_version_t;

static int
darwin_compare_version(darwin_version_t version)
{
    static darwin_version_t actual;

    if (!actual.major) {
	char	s[256] = { 0 };
	size_t	sz = sizeof(s);

	if (sysctlbyname("kern.osrelease", s, &sz, NULL, 0) == 0)
	    sscanf(s, "%hd.%hd.%hd", &actual.major, &actual.minor, &actual.patch);
    }

    if (actual.major != version.major)
	return actual.major - version.major;
    if (actual.minor != version.minor)
        return actual.minor - version.minor;
    if (actual.patch != version.patch)
        return actual.patch - version.patch;
    return 0;
}

static int
darwin_version_in_range(darwin_version_t v_lower, darwin_version_t v_upper)
{
    return 0 <= darwin_compare_version(v_lower) &&
		darwin_compare_version(v_upper) < 0;
}

static double
darwin_ticks_to_nsecs(const double scheduler_ticks)
{
    static double darwin_nsec_per_tick = -1;

    if (darwin_nsec_per_tick < 0) {
	const double nsec_per_sec = 1e9;
	long ticks_per_sec = sysconf(_SC_CLK_TCK);
	darwin_nsec_per_tick = nsec_per_sec / ticks_per_sec;
	assert(darwin_nsec_per_tick != 0.0);
    }
    return scheduler_ticks * darwin_nsec_per_tick;
}

static size_t
darwin_processes(struct kinfo_proc **kinfop)
{
    int			mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0 };
    unsigned int	retries;
    struct kinfo_proc	*p, *kinfo = NULL;

    for (retries = 0; retries < 4; retries++) {
	size_t size = 0;

	if (sysctl(mib, 4, NULL, &size, NULL, 0) < 0) {
	    pmNotifyErr(LOG_ERR, "Cannot get process count: %s", osstrerror());
	    goto failed;
	}
	if (size == 0) {
	    pmNotifyErr(LOG_ERR, "Unable to retrieve process table size");
	    goto failed;
	}

	size += 16 * retries * retries * sizeof(struct kinfo_proc);
	if ((p = realloc(kinfo, size)) == NULL) {
	    pmNotifyErr(LOG_ERR, "Out-of-memory on kinfo_proc resize");
	    goto failed;
	}
	kinfo = p;

	if (sysctl(mib, 4, kinfo, &size, NULL, 0) == 0) {
	    *kinfop = kinfo;
	    return size / sizeof(struct kinfo_proc);
	}

	if (errno != ENOMEM)
	    break;
    }
    pmNotifyErr(LOG_ERR, "Cannot get kinfo_proc array: %s", osstrerror());

failed:
    free(kinfo);
    *kinfop = NULL;
    return 0;
}

static char *
darwin_command_line(pid_t pid, char *buf, size_t buflen)
{
    int		mib[3], nargs, c = 0;
    char	*procargs, *sp, *np, *cp;
    size_t	size;
    static int	argmax;

    if (argmax <= 0) {
	/* Get the maximum process arguments size. */
	mib[0] = CTL_KERN;
	mib[1] = KERN_ARGMAX;
	size = sizeof(argmax);
	if (sysctl(mib, 2, &argmax, &size, NULL, 0) == -1 || argmax <= 0)
	    return NULL;
    }

    /* Allocate space for the arguments. */
    if ((procargs = (char *)malloc(argmax)) == NULL)
        return NULL;

    /*
     * Make a sysctl() call to get the raw argument space of the process.
     * The layout is documented in start.s.  In summary, it looks like:
     *
     * /---------------\ 0x00000000
     * :               :
     * :               :
     * |---------------|
     * | argc          |
     * |---------------|
     * | arg[0]        |
     * |---------------|
     * :               :
     * :               :
     * |---------------|
     * | arg[argc - 1] |
     * |---------------|
     * | 0             |
     * |---------------|
     * | env[0]        |
     * |---------------|
     * :               :
     * :               :
     * |---------------|
     * | env[n]        |
     * |---------------|
     * | 0             |
     * |---------------| <-- Beginning of data returned by sysctl() is here.
     * | argc          |
     * |---------------|
     * | exec_path     |
     * |:::::::::::::::|
     * |               |
     * | String area.  |
     * |               |
     * |---------------| <-- Top of stack.
     * :               :
     * :               :
     * \---------------/ 0xffffffff
     */

    mib[0] = CTL_KERN;
    mib[1] = KERN_PROCARGS2;
    mib[2] = pid;
    size = (size_t)argmax;
    if (sysctl(mib, 3, procargs, &size, NULL, 0) == -1)
        goto error;

    memcpy(&nargs, procargs, sizeof(nargs));
    cp = procargs + sizeof(nargs);

    /* Skip the saved exec_path */
    for (; cp < &procargs[size]; cp++) {
        if (*cp == '\0') /* end of exec_path */
            break;
    }
    if (cp == &procargs[size])
        goto error;

    /* Skip trailing '\0' characters. */
    for ( ; cp < &procargs[size]; cp++ ) {
        if (*cp != '\0') /* reached first argument */
            break;
    }
    if (cp == &procargs[size])
        goto error;

    /* Save where the argv[0] string starts */
    sp = cp;

    size_t end = 0;
    for (np = NULL; c < nargs && cp < &procargs[size]; cp++) {
	if (*cp == '\0') {
            c++;
	    if (np != NULL) /* convert previous '\0' */
                *np = ' ';
            np = cp; /* note location of current '\0' */
	    if (end == 0)
		end = (size_t)(cp - sp);
	}
    }
    /*
     * sp points to the beginning of the arguments/environment string,
     * and np should point to the '\0' terminator for the string.
     */
    if (np == NULL || np == sp)
	goto error; /* empty or unterminated string */

    pmstrncpy(buf, buflen, sp);
    free(procargs);
    return buf;

error:
    free(procargs);
    return NULL;
}

static size_t
darwin_process_threads(darwin_procs_t *processes, darwin_runq_t *runq,
	darwin_proc_t *proc)
{
    mach_msg_type_number_t thread_count, i;
    thread_array_t	threads;
    size_t		bytes, count = 0;
    task_t		task;
    pid_t		id;
    char		*p, pid[32];

    if (!(proc->flags & PROC_FLAG_PINFO) || proc->state[0] == 'Z')
	return 0;

    if (task_for_pid(mach_task_self(), proc->id, &task) != KERN_SUCCESS)
	return 0;

    if (task_threads(task, &threads, &thread_count) != KERN_SUCCESS) {
	mach_port_deallocate(mach_task_self(), task);
	return 0;
    }

    for (i = 0; i < thread_count; i++) {
	__pmHashNode	*node;
	darwin_proc_t	*thread;
	thread_extended_info_data_t	extended_info;
	thread_identifier_info_data_t	identifer_info;
	mach_msg_type_number_t	extended_info_n = THREAD_EXTENDED_INFO_COUNT;
	mach_msg_type_number_t	identifer_info_n = THREAD_IDENTIFIER_INFO_COUNT;

	if (thread_info(threads[i], THREAD_IDENTIFIER_INFO, (thread_info_t)
			&identifer_info, &identifer_info_n) != KERN_SUCCESS) {
	    count++;
	    continue;
	}
	if (thread_info(threads[i], THREAD_EXTENDED_INFO, (thread_info_t)
			&extended_info, &extended_info_n) != KERN_SUCCESS) {
	    count++;
	    continue;
	}

	id = identifer_info.thread_id;
	if ((node = __pmHashSearch(id, processes))) {
	    thread = (darwin_proc_t *)node->data;
	    assert(thread->id == id);
	} else if ((thread = (darwin_proc_t *)malloc(sizeof(darwin_proc_t)))) {
	    memcpy(thread, proc, sizeof(darwin_proc_t));
	    thread->psargs = thread->instname = NULL;
	    thread->id = id;
	    __pmHashAdd(thread->id, (void *)thread, processes);
	} else {
	    pmNotifyErr(LOG_ERR, "Out-of-memory processing thread %d", id);
	    count++;
	    continue;
	}
	thread->flags = (PROC_FLAG_VALID | PROC_FLAG_THREAD);
	thread->utime = extended_info.pth_user_time;
	thread->stime = extended_info.pth_system_time;
	thread->priority = extended_info.pth_curpri;

	if (extended_info.pth_run_state & TH_STATE_UNINTERRUPTIBLE) {
	    pmsprintf(thread->state, sizeof(thread->state), "B");
	    runq->blocked++;
	} else if (extended_info.pth_run_state & TH_STATE_RUNNING) {
	    pmsprintf(thread->state, sizeof(thread->state), "R");
	    runq->runnable++;
	} else if (extended_info.pth_run_state & TH_STATE_STOPPED) {
	    pmsprintf(thread->state, sizeof(thread->state), "T");
	    runq->stopped++;
	} else if (extended_info.pth_run_state & TH_STATE_WAITING) {
	    pmsprintf(thread->state, sizeof(thread->state), "S");
	    runq->sleeping++;
	} else if (extended_info.pth_run_state & TH_STATE_HALTED) {
	    pmsprintf(thread->state, sizeof(thread->state), "T");
	    runq->stopped++;
	} else if (extended_info.pth_flags & TH_FLAGS_SWAPPED) {
	    pmsprintf(thread->state, sizeof(thread->state), "SW");
	    runq->swapped++;
	} else {
	    pmsprintf(thread->state, sizeof(thread->state), "?");
	    runq->unknown++;
	}

	if ((p = extended_info.pth_name) != NULL) {
	    bytes = pmsprintf(pid, sizeof(pid), "%06d ", proc->id);
	    bytes += strlen(p) + 1;
	    if ((thread->instname = (char *)malloc(bytes)) != NULL)
		pmsprintf(thread->instname, bytes, "%s%s", pid, p);
	} else {
	    thread->instname = strdup(proc->instname);
	}
    }

    count = thread_count - count;
    bytes = count * sizeof(thread_port_array_t);
    vm_deallocate(mach_task_self(), (vm_address_t)threads, bytes);
    mach_port_deallocate(mach_task_self(), task);
    return count;
}

static __pmHashWalkState
darwin_invalidate_node(const __pmHashNode *node, void *data)
{
    darwin_proc_t	*proc = (darwin_proc_t *)node->data;

    (void)data;
    proc->flags = 0;
    return PM_HASH_WALK_NEXT;
}

static __pmHashWalkState
darwin_update_instance(const __pmHashNode *node, void *data)
{
    darwin_proc_t	*proc = (darwin_proc_t *)node->data;
    pmdaIndom		*indom = (pmdaIndom *)data;
    pmdaInstid		*instid = &indom->it_set[indom->it_numinst];

    if (!(proc->flags & PROC_FLAG_VALID)) {
	free(proc->instname);
	return PM_HASH_WALK_DELETE_NEXT;
    }

    indom->it_numinst++;
    instid->i_inst = proc->id;		/* internal instid is PID */
    instid->i_name = proc->instname;	/* reference, do not free */
    return PM_HASH_WALK_NEXT;
}

void
darwin_refresh_processes(pmdaIndom *indomp, darwin_procs_t *processes,
	darwin_runq_t *runq, int want_threads)
{
    struct kinfo_proc	*procs;
    pmdaInstid		*insts;
    size_t		bytes, count, i, total;
    static int		have_threads = -1;

    if (have_threads == -1) {
	/* disabled on macOS High Sierra due to a kernel bug */
	have_threads = !darwin_version_in_range(
				(darwin_version_t) {17, 0, 0},
				(darwin_version_t) {17, 5, 0});
    }

    /*
     * Invalidate all and harvest procs that have exited later
     * (i.e. those still invalidated after walking active set).
     * Also reset run-queue counters for fresh calculation.
     */
    __pmHashWalkCB(darwin_invalidate_node, NULL, processes);
    memset(runq, 0, sizeof(*runq));

    /*
     * Use kinfo_procs for initial scan, then find additional
     * information for each individual process using libproc.
     */
    count = total = darwin_processes(&procs);

    for (i = 0; i < count; i++) {
	struct proc_taskinfo pti;
	struct extern_proc *xproc = &procs[i].kp_proc;
	struct eproc	*eproc = &procs[i].kp_eproc;
	darwin_proc_t	*proc;
	__pmHashNode	*node;
	char		path[PROC_PIDPATHINFO_MAXSIZE];
	char		pid[32];
	char		*p;

	if ((node = __pmHashSearch(xproc->p_pid, processes))) {
	    proc = (darwin_proc_t *)node->data;
	    assert(proc->id == xproc->p_pid);
	} else if ((proc = (darwin_proc_t *)calloc(1, sizeof(darwin_proc_t)))) {
	    proc->cwd_id = proc->exe_id = proc->cmd_id = proc->msg_id = -1;
	    proc->id = xproc->p_pid;
	    __pmHashAdd(proc->id, (void *)proc, processes);
	} else {
	    total--;
	    pmNotifyErr(LOG_ERR, "Out-of-memory processing PID %d", xproc->p_pid);
	    continue;
	}

	proc->flags = PROC_FLAG_VALID;	/* new or still running */
	proc->ppid = eproc->e_ppid;
	proc->pgid = eproc->e_pgid;
	proc->tpgid = eproc->e_tpgid;
	proc->suid = eproc->e_pcred.p_svuid;
	proc->sgid = eproc->e_pcred.p_svgid;
	proc->euid = eproc->e_pcred.p_ruid;
	proc->egid = eproc->e_pcred.p_rgid;
	proc->uid = eproc->e_ucred.cr_uid;
	proc->gid = eproc->e_ucred.cr_ngroups > 0 ?
		    eproc->e_ucred.cr_groups[0] : eproc->e_pcred.p_rgid;
	proc->ngid = eproc->e_ucred.cr_ngroups;
	proc->priority = xproc->p_priority;
	proc->usrpri = xproc->p_usrpri;
	proc->nice = xproc->p_nice;
	proc->tty = eproc->e_tdev;
	memcpy(proc->comm, xproc->p_comm, MAXCOMLEN);
	proc->comm[MAXCOMLEN] = '\0';
	memcpy(proc->wchan, eproc->e_wmesg, WMESGLEN);
	proc->wchan[WMESGLEN] = '\0';
	proc->wchan_addr = (uint64_t)xproc->p_wchan;
	proc->start_time = xproc->p_starttime.tv_sec;
	proc->translated = xproc->p_flag & P_TRANSLATED;
	proc->threads = 1;

	/* update the runq statistics */
	if (xproc->p_stat == SRUN) {
	    pmsprintf(proc->state, sizeof(proc->state), "R");
	    runq->runnable++;
	} else if (xproc->p_stat == SSLEEP) {
	    pmsprintf(proc->state, sizeof(proc->state), "S");
	    runq->sleeping++;
	} else if (xproc->p_stat == SZOMB) {
	    pmsprintf(proc->state, sizeof(proc->state), "Z");
	    runq->defunct++;
	} else if (xproc->p_stat == SSTOP) {
	    pmsprintf(proc->state, sizeof(proc->state), "T");
	    runq->stopped++;
	} else if (xproc->p_stat == SIDL) {
	    pmsprintf(proc->state, sizeof(proc->state), "B");
	    runq->blocked++;
	} else {
	    pmsprintf(proc->state, sizeof(proc->state), "?");
	    runq->unknown++;
	}

	if (proc->msg_id == -1 && xproc->p_wmesg)
	    proc->msg_id = proc_strings_insert(xproc->p_wmesg);

	/* full command line arguments */
	if (proc->cmd_id == -1) {
	    char cmd[PROC_CMD_MAXLEN];

	    if ((darwin_command_line(proc->id, cmd, sizeof(cmd))) == NULL)
		pmstrncpy(cmd, sizeof(cmd), proc->comm);
	    proc->cmd_id = proc_strings_insert(cmd);
	    p = proc_strings_lookup(proc->cmd_id);
	    if ((proc->psargs = strchr(p, ' ')) != NULL)
		proc->psargs++;	/* move past space character */
	}

	/* full path to the executable */
	if (proc_pidpath(proc->id, path, sizeof(path)) <= 0)
	    pmstrncpy(path, sizeof(path), proc->comm);
	proc->exe_id = proc_strings_insert(path);

	/*
	 * The external instance name is the PID followed by the
	 * full executable path, e.g.  "012345 /path/to/command".
	 * The full command line is the proc.psinfo.psargs value.
	 */
	if (proc->instname == NULL) {
	    bytes = pmsprintf(pid, sizeof(pid), "%06d ", proc->id);
	    bytes += strlen(path) + 1;
	    if ((proc->instname = (char *)malloc(bytes)) != NULL)
		pmsprintf(proc->instname, bytes, "%s%s", pid, path);
	}

	/* current working directory */
	if (proc->cwd_id == -1) {
	    struct proc_vnodepathinfo vpi;

	    if (proc_pidinfo(proc->id, PROC_PIDVNODEPATHINFO, 0, &vpi,
		    sizeof(vpi)) > 0 &&
		vpi.pvi_cdir.vip_path[0] != '\0')
		proc->cwd_id = proc_strings_insert(vpi.pvi_cdir.vip_path);
	}

	if (have_threads && want_threads)
	    total += darwin_process_threads(processes, runq, proc);

	/* additional stats from libproc.h interfaces */
	if (proc_pidinfo(proc->id, PROC_PIDTASKINFO, 0, &pti,
		PROC_PIDTASKINFO_SIZE) != PROC_PIDTASKINFO_SIZE)
	    continue;
	proc->flags |= PROC_FLAG_PINFO;
	proc->majflt = pti.pti_faults;
	proc->threads = pti.pti_threadnum;
	proc->utime = darwin_ticks_to_nsecs(pti.pti_total_user);
	proc->stime = darwin_ticks_to_nsecs(pti.pti_total_system);
	proc->size = pti.pti_virtual_size / 1024;
	proc->rss = pti.pti_resident_size / 1024;
	proc->pswitch = pti.pti_csw;
    }

    free(procs);

    /*
     * Allocate the instance domain to be exported this sample;
     * harvest any processes that are still not marked invalid.
     */
    count = total * sizeof(pmdaInstid);
    if ((insts = (pmdaInstid *)realloc(indomp->it_set, count)) != NULL) {
	indomp->it_set = insts;
	indomp->it_numinst = 0; /* updated in the callback below */
	__pmHashWalkCB(darwin_update_instance, indomp, processes);
    } else {
	free(indomp->it_set);
	indomp->it_set = NULL;
	indomp->it_numinst = 0;
    }
}
