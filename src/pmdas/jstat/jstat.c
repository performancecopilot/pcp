/*
 * jstat (Java Statistics program) PMDA 
 *
 * Copyright (c) 2007 Aconex.  All Rights Reserved.
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

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include <syslog.h>
#include <pthread.h>
#include "./jstat.h"

jstat_t		*jstat;
int		jstat_count;
char		*jstat_pcp_dir_name;
struct stat	jstat_pcp_dir_stat;
pmdaInstid	*jstat_insts;
pmdaIndom	indomtab[] = { { JSTAT_INDOM, 0, 0 }, { ACTIVE_INDOM, 0, 0 } };
pmInDom		*jstat_indom = &indomtab[JSTAT_INDOM].it_indom;

pthread_t	refreshpid;
pthread_mutex_t	refreshmutex;
int		refreshdelay = 5;	/* default poll every five seconds */

pmdaMetric metrictab[] = {
    /* jstat.synchronizer.contended_lock_attempts */
    { NULL, { PMDA_PMID(0,0), PM_TYPE_64, JSTAT_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    /* jstat.synchronizer.deflations */
    { NULL, { PMDA_PMID(0,1), PM_TYPE_64, JSTAT_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    /* jstat.synchronizer.futile_wakeups */
    { NULL, { PMDA_PMID(0,2), PM_TYPE_64, JSTAT_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    /* jstat.synchronizer.inflations */
    { NULL, { PMDA_PMID(0,3), PM_TYPE_64, JSTAT_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    /* jstat.synchronizer.notifications	*/
    { NULL, { PMDA_PMID(0,4), PM_TYPE_64, JSTAT_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    /* jstat.synchronizer.parks	*/
    { NULL, { PMDA_PMID(0,5), PM_TYPE_64, JSTAT_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
};

static int
jstat_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *avp)
{
    __pmID_int	*idp = (__pmID_int *)&(mdesc->m_desc.pmid);

    if (jstat[inst].fetched == 0)
	return PM_ERR_AGAIN;

    switch (idp->item) {
	case 0:
		if (jstat[inst].contended_lock_attempts == -1) return 0;
		avp->ll = jstat[inst].contended_lock_attempts;
		break;
	case 1:
		if (jstat[inst].deflations == -1) return 0;
		avp->ll = jstat[inst].deflations;
		break;
	case 2:
		if (jstat[inst].futile_wakeups == -1) return 0;
		avp->ll = jstat[inst].futile_wakeups;
		break;
	case 3:
		if (jstat[inst].inflations == -1) return 0;
		avp->ll = jstat[inst].inflations;
		break;
	case 4:
		if (jstat[inst].notifications == -1) return 0;
		avp->ll = jstat[inst].notifications;
		break;
	case 5:
		if (jstat[inst].parks == -1) return 0;
		avp->ll = jstat[inst].parks;
		break;
	default:
		return PM_ERR_PMID;
    }

    return 1;
}

void
jstat_indom_clear(int inst)
{
    jstat_t *jp = &jstat[inst];

    if (jp->fin)
	fclose(jp->fin);
    free(jp->command);
    free(jp->name);
    memset(jp, 0, sizeof(jstat_t));

    pmdaCacheStore(*jstat_indom, PMDA_CACHE_HIDE,
			jstat_insts[inst].i_name, &jstat[inst]);
    if (pmDebug & DBG_TRACE_INDOM)
	__pmNotifyErr(LOG_DEBUG, "Hid instance domain %s (inst=%d)",
			jstat_insts[inst].i_name, inst);
}

int
jstat_lookup_pid(char *fname, jstat_t *jp, int instid)
{
    FILE	*fp;
    int		pid;

    if ((fp = fopen(fname, "r")) == NULL) {
	__pmNotifyErr(LOG_ERR, "Unreadable pid file %d\n", fname);
	jp->error = 1;
    } else {
	if (fscanf(fp, "%u\n", &pid) != 1) {
	    __pmNotifyErr(LOG_ERR, "Unparsable pid file %d\n", fname);
	    jp->error = 1;
	}
	fclose(fp);
    }
    return pid;
}

void
jstat_indom_setup(void)
{
    struct dirent	*dirent;
    char		*suffix;
    DIR			*pcpdir;
    int			i, sz;

    if ((pcpdir = opendir(jstat_pcp_dir_name)) == NULL) {
	__pmNotifyErr(LOG_ERR, "cannot open %s: %s\n", jstat_pcp_dir_name,
			pmErrStr(-errno));
	exit(1);
    }

    /*
     * Build the base instance domain and jstat data structures from the
     * jstat pcp directory.  Note that when the PMDA starts at bootup it
     * is unlikely any java processes will be running.
     */

    while ((dirent = readdir(pcpdir))) {
	if ((suffix = strstr(dirent->d_name, ".pcp.pid")) == NULL)
	    continue;
	*suffix = '\0';	 /* terminate at start of matching suffix */
	for (i = 0; i < jstat_count; i++)	/* is name in table already? */
	    if (strcmp(jstat_insts[i].i_name, dirent->d_name) == 0)
		break;
	if (i != jstat_count)
	    continue;
	jstat_count++;
	sz = sizeof(pmdaInstid) * jstat_count;
	if ((jstat_insts = realloc(jstat_insts, sz)) == NULL)
	    __pmNoMem("jstat.insts", sz, PM_FATAL_ERR);
	sz = sizeof(jstat_t) * jstat_count;
	if ((jstat = realloc(jstat, sz)) == NULL)
	    __pmNoMem("jstat.indom", sz, PM_FATAL_ERR);
	memset(&jstat[i], 0, sizeof(jstat_t));
	if ((jstat[i].name = strdup(dirent->d_name)) == NULL)
	    __pmNoMem("jstat.inst", strlen(dirent->d_name), PM_FATAL_ERR);
	__pmNotifyErr(LOG_INFO, "Adding new instance %s", dirent->d_name);
	jstat_insts[i].i_name = jstat[i].name;
	jstat_insts[i].i_inst = i;

	pmdaCacheStore(*jstat_indom, PMDA_CACHE_ADD,
			jstat_insts[i].i_name, &jstat[i]);
    }
    closedir(pcpdir);

    indomtab[JSTAT_INDOM].it_numinst = jstat_count;
    indomtab[JSTAT_INDOM].it_set = jstat_insts;
    indomtab[ACTIVE_INDOM].it_numinst = jstat_count;
    indomtab[ACTIVE_INDOM].it_set = jstat_insts;
}

void
jstat_indom_check(void)
{
    int		i, sts;
    char	pidfile[256], statfile[64];
    struct stat	sbuf;

    pthread_mutex_lock(&refreshmutex);

    if (stat(jstat_pcp_dir_name, &sbuf) < 0) {
	__pmNotifyErr(LOG_ERR, "cannot stat %s: %s\n",
				jstat_pcp_dir_name, pmErrStr(-errno));
	exit(1);
    } else if (sbuf.st_mtime != jstat_pcp_dir_stat.st_mtime) {
	jstat_pcp_dir_stat = sbuf;	/* struct copy */
	jstat_indom_setup();
    }

    for (i = 0; i < jstat_count; i++) {
	/* check if pidfile has changed */
	snprintf(pidfile, sizeof(pidfile), "%s/%s.pcp.pid",
		 jstat_pcp_dir_name, jstat[i].name);
	if (stat(pidfile, &sbuf) < 0) {
	    if (jstat[i].error == 0) {
		__pmNotifyErr(LOG_ERR, "stat failed on %s (%s): %s\n",
			pidfile, jstat[i].name, pmErrStr(-errno));
		jstat[i].error = 1;
	    }
	} else {
	    jstat[i].error = 0;
	    if (sbuf.st_mtime != jstat[i].pidstat.st_mtime) {
		jstat[i].pid = jstat_lookup_pid(pidfile, &jstat[i], i);
		if (jstat[i].error) {
		    jstat_indom_clear(i);
		    continue;
		}
		jstat[i].pidstat = sbuf;
		sprintf(jstat[i].command, JSTAT_COMMAND, jstat[i].pid);
		__pmNotifyErr(LOG_INFO, "Initialised instance %s (PID=%d)",
				jstat[i].name, jstat[i].pid);
	    }
	    snprintf(statfile, sizeof(statfile), "/proc/%d/stat", jstat[i].pid);
	    if (stat(statfile, &sbuf) < 0) {
		if (jstat[i].error == 0) {
		    __pmNotifyErr(LOG_ERR, "Instance %s (PID=%d) not running",
					jstat[i].name, jstat[i].pid);
		    jstat[i].error = 1;
		}
		jstat_indom_clear(i);
		continue;
	    }
	    if ((sts = pmdaCacheStore(*jstat_indom, PMDA_CACHE_ADD,
				jstat_insts[i].i_name, &jstat[i])) < 0)
		__pmNotifyErr(LOG_ERR, "pmdaCacheStore failed: %s",
				pmErrStr(sts));
	    if (pmDebug & DBG_TRACE_INDOM)
		__pmNotifyErr(LOG_DEBUG, "Added instance domain %s (inst=%d)",
				jstat_insts[i].i_name, i);
	}
    }

    pthread_mutex_unlock(&refreshmutex);
}

int
jstat_instance(pmInDom indom, int i, char *s, __pmInResult **ir, pmdaExt *pmda)
{
    jstat_indom_check();
    return pmdaInstance(indom, i, s, ir, pmda);
}

int
jstat_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    int sts;

    if (pthread_kill(refreshpid, 0) != 0) {
	__pmNotifyErr(LOG_ERR, "refresh thread has exited.");
	exit(1);
    }
    jstat_indom_check();
    pthread_mutex_lock(&refreshmutex);
    sts = pmdaFetch(numpmid, pmidlist, resp, pmda);
    pthread_mutex_unlock(&refreshmutex);
    return sts;
}

void
jstat_parse(int inst)
{
    char line[1024];
    char *name, *value;
    jstat_t *jp = &jstat[inst];
    FILE *fp;

    fp = popen(jp->command, "r");
    if (fp == NULL) {
	__pmNotifyErr(LOG_ERR, "pipe failed (%s): %s", jp->command,
			pmErrStr(-errno));
	pthread_mutex_lock(&refreshmutex);
	jstat_indom_clear(inst);
	goto unlock;
    }

    pthread_mutex_lock(&refreshmutex);
    if (inst >= jstat_count)
	goto unlock;

    while (fp && fgets(line, sizeof(line), fp) != NULL) {
	name = value = line;
	if (strsep(&value, "=") == NULL)
	    continue;
	if (strcmp(name, "sun.rt._sync_ContendedLockAttempts") == 0) {
	    jp->contended_lock_attempts = atoll(value);
	    continue;
	}
	if (strcmp(name, "sun.rt._sync_Deflations") == 0) {
	    jp->deflations = atoll(value);
	    continue;
	}
	if (strcmp(name, "sun.rt._sync_FutileWakeups") == 0) {
	    jp->futile_wakeups = atoll(value);
	    continue;
	}
	if (strcmp(name, "sun.rt._sync_Inflations") == 0) {
	    jp->inflations = atoll(value);
	    continue;
	}
	if (strcmp(name, "sun.rt._sync_Notifications") == 0) {
	    jp->notifications = atoll(value);
	    continue;
	}
	if (strcmp(name, "sun.rt._sync_Parks") == 0) {
	    jp->parks = atoll(value);
	    continue;
	}
    }
    jp->fetched = 1;

unlock:
    pthread_mutex_unlock(&refreshmutex);

    if (fp)
	pclose(fp);
}

void
refresh(void *unused)
{
    int inst;

    for (;;) {
	for (inst = 0;  inst < jstat_count; inst++) {
	    fprintf(stderr, "Refresh daemon awake: processing JVM %d\n", inst);
	    jstat_parse(inst);
	}
	sleep(refreshdelay);
    }
}

void
jstat_init(pmdaInterface *dp)
{
    int		i;

    if (dp->status != 0)
	return;

    dp->version.two.fetch = jstat_fetch;
    dp->version.two.instance = jstat_instance;
    pmdaSetFetchCallBack(dp, jstat_fetchCallBack);
    pmdaInit(dp, indomtab, sizeof(indomtab)/sizeof(indomtab[0]),
		metrictab, sizeof(metrictab)/sizeof(metrictab[0]));
    jstat_indom_check();

    /* start the thread for async fetches */
    if ((i = pthread_create(&refreshpid, NULL, (void (*))refresh, NULL)) != 0)
	refreshpid = i;
    if (refreshpid < 0)
	dp->status = refreshpid;
    else
	dp->status = 0;

    pthread_mutex_init(&refreshmutex, NULL);
}

void
jstat_done(void)
{
    if (refreshpid > 0) {
	pthread_kill(refreshpid, SIGTERM);
	refreshpid = 0;
    }
}

void
usage(void)
{
    fprintf(stderr,
	    "Usage: %s [options] [directory]\n\n"
	    "Options:\n"
	    "  -d domain    use domain (numeric) for metrics domain of PMDA\n"
	    "  -l logfile   redirect diagnostics and trace output to logfile\n"
	    "  -r refresh   update metrics every refresh seconds\n",
	    pmProgname);		
    exit(1);
}

int
main(int argc, char **argv)
{
    int			err = 0;
    char		*endnum;
    pmdaInterface	dispatch;
    int			c;
    char		helptext[MAXPATHLEN];

    pmProgname = basename(argv[0]);
    snprintf(helptext, sizeof(helptext), "%s/jstat/help",
		pmGetConfig("PCP_PMDAS_DIR"));
    pmdaDaemon(&dispatch, PMDA_INTERFACE_3, pmProgname, JSTAT,
		"jstat.log", helptext);

    while ((c = pmdaGetOpt(argc, argv, "D:d:h:i:l:pu:" "r:?", 
			   &dispatch, &err)) != EOF) {
	switch (c) {
	    case 'r':
		refreshdelay = (int)strtol(optarg, &endnum, 10);
		if (*endnum != '\0') {
		    fprintf(stderr, "%s: "
			"-r requires numeric (number of seconds) argument\n",
			pmProgname);
		    err++;
		}
		break;
	    case '?':
		err++;
	}
    }

    if (err || (argc - optind) > 1)
	usage();
    else if ((argc - optind) == 1)
	jstat_pcp_dir_name = argv[optind];
    else
	jstat_pcp_dir_name = "/var/tmp/jstat";

    pmdaOpenLog(&dispatch);
    jstat_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);
    jstat_done();
    exit(0);
    /*NOTREACHED*/
}
