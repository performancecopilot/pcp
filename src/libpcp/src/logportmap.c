/*
 * Copyright (c) 1995-2003 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#ident "$Id: logportmap.c,v 1.6 2005/02/22 05:01:12 markgw Exp $"

#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <dirent.h>
#include <strings.h>
#include <string.h>
#include <errno.h>
#include "pmapi.h"
#include "impl.h"

#if defined(HAVE_CONST_DIRENT)
#define MYDIRENT const struct dirent
#else
#define MYDIRENT struct dirent
#endif

#ifdef IS_SOLARIS
extern int scandir(const char *, struct dirent ***, int(*)(MYDIRENT *), int(*)(MYDIRENT **, MYDIRENT **)); 
extern int alphasort(MYDIRENT **, MYDIRENT **);
#endif

static __pmLogPort *logport = NULL;
					/* array of all known pmlogger ports */
static int	nlogports = 0;		/* no. of elements used in logports array */
static int	szlogport = 0;		/* size of logport array */

/* Make sure the logports array is large enough to hold newsize entries.  Free
 * any currently allocated names and zero the first newsize entries.
 */
static int
resize_logports(int newsize)
{
    int	i;
    int	need;

    if (nlogports) {
	for (i = 0; i < nlogports; i++) {
	    if (logport[i].pmcd_host != NULL)
		free(logport[i].pmcd_host);
	    if (logport[i].archive != NULL)
		free(logport[i].archive);
	    if (logport[i].name != NULL)
		free(logport[i].name);
	}
	memset(logport, 0, nlogports * sizeof(__pmLogPort));
    }
    nlogports = 0;
    if (szlogport >= newsize)
	return 0;
    free(logport);
    need = newsize * (int)sizeof(__pmLogPort);
    if ((logport = (__pmLogPort *)malloc(need)) == NULL) {
	szlogport = 0;
	return -1;
    }
    memset(logport, 0, need);
    szlogport = newsize;
    return 0;
}

#if defined(IS_DARWIN)	/* No procfs on Mac OS X */
#include <sys/sysctl.h>
#define PROCFS_ENTRY_SIZE	16  /* encompass size of entry for pid */

static int
exists_process(pid_t pid)
{
    struct kinfo_proc kp;
    size_t len = sizeof(kp);
    int mib[4];

    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_PID;
    mib[3] = pid;
    if (sysctl(mib, 4, &kp, &len, NULL, 0) == -1)
       return 0;
    return (len > 0);
}
#else

/*
 * system dependent path to /proc/.../<pid>
 */
#if defined(HAVE_PROCFS)
#define PROCFS			"/proc"
#else
!bozo!
#endif

#define PROCFS_ENTRY_SIZE	40 /* encompass size of entry for pid in PROCFS */
#define PROCFS_PATH_SIZE	(sizeof(PROCFS)+PROCFS_ENTRY_SIZE)

static int 
exists_process(pid_t pid)
{
    static char proc_buf[PROCFS_PATH_SIZE];

    snprintf(proc_buf, sizeof(proc_buf), "%s/%d", PROCFS, (int)pid);
    return (access(proc_buf, F_OK) == 0);
}
#endif

/* Used by scandir to determine which files are pmlogger port files.  The valid
 * files are numbers (pids) or PM_LOG_PRIMARY_LINK for the primary logger.
 */
static int
is_portfile(MYDIRENT *dep)
{
    char	*endp;
    int		pid;

    pid = (int)strtol(dep->d_name, &endp, 10);
    if (pid > 1) {
	return exists_process(pid);
    }
    else {
	return strcmp(dep->d_name, PM_LOG_PRIMARY_LINK) == 0;
    }
}

/* The following function is used for selecting particular port files rather
 * than all valid files.  snprintf the pid of the pmlogger process or the
 * special constant PM_LOG_PRIMARY_LINK into the match array first.
 */
static char match[PROCFS_ENTRY_SIZE];

static int
is_match(MYDIRENT *dep)
{
    return strcmp(match, dep->d_name) == 0;
}

/* Return (in result) a list of active pmlogger ports on the local machine.
 * The return value of the function is the number of elements in the array.
 * The caller must NOT free any part of the result stucture, it's storage is
 * managed here.  Subsequent calls will overwrite the data so the caller should
 * copy it if persistence is required.
 */
int
__pmLogFindLocalPorts(int pid, __pmLogPort **result)
{
    static char		*dir = NULL;
    static int		lendir;
    int			i, j, n;
    int			nf;		/* number of port files found */
    struct dirent	**files;	/* array of port file dirents */
    char		*p;
    int			len;
    static char		*namebuf = NULL;
					/* for building file names */
    static int		sznamebuf = 0;	/* current size of namebuf */
    int			(*scanfn)(MYDIRENT *dep);
    FILE		*pfile;
    char		buf[MAXPATHLEN];

    if (result == NULL)
	return -EINVAL;

    if (dir == NULL) {
	dir = PM_LOG_PORT_DIR;
	lendir = (int)strlen(dir);
    }

    /* Set up the appropriate function to select files from the control port
     * directory.  Anticipate that this will usually be an exact match for
     * the primary logger control port.
     */
    scanfn = is_match;
    switch (pid) {
	case PM_LOG_PRIMARY_PID:	/* primary logger control (single) */
	    strcpy(match, PM_LOG_PRIMARY_LINK);
	    break;

	case PM_LOG_ALL_PIDS:		/* find all ports */
	    scanfn = is_portfile;
	    break;

	default:			/* a specific pid (single) */
	    if (!exists_process(pid)) {
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_LOG) {
		    fprintf(stderr, "__pmLogFindLocalPorts() -> 0, "
				"pid(%d) doesn't exist\n", pid);
		}
#endif
		*result = NULL;
		return 0;
	    }
	    snprintf(match, sizeof(match), "%d", pid);
	    break;
    }

    if ((nf = scandir(dir, &files, scanfn, alphasort)) == -1) {
	pmprintf("__pmLogFindLocalPorts: scandir: %s\n", strerror(errno));
	pmflush();
	return -errno;
    }
    if (resize_logports(nf) < 0)
	return -errno;
    if (nf == 0) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOG) {
	    fprintf(stderr, "__pmLogFindLocalPorts() -> 0, "
			"num files = 0\n");
	}
#endif
	*result = NULL;
	return 0;
    }

    /* make a buffer for the longest complete pathname found */
    len = (int)strlen(files[0]->d_name);
    for (i = 1; i < nf; i++)
	if ((j = (int)strlen(files[i]->d_name)) > len)
	    len = j;
    /* +1 for trailing /, +1 for null termination */
    len += lendir + 2;
    if (len > sznamebuf) {
	if (namebuf != NULL)
	    free(namebuf);
	if ((namebuf = (char *)malloc(len)) == NULL) {
	    __pmNoMem("__pmLogFindLocalPorts.namebuf", len, PM_RECOV_ERR);
	    return -errno;
	}
	sznamebuf = len;
    }

    /* namebuf is the complete pathname, p points to the trailing filename
     * within namebuf.
     */
    strcpy(namebuf, dir);
    p = namebuf + lendir;
    *p++ = '/';

    /* open the file, try to read the port number and add the port to the
     * logport array if successful.
     */
    for (i = 0; i < nf; i++) {
	char		*fname = files[i]->d_name;
	int		err = 0;
	__pmLogPort	*lpp = &logport[nlogports];
	
	strcpy(p, fname);
	if ((pfile = fopen(namebuf, "r")) == NULL) {
	    pmprintf("__pmLogFindLocalPorts: pmlogger port file %s: %s\n",
		    namebuf, strerror(errno));
	    free(files[i]);
	    pmflush();
	    continue;
	}
	if (!err && fgets(buf, MAXPATHLEN, pfile) == NULL) {
	    if (feof(pfile)) {
		clearerr(pfile);
		pmprintf("__pmLogFindLocalPorts: pmlogger port file %s empty!\n",
			namebuf);
	    }
	    else
		pmprintf("__pmLogFindLocalPorts: pmlogger port file %s: %s\n",
			namebuf, strerror(errno));
	    err = 1;
	}
	else {
	    char	*endp;

	    lpp->port = (int)strtol(buf, &endp, 10);
	    if (*endp != '\n') {
		pmprintf("__pmLogFindLocalPorts: pmlogger port file %s: no port number\n",
			namebuf);
		err = 1;
	    }
	    else {
		lpp->pid = (int)strtol(fname, &endp, 10);
		if (*endp != '\0') {
		    if (strcmp(fname, PM_LOG_PRIMARY_LINK) == 0)
			lpp->pid = PM_LOG_PRIMARY_PORT;
		    else {
			pmprintf("__pmLogFindLocalPorts: unrecognised pmlogger port file %s\n",
				namebuf);
			err = 1;
		    }
		}
	    }
	}
	if (err) {
	    pmflush();
	    fclose(pfile);
	}
	else {
	    if (fgets(buf, MAXPATHLEN, pfile) == NULL) {
		pmprintf("__pmLogFindLocalPorts: pmlogger port file %s: no PMCD host name\n",
			namebuf);
		pmflush();
	    }
	    else {
		char	*q = strchr(buf, '\n');
		if (q != NULL)
		    *q = '\0';
		lpp->pmcd_host = strdup(buf);
		if (fgets(buf, MAXPATHLEN, pfile) == NULL) {
		    pmprintf("__pmLogFindLocalPorts: pmlogger port file %s: no archive base pathname\n",
			    namebuf);
		    pmflush();
		}
		else {
		    char	*q = strchr(buf, '\n');
		    if (q != NULL)
			*q = '\0';
		    lpp->archive = strdup(buf);
		}
	    }
	    fclose(pfile);
	    if ((lpp->name = strdup(fname)) != NULL)
		nlogports++;
	    else {
		if (lpp->pmcd_host != NULL) {
		    free(lpp->pmcd_host);
		    lpp->pmcd_host = NULL;
		}
		if (lpp->archive != NULL) {
		    free(lpp->archive);
		    lpp->archive = NULL;
		}
		break;
	    }
	}
	free(files[i]);
    }
    
    if (i == nf) {			/* all went well */
	n = nlogports;
	*result = logport;
    }
    else {				/* strdup error on fname, clean up */
	*result = NULL;
	for (j = i; j < nf; j++)
	    free(files[j]);
	n = -errno;
    }
    free(files);
    return n;
}

/*
 * Return 1 if hostname corresponds to the current host, 0 if not and < 0 for
 * an error.
 */
int
__pmIsLocalhost(const char *hostname)
{
    int sts = 0;

    if (strcasecmp(hostname, "localhost") == 0)
	return 1;
    else {
	char lhost[MAXHOSTNAMELEN+1];
	struct hostent * he;
	extern int errno;

	if (gethostname (lhost, MAXHOSTNAMELEN) < 0)
	   return -errno;

        if ((he = gethostbyname(lhost)) != NULL ) {
	    int i;
	    unsigned int * laddrs;
	    for ( i=0; he->h_addr_list[i] != NULL; i++ ) ;

	    laddrs = (unsigned int *)calloc(i, sizeof (unsigned int));
	    if ( laddrs != NULL ) {
		int k;
		for ( k=0; k < i; k++ ) {
		    laddrs[k] = ((struct in_addr *)he->h_addr_list[k])->s_addr;
		}

		if ((he = gethostbyname(hostname)) == NULL)
		    return -EHOSTUNREACH;

		for ( i--; i >= 0; i-- ) {
		    for (k = 0; he->h_addr_list[k] != NULL; k++) {
			struct in_addr *s=(struct in_addr *)he->h_addr_list[k];
			if (s->s_addr == laddrs[i]) {
			    free (laddrs);
			    return (1);
			}
		    }
		}

		free (laddrs);
	    }
	}
    }

    return sts;
}

/* Return (in result) a list of active pmlogger ports on the specified machine.
 * The return value of the function is the number of elements in the array.
 * The caller must NOT free any part of the result stucture, it's storage is
 * managed here.  Subsequent calls will overwrite the data so the caller should
 * copy it if persistence is required.
 */
int
__pmLogFindPort(const char *host, int pid, __pmLogPort **lpp)
{
    int			ctx, oldctx;
    int			sts, numval;
    int			i, j;
    int			findone = pid != PM_LOG_ALL_PIDS;
    int			localcon = 0;	/* > 0 for local connection */
    pmDesc		desc;
    pmResult		*res;
    char		*namelist[] = {"pmcd.pmlogger.port"};
    pmID		pmid;

    *lpp = NULL;		/* pass null back in event of error */
    localcon = __pmIsLocalhost(host);
    if (localcon > 0)
	/* do the work here instead of making PMCD do it */
	return __pmLogFindLocalPorts(pid, lpp);
    else if (localcon < 0)
	return localcon;

    /* note: there may not be a current context */
    oldctx = pmWhichContext();

    if ((ctx = pmNewContext(PM_CONTEXT_HOST, host)) < 0)
	return ctx;
    if ((sts = pmLookupName(1, namelist, &pmid)) < 0)
	goto ctxErr;

    if ((sts = pmLookupDesc(pmid, &desc)) < 0)
	goto ctxErr;
    if ((sts = pmFetch(1, &pmid, &res) < 0))
	goto ctxErr;
    if ((sts = numval = res->vset[0]->numval) < 0)
	goto resErr;
    j = 0;
    if (numval) {
	if (resize_logports(findone ? 1 : numval) < 0) {
	    sts = -errno;
	    goto resErr;
	}
	/* scan the pmResult, copying matching pid(s) to logport */
	for (i = j = 0; i < numval; i++) {
	    __pmLogPort	*p = &logport[j];
	    pmValue	*vp = &res->vset[0]->vlist[i];

	    if (vp->inst == 1)	/* old vcr instance (pseudo-init) */
		continue;
	    if (findone && vp->inst != pid)
		continue;
	    p->pid = vp->inst;
	    p->port = vp->value.lval;
	    sts = pmNameInDom(desc.indom, p->pid, &p->name);
	    if (sts < 0) {
		p->name = NULL;
		goto resErr;
	    }
	    j++;
	    if (findone)		/* found one, stop searching */
		break;
	}
	*lpp = logport;
    }
    sts = j;			/* the number actually added */

resErr:
    pmFreeResult(res);
ctxErr:
    if (oldctx >= 0)
	pmUseContext(oldctx);
    pmDestroyContext(ctx);
    return sts;
}
