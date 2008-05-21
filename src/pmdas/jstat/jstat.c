/*
 * jstat (Java Statistics program) PMDA 
 *
 * Copyright (c) 2007-2008 Aconex.  All Rights Reserved.
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

#include "jstat.h"
#include <ctype.h>
#include <sys/stat.h>
#if defined(HAVE_PTHREAD_H)
#include <pthread.h>
#endif
#if defined(HAVE_SYS_WAIT_H)
#include <sys/wait.h>
#endif

jstat_t		*jstat;
int		jstat_hertz = 1000;
int		jstat_count;
char		*jstat_pcp_dir_name;
struct stat	jstat_pcp_dir_stat;
pmdaInstid	*jstat_insts;
pmdaIndom	indomtab[] = { { JSTAT_INDOM, 0, 0 } };
pmInDom		*jstat_indom = &indomtab[JSTAT_INDOM].it_indom;

pthread_t	refreshpid;
pthread_mutex_t	refreshmutex;
int		refreshdelay = 5;	/* by default, poll every 5 secs */

char *java_home;	/* JAVA_HOME environment variable value */
char *jstat_path;	/* PATH to jstat binary (usually $JAVA_HOME/bin) */

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

    /* jstat.control.command */
    { NULL, { PMDA_PMID(0,6), PM_TYPE_STRING, JSTAT_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,0,0,0,0,0) } },
    /* jstat.control.refresh */
    { NULL, { PMDA_PMID(0,7), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,1,0,0,PM_TIME_SEC,0) } },

    /* jstat.gc.minor.count */
    { NULL, { PMDA_PMID(0,8), PM_TYPE_64, JSTAT_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    /* jstat.gc.minor.time */
    { NULL, { PMDA_PMID(0,9), PM_TYPE_64, JSTAT_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) } },
    /* jstat.gc.major.count */
    { NULL, { PMDA_PMID(0,10), PM_TYPE_64, JSTAT_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    /* jstat.gc.major.time */
    { NULL, { PMDA_PMID(0,11), PM_TYPE_64, JSTAT_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) } },

    /* jstat.memory.eden.capacity */
    { NULL, { PMDA_PMID(0,12), PM_TYPE_64, JSTAT_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },
    /* jstat.memory.eden.init_capacity */
    { NULL, { PMDA_PMID(0,13), PM_TYPE_64, JSTAT_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },
    /* jstat.memory.eden.max_capacity */
    { NULL, { PMDA_PMID(0,14), PM_TYPE_64, JSTAT_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },
    /* jstat.memory.eden.used */
    { NULL, { PMDA_PMID(0,15), PM_TYPE_64, JSTAT_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },
    /* jstat.memory.survivor0.capacity */
    { NULL, { PMDA_PMID(0,16), PM_TYPE_64, JSTAT_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },
    /* jstat.memory.survivor0.init_capacity */
    { NULL, { PMDA_PMID(0,17), PM_TYPE_64, JSTAT_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },
    /* jstat.memory.survivor0.max_capacity */
    { NULL, { PMDA_PMID(0,18), PM_TYPE_64, JSTAT_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },
    /* jstat.memory.survivor0.used */
    { NULL, { PMDA_PMID(0,19), PM_TYPE_64, JSTAT_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },
    /* jstat.memory.survivor1.capacity */
    { NULL, { PMDA_PMID(0,20), PM_TYPE_64, JSTAT_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },
    /* jstat.memory.survivor1.init_capacity */
    { NULL, { PMDA_PMID(0,21), PM_TYPE_64, JSTAT_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },
    /* jstat.memory.survivor1.max_capacity */
    { NULL, { PMDA_PMID(0,22), PM_TYPE_64, JSTAT_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },
    /* jstat.memory.survivor1.used */
    { NULL, { PMDA_PMID(0,23), PM_TYPE_64, JSTAT_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },
    /* jstat.memory.old.capacity */
    { NULL, { PMDA_PMID(0,24), PM_TYPE_64, JSTAT_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },
    /* jstat.memory.old.init_capacity */
    { NULL, { PMDA_PMID(0,25), PM_TYPE_64, JSTAT_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },
    /* jstat.memory.old.max_capacity */
    { NULL, { PMDA_PMID(0,26), PM_TYPE_64, JSTAT_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },
    /* jstat.memory.old.used */
    { NULL, { PMDA_PMID(0,27), PM_TYPE_64, JSTAT_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },
    /* jstat.memory.permanent.capacity */
    { NULL, { PMDA_PMID(0,28), PM_TYPE_64, JSTAT_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },
    /* jstat.memory.permanent.init_capacity */
    { NULL, { PMDA_PMID(0,29), PM_TYPE_64, JSTAT_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },
    /* jstat.memory.permanent.max_capacity */
    { NULL, { PMDA_PMID(0,30), PM_TYPE_64, JSTAT_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },
    /* jstat.memory.permanent.used */
    { NULL, { PMDA_PMID(0,31), PM_TYPE_64, JSTAT_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },
};

static int
jstat_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *avp)
{
    __pmID_int	*idp = (__pmID_int *)&(mdesc->m_desc.pmid);

    if (idp->item == 7) {	/* jstat.control.refresh */
	avp->l = refreshdelay;
	return 1;
    }

    if (idp->item != 6 && jstat[inst].fetched == 0)
	return PM_ERR_AGAIN;

    switch (idp->item) {
    case 0:	/* jstat.synchronizer.contended_lock_attempts */
	if (jstat[inst].contended_lock_attempts == -1) return 0;
	avp->ll = jstat[inst].contended_lock_attempts;
	break;
    case 1:	/* jstat.synchronizer.deflations */
	if (jstat[inst].deflations == -1) return 0;
	avp->ll = jstat[inst].deflations;
	break;
    case 2:	/* jstat.synchronizer.futile_wakeups */
	if (jstat[inst].futile_wakeups == -1) return 0;
	avp->ll = jstat[inst].futile_wakeups;
	break;
    case 3:	/* jstat.synchronizer.inflations */
	if (jstat[inst].inflations == -1) return 0;
	avp->ll = jstat[inst].inflations;
	break;
    case 4:	/* jstat.synchronizer.notifications	*/
	if (jstat[inst].notifications == -1) return 0;
	avp->ll = jstat[inst].notifications;
	break;
    case 5:	/* jstat.synchronizer.parks	*/
	if (jstat[inst].parks == -1) return 0;
	avp->ll = jstat[inst].parks;
	break;
    case 6:	/* jstat.control.command */
	avp->cp = jstat[inst].command;
	break;

    case 8:	/* jstat.gc.minor.count */
	if (jstat[inst].minor_gc_count == -1) return 0;
	avp->ll = jstat[inst].minor_gc_count;
	break;
    case 9:	/* jstat.gc.minor.time */
	if (jstat[inst].minor_gc_time == -1) return 0;
	avp->ll = jstat[inst].minor_gc_time / jstat_hertz;
	break;
    case 10:	/* jstat.gc.major.count */
	if (jstat[inst].major_gc_count == -1) return 0;
	avp->ll = jstat[inst].major_gc_count;
	break;
    case 11:	/* jstat.gc.major.time */
	if (jstat[inst].major_gc_time == -1) return 0;
	avp->ll = jstat[inst].major_gc_time / jstat_hertz;
	break;

    case 12:	/* jstat.memory.eden.capacity */
	if (jstat[inst].eden_capacity == -1) return 0;
	avp->ll = jstat[inst].eden_capacity;
	break;
    case 13:	/* jstat.memory.eden.init_capacity */
	if (jstat[inst].eden_init_capacity == -1) return 0;
	avp->ll = jstat[inst].eden_init_capacity;
	break;
    case 14:	/* jstat.memory.eden.max_capacity */
	if (jstat[inst].eden_max_capacity == -1) return 0;
	avp->ll = jstat[inst].eden_max_capacity;
	break;
    case 15:	/* jstat.memory.eden.used */
	if (jstat[inst].eden_used == -1) return 0;
	avp->ll = jstat[inst].eden_used;
	break;
    case 16:	/* jstat.memory.survivor0.capacity */
	if (jstat[inst].survivor0_capacity == -1) return 0;
	avp->ll = jstat[inst].survivor0_capacity;
	break;
    case 17:	/* jstat.memory.survivor0.init_capacity */
	if (jstat[inst].survivor0_init_capacity == -1) return 0;
	avp->ll = jstat[inst].survivor0_init_capacity;
	break;
    case 18:	/* jstat.memory.survivor0.max_capacity */
	if (jstat[inst].survivor0_max_capacity == -1) return 0;
	avp->ll = jstat[inst].survivor0_max_capacity;
	break;
    case 19:	/* jstat.memory.survivor0.used */
	if (jstat[inst].survivor0_used == -1) return 0;
	avp->ll = jstat[inst].survivor0_used;
	break;
    case 20:	/* jstat.memory.survivor1.capacity */
	if (jstat[inst].survivor1_capacity == -1) return 0;
	avp->ll = jstat[inst].survivor1_capacity;
	break;
    case 21:	/* jstat.memory.survivor1.init_capacity */
	if (jstat[inst].survivor1_init_capacity == -1) return 0;
	avp->ll = jstat[inst].survivor1_init_capacity;
	break;
    case 22:	/* jstat.memory.survivor1.max_capacity */
	if (jstat[inst].survivor1_max_capacity == -1) return 0;
	avp->ll = jstat[inst].survivor1_max_capacity;
	break;
    case 23:	/* jstat.memory.survivor1.used */
	if (jstat[inst].survivor1_used == -1) return 0;
	avp->ll = jstat[inst].survivor1_used;
	break;
    case 24:	/* jstat.memory.old.capacity */
	if (jstat[inst].old_capacity == -1) return 0;
	avp->ll = jstat[inst].old_capacity;
	break;
    case 25:	/* jstat.memory.old.init_capacity */
	if (jstat[inst].old_init_capacity == -1) return 0;
	avp->ll = jstat[inst].old_init_capacity;
	break;
    case 26:	/* jstat.memory.old.max_capacity */
	if (jstat[inst].old_max_capacity == -1) return 0;
	avp->ll = jstat[inst].old_max_capacity;
	break;
    case 27:	/* jstat.memory.old.used */
	if (jstat[inst].old_used == -1) return 0;
	avp->ll = jstat[inst].old_used;
	break;
    case 28:	/* jstat.memory.permanent.capacity */
	if (jstat[inst].permanent_capacity == -1) return 0;
	avp->ll = jstat[inst].permanent_capacity;
	break;
    case 29:	/* jstat.memory.permanent.init_capacity */
	if (jstat[inst].permanent_init_capacity == -1) return 0;
	avp->ll = jstat[inst].permanent_init_capacity;
	break;
    case 30:	/* jstat.memory.permanent.max_capacity */
	if (jstat[inst].permanent_max_capacity == -1) return 0;
	avp->ll = jstat[inst].permanent_max_capacity;
	break;
    case 31:	/* jstat.memory.permanent.used */
	if (jstat[inst].permanent_used == -1) return 0;
	avp->ll = jstat[inst].permanent_used;
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

    free(jp->command);
    free(jp->name);
    memset(jp, 0, sizeof(jstat_t));

    pmdaCacheStore(*jstat_indom, PMDA_CACHE_HIDE,
			jstat_insts[inst].i_name, &jstat[inst]);
    if (pmDebug & DBG_TRACE_INDOM)
	__pmNotifyErr(LOG_DEBUG, "Hid instance domain %s (inst=%d)",
			jstat_insts[inst].i_name, inst);
}

char *
jstat_command(int pid)
{
    size_t size;
    char *command;

    size = (jstat_path == NULL) ? 0 : (strlen(jstat_path) + 1);
    size += strlen(JSTAT_COMMAND) + 32;
    if ((command = malloc(size)) == NULL)
	__pmNoMem("jstat.command", size, PM_FATAL_ERR);
    if (jstat_path == NULL)
	sprintf(command, "%s%u", JSTAT_COMMAND, pid);
    else
	sprintf(command, "%s/%s%u", jstat_path, JSTAT_COMMAND, pid);
    return command;
}

void
jps_parse(FILE *fp)
{
    int inst, sts, pid;
    char *endnum, *p;
    char line[1024];
    jstat_t *jp;
    size_t sz;

    while (fgets(line, sizeof(line), fp) != NULL) {
	pid = (int)strtol(line, &endnum, 10);
	if (pid < 1 || *endnum != ' ') {
	    __pmNotifyErr(LOG_ERR, "Unexpected jps output - %s", line);
	    continue;
	}
	for (p = line; *p != '\0'; p++)
	    if (!isblank(*p) && isspace(*p))
		*p = '\0';
	if (strncasecmp(endnum, " jps", 4) == 0)
	    continue;
	if (strncasecmp(endnum, " -- process", 11) == 0) /* unavailable */
	    continue;

	sts = pmdaCacheLookupName(*jstat_indom, line, NULL, (void**)&jp);
	if (sts == PMDA_CACHE_ACTIVE)	/* repeated output line from jps? */
	    continue;
	if (sts == PMDA_CACHE_INACTIVE) {   /* re-activate for next fetch */
	    jp->fetched = 0;
	    pmdaCacheStore(*jstat_indom, PMDA_CACHE_ADD, line, jp);
	    continue;
	}

	inst = jstat_count;
	jstat_count++;
	sz = sizeof(pmdaInstid) * jstat_count;
	if ((jstat_insts = realloc(jstat_insts, sz)) == NULL)
	    __pmNoMem("jstat.insts", sz, PM_FATAL_ERR);
	sz = sizeof(jstat_t) * jstat_count;
	if ((jstat = realloc(jstat, sz)) == NULL)
	    __pmNoMem("jstat.indom", sz, PM_FATAL_ERR);
	jp = &jstat[inst];
	memset(jp, 0, sizeof(jstat_t));
	if ((jp->name = strdup(line)) == NULL)
	    __pmNoMem("jstat.inst", strlen(line), PM_FATAL_ERR);
	jp->pid = pid;
	jp->fetched = 0;
	jp->command = jstat_command(pid);
	__pmNotifyErr(LOG_INFO, "Adding new instance %s (PID=%d)", line, pid);
	jstat_insts[inst].i_name = jp->name;
	jstat_insts[inst].i_inst = inst;

	pmdaCacheStore(*jstat_indom, PMDA_CACHE_ADD, line, jp);
    }

    indomtab[JSTAT_INDOM].it_numinst = jstat_count;
    indomtab[JSTAT_INDOM].it_set = jstat_insts;
}

void
jstat_indom_check(void)
{
    FILE *pp;
    static int initialised;
    static char jps[BUFFER_MAXLEN];

    if (!initialised) {
	if (jstat_path == NULL)
	    strcpy(jps, "jps");
	else
	    sprintf(jps, "%s/jps", jstat_path);
	initialised = 1;
    }

    /* deactivate all active instances */
    pmdaCacheOp(*jstat_indom, PMDA_CACHE_INACTIVE);

    if ((pp = popen(jps, "r")) == NULL)
	__pmNotifyErr(LOG_ERR, "popen failed (%s): %s", jps, strerror(errno));
    else {
	pthread_mutex_lock(&refreshmutex);
	jps_parse(pp);
	pthread_mutex_unlock(&refreshmutex);
	pclose(pp);
    }
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
jstat_parse_gc_collector(jstat_t *jp, const char *name, const char *value)
{
    if (strcmp(name, "0.invocations") == 0)
	jp->minor_gc_count = atoll(value);
    else if (strcmp(name, "0.time") == 0)
	jp->minor_gc_time = atoll(value);
    else if (strcmp(name, "1.invocations") == 0)
	jp->major_gc_count = atoll(value);
    else if (strcmp(name, "1.time") == 0)
	jp->major_gc_time = atoll(value);
}

void
jstat_parse_gc_generation(jstat_t *jp, const char *name, const char *value)
{
    if (strcmp(name, "0.space.0.capacity") == 0)
	jp->eden_capacity = atoll(value);
    else if (strcmp(name, "0.space.0.initCapacity") == 0)
	jp->eden_init_capacity = atoll(value);
    else if (strcmp(name, "0.space.0.maxCapacity") == 0)
	jp->eden_max_capacity = atoll(value);
    else if (strcmp(name, "0.space.0.used") == 0)
	jp->eden_used = atoll(value);
    else if (strcmp(name, "0.space.1.capacity") == 0)
	jp->survivor0_capacity = atoll(value);
    else if (strcmp(name, "0.space.1.initCapacity") == 0)
	jp->survivor0_init_capacity = atoll(value);
    else if (strcmp(name, "0.space.1.maxCapacity") == 0)
	jp->survivor0_max_capacity = atoll(value);
    else if (strcmp(name, "0.space.1.used") == 0)
	jp->survivor0_used = atoll(value);
    else if (strcmp(name, "0.space.2.capacity") == 0)
	jp->survivor1_capacity = atoll(value);
    else if (strcmp(name, "0.space.2.initCapacity") == 0)
	jp->survivor1_init_capacity = atoll(value);
    else if (strcmp(name, "0.space.2.maxCapacity") == 0)
	jp->survivor1_max_capacity = atoll(value);
    else if (strcmp(name, "0.space.2.used") == 0)
	jp->survivor1_used = atoll(value);
    else if (strcmp(name, "1.space.0.capacity") == 0)
	jp->old_capacity = atoll(value);
    else if (strcmp(name, "1.space.0.initCapacity") == 0)
	jp->old_init_capacity = atoll(value);
    else if (strcmp(name, "1.space.0.maxCapacity") == 0)
	jp->old_max_capacity = atoll(value);
    else if (strcmp(name, "1.space.0.used") == 0)
	jp->old_used = atoll(value);
    else if (strcmp(name, "2.space.0.capacity") == 0)
	jp->permanent_capacity = atoll(value);
    else if (strcmp(name, "2.space.0.initCapacity") == 0)
	jp->permanent_init_capacity = atoll(value);
    else if (strcmp(name, "2.space.0.maxCapacity") == 0)
	jp->permanent_max_capacity = atoll(value);
    else if (strcmp(name, "2.space.0.used") == 0)
	jp->permanent_used = atoll(value);
}

void
jstat_parse_sync(jstat_t *jp, const char *name, const char *value)
{
    if (strcmp(name, "ContendedLockAttempts") == 0)
	jp->contended_lock_attempts = atoll(value);
    else if (strcmp(name, "Deflations") == 0)
	jp->deflations = atoll(value);
    else if (strcmp(name, "FutileWakeups") == 0)
	jp->futile_wakeups = atoll(value);
    else if (strcmp(name, "Inflations") == 0)
	jp->inflations = atoll(value);
    else if (strcmp(name, "Notifications") == 0)
	jp->notifications = atoll(value);
    else if (strcmp(name, "Parks") == 0)
	jp->parks = atoll(value);
}

int
jstat_parse(jstat_t *jp, FILE *fp)
{
    int count = 0;
    char line[1024];
    char *name, *value;

    while (fgets(line, sizeof(line), fp) != NULL) {
	count++;
	name = value = line;
	if (strsep(&value, "=") == NULL)
	    continue;
	else if (strncmp(name, "sun.gc.collector.", 17) == 0)
	    jstat_parse_gc_collector(jp, name + 17, value);
	else if (strncmp(name, "sun.gc.generation.", 18) == 0)
	    jstat_parse_gc_generation(jp, name + 18, value);
	else if (strncmp(name, "sun.rt._sync_", 13) == 0)
	    jstat_parse_sync(jp, name + 13, value);
    }
    return count;
}

void
jstat_execute(int inst)
{
    int sts;
    FILE *pp;
    jstat_t *jp;
    char command[BUFFER_MAXLEN];

    pthread_mutex_lock(&refreshmutex);
    if (inst < jstat_count) {
	jp = &jstat[inst];
	strncpy(command, jp->command, sizeof(command));
	sts = 0;
    } else {
	sts = E2BIG;	/* process removed from instance domain */
    }
    pthread_mutex_unlock(&refreshmutex);
    if (sts)
	return;

    if ((pp = popen(command, "r")) == NULL) {
	sts = errno;
	__pmNotifyErr(LOG_ERR, "popen failed (%s): %s", command, strerror(sts));
    }

    pthread_mutex_lock(&refreshmutex);
    jp = &jstat[inst];
    jp->fetched = 1;
    if (!sts && jstat_parse(jp, pp) == 0)
	__pmNotifyErr(LOG_DEBUG, "jstat produced no output (%s)", command);
    pthread_mutex_unlock(&refreshmutex);

    pclose(pp);
}

void
jstat_reaper(int unused)
{
    pid_t pid;

    do {
	pid = waitpid(-1, &unused, WNOHANG);
    } while (pid > 0);
}

void
jstat_refresh(void *unused)
{
    int inst;

    for (;;) {
	for (inst = 0;  inst < jstat_count; inst++)
	    jstat_execute(inst);
	jstat_reaper(0);
	sleep(refreshdelay);
    }
}

void
jstat_init(pmdaInterface *dp)
{
    if (dp->status != 0)
	return;

    dp->version.two.fetch = jstat_fetch;
    dp->version.two.instance = jstat_instance;
    pmdaSetFetchCallBack(dp, jstat_fetchCallBack);
    pmdaInit(dp, indomtab, sizeof(indomtab)/sizeof(indomtab[0]),
		metrictab, sizeof(metrictab)/sizeof(metrictab[0]));
    jstat_indom_check();

    /* start the thread for async fetches */
    signal(SIGCHLD, jstat_reaper);
    dp->status = pthread_create(&refreshpid, NULL, (void(*))jstat_refresh,NULL);

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
	    "  -J path      JAVA_HOME environment variable setting\n"
	    "  -P path      path to jstat binary (this is not a $PATH)\n"
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

    while ((c = pmdaGetOpt(argc, argv, "D:d:h:i:l:pu:" "H:J:P:r:?",
			   &dispatch, &err)) != EOF) {
	switch (c) {
	    case 'J':
		java_home = optarg;
		break;
	    case 'P':
		jstat_path = optarg;
		break;
	    case 'H':
		jstat_hertz = (int)strtol(optarg, &endnum, 10);
		if (*endnum != '\0') {
		    fprintf(stderr, "%s: "
			"-H requires numeric (number of seconds) argument\n",
			pmProgname);
		    err++;
		}
		break;
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

    if (java_home)
	setenv("JAVA_HOME", java_home, 1);

    pmdaOpenLog(&dispatch);
    jstat_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);
    jstat_done();
    exit(0);
}
