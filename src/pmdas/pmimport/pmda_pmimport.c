/*
 * Copyright (c) 2026 Red Hat.
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
 * pmdapmimport - reports status of active PCP import tools via status
 * files in PCP_IMPORTRUN_DIR.
 *
 * Three instanced metrics, with the import tool name as instance:
 *   pmimport.archive  - current archive base path
 *   pmimport.version  - tool version string
 *   pmimport.args     - tool-specific arguments (activities, modules, etc.)
 *
 * Three singleton metrics (no indom) mirroring pmcd equivalents:
 *   pmimport.hostname - local hostname
 *   pmimport.timezone - local timezone ($TZ)
 *   pmimport.zoneinfo - Olson timezone tzfile identifier
 */

#include "pmapi.h"
#include "libpcp.h"
#include "pmda.h"
#include "domain.h"
#include <sys/stat.h>
#include <dirent.h>

#if defined(HAVE_STAT_TIMESTRUC)
typedef timestruc_t	pmimport_mtime_t;
#elif defined(HAVE_STAT_TIMESPEC) || defined(HAVE_STAT_TIMESPEC_T)
typedef struct timespec	pmimport_mtime_t;
#else
typedef time_t		pmimport_mtime_t;
#endif

#define PMIMPORT_MAX	8

typedef struct {
    char	    archive[256];
    char	    version[32];
    char	    args[512];
    pid_t	    pid;
    pmimport_mtime_t mtime;
} pmimport_t;

static int
mtime_changed(pmimport_t *imp, const struct stat *st)
{
#if defined(HAVE_ST_MTIME_WITH_E) && defined(HAVE_STAT_TIME_T)
    if (st->st_mtime == imp->mtime)
	return 0;
    imp->mtime = st->st_mtime;
#elif defined(HAVE_ST_MTIME_WITH_SPEC)
    if (st->st_mtimespec.tv_sec  == imp->mtime.tv_sec &&
	st->st_mtimespec.tv_nsec == imp->mtime.tv_nsec)
	return 0;
    imp->mtime = st->st_mtimespec;
#else
    if (st->st_mtim.tv_sec  == imp->mtime.tv_sec &&
	st->st_mtim.tv_nsec == imp->mtime.tv_nsec)
	return 0;
    imp->mtime = st->st_mtim;
#endif
    return 1;
}

static pmInDom		pmimport_indom;
static char		pmimport_hostname[MAXHOSTNAMELEN];
static char		*pmimport_zoneinfo;

static pmdaIndom	indomtab[] = {
    { 0 /* filled in init */, 0, NULL },
};

static pmdaMetric	metrictab[] = {
/* pmimport.archive */
    { NULL, { PMDA_PMID(0,0), PM_TYPE_STRING, 0 /* indom */, PM_SEM_DISCRETE,
	      PMDA_PMUNITS(0,0,0,0,0,0) } },
/* pmimport.version */
    { NULL, { PMDA_PMID(0,1), PM_TYPE_STRING, 0 /* indom */, PM_SEM_DISCRETE,
	      PMDA_PMUNITS(0,0,0,0,0,0) } },
/* pmimport.args */
    { NULL, { PMDA_PMID(0,2), PM_TYPE_STRING, 0 /* indom */, PM_SEM_DISCRETE,
	      PMDA_PMUNITS(0,0,0,0,0,0) } },
/* pmimport.hostname */
    { NULL, { PMDA_PMID(1,0), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
	      PMDA_PMUNITS(0,0,0,0,0,0) } },
/* pmimport.timezone */
    { NULL, { PMDA_PMID(1,1), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
	      PMDA_PMUNITS(0,0,0,0,0,0) } },
/* pmimport.zoneinfo */
    { NULL, { PMDA_PMID(1,2), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
	      PMDA_PMUNITS(0,0,0,0,0,0) } },
};

static void
refresh_pmimport(void)
{
    const char	    *importdir;
    char	    path[256];
    char	    line[512];
    DIR		    *dp;
    struct dirent   *de;
    struct stat	    st;
    FILE	    *fp;
    pmimport_t	    *imp;
    int		    n = 0;

    importdir = pmGetConfig("PCP_IMPORTRUN_DIR");

    /* Mark all existing instances inactive; only seen ones get reactivated */
    pmdaCacheOp(pmimport_indom, PMDA_CACHE_INACTIVE);

    if ((dp = opendir(importdir)) == NULL)
	return;

    while ((de = readdir(dp)) != NULL) {
	int	skip = 0;

	if (de->d_name[0] == '.')
	    continue;

	pmsprintf(path, sizeof(path), "%s/%s", importdir, de->d_name);
	if ((fp = fopen(path, "r")) == NULL)
	    continue;
	if (fstat(fileno(fp), &st) != 0 || !S_ISREG(st.st_mode)) {
	    fclose(fp);
	    continue;
	}

	/* Lookup existing cache entry or allocate a new one */
	if (pmdaCacheLookupName(pmimport_indom, de->d_name, NULL,
				(void **)&imp) < 0 || imp == NULL) {
	    imp = calloc(1, sizeof(*imp));
	    if (imp == NULL) {
		fclose(fp);
		continue;
	    }
	}

	/* Re-read only if mtime changed; always recheck pid liveness */
	if (!mtime_changed(imp, &st)) {
	    fclose(fp);
	    skip = (imp->pid > 0 && !__pmProcessExists(imp->pid));
	} else {
	    imp->version[0] = '\0';
	    imp->args[0]    = '\0';
	    imp->archive[0] = '\0';
	    imp->pid        = 0;

	    while (fgets(line, sizeof(line), fp)) {
		char	*end;
		long	pid;

		line[strcspn(line, "\n\r")] = '\0';
		if (strncmp(line, "pid=", 4) == 0) {
		    pid = strtol(line + 4, &end, 10);
		    if (end != line + 4 && *end == '\0' && pid > 0) {
			imp->pid = (pid_t)pid;
			if (!__pmProcessExists(imp->pid))
			    skip = 1;	/* stale file from crashed daemon */
		    }
		}
		else if (strncmp(line, "version=", 8) == 0)
		    pmstrncpy(imp->version, sizeof(imp->version), line + 8);
		else if (strncmp(line, "args=", 5) == 0)
		    pmstrncpy(imp->args, sizeof(imp->args), line + 5);
		else if (strncmp(line, "archive=", 8) == 0)
		    pmstrncpy(imp->archive, sizeof(imp->archive), line + 8);
	    }
	    fclose(fp);
	}

	/* Count active tools; stale entries do not consume a slot */
	if (!skip && ++n > PMIMPORT_MAX) {
	    pmNotifyErr(LOG_WARNING, "pmimport: too many tools"
			" (max %d), skipping \"%s\"",
			PMIMPORT_MAX, de->d_name);
	    skip = 1;
	}

	if (skip)
	    pmdaCacheStore(pmimport_indom, PMDA_CACHE_INACTIVE, de->d_name, imp);
	else
	    pmdaCacheStore(pmimport_indom, PMDA_CACHE_ADD, de->d_name, imp);
    }
    closedir(dp);

    pmdaCacheOp(pmimport_indom, PMDA_CACHE_SAVE);
}

static int
pmimport_fetchcb(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    int		    cluster = pmID_cluster(mdesc->m_desc.pmid);
    int		    item = pmID_item(mdesc->m_desc.pmid);
    pmimport_t	    *imp;

    if (cluster == 1) {
	/* singleton metrics: hostname, timezone, zoneinfo */
	switch (item) {
	case 0:	/* pmimport.hostname */
	    if (pmimport_hostname[0] == '\0') {
		(void)gethostname(pmimport_hostname, sizeof(pmimport_hostname));
		pmimport_hostname[sizeof(pmimport_hostname)-1] = '\0';
	    }
	    atom->cp = pmimport_hostname;
	    break;
	case 1:	/* pmimport.timezone — not cached; catches DST changes */
	    atom->cp = __pmTimezone();
	    break;
	case 2:	/* pmimport.zoneinfo — stable Olson name, cache it */
	    if (pmimport_zoneinfo == NULL)
		pmimport_zoneinfo = __pmZoneinfo();
	    atom->cp = pmimport_zoneinfo ? pmimport_zoneinfo : "";
	    break;
	default:
	    return PM_ERR_PMID;
	}
	return PMDA_FETCH_STATIC;
    }

    if (pmdaCacheLookup(pmimport_indom, inst, NULL, (void **)&imp) < 0 ||
	imp == NULL)
	return PM_ERR_INST;

    switch (item) {
    case 0:	/* pmimport.archive */
	atom->cp = imp->archive;
	break;
    case 1:	/* pmimport.version */
	atom->cp = imp->version;
	break;
    case 2:	/* pmimport.args */
	atom->cp = imp->args;
	break;
    default:
	return PM_ERR_PMID;
    }
    return PMDA_FETCH_STATIC;
}

static int
pmimport_fetch(int numpmid, pmID pmidlist[], pmdaResult **resp, pmdaExt *pmda)
{
    refresh_pmimport();
    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

static int
pmimport_instance(pmInDom indom, int inst, char *name, pmInResult **result,
		  pmdaExt *pmda)
{
    refresh_pmimport();
    return pmdaInstance(indom, inst, name, result, pmda);
}

void
__PMDA_INIT_CALL
pmimport_init(pmdaInterface *dp)
{
    char	helppath[MAXPATHLEN];
    int		sep = pmPathSeparator();
    int		i;

    pmsprintf(helppath, sizeof(helppath), "%s%c" "pmimport" "%c" "help",
	      pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDSO(dp, PMDA_INTERFACE_7, "pmimport DSO", helppath);

    pmimport_indom = pmInDom_build(PMIMPORT, 0);
    indomtab[0].it_indom = pmimport_indom;

    for (i = 0; i < (int)(sizeof(metrictab)/sizeof(metrictab[0])); i++) {
	if (pmID_cluster(metrictab[i].m_desc.pmid) == 0)
	    metrictab[i].m_desc.indom = pmimport_indom;
	/* cluster 1 singleton metrics keep PM_INDOM_NULL from metrictab[] */
    }

    pmdaSetFetchCallBack(dp, pmimport_fetchcb);
    pmdaInit(dp, indomtab, 1, metrictab,
	     sizeof(metrictab)/sizeof(metrictab[0]));

    /* Restore any previously persisted instance domain state */
    pmdaCacheOp(pmimport_indom, PMDA_CACHE_LOAD);

    dp->version.seven.fetch    = pmimport_fetch;
    dp->version.seven.instance = pmimport_instance;
}
