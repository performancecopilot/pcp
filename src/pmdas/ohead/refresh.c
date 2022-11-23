/*
 * Copyright (c) 2012 Red Hat.
 * Copyright (c) 1995-2003 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2022 Ken McDonell.  All Rights Reserved.
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

#include "ohead.h"
#include <ctype.h>
#if defined(HAVE_PTHREAD_H)
#include <pthread.h>
#else
#error "Need pthread support"
#endif
#if defined(HAVE_SYS_PRCTL_H)
#include <sys/prctl.h>
#endif

extern pthread_mutex_t	proctab_mutex;	/* control access to proctab[] */

__uint32_t	cycles;			/* count of refresh cycles */

/*
 * used to find processes of interest and all descendents
 */
typedef struct {
    pid_t	pid;
    pid_t	ppid;
    int		want;		/* == group # if interesting, else -1 */
    int		done;		/* == 1 if children scanned for */
} pidtab_t;
static pidtab_t	*pidtab;
static int	maxnpid;	/* size of pidtab[] */
static int	npid;		/* entries in use */

static char *
get_stat_line(pid_t pid)
{
    static size_t	buflen = 0;
    static char		*buf;
    char		lbuf[128];
    char		path[MAXPATHLEN];
    char		*p;
    char		*buf_tmp;
    int			fd;
    int			nch;
    int			len;

    pmsprintf(path, sizeof(path), "%s/proc/%" FMT_PID "/stat", proc_prefix, pid);
    if ((fd = open(path, O_RDONLY)) < 0) {
	/*
	 * sort of expected, as /proc entry may have gone away ...
	 */
	if (pmDebugOptions.appl1)
	    fprintf(stderr, "get_stat_line: open(\"%s\") failed: %s\n",
		path, pmErrStr(-oserror()));
	return NULL;
    }
    p = buf;
    for (len = 0; ; ) {
	if ((nch = read(fd, lbuf, sizeof(lbuf))) <= 0)
	    break;
	len += nch;
	if (buflen < len) {
	    buf_tmp = (char *)realloc(buf, len+1);
	    if (buf_tmp == NULL) {
		/* this is close to fatal! */
		fprintf(stderr, "get_stat_line(%" FMT_PID "): buf realloc(%d): failed\n", pid, len+1);
		close(fd);
		return NULL;
	    }
	    buf = buf_tmp;
	    buflen = len;
	    p = buf + len - nch;
	}
	memcpy(p, lbuf, nch);
	p += nch;
    }
    close(fd);
    if (len > 0)
	*p = '\0';
    else {
	fprintf(stderr, "get_stat_line(%" FMT_PID "): botch: len=%d\n", pid, len);
	return NULL;
    }

    return buf;
}

static int
getinfo(void)
{
    int				i;
    int				j;
    int				gone;
    char			*buf;
    char			*p;
    char			*cmd;
    long			utime;
    long			stime;
    grouptab_t			*gp;
    proctab_t			*pp;
    struct timeval		now;
    static struct timeval	prior = { 0, 0 };
    double			delta;

    pthread_mutex_lock(&proctab_mutex);
    cycles++;			/* protected by lock */
    if (pmDebugOptions.appl1)
	fprintf(stderr, "getinfo: start cycle %d npid=%d\n", cycles, npid);

    /*
     * reset nproc (count of non-deleted entries) and nproc_active
     * (count of active processes in last interval) for all groups
     * and clear "seen" state for all proctab[] entries
     */
    for (gp = grouptab; gp < &grouptab[ngroup]; gp++) {
	gp->nproc = gp->nproc_active = 0;
	for (j = 0; j < gp->nproctab; j++)
	    gp->proctab[j].state &= (~P_SEEN);
    }

    for (i = 0; i < npid; i++) {
	if (pidtab[i].want != -1) {
	    buf = get_stat_line(pidtab[i].pid);
	    if (buf == NULL)
		continue;
	    if (pmDebugOptions.appl1 && pmDebugOptions.desperate)
		fprintf(stderr, "getinfo: stat: %s", buf);
	    gp = &grouptab[pidtab[i].want];
	    gone = -1;
	    for (j = 0; j < gp->nproctab; j++) {
		if (gone == -1 && (gp->proctab[j].state & P_GONE)) {
		    gone = j;
		    continue;
		}
		if (gp->proctab[j].pid == pidtab[i].pid)
		    break;
	    }
	    if (j == gp->nproctab) {
		/*
		 * new process, add to proctab[] ...
		 */
		if (gone == -1) {
		    /*
		     * no deleted entries ... expand table
		     */
		    proctab_t	*proctab_tmp;
		    gp->nproctab++;
		    proctab_tmp = (proctab_t *)realloc(gp->proctab, gp->nproctab * sizeof(proctab_t));
		    if (proctab_tmp == NULL) {
			fprintf(stderr, "getinfo: [%d] %s: fatal: proctab[] realloc(%ld) failed\n", (int)(gp - grouptab), gp->name, (long)gp->nproctab * sizeof(proctab_t));
			exit(1);
		    }
		    gp->proctab = proctab_tmp;
		    pp = &gp->proctab[j];
		    memset((void *)pp, 0, sizeof(proctab_t));
		}
		else {
		    /*
		     * use deleted slot
		     */
		    pp = &gp->proctab[gone];
		}
		pp->pid = pidtab[i].pid;
		p = strchr(buf, '(');	/* skip to cmd */
		cmd = ++p;
		p = strrchr(buf, ')');	/* find ') after cmd */
		*p++ = '\0';
		pp->cmd = strdup(cmd);
		pp->state = P_INIT;
	    }
	    else {
		pp = &gp->proctab[j];
		p = strrchr(buf, ')');	/* find ') after cmd */
		p++;
		pp->state = P_DATA;
	    }
	    if (pmDebugOptions.appl1) {
		if ((pp->state & P_INIT))
		    fprintf(stderr, "[%d] %s: add pid %" FMT_PID " cmd \"%s\" proctab[%d]", (int)(gp - grouptab), gp->name, pp->pid, pp->cmd, j);
		else
		    fprintf(stderr, "[%d] %s: update pid %" FMT_PID " cmd \"%s\" proctab[%d]", (int)(gp - grouptab), gp->name, pp->pid, pp->cmd, j);
	    }
	    gp->nproc++;
	    /* at space before state */
	    p = strchr(++p, ' ');	/* skip to start of ppid */
	    p = strchr(++p, ' ');	/* skip to start of pgrp */
	    p = strchr(++p, ' ');	/* skip to start of session */
	    p = strchr(++p, ' ');	/* skip to start of tty */
	    p = strchr(++p, ' ');	/* skip to start of tty_pgrp */
	    p = strchr(++p, ' ');	/* skip to start of flags */
	    p = strchr(++p, ' ');	/* skip to start of minflt */
	    p = strchr(++p, ' ');	/* skip to start of cminflt */
	    p = strchr(++p, ' ');	/* skip to start of majflt */
	    p = strchr(++p, ' ');	/* skip to start of cmajflt */
	    p = strchr(++p, ' ');	/* skip to start of utime */
	    utime = strtoul(++p, &p, 10) * 1000 / hertz;
	    stime = strtoul(++p, &p, 10) * 1000 / hertz;
	    if ((pp->state & P_DATA)) {
		/* not first time, so we have data to report */
		pp->burn.utime = utime - pp->prior.utime;
		pp->burn.stime = stime - pp->prior.stime;
		if (pp->burn.utime + pp->burn.stime > 0)
		    gp->nproc_active++;
	    }
	    else {
		pp->burn.utime = pp->burn.stime = 0;
	    }
	    pp->prior.utime = utime;
	    pp->prior.stime = stime;
	    pp->state |= P_SEEN;
	    if (pmDebugOptions.appl1)
		fprintf(stderr, " utime=%ld stime=%ld state=0x%x\n", utime, stime, pp->state);
	}
    }

    /*
     * punch the clock, and compute burn rates ...
     */
    pmtimevalNow(&now);
    delta = pmtimevalSub(&now, &prior);
    for (gp = grouptab; gp < &grouptab[ngroup]; gp++) {
	prior = now;
	for (j = 0; j < gp->nproctab; j++) {
	    if ((gp->proctab[j].state & P_DATA)) {
		gp->proctab[j].burn.utime /= (1000 * delta);
		gp->proctab[j].burn.stime /= (1000 * delta);
	    }
	}
    }

    /*
     * now mark proctab[] entries as deleted for processes that
     * have gone away
     */
    for (gp = grouptab; gp < &grouptab[ngroup]; gp++) {
	for (j = 0; j < gp->nproctab; j++) {
	    pp = &gp->proctab[j];
	    if ((pp->state & P_SEEN) == 0) {
		if (pmDebugOptions.appl1)
		    fprintf(stderr, "[%d] %s: mark proctab[%d] deleted\n", (int)(gp - grouptab), gp->name, j);
		pp->state = P_GONE;
	    }
	}
    }

    pthread_mutex_unlock(&proctab_mutex);

    return 0;
}

static void
onhup(int dummy)
{
    _exit(0);
}

/*
 * the pthread starts here to refresh the metric values periodically
 */
void
refresh(void *dummy)
{
    DIR				*dirp;
    struct dirent		*dp;
    char			dir[MAXPATHLEN];
    int				i;
    grouptab_t			*gp;
    int				found;
    int				sts;
    struct timeval		now;

    signal(SIGHUP, onhup);
#if defined (PR_TERMCHILD)
    prctl(PR_TERMCHILD);          	/* SIGHUP when the parent dies */
#elif defined (PR_SET_PDEATHSIG)
    prctl(PR_SET_PDEATHSIG, SIGTERM);
#elif defined(IS_SOLARIS) || defined(IS_DARWIN) || defined(IS_MINGW) || defined(IS_AIX) || defined(IS_FREEBSD) || defined(IS_GNU) || defined(IS_NETBSD) || defined(IS_OPENBSD)
    /* no signals here for child exit */
#else
!bozo: cant decide between PR_TERMCHILD and PR_SET_PDEATHSIG
#endif
    pmsprintf(dir, sizeof(dir), "%s/proc", proc_prefix);

    for ( ; ; ) {
	pmtimevalNow(&now);
	if (pmDebugOptions.appl1)
	    fprintf(stderr, "Start refresh @ %s", ctime(&now.tv_sec));

	if ((dirp = opendir(dir)) == NULL) {
	    fprintf(stderr, "refresh: opendir(\"%s\") failed: %s\n",
		dir, pmErrStr(-oserror()));
	    pthread_exit(NULL);
	}
	npid = 0;

	while ((dp = readdir(dirp)) != NULL) {
	    if (isdigit((int)dp->d_name[0])) {
		char	*buf;
		char	*p;
		char	*cmd;
		pid_t	pid;
		pid_t	ppid;

		pid = strtoul(dp->d_name, NULL, 10);
		buf = get_stat_line(pid);
		if (buf == NULL)
		    continue;
		if (pmDebugOptions.appl1 && pmDebugOptions.desperate)
		    fprintf(stderr, "stat: %s", buf);
		p = strchr(buf, ' ');	/* skip past pid */
		p++;			/* and space */
		p++;			/* and ( */
		cmd = p;
		p = strrchr(p, ')');	/* scan for last ) */
		*p++ = '\0';
		p = strchr(++p, ' ');		/* skip past state */
		ppid = strtoul(++p, &p, 10);

		if (npid >= maxnpid) {
		    /*
		     * extend pidtab[] in 1024-entry chunks
		     */
		    pidtab_t	*pidtab_tmp;
		    maxnpid += 1024;
		    pidtab_tmp = (pidtab_t *)realloc(pidtab, maxnpid*sizeof(pidtab_t));
		    if (pidtab_tmp == NULL) {
			fprintf(stderr, "refresh: fatal: pidtab[] realloc(%ld) failed\n", (long)maxnpid*sizeof(pidtab_t));
			exit(1);
		    }
		    pidtab = pidtab_tmp;
		}
		pidtab[npid].pid = pid;
		pidtab[npid].ppid = ppid;
		pidtab[npid].want = -1;
		pidtab[npid].done = 0;
		for (gp = grouptab; gp < &grouptab[ngroup]; gp++) {
		    sts = regexec(&gp->regex, cmd, 0, NULL, 0);
		    if (sts == 0) {
			pidtab[npid].want = (int)(gp - grouptab);
			if (pmDebugOptions.appl1 && pmDebugOptions.desperate)
			    fprintf(stderr, "refresh: pidtab[%d] => pid %" FMT_PID " cmd \"%s\" for group [%d] %s\n", npid, pid, cmd, (int)(gp - grouptab), gp->name);
			break;
		    }
		}
		npid++;

	    }
	}
	closedir(dirp);

	/*
	 * need to find all descendents for the "interesting" processes now ...
	 */
	for ( ; ; ) {
	    found = 0;
	    for (i = 0; i < npid; i++) {
		if (pidtab[i].want != -1 && pidtab[i].done == 0) {
		    int	j;
		    found++;
		    pidtab[i].done = 1;
		    for (j = 0; j < npid; j++) {
			if (pidtab[j].want != -1 && pidtab[j].ppid == pidtab[i].pid)
			    pidtab[i].want = pidtab[j].want;
		    }
		}
	    }
	    if (found == 0)
		break;
	}

	if ((sts = getinfo()) < 0) {
	    if (pmDebugOptions.appl1)
		fprintf(stderr, "refresh: getinfo failed: %s\n", pmErrStr(sts));
	}
	pmtimevalNow(&now);
	if (pmDebugOptions.appl1)
	    fprintf(stderr, "End refresh @ %s", ctime(&now.tv_sec));
	sleep(refreshtime);
    }
}
