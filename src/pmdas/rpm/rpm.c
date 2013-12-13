/*
 * RPM Package Manager PMDA
 *
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
 */

#include <sys/stat.h>
#include <pthread.h>
#include <search.h>
#include <sys/inotify.h>
#include <rpm/rpmlib.h>
#include <rpm/header.h>
#include <rpm/rpmts.h>
#include <rpm/rpmdb.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/pmda.h>
#include "domain.h"
#include "timer.h"
#include "rpm.h"

static pmdaIndom indomtab[] = {
    {RPM_INDOM, 0, NULL},
    {CACHE_INDOM, 1, NULL},
    {STRINGS_INDOM, 2, NULL},
};

static pmdaMetric metrictab[] = {
    /* PMDA internals metrics - timing, count of refreshes, memory */
    { NULL, { PMDA_PMID(0, REFRESH_COUNT_ID), PM_TYPE_U64,
	PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE)}},
    { NULL, { PMDA_PMID(0, REFRESH_TIME_USER_ID), PM_TYPE_DOUBLE,
	PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_SEC,0)}},
    { NULL, { PMDA_PMID(0, REFRESH_TIME_KERNEL_ID), PM_TYPE_DOUBLE,
	PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_SEC,0)}},
    { NULL, { PMDA_PMID(0, REFRESH_TIME_ELAPSED_ID), PM_TYPE_DOUBLE,
	PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_SEC,0)}},
    { NULL, { PMDA_PMID(0, DATASIZE_ID), PM_TYPE_U32,
	PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0)}},

    /* rpm package metrics */
    { NULL, { PMDA_PMID(1, ARCH_ID), PM_TYPE_STRING,
	RPM_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0)}},
    { NULL, { PMDA_PMID(1, BUILDHOST_ID), PM_TYPE_STRING,
	RPM_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0)}},
    { NULL, { PMDA_PMID(1, BUILDTIME_ID), PM_TYPE_U32,
	RPM_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(0,1,0,0,PM_TIME_SEC,0)}},
    { NULL, { PMDA_PMID(1, DESCRIPTION_ID), PM_TYPE_STRING,
	RPM_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0)}},
    { NULL, { PMDA_PMID(1, EPOCH_ID), PM_TYPE_U32,
	RPM_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0)}},
    { NULL, { PMDA_PMID(1, GROUP_ID), PM_TYPE_STRING,
	RPM_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0)}},
    { NULL, { PMDA_PMID(1, INSTALLTIME_ID), PM_TYPE_U32,
	RPM_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(0,1,0,0,PM_TIME_SEC,0)}},
    { NULL, { PMDA_PMID(1, LICENSE_ID), PM_TYPE_STRING,
	RPM_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0)}},
    { NULL, { PMDA_PMID(1, PACKAGER_ID), PM_TYPE_STRING,
	RPM_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0)}},
    { NULL, { PMDA_PMID(1, RELEASE_ID), PM_TYPE_STRING,
	RPM_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0)}},
    { NULL, { PMDA_PMID(1, SIZE_ID), PM_TYPE_U32,
	RPM_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0)}},
    { NULL, { PMDA_PMID(1, SOURCERPM_ID), PM_TYPE_STRING,
	RPM_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0)}},
    { NULL, { PMDA_PMID(1, SUMMARY_ID), PM_TYPE_STRING,
	RPM_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0)}},
    { NULL, { PMDA_PMID(1, URL_ID), PM_TYPE_STRING,
	RPM_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0)}},
    { NULL, { PMDA_PMID(1, VENDOR_ID), PM_TYPE_STRING,
	RPM_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0)}},
    { NULL, { PMDA_PMID(1, VERSION_ID), PM_TYPE_STRING,
	RPM_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0)}},
};

static pthread_t indom_thread;		/* load the instances initially */
static pthread_t inotify_thread;	/* runs when the rpmdb changes */
static unsigned long long numrefresh;	/* updated by background threads */
static pthread_mutex_t indom_mutex;

static int isDSO = 1;			/* invoked as shlib or daemon */
static char *username;
static char *dbpath = "/var/lib/rpm";

static pmInDom
INDOM(int serial)
{
    return indomtab[serial].it_indom;
}

static char *
dict_lookup(int index)
{
    char *value;
    pmInDom dict = INDOM(STRINGS_INDOM);

    if (pmdaCacheLookup(dict, index, &value, NULL) == PMDA_CACHE_ACTIVE)
        return value;
    return "";
}

static int
dict_insert(const char *string)
{
    pmInDom dict = INDOM(STRINGS_INDOM);
    if (!string)
	string = "";
    return pmdaCacheStore(dict, PMDA_CACHE_ADD, string, NULL);
}

static int
rpm_fetch_pmda(int item, pmAtomValue *atom)
{
    int sts = PMDA_FETCH_STATIC;
    unsigned long datasize;

    switch (item) {
    case REFRESH_COUNT_ID:		/* rpm.refresh.count */
	atom->ull = numrefresh;
	break;
    case REFRESH_TIME_USER_ID:		/* rpm.refresh.time.user */
	atom->d = get_user_timer();
	break;
    case REFRESH_TIME_KERNEL_ID:	/* rpm.refresh.time.kernel */
	atom->d = get_kernel_timer();
	break;
    case REFRESH_TIME_ELAPSED_ID:	/* rpm.refresh.time.elapsed */
	atom->d = get_elapsed_timer();
	break;
    case DATASIZE_ID:			/* rpm.datasize */
	__pmProcessDataSize(&datasize);
	atom->ul = datasize;
	break;
    default:
	sts = PM_ERR_PMID;
	break;
    }
    return sts;
}

static int
rpm_fetch_package(int item, unsigned int inst, pmAtomValue *atom)
{
    package *p;
    char *name;
    int sts;

    sts = pmdaCacheLookup(INDOM(RPM_INDOM), inst, &name, (void **)&p);
    if (sts < 0 || sts == PMDA_CACHE_INACTIVE)
	return PM_ERR_INST;

    sts = PMDA_FETCH_STATIC;
    switch (item) {
    case ARCH_ID:
	atom->cp = dict_lookup(p->values.arch);
	break;
    case BUILDHOST_ID:
	atom->cp = dict_lookup(p->values.buildhost);
	break;
    case BUILDTIME_ID:
	atom->ul = p->values.buildtime;
	break;
    case DESCRIPTION_ID:
	atom->cp = dict_lookup(p->values.description);
	break;
    case EPOCH_ID:
	atom->ul = p->values.epoch;
	break;
    case GROUP_ID:
	atom->cp = dict_lookup(p->values.group);
	break;
    case INSTALLTIME_ID:
	atom->ul = p->values.installtime;
	break;
    case LICENSE_ID:
	atom->cp = dict_lookup(p->values.license);
	break;
    case PACKAGER_ID:
	atom->cp = dict_lookup(p->values.packager);
	break;
    case RELEASE_ID:
	atom->cp = dict_lookup(p->values.release);
	break;
    case SIZE_ID:
	atom->ul = p->values.size;
	break;
    case SOURCERPM_ID:
	atom->cp = dict_lookup(p->values.sourcerpm);
	break;
    case SUMMARY_ID:
	atom->cp = dict_lookup(p->values.summary);
	break;
    case URL_ID:
	atom->cp = dict_lookup(p->values.url);
	break;
    case VENDOR_ID:
	atom->cp = dict_lookup(p->values.vendor);
	break;
    case VERSION_ID:
	atom->cp = dict_lookup(p->values.version);
	break;
    default:
	sts = PM_ERR_PMID;
	break;
    }
    return sts;
}

static int
rpm_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    __pmID_int *idp = (__pmID_int *) &mdesc->m_desc.pmid;
    int sts;

    pthread_mutex_lock(&indom_mutex);
    switch (idp->cluster) {
    case 0:
	if (inst != PM_IN_NULL)
	    sts = PM_ERR_INST;
	else
	    sts = rpm_fetch_pmda(idp->item, atom);
	break;
    case 1:
	sts = rpm_fetch_package(idp->item, inst, atom);
	break;
    default:
	sts = PM_ERR_PMID;
	break;
    }
    pthread_mutex_unlock(&indom_mutex);
    return sts;
}

/*
 * Sync the active rpm package instances with the reference database
 * maintained by the background threads.
 */
static void
rpm_indom_refresh(unsigned long long refresh)
{
    pmInDom rpmdb, cache;
    package *p;
    char *name;
    int sts;

    rpmdb = INDOM(RPM_INDOM);
    cache = INDOM(CACHE_INDOM);

    pmdaCacheOp(rpmdb, PMDA_CACHE_INACTIVE);

    pthread_mutex_lock(&indom_mutex);
    for (pmdaCacheOp(cache, PMDA_CACHE_WALK_REWIND);;) {
	if ((sts = pmdaCacheOp(cache, PMDA_CACHE_WALK_NEXT)) < 0)
	    break;
	if ((pmdaCacheLookup(cache, sts, &name, (void **)&p) < 0) || !p)
	    continue;
	if (p->refresh < refresh)
	    continue;
	pmdaCacheStore(rpmdb, PMDA_CACHE_ADD, name, (void *)p);
    }
    pthread_mutex_unlock(&indom_mutex);
}

/*
 * Sync up with the (initial) indom loading thread
 */
static int
notready(pmdaExt *pmda)
{
    __pmSendError(pmda->e_outfd, FROM_ANON, PM_ERR_PMDANOTREADY);
    pthread_join(indom_thread, NULL);
    return PM_ERR_PMDAREADY;
}

/*
 * Called once for each pmFetch(3) operation
 */
static int
rpm_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    unsigned long long refresh;

    pthread_mutex_lock(&indom_mutex);
    refresh = numrefresh;
    pthread_mutex_unlock(&indom_mutex);

    if (refresh == 0)
	return notready(pmda);
    rpm_indom_refresh(refresh);
    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

/*
 * Called once for each pmGetInDom(3) operation
 */
static int
rpm_instance(pmInDom id, int i, char *name, __pmInResult **in, pmdaExt *pmda)
{
    unsigned long long refresh;

    pthread_mutex_lock(&indom_mutex);
    refresh = numrefresh;
    pthread_mutex_unlock(&indom_mutex);

    if (refresh == 0)
	return notready(pmda);
    rpm_indom_refresh(refresh);
    return pmdaInstance(id, i, name, in, pmda);
}

static const char *
rpm_extract_string(rpmtd td, Header h, int tag)
{
    headerGet(h, tag, td, HEADERGET_EXT | HEADERGET_MINMEM);
    /*
     * RPM_STRING_ARRAY_TYPE being the alternative, e.g. filenames
     * (which we never expect to see, for the metrics we export).
     */
    if (td->type == RPM_STRING_ARRAY_TYPE)
	__pmNotifyErr(LOG_ERR,
		"rpm_extract_string: unexpected string array: %d", tag);

    return rpmtdGetString(td);
}

static __uint32_t
rpm_extract_value(rpmtd td, Header h, int tag)
{
    __uint32_t value;

    headerGet(h, tag, td, HEADERGET_EXT | HEADERGET_MINMEM);
    switch (td->type) {
    case RPM_INT8_TYPE:
	value = ((char *)(td->data))[0];
	break;
    case RPM_INT16_TYPE:
	value = ((short *)(td->data))[0];
	break;
    case RPM_INT32_TYPE:
	value = ((int *)(td->data))[0];
	break;
    case RPM_INT64_TYPE:
	value = ((long long *)(td->data))[0];
	break;
    default:
	value = 0;
	break;
    }
    return value;
}

static void
rpm_extract_metadata(const char *name, rpmtd td, Header h, metadata *m)
{
    if (pmDebug & DBG_TRACE_APPL0)
	__pmNotifyErr(LOG_INFO, "updating package %s metadata", name);

    m->arch = dict_insert(rpm_extract_string(td, h, RPMTAG_ARCH));
    m->buildhost = dict_insert(rpm_extract_string(td, h, RPMTAG_BUILDHOST));
    m->buildtime = rpm_extract_value(td, h, RPMTAG_BUILDTIME);
    m->description = dict_insert(rpm_extract_string(td, h, RPMTAG_DESCRIPTION));
    m->epoch = rpm_extract_value(td, h, RPMTAG_EPOCH);
    m->group = dict_insert(rpm_extract_string(td, h, RPMTAG_GROUP));
    m->installtime = rpm_extract_value(td, h, RPMTAG_INSTALLTIME);
    m->license = dict_insert(rpm_extract_string(td, h, RPMTAG_LICENSE));
    m->packager = dict_insert(rpm_extract_string(td, h, RPMTAG_PACKAGER));
    m->release = dict_insert(rpm_extract_string(td, h, RPMTAG_RELEASE));
    m->size = rpm_extract_value(td, h, RPMTAG_SIZE);
    m->sourcerpm = dict_insert(rpm_extract_string(td, h, RPMTAG_SOURCERPM));
    m->summary = dict_insert(rpm_extract_string(td, h, RPMTAG_SUMMARY));
    m->url = dict_insert(rpm_extract_string(td, h, RPMTAG_URL));
    m->vendor = dict_insert(rpm_extract_string(td, h, RPMTAG_VENDOR));
    m->version = dict_insert(rpm_extract_string(td, h, RPMTAG_VERSION));
}

/*
 * Refresh the RPM package names and values in the cache
 */
void *
rpm_update_cache(void *ptr)
{
    rpmtd td;
    rpmts ts;
    rpmdbMatchIterator mi;
    unsigned long long refresh;
    Header h;
    static int cache_err = 0;

    pthread_mutex_lock(&indom_mutex);
    start_timing();
    refresh = numrefresh + 1;	/* current iteration */
    pthread_mutex_unlock(&indom_mutex);

    td = rpmtdNew();
    ts = rpmtsCreate();

    rpmReadConfigFiles(NULL, NULL);

    /* Iterate through the entire list of RPMs, extract names and values */
    mi = rpmtsInitIterator(ts, RPMDBI_PACKAGES, NULL, 0);
    while ((h = rpmdbNextIterator(mi)) != NULL) {
	headerGet(h, RPMTAG_NAME, td, HEADERGET_EXT | HEADERGET_MINMEM);
	const char *name = rpmtdGetString(td);
	metadata meta;
	package *pp = NULL;
	int sts, err = 0;

	/* extract an on-stack copy of the package metadata, may do I/O */
	rpm_extract_metadata(name, td, h, &meta);

	/* we now have our data and cannot need more I/O; lock and load */
	pthread_mutex_lock(&indom_mutex);
	sts = pmdaCacheLookupName(INDOM(CACHE_INDOM), name, NULL, (void **)&pp);
	if (sts == PM_ERR_INST || (sts >= 0 && pp == NULL)) {
	    /* allocate space for new package entry for the cache */
	    if ((pp = calloc(1, sizeof(package))) == NULL)
		err = 1;
	} else if (sts < 0) {
	    err = 1;
	}

	if (!err) {
	    /* update values in cache entry for this package (locked) */
	    pp->refresh = refresh;
	    memcpy(&pp->values, &meta, sizeof(metadata));
	    pmdaCacheStore(INDOM(CACHE_INDOM), PMDA_CACHE_ADD, name, (void *)pp);
	} else {
	    /* ensure the logfile isn't spammed over and over */
	    if (cache_err++ < 10) {
		fprintf(stderr, "rpm_refresh_cache: "
			"pmdaCacheLookupName(%s, %s, ... %p) failed: %s\n",
			pmInDomStr(INDOM(CACHE_INDOM)), name, pp, pmErrStr(sts));
	    }
	}
	pthread_mutex_unlock(&indom_mutex);
    }
    rpmdbFreeIterator(mi);
    rpmtsFree(ts);

    pthread_mutex_lock(&indom_mutex);
    stop_timing();
    numrefresh = refresh;	/* current iteration complete */
    pthread_mutex_unlock(&indom_mutex);
    return NULL;
}

/*
 * Notice when the rpm database changes and reload the instances.
 */
void *
rpm_inotify(void *ptr)
{
    char buffer[EVENT_BUF_LEN];
    int fd;

    fd = inotify_init();
    inotify_add_watch(fd, dbpath, IN_CLOSE_WRITE);

    while (1) {
	int i = 0;
	int read_count;
	int need_refresh = 0;

	/* Wait for changes in the rpm database */
	read_count = read(fd, buffer, EVENT_BUF_LEN);
	if (pmDebug & DBG_TRACE_APPL1)
	    __pmNotifyErr(LOG_INFO, "rpm_inotify: read_count=%d", read_count);
	while (i < read_count) {
	    struct inotify_event *event =
		(struct inotify_event *) &buffer[i];
	    if (event->mask & IN_CLOSE_WRITE)
		need_refresh++;
	    i++;
	}
	if (pmDebug & DBG_TRACE_APPL1)
	    __pmNotifyErr(LOG_INFO, "rpm_inotify: need_refresh=%d",
		      need_refresh);
	if (!need_refresh)
	    continue;
	rpm_update_cache(ptr);
	if (pmDebug & DBG_TRACE_APPL1)
	    __pmNotifyErr(LOG_INFO, "rpm_inotify: refresh done");
    }
    return NULL;
}

/*
 * Initialize the daemon/.so agent.
 */

void
__PMDA_INIT_CALL
rpm_init(pmdaInterface * dp)
{
    if (isDSO) {
	int sep = __pmPathSeparator();
	char helppath[MAXPATHLEN];

	snprintf(helppath, sizeof(helppath), "%s%c" "rpm" "%c" "help",
                pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
	pmdaDSO(dp, PMDA_INTERFACE_5, "rpm DSO", helppath);
    }
    else {
	__pmSetProcessIdentity(username);
    }

    if (dp->status != 0)
	return;

    dp->version.any.fetch = rpm_fetch;
    dp->version.any.instance = rpm_instance;
    pmdaSetFetchCallBack(dp, rpm_fetchCallBack);

    pmdaInit(dp, indomtab, sizeof(indomtab) / sizeof(indomtab[0]),
		metrictab, sizeof(metrictab) / sizeof(metrictab[0]));

    pmdaCacheOp(INDOM(STRINGS_INDOM), PMDA_CACHE_STRINGS);

    pthread_mutex_init(&indom_mutex, NULL);
    /* Load rpms into instance table */
    pthread_create(&indom_thread, NULL, rpm_update_cache, NULL);
    /* Note changes to the rpm database */
    pthread_create(&inotify_thread, NULL, rpm_inotify, NULL);
}

static void
usage(void)
{
    fprintf(stderr, "Usage: %s [options]\n\n", pmProgname);
    fprintf(stderr, "Options:\n"
	  "  -C           parse the RPM database, and exit\n"
	  "  -d domain    use domain (numeric) for metrics domain of PMDA\n"
	  "  -l logfile   write log into logfile rather than using default log name\n"
	  "  -r path      path to directory containing RPM database (default %s)\n"
	  "  -U username  user account to run under (default \"pcp\")\n"
	  "\nExactly one of the following options may appear:\n"
	  "  -i port      expect PMCD to connect on given inet port (number or name)\n"
	  "  -p           expect PMCD to supply stdin/stdout (pipe)\n"
	  "  -u socket    expect PMCD to connect on given unix domain socket\n"
	  "  -6 port      expect PMCD to connect on given ipv6 port (number or name)\n",
	  dbpath);
    exit(1);
}

/*
 * Set up the agent if running as a daemon.
 */

int
main(int argc, char **argv)
{
    int c, err = 0;
    int Cflag, sep = __pmPathSeparator();
    pmdaInterface dispatch;
    char helppath[MAXPATHLEN];

    isDSO = 0;
    __pmSetProgname(argv[0]);
    __pmProcessDataSize(NULL);
    __pmGetUsername(&username);

    snprintf(helppath, sizeof(helppath), "%s%c" "rpm" "%c" "help",
	     pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_5, pmProgname, RPM,
	       "rpm.log", helppath);

    while ((c =
	    pmdaGetOpt(argc, argv, "CD:d:i:l:pr:u:6:U:?", &dispatch,
		       &err)) != EOF) {
	switch (c) {
	case 'C':
	    Cflag++;
	    break;
	case 'U':
	    username = optarg;
	    break;
	case 'r':
	    dbpath = optarg;
	    break;
	default:
	    err++;
	}
    }
    if (err)
	usage();

    pmdaOpenLog(&dispatch);
    rpm_init(&dispatch);
    if (Cflag) {
	rpm_update_cache(NULL);
	exit(0);
    }
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);

    exit(0);
}
