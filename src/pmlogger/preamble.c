/*
 * Copyright (c) 1995-2003 Silicon Graphics, Inc.  All Rights Reserved.
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "logger.h"

extern char	    	*archBase;	/* base name for log files */
extern int		ctlport;	/* pmlogger control port number */
extern int		archive_version; 

/*
 * this routine creates the "fake" pmResult to be added to the
 * start of the archive log to identify information about the
 * archive beyond what is in the archive label.
 */

/* encode the domain(x), cluster (y) and item (z) parts of the PMID */
#define PMID(x,y,z) ((x<<22)|(y<<10)|z)

/* encode the domain(x) and serial (y) parts of the pmInDom */
#define INDOM(x,y) ((x<<22)|y)

/*
 * Note: these pmDesc entries MUST correspond to the corrsponding
 *	 entries from the real PMDA ...
 *	 We fake it out here to accommodate logging from PCP 1.1
 *	 PMCD's and to avoid round-trip dependencies in setting up
 *	 the preamble
 */
static pmDesc	desc[] = {
/* pmcd.pmlogger.host */
    { PMID(2,3,3), PM_TYPE_STRING, INDOM(2,1), PM_SEM_DISCRETE, {0,0,0,0,0,0} },
/* pmcd.pmlogger.port */
    { PMID(2,3,0), PM_TYPE_U32, INDOM(2,1), PM_SEM_DISCRETE, {0,0,0,0,0,0} },
/* pmcd.pmlogger.archive */
    { PMID(2,3,2), PM_TYPE_STRING, INDOM(2,1), PM_SEM_DISCRETE, {0,0,0,0,0,0} },
};
/* names added for version 2 archives */
static char*	names[] = {
"pmcd.pmlogger.host",
"pmcd.pmlogger.port",
"pmcd.pmlogger.archive"
};

static int	n_metric = sizeof(desc) / sizeof(desc[0]);

int
do_preamble(void)
{
    int		sts;
    int		i;
    int		j;
    pid_t	mypid = getpid();
    pmResult	*res;
    __pmPDU	*pb;
    pmAtomValue	atom;
    __pmTimeval	tmp;
    char	path[MAXPATHLEN];
    char	host[MAXHOSTNAMELEN];
    extern struct timeval       last_stamp;

    /* start to build the pmResult */
    res = (pmResult *)malloc(sizeof(pmResult) + (n_metric - 1) * sizeof(pmValueSet *));
    if (res == NULL)
	return -errno;

    res->numpmid = n_metric;
    last_stamp = res->timestamp = epoch;	/* struct assignment */
    tmp.tv_sec = (__int32_t)epoch.tv_sec;
    tmp.tv_usec = (__int32_t)epoch.tv_usec;

    for (i = 0; i < n_metric; i++) {
	res->vset[i] = (pmValueSet *)__pmPoolAlloc(sizeof(pmValueSet));
	if (res->vset[i] == NULL)
	    return -errno;
	res->vset[i]->pmid = desc[i].pmid;
	res->vset[i]->numval = 1;
	/* special case for each value 0 .. n_metric-1 */
	if (desc[i].pmid == PMID(2,3,3)) {
	    /* my fully qualified hostname, cloned from the pmcd PMDA */
	    struct hostent	*hep = NULL;
	    (void)gethostname(host, MAXHOSTNAMELEN);
	    host[MAXHOSTNAMELEN-1] = '\0';
	    hep = gethostbyname(host);
	    if (hep != NULL)
		atom.cp = hep->h_name;
	    else
		atom.cp = host;
	 }
	 else if (desc[i].pmid == PMID(2,3,0)) {
	    /* my control port number, from ports.c */
	    atom.l = ctlport;
	 }
	 else if (desc[i].pmid == PMID(2,3,2)) {
	    /*
	     * the full pathname to the base of the archive, cloned
	     * from GetPort() in ports.c
	     */
	    if (*archBase == '/')
		atom.cp = archBase;
	    else {
		if (getcwd(path, MAXPATHLEN) == NULL)
		    atom.cp = archBase;
		else {
		    strcat(path, "/");
		    strcat(path, archBase);
		    atom.cp = path;
		}
	    }
	}

	sts = __pmStuffValue(&atom, 0,  &res->vset[i]->vlist[0], desc[i].type);
	if (sts < 0)
	    return sts;
	res->vset[i]->vlist[0].inst = mypid;
	res->vset[i]->valfmt = sts;
    }

    if ((sts = __pmEncodeResult(fileno(logctl.l_mfp), res, &pb)) < 0)
	return sts;

    __pmOverrideLastFd(fileno(logctl.l_mfp));	/* force use of log version */
    /* and start some writing to the archive log files ... */
    if ((sts = __pmLogPutResult(&logctl, pb)) < 0)
	return sts;

    for (i = 0; i < n_metric; i++) {
	if (archive_version == PM_LOG_VERS02) {
	    if ((sts = __pmLogPutDesc(&logctl, &desc[i], 1, &names[i])) < 0)
		return sts;
        }
        else {
	    if ((sts = __pmLogPutDesc(&logctl, &desc[i], 0, NULL)) < 0)
		return sts;
        }
	if (desc[i].indom == PM_INDOM_NULL)
	    continue;
	for (j = 0; j < i; j++) {
	    if (desc[i].indom == desc[j].indom)
		break;
	}
	if (j == i) {
	    /* need indom ... force one with my PID as the only instance */
	    int		*instid;
	    char	**instname;

	    if ((instid = (int *)malloc(sizeof(*instid))) == NULL)
		return -errno;
	    *instid = mypid;
	    sprintf(path, "%d", (int)mypid);
	    if ((instname = (char **)malloc(sizeof(char *)+strlen(path)+1)) == NULL)
		return -errno;
	    /*
	     * this _is_ correct ... instname[] is a one element array
	     * with the string value immediately following
	     */
	    instname[0] = (char *)&instname[1];
            strcpy(instname[0], path);
	    /*
	     * Note.	DO NOT free instid and instname ... they get hidden
	     *		away in addindom() below __pmLogPutInDom()
	     */
	    if ((sts = __pmLogPutInDom(&logctl, desc[i].indom, &tmp, 1, instid, instname)) < 0)
		return sts;
	}
    }

    /* fudge the temporal index */
    fflush(logctl.l_mfp);
    fseek(logctl.l_mfp, sizeof(__pmLogLabel)+2*sizeof(int), SEEK_SET);
    fflush(logctl.l_mdfp);
    fseek(logctl.l_mdfp, sizeof(__pmLogLabel)+2*sizeof(int), SEEK_SET);
    __pmLogPutIndex(&logctl, &tmp);
    fseek(logctl.l_mfp, 0L, SEEK_END);
    fseek(logctl.l_mdfp, 0L, SEEK_END);

    /*
     * and now free stuff
     * Note:	error returns cause mem leaks, but this routine
     *		is only ever called once, so tough luck
     */
    for (i = 0; i < n_metric; i++)
	__pmPoolFree(res->vset[i], sizeof(pmValueSet));
    free(res);

    return 0;
}
