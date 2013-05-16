/*
 * Linux proc/<pid>/{stat,statm,status,maps} Clusters
 *
 * Copyright (c) 2000,2004,2006 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2010 Aconex.  All Rights Reserved.
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

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include "proc_pid.h"

static proc_pid_list_t allpids;

int
compare_pid(const void *pa, const void *pb)
{
    int a = *(int *)pa;
    int b = *(int *)pb;
    return a - b;
}

void
pidlist_append(proc_pid_list_t *list, const char *pidname)
{
    if (list->count >= list->size) {
	list->size += 64;
	if (!(list->pids = (int *)realloc(list->pids, list->size * sizeof(int)))) {
	    perror("pidlist_append: out of memory");
	    exit(1); /* no recovery from this */
	}
    }
    list->pids[list->count++] = atoi(pidname);
}

static int
refresh_pidlist()
{
    DIR *dirp, *taskdirp;
    struct dirent *dp, *tdp;
    char taskpath[1024];

    if ((dirp = opendir("/proc")) == NULL)
	return -oserror();

    allpids.count = 0;
    while ((dp = readdir(dirp)) != NULL) {
	if (isdigit((int)dp->d_name[0])) {
	    pidlist_append(&allpids, dp->d_name);
	    /* readdir on /proc ignores threads */ 
	    sprintf(taskpath, "/proc/%s/task", dp->d_name);
	    if ((taskdirp = opendir(taskpath)) != NULL) {
		while ((tdp = readdir(taskdirp)) != NULL) {
		    if (!isdigit((int)tdp->d_name[0]) || strcmp(dp->d_name, tdp->d_name) == 0)
		    	continue;
		    pidlist_append(&allpids, tdp->d_name);
		}
		closedir(taskdirp);
	    }
	}
    }
    closedir(dirp);

    qsort(allpids.pids, allpids.count, sizeof(int), compare_pid);
    return allpids.count;
}

int
refresh_proc_pidlist(proc_pid_t *proc_pid, proc_pid_list_t *pidlist)
{
    int i;
    int fd;
    char *p;
    char buf[1024];
    __pmHashNode *node, *next, *prev;
    proc_pid_entry_t *ep;
    pmdaIndom *indomp = proc_pid->indom;

    if (indomp->it_numinst < pidlist->count)
	indomp->it_set = (pmdaInstid *)realloc(indomp->it_set,
						pidlist->count * sizeof(pmdaInstid));
    indomp->it_numinst = pidlist->count;

    /*
     * invalidate all entries so we can harvest pids that have exited
     */
    for (i=0; i < proc_pid->pidhash.hsize; i++) {
	for (node=proc_pid->pidhash.hash[i]; node != NULL; node = node->next) {
	    ep = (proc_pid_entry_t *)node->data;
	    ep->valid = 0;
	    ep->stat_fetched = 0;
	    ep->statm_fetched = 0;
	    ep->status_fetched = 0;
	    ep->schedstat_fetched = 0;
	    ep->maps_fetched = 0;
	    ep->io_fetched = 0;
	    ep->wchan_fetched = 0;
	    ep->fd_fetched = 0;
	}
    }

    /*
     * walk pidlist and add new pids to the hash table,
     * marking entries valid as we go ...
     */
    for (i=0; i < pidlist->count; i++) {
	node = __pmHashSearch(pidlist->pids[i], &proc_pid->pidhash);
	if (node == NULL) {
	    int k = 0;

	    ep = (proc_pid_entry_t *)malloc(sizeof(proc_pid_entry_t));
	    memset(ep, 0, sizeof(proc_pid_entry_t));

	    ep->id = pidlist->pids[i];

	    sprintf(buf, "/proc/%d/cmdline", pidlist->pids[i]);
	    if ((fd = open(buf, O_RDONLY)) >= 0) {
		sprintf(buf, "%06d ", pidlist->pids[i]);
		if ((k = read(fd, buf+7, sizeof(buf)-8)) > 0) {
		    p = buf + k +7;
		    *p-- = '\0';
		    /* Skip trailing nils, i.e. don't replace them */
		    while (buf+7 < p) {
			if (*p-- != '\0') {
				break;
			}
		    }
		    /* Remove NULL terminators from cmdline string array */
		    /* Suggested by Mike Mason <mmlnx@us.ibm.com> */
		    while (buf+7 < p) {
			if (*p == '\0') *p = ' ';
			p--;
		    }
		}
		close(fd);
	    }

	    if (k == 0) {
		/*
		 * If a process is swapped out, /proc/<pid>/cmdline
		 * returns an empty string so we have to get it
		 * from /proc/<pid>/status or /proc/<pid>/stat
		 */
		sprintf(buf, "/proc/%d/status", pidlist->pids[i]);
		if ((fd = open(buf, O_RDONLY)) >= 0) {
		    /* We engage in a bit of a hanky-panky here:
		     * the string should look like "123456 (name)",
		     * we get it from /proc/XX/status as "Name:   name\n...",
		     * to fit the 6 digits of PID and opening parenthesis, 
	             * save 2 bytes at the start of the buffer. 
                     * And don't forget to leave 2 bytes for the trailing 
		     * parenthesis and the nil. Here is
		     * an example of what we're trying to achieve:
		     * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+
		     * |  |  | N| a| m| e| :|\t| i| n| i| t|\n| S|...
		     * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+
		     * | 0| 0| 0| 0| 0| 1|  | (| i| n| i| t| )|\0|...
		     * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+ */
		    if ((k = read(fd, buf+2, sizeof(buf)-4)) > 0) {
			int bc;

			if ((p = strchr(buf+2, '\n')) == NULL)
			    p = buf+k;
			p[0] = ')'; 
			p[1] = '\0';
			bc = sprintf(buf, "%06d ", pidlist->pids[i]); 
			buf[bc] = '(';
		    }
		    close(fd);
		}
	    }

	    if (k <= 0) {
		/* hmm .. must be exiting */
	    	sprintf(buf, "%06d <exiting>", pidlist->pids[i]);
	    }

	    ep->name = strdup(buf);

	    __pmHashAdd(pidlist->pids[i], (void *)ep, &proc_pid->pidhash);
	    // fprintf(stderr, "## ADDED \"%s\" to hash table\n", buf);
	}
	else
	    ep = (proc_pid_entry_t *)node->data;
	
	/* mark pid as still existing */
	ep->valid = 1;

	/* refresh the indom pointer */
	indomp->it_set[i].i_inst = ep->id;
	indomp->it_set[i].i_name = ep->name;
    }

    /* 
     * harvest exited pids from the pid hash table
     */
    for (i=0; i < proc_pid->pidhash.hsize; i++) {
	for (prev=NULL, node=proc_pid->pidhash.hash[i]; node != NULL;) {
	    next = node->next;
	    ep = (proc_pid_entry_t *)node->data;
	    // fprintf(stderr, "CHECKING key=%d node=" PRINTF_P_PFX "%p prev=" PRINTF_P_PFX "%p next=" PRINTF_P_PFX "%p ep=" PRINTF_P_PFX "%p valid=%d\n",
	    	// ep->id, node, prev, node->next, ep, ep->valid);
	    if (ep->valid == 0) {
	        // fprintf(stderr, "DELETED key=%d name=\"%s\"\n", ep->id, ep->name);
		if (ep->name != NULL)
		    free(ep->name);
		if (ep->stat_buf != NULL)
		    free(ep->stat_buf);
		if (ep->status_buf != NULL)
		    free(ep->status_buf);
		if (ep->statm_buf != NULL)
		    free(ep->statm_buf);
		if (ep->maps_buf != NULL)
		    free(ep->maps_buf);
		if (ep->schedstat_buf != NULL)
		    free(ep->schedstat_buf);
		if (ep->io_buf != NULL)
		    free(ep->io_buf);
		if (ep->wchan_buf != NULL)
		    free(ep->wchan_buf);

	    	if (prev == NULL)
		    proc_pid->pidhash.hash[i] = node->next;
		else
		    prev->next = node->next;
		free(ep);
		free(node);
	    }
	    else {
	    	prev = node;
	    }
	    if ((node = next) == NULL)
	    	break;
	}
    }

    return pidlist->count;
}

int
refresh_proc_pid(proc_pid_t *proc_pid)
{
    if (refresh_pidlist() <= 0)
    	return -oserror();

#if PCP_DEBUG
    if (pmDebug & DBG_TRACE_LIBPMDA)
	fprintf(stderr, "refresh_proc_pid: found %d pids\n", allpids.count);
#endif

    return refresh_proc_pidlist(proc_pid, &allpids);
}


/*
 * fetch a proc/<pid>/stat entry for pid
 */
proc_pid_entry_t *
fetch_proc_pid_stat(int id, proc_pid_t *proc_pid)
{
    int fd;
    int sts = 0;
    int n;
    __pmHashNode *node = __pmHashSearch(id, &proc_pid->pidhash);
    proc_pid_entry_t *ep;
    char buf[1024];

    if (node == NULL)
    	return NULL;
    ep = (proc_pid_entry_t *)node->data;

    if (ep->stat_fetched == 0) {
	sprintf(buf, "/proc/%d/stat", ep->id);
	if ((fd = open(buf, O_RDONLY)) < 0)
	    sts = -oserror();
	else
	if ((n = read(fd, buf, sizeof(buf))) < 0)
	    sts = -oserror();
	else {
	    if (n == 0)
		/* eh? */
	    	sts = -1;
	    else {
		if (ep->stat_buflen <= n) {
		    ep->stat_buflen = n;
		    ep->stat_buf = (char *)realloc(ep->stat_buf, n);
		}
		memcpy(ep->stat_buf, buf, n);
		ep->stat_buf[n-1] = '\0';
		sts = 0;
	    }
	}
	if (fd >= 0)
		close(fd);
	ep->stat_fetched = 1;
    }

    if (ep->wchan_fetched == 0) {
	sprintf(buf, "/proc/%d/wchan", ep->id);
	if ((fd = open(buf, O_RDONLY)) < 0)
	    sts = 0;	/* ignore failure here, backwards compat */
	else
	if ((n = read(fd, buf, sizeof(buf)-1)) < 0)
	    sts = -oserror();
	else {
	    if (n == 0)
		/* eh? */
	    	sts = -1;
	    else {
		n++;	/* no terminating null (from kernel) */
		if (ep->wchan_buflen <= n) {
		    ep->wchan_buflen = n;
		    ep->wchan_buf = (char *)realloc(ep->wchan_buf, n);
		}
		memcpy(ep->wchan_buf, buf, n);
		ep->wchan_buf[n-1] = '\0';
		sts = 0;
	    }
	}
	if (fd >= 0)
	    close(fd);
	ep->wchan_fetched = 1;
    }

    if (sts < 0)
    	return NULL;
    return ep;
}

/*
 * fetch a proc/<pid>/status entry for pid
 * Added by Mike Mason <mmlnx@us.ibm.com>
 */
proc_pid_entry_t *
fetch_proc_pid_status(int id, proc_pid_t *proc_pid)
{
    int sts = 0;
    __pmHashNode *node = __pmHashSearch(id, &proc_pid->pidhash);
    proc_pid_entry_t *ep;

    if (node == NULL)
	return NULL;
    ep = (proc_pid_entry_t *)node->data;

    if (ep->status_fetched == 0) {
	int	fd;
	int	n;
	char	buf[1024];
	char	*curline;

	sprintf(buf, "/proc/%d/status", ep->id);
	if ((fd = open(buf, O_RDONLY)) < 0)
	    sts = -oserror();
	else if ((n = read(fd, buf, sizeof(buf))) < 0)
	    sts = -oserror();
	else {
	    if (n == 0)
		sts = -1;
	    else {
		if (ep->status_buflen < n) {
		    ep->status_buflen = n;
		    ep->status_buf = (char *)realloc(ep->status_buf, n);
		}

		if (ep->status_buf == NULL)
		    sts = -1;
		else {
		    memcpy(ep->status_buf, buf, n);
		    ep->status_buf[n-1] = '\0';
		}
	    }
	}

	if (sts == 0) {
	    /* assign pointers to individual lines in buffer */
	    curline = ep->status_buf;

	    while (strncmp(curline, "Uid:", 4)) {
		curline = index(curline, '\n') + 1;
	    }

	    /* user & group IDs */
	    ep->status_lines.uid = strsep(&curline, "\n");
	    ep->status_lines.gid = strsep(&curline, "\n");

	    while (curline) {
		if (strncmp(curline, "VmSize:", 7) == 0) {
		    /* memory info - these lines don't exist for kernel threads */
		    ep->status_lines.vmsize = strsep(&curline, "\n");
		    ep->status_lines.vmlck = strsep(&curline, "\n");
		    if (strncmp(curline, "VmRSS:", 6) != 0)
			curline = index(curline, '\n') + 1; // Have VmPin: ?
		    if (strncmp(curline, "VmRSS:", 6) != 0)
			curline = index(curline, '\n') + 1; // Have VmHWM: ?
		    ep->status_lines.vmrss = strsep(&curline, "\n");
		    ep->status_lines.vmdata = strsep(&curline, "\n");
		    ep->status_lines.vmstk = strsep(&curline, "\n");
		    ep->status_lines.vmexe = strsep(&curline, "\n");
		    ep->status_lines.vmlib = strsep(&curline, "\n");
		    curline = index(curline, '\n') + 1; // skip VmPTE
		    ep->status_lines.vmswap = strsep(&curline, "\n");
		    ep->status_lines.threads = strsep(&curline, "\n");
		} else
		if (strncmp(curline, "SigPnd:", 7) == 0) {
		    /* signal masks */
		    ep->status_lines.sigpnd = strsep(&curline, "\n");
		    ep->status_lines.sigblk = strsep(&curline, "\n");
		    ep->status_lines.sigign = strsep(&curline, "\n");
		    ep->status_lines.sigcgt = strsep(&curline, "\n");
		    break; /* we're done */
		} else {
		    curline = index(curline, '\n') + 1;
		}
	    }

	}
	if (fd >= 0)
	    close(fd);
    }

    ep->status_fetched = 1;

    return (sts < 0) ? NULL : ep;
}

/*
 * fetch a proc/<pid>/statm entry for pid
 */
proc_pid_entry_t *
fetch_proc_pid_statm(int id, proc_pid_t *proc_pid)
{
    int fd;
    int sts = 0;
    int n;
    __pmHashNode *node = __pmHashSearch(id, &proc_pid->pidhash);
    proc_pid_entry_t *ep;
    char buf[1024];

    if (node == NULL)
    	return NULL;
    ep = (proc_pid_entry_t *)node->data;

    if (ep->statm_fetched == 0) {
	sprintf(buf, "/proc/%d/statm", ep->id);
	if ((fd = open(buf, O_RDONLY)) < 0)
	    sts = -oserror();
	else
	if ((n = read(fd, buf, sizeof(buf))) < 0)
	    sts = -oserror();
	else {
	    if (n == 0)
	    	/* eh? */
		sts = -1;
	    else {
		if (ep->statm_buflen <= n) {
		    ep->statm_buflen = n;
		    ep->statm_buf = (char *)realloc(ep->statm_buf, n);
		}
		memcpy(ep->statm_buf, buf, n);
		ep->statm_buf[n-1] = '\0';
	    }
	}

	if (fd >= 0)
	    close(fd);
	ep->statm_fetched = 1;
    }

    if (sts < 0)
    	return NULL;
    return ep;
}


/*
 * fetch a proc/<pid>/maps entry for pid
 * WARNING: This can be very large!  Only ask for it if you really need it.
 * Added by Mike Mason <mmlnx@us.ibm.com>
 */
proc_pid_entry_t *
fetch_proc_pid_maps(int id, proc_pid_t *proc_pid)
{
    int fd;
    int sts = 0;
    int n;
    int len = 0;
    __pmHashNode *node = __pmHashSearch(id, &proc_pid->pidhash);
    proc_pid_entry_t *ep;
    char buf[1024];
    char *maps_bufptr = NULL;

    if (node == NULL)
	return NULL;

    ep = (proc_pid_entry_t *)node->data;

    if (ep->maps_fetched == 0) {
	sprintf(buf, "/proc/%d/maps", ep->id);
	if ((fd = open(buf, O_RDONLY)) < 0)
	    sts = -oserror();
	else {
	    while ((n = read(fd, buf, sizeof(buf))) > 0) {
		len += n;
		if (ep->maps_buflen <= len) {
		    ep->maps_buflen = len + 1;
		    ep->maps_buf = (char *)realloc(ep->maps_buf, ep->maps_buflen);
		}
		maps_bufptr = ep->maps_buf + len - n;
		memcpy(maps_bufptr, buf, n);
	    }
	    ep->maps_fetched = 1;
	    /* If there are no maps, make maps_buf point to a zero length string. */
	    if (ep->maps_buflen == 0) {
		ep->maps_buf = (char *)malloc(1);
		ep->maps_buflen = 1;
	    }
	    ep->maps_buf[ep->maps_buflen - 1] = '\0';
	    close(fd);
	}
    }

    if (sts < 0)
	return NULL;
    return ep;
}

/*
 * fetch a proc/<pid>/schedstat entry for pid
 */
proc_pid_entry_t *
fetch_proc_pid_schedstat(int id, proc_pid_t *proc_pid)
{
    int fd;
    int sts = 0;
    int n;
    __pmHashNode *node = __pmHashSearch(id, &proc_pid->pidhash);
    proc_pid_entry_t *ep;
    char buf[1024];

    if (node == NULL)
    	return NULL;
    ep = (proc_pid_entry_t *)node->data;

    if (ep->schedstat_fetched == 0) {
	sprintf(buf, "/proc/%d/schedstat", ep->id);
	if ((fd = open(buf, O_RDONLY)) < 0)
	    sts = -oserror();
	else
	if ((n = read(fd, buf, sizeof(buf))) < 0)
	    sts = -oserror();
	else {
	    if (n == 0)
		/* eh? */
	    	sts = -1;
	    else {
		if (ep->schedstat_buflen <= n) {
		    ep->schedstat_buflen = n;
		    ep->schedstat_buf = (char *)realloc(ep->schedstat_buf, n);
		}
		memcpy(ep->schedstat_buf, buf, n);
		ep->schedstat_buf[n-1] = '\0';
	    }
	}
	if (fd >= 0) {
	    close(fd);
	    ep->schedstat_fetched = 1;
	}
    }

    if (sts < 0)
	return NULL;
    return ep;
}

/*
 * fetch a proc/<pid>/io entry for pid
 *
 * Depends on kernel built with CONFIG_TASK_IO_ACCOUNTING=y
 * which means the following must also be set:
 * CONFIG_TASKSTATS=y
 * CONFIG_TASK_DELAY_ACCT=y
 * CONFIG_TASK_XACCT=y
 */
proc_pid_entry_t *
fetch_proc_pid_io(int id, proc_pid_t *proc_pid)
{
    int sts = 0;
    __pmHashNode *node = __pmHashSearch(id, &proc_pid->pidhash);
    proc_pid_entry_t *ep;

    if (node == NULL)
	return NULL;
    ep = (proc_pid_entry_t *)node->data;

    if (ep->io_fetched == 0) {
	int	fd;
	int	n;
	char	buf[1024];
	char	*curline;

	sprintf(buf, "/proc/%d/io", ep->id);
	if ((fd = open(buf, O_RDONLY)) < 0)
	    sts = -oserror();
	else if ((n = read(fd, buf, sizeof(buf))) < 0)
	    sts = -oserror();
	else {
	    if (n == 0)
		sts = -1;
	    else {
		if (ep->io_buflen < n) {
		    ep->io_buflen = n;
		    ep->io_buf = (char *)realloc(ep->io_buf, n);
		}

		if (ep->io_buf == NULL)
		    sts = -1;
		else {
		    memcpy(ep->io_buf, buf, n);
		    ep->io_buf[n-1] = '\0';
		}
	    }
	}

	if (sts == 0) {
	    /* assign pointers to individual lines in buffer */
	    curline = ep->io_buf;
	    ep->io_lines.rchar = strsep(&curline, "\n");
	    ep->io_lines.wchar = strsep(&curline, "\n");
	    ep->io_lines.syscr = strsep(&curline, "\n");
	    ep->io_lines.syscw = strsep(&curline, "\n");
	    ep->io_lines.readb = strsep(&curline, "\n");
	    ep->io_lines.writeb = strsep(&curline, "\n");
	    ep->io_lines.cancel = strsep(&curline, "\n");
	    ep->io_fetched = 1;
	}
	if (fd >= 0)
	    close(fd);
    }

    return (sts < 0) ? NULL : ep;
}

/*
 * fetch a proc/<pid>/fd entry for pid
 */
proc_pid_entry_t *
fetch_proc_pid_fd(int id, proc_pid_t *proc_pid)
{
    __pmHashNode *node = __pmHashSearch(id, &proc_pid->pidhash);
    proc_pid_entry_t *ep;

    if (node == NULL)
	return NULL;
    ep = (proc_pid_entry_t *)node->data;

    if (ep->fd_fetched == 0) {
	char	buf[PATH_MAX];
	uint32_t de_count = 0;
	DIR	*dir;

	sprintf(buf, "/proc/%d/fd", ep->id);
	dir = opendir(buf);
	if (dir == NULL) {
	    __pmNotifyErr(LOG_ERR, "failed to open pid fd path %s\n",
			  buf);
	    return NULL;
	}
	while (readdir(dir) != NULL) {
	    de_count++;
	}
	closedir(dir);
	ep->fd_count = de_count - 2; /* subtract cwd and parent entries */
    }
    ep->fd_fetched = 1;

    return ep;
}

/*
 * Extract the ith (space separated) field from a char buffer.
 * The first field starts at zero. 
 * BEWARE: return copy is in a static buffer.
 */
char *
_pm_getfield(char *buf, int field)
{
    static int retbuflen = 0;
    static char *retbuf = NULL;
    char *p;
    int i;

    if (buf == NULL)
    	return NULL;

    for (p=buf, i=0; i < field; i++) {
	/* skip to the next space */
    	for (; *p && !isspace((int)*p); p++) {;}

	/* skip to the next word */
    	for (; *p && isspace((int)*p); p++) {;}
    }

    /* return a null terminated copy of the field */
    for (i=0; ; i++) {
	if (isspace((int)p[i]) || p[i] == '\0' || p[i] == '\n')
	    break;
    }

    if (i >= retbuflen) {
	retbuflen = i+4;
	retbuf = (char *)realloc(retbuf, retbuflen);
    }
    memcpy(retbuf, p, i);
    retbuf[i] = '\0';

    return retbuf;
}
