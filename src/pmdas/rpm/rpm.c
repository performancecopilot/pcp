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

/*
 * Metrics describing internals of pmdarpm operation  (Cluster 0)
 */
#define REFRESH_COUNT_IDX 0
#define REFRESH_TIME_USER_IDX 1
#define REFRESH_TIME_KERNEL_IDX 2
#define REFRESH_TIME_ELAPSED_IDX 3
#define DATASIZE_IDX 4

/*
 * List of metrics corresponding to rpm --querytags  (Cluster 1)
 * corresponds to metrictab to give the tag rpm uses to fetch the given value
 */
#define RPMTAG_ARCH_IDX 0
#define RPMTAG_BUILDHOST_IDX 1
#define RPMTAG_BUILDTIME_IDX 2
#define RPMTAG_DESCRIPTION_IDX 3
#define RPMTAG_EPOCH_IDX 4
#define RPMTAG_GROUP_IDX 5
#define RPMTAG_INSTALLTIME_IDX 6
#define RPMTAG_LICENSE_IDX 7
#define RPMTAG_NAME_IDX 8
#define RPMTAG_PACKAGER_IDX 9
#define RPMTAG_RELEASE_IDX 10
#define RPMTAG_SIZE_IDX 11
#define RPMTAG_SOURCERPM_IDX 12
#define RPMTAG_SUMMARY_IDX 13
#define RPMTAG_URL_IDX 14
#define RPMTAG_VENDOR_IDX 15
#define RPMTAG_VERSION_IDX 16
static struct metrics {
    int tag;
    int type;
} metrics[] = {
    [RPMTAG_ARCH_IDX] = {RPMTAG_ARCH, 0},
    [RPMTAG_BUILDHOST_IDX] = {RPMTAG_BUILDHOST, 0},
    [RPMTAG_BUILDTIME_IDX] = {RPMTAG_BUILDTIME, 0},
    [RPMTAG_DESCRIPTION_IDX] = {RPMTAG_DESCRIPTION, 0},
    [RPMTAG_EPOCH_IDX] = {RPMTAG_EPOCH, 0},
    [RPMTAG_GROUP_IDX] = {RPMTAG_GROUP, 0},
    [RPMTAG_INSTALLTIME_IDX] = {RPMTAG_INSTALLTIME, 0},
    [RPMTAG_LICENSE_IDX] = {RPMTAG_LICENSE, 0},
    [RPMTAG_NAME_IDX] = {RPMTAG_NAME, 0},
    [RPMTAG_PACKAGER_IDX] = {RPMTAG_PACKAGER, 0},
    [RPMTAG_RELEASE_IDX] = {RPMTAG_RELEASE, 0},
    [RPMTAG_SIZE_IDX] = {RPMTAG_SIZE, 0},
    [RPMTAG_SOURCERPM_IDX] = {RPMTAG_SOURCERPM, 0},
    [RPMTAG_SUMMARY_IDX] = {RPMTAG_SUMMARY, 0},
    [RPMTAG_URL_IDX] = {RPMTAG_URL, 0},
    [RPMTAG_VENDOR_IDX] = {RPMTAG_VENDOR, 0},
    [RPMTAG_VERSION_IDX] = {RPMTAG_VERSION, 0}
};


#define METRICTAB_ENTRY(M,N,T) (&metrics[N].tag), { PMDA_PMID (M, N), T, 0, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },

static pmdaMetric metrictab[] = {
    { NULL, {PMDA_PMID(0, REFRESH_COUNT_IDX),
      PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)} },
    { NULL, {PMDA_PMID(0, REFRESH_TIME_USER_IDX),
      PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_SEC, 0)} },
    { NULL, {PMDA_PMID(0, REFRESH_TIME_KERNEL_IDX),
      PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_SEC, 0)} },
    { NULL, {PMDA_PMID(0, REFRESH_TIME_ELAPSED_IDX),
      PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_SEC, 0)} },
    { NULL, {PMDA_PMID(0, REFRESH_TIME_ELAPSED_IDX),
      PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_SEC, 0)} },
    { NULL, {PMDA_PMID(0, DATASIZE_IDX),
      PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0)} },
    {METRICTAB_ENTRY (1, RPMTAG_ARCH_IDX, PM_TYPE_STRING)},
    {METRICTAB_ENTRY (1, RPMTAG_BUILDHOST_IDX, PM_TYPE_STRING)},
    {METRICTAB_ENTRY (1, RPMTAG_BUILDTIME_IDX, PM_TYPE_U32)},
    {METRICTAB_ENTRY (1, RPMTAG_DESCRIPTION_IDX, PM_TYPE_STRING)},
    {METRICTAB_ENTRY (1, RPMTAG_EPOCH_IDX, PM_TYPE_U32)},
    {METRICTAB_ENTRY (1, RPMTAG_GROUP_IDX, PM_TYPE_STRING)},
    {METRICTAB_ENTRY (1, RPMTAG_INSTALLTIME_IDX, PM_TYPE_U32)},
    {METRICTAB_ENTRY (1, RPMTAG_LICENSE_IDX, PM_TYPE_STRING)},
    {METRICTAB_ENTRY (1, RPMTAG_NAME_IDX, PM_TYPE_STRING)},
    {METRICTAB_ENTRY (1, RPMTAG_PACKAGER_IDX, PM_TYPE_STRING)},
    {METRICTAB_ENTRY (1, RPMTAG_RELEASE_IDX , PM_TYPE_STRING)},
    {METRICTAB_ENTRY (1, RPMTAG_SIZE_IDX , PM_TYPE_U32)},
    {METRICTAB_ENTRY (1, RPMTAG_SOURCERPM_IDX , PM_TYPE_STRING)},
    {METRICTAB_ENTRY (1, RPMTAG_SUMMARY_IDX , PM_TYPE_STRING)},
    {METRICTAB_ENTRY (1, RPMTAG_URL_IDX , PM_TYPE_STRING)},
    {METRICTAB_ENTRY (1, RPMTAG_VENDOR_IDX , PM_TYPE_STRING)},
    {METRICTAB_ENTRY (1, RPMTAG_VERSION_IDX , PM_TYPE_STRING)},
};

// RPM cache information.  A linked list of pcp pmAtonValue structures.
// RPMTAG_NAME will yield the name.  The type for each metric is stored and
// saved in metrics.

typedef struct cache_entry {
    pmAtomValue entry[(RPMTAG_NVRA - RPMTAG_NAME) + 1 +
		      (RPMTAG_FIRSTFREE_TAG - RPMTAG_FILENAMES)];
    struct cache_entry *next;
} cache_entry;
static cache_entry *cache;
static cache_entry *current_cache_entry;

// To load the instances
static pthread_t indom_thread;
// To notice when the rpm database changes
static pthread_t inotify_thread;
static unsigned long long numrefresh = 0;

static pthread_mutex_t indom_mutex;

// Load the instances dynamically
pmdaIndom indomtab[] = {
#define RPM_INDOM	0
    {RPM_INDOM, 0, NULL},
};
static pmInDom *rpm_indom = &indomtab[0].it_indom;

// Invoked as a .so or as a daemon?
static int isDSO = 1;
static char *username;
static char *dbpath = "/var/lib/rpm";

/*
 * Callback provided to pmdaFetch to fetch values from rpm db corresponding to metric_querytags
 */

static int
rpm_fetchCallBack(pmdaMetric * mdesc, unsigned int inst,
		  pmAtomValue * atom)
{
    int sts = PMDA_FETCH_STATIC;
    __pmID_int *idp = (__pmID_int *) & (mdesc->m_desc.pmid);
    pmAtomValue this_atom;
    int type;
    int tag;

    // Get the instance name for this instance
    char *rpm_inst_name;
    cache_entry *this_cache_entry;

    if (idp->cluster == 0) {
	unsigned long	datasize;

	sts = PMDA_FETCH_STATIC;
	pthread_mutex_lock(&indom_mutex);
	switch (idp->item) {
	case REFRESH_COUNT_IDX:		/* rpm.refresh.count */
	    atom->ull = numrefresh;
	    break;
	case REFRESH_TIME_USER_IDX:	/* rpm.refresh.time.user */
	    atom->d = get_user_timer();
	    break;
	case REFRESH_TIME_KERNEL_IDX:	/* rpm.refresh.time.kernel */
	    atom->d = get_kernel_timer();
	    break;
	case REFRESH_TIME_ELAPSED_IDX:	/* rpm.refresh.time.elapsed */
	    atom->d = get_elapsed_timer();
	    break;
	case DATASIZE_IDX:		/* rpm.datasize */
	    __pmProcessDataSize(&datasize);
	    atom->ul = datasize;
	    break;
	default:
	    sts = PM_ERR_PMID;
	    break;
 	}
	pthread_mutex_unlock(&indom_mutex);
	return sts;
    }

    pthread_mutex_lock(&indom_mutex);

    int cacheidx = pmdaCacheLookup(*rpm_indom, inst, &rpm_inst_name, (void*)&this_cache_entry);

    pthread_mutex_unlock(&indom_mutex);

    if (cacheidx < 0) {
	__pmNotifyErr(LOG_ERR, "pmdaCacheLookup failed: inst=%d: %s", inst,
		      pmErrStr(cacheidx));
	return PM_ERR_INST;
    }

    type = ((struct metrics *) mdesc->m_user)->type;
    tag = ((struct metrics *) mdesc->m_user)->tag;
    int metric_idx;	// index of this rpms' metric
    for (metric_idx = 0;
	 metric_idx < (sizeof(metrics) / sizeof(struct metrics));
	 metric_idx++)
	if (metrics[metric_idx].tag == tag)
	    break;

    this_atom = this_cache_entry->entry[metric_idx];

    switch (type) {
    case PM_TYPE_STRING:
	if (this_atom.cp)
	    atom->cp = strdup(this_atom.cp);
	else
	    atom->cp = strdup("");
	sts = PMDA_FETCH_DYNAMIC;
	break;
    case PM_TYPE_32:
	atom->l = this_atom.l;
	sts = PMDA_FETCH_STATIC;
	break;
    case PM_TYPE_64:
	atom->ll = this_atom.ll;
	sts = PMDA_FETCH_STATIC;
	break;
    default:
	return PM_ERR_TYPE;
    }

    return sts;
}

/*
  Sync up with the (initial) indom loading thread
 */

static int
notready(pmdaExt *pmda)
{
    __pmSendError(pmda->e_outfd, FROM_ANON, PM_ERR_PMDANOTREADY);
    pthread_join(indom_thread, NULL);
    return PM_ERR_PMDAREADY;
}

/*
  Called once for each pmFetch(3) operation
*/
static int
rpm_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    if (numrefresh == 0)
	return notready(pmda);
    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}


/*
  Called once for each pmGetInDom(3) operation
*/
static int
rpm_instance(pmInDom id, int i, char *name, __pmInResult **in, pmdaExt *pmda)
{
    if (numrefresh == 0)
	return notready(pmda);
    return pmdaInstance(id, i, name, in, pmda);
}
 
/*
 * Load the rpm module names into the instance table
 */

int
rpm_update_cache(void *ptr)
{
    rpmts ts = NULL;
    Header h;
    rpmdbMatchIterator mi;
    rpmtd td;
    int sts = 0;


    start_timing();
    td = rpmtdNew();
    ts = rpmtsCreate();

    rpmReadConfigFiles(NULL, NULL);

    pthread_mutex_lock(&indom_mutex);
    cache_entry *this_cache_entry;
    // Is this an inotify rpm reload?
    if (!cache) {
	cache = current_cache_entry = malloc(sizeof(cache_entry));
	memset(cache, 0, sizeof(cache_entry));
    }
    // Iterate through the entire list of rpms
    mi = rpmtsInitIterator(ts, RPMDBI_PACKAGES, NULL, 0);

    while ((h = rpmdbNextIterator(mi)) != NULL) {
	this_cache_entry = malloc(sizeof(cache_entry));
	memset(this_cache_entry, 0, sizeof(cache_entry));
	current_cache_entry->next = this_cache_entry;
	current_cache_entry = this_cache_entry;
	current_cache_entry->next = NULL;

	int i;
	for (i = 0; i < (sizeof(metrics) / sizeof(struct metrics)); i++) {
	    headerGet(h, RPMTAG_NAME, td, HEADERGET_EXT);
	    const char *rpmname = rpmtdGetString(td);

	    // If we already have this rpm then grab the next one
	    // ?? we need to check the rpm version and reload if it is a newer rpm version
	    if (pmdaCacheLookupName (*rpm_indom, rpmname, NULL, NULL) > 0)
		continue;

	    headerGet(h, metrics[i].tag, td, HEADERGET_EXT);
	    switch (td->type) {
		// The rpm value is a string
	    case RPM_STRING_TYPE:
		{
		    const char *rpmval = rpmtdGetString(td);
		    current_cache_entry->entry[i].cp = strdup(rpmval);
		    if (current_cache_entry == NULL) {
			__pmNotifyErr(LOG_INFO, "strdup(%d) for %d failed",
				      (int) strlen(rpmval), i);
			sts = -ENOMEM;
			break;
		    }
		    metrics[i].type = PM_TYPE_STRING;
		    break;
		}
		// The rpm value is an array of strings
	    case RPM_STRING_ARRAY_TYPE:
		{
		    // concatenate the string parts
		    char **strings;
		    strings = td->data;
		    char *ccecp = 0;
		    int idx;
		    int string_length = 0;

		    for (idx = 0; idx < td->count; idx++) {
			if (string_length > 2000) {
			    ccecp = realloc(ccecp, string_length + 3);
			    strcat(ccecp, "...");
			    break;
			}
			if (idx == 0) {
			    string_length = strlen(strings[idx]) + 1;
			    ccecp = malloc(string_length);
			    if (ccecp == NULL)
				__pmNotifyErr(LOG_INFO,
					      "malloc(%d) for %d failed",
					      (int) strlen(strings[idx]),
					      i);
			    strcpy(ccecp, strings[idx]);
			}
			else {
			    string_length = strlen(ccecp) +
				strlen(strings[idx]) + 2;
			    ccecp = realloc(ccecp, string_length);
			    if (ccecp == NULL)
				__pmNotifyErr(LOG_INFO,
					      "realloc(%d) for %d failed",
					      (int) strlen(ccecp) +
					      (int) strlen(strings[idx]),
					      i);
			    strcat(ccecp, "\n");
			    strcat(ccecp, strings[idx]);
			}
		    }
		    current_cache_entry->entry[i].cp = ccecp;
		    free(td->data);
		    metrics[i].type = PM_TYPE_STRING;
		    break;
		}
		// The rpm value is an int
		// ?? Handle array of ints
	    case RPM_INT8_TYPE:
		current_cache_entry->entry[i].l = ((char *) (td->data))[0];
		// type defaults to 0 == PM_TYPE_32
		break;
	    case RPM_INT16_TYPE:
		current_cache_entry->entry[i].l =
		    ((short *) (td->data))[0];
		// type defaults to 0 == PM_TYPE_32
		break;
	    case RPM_INT32_TYPE:
		current_cache_entry->entry[i].l = ((int *) (td->data))[0];
		// type defaults to 0 == PM_TYPE_32
		break;
	    case RPM_INT64_TYPE:
		current_cache_entry->entry[i].ll =
		    ((long long *) (td->data))[0];
		metrics[i].type = PM_TYPE_64;
		break;
	    default:
		break;
	    }
	    rpmtdReset(td);
	}
	// headerFree(h);
    }
    rpmdbFreeIterator(mi);
    rpmtsFree(ts);

    pthread_mutex_unlock(&indom_mutex);
    stop_timing();
    return sts;
}


/*
 * Load the rpm module names into the instance table
 */

void *
rpm_update_indom(void *ptr)
{
    int sts;
    cache_entry *this_cache_entry;

    rpm_update_cache(NULL);

    start_timing();
    pthread_mutex_lock(&indom_mutex);
    sts = pmdaCacheOp(*rpm_indom, PMDA_CACHE_INACTIVE);
    if (sts < 0)
	__pmNotifyErr(LOG_ERR,
		      "pmdaCacheOp(INACTIVE) failed: indom=%s: %s",
		      pmInDomStr(*rpm_indom), pmErrStr(sts));

    // Iterate through the entire list of rpms.  The pmda cache are the rpm module names

    for (this_cache_entry = cache; this_cache_entry != NULL;
	 this_cache_entry = this_cache_entry->next) {
	ENTRY e;
	e.key = this_cache_entry->entry[RPMTAG_NAME_IDX].cp;
	if (!e.key)
	    continue;
	sts =
	    pmdaCacheStore(*rpm_indom, PMDA_CACHE_ADD, e.key,
			   (void *) this_cache_entry);

	if (sts < 0)
	    __pmNotifyErr(LOG_ERR, "pmdaCacheStore failed: indom=%s: %s",
			  pmInDomStr(*rpm_indom), pmErrStr(sts));
    }

    stop_timing();
    pthread_mutex_unlock(&indom_mutex);
    numrefresh++;

    return NULL;
}

/*
 * Notice when the rpm database changes and reload the instances.
 */

void *
rpm_inotify(void *ptr)
{
#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define EVENT_BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )
    char buffer[EVENT_BUF_LEN];
    int fd;

    fd = inotify_init();

    // ?? parameterize the path, check return code
    inotify_add_watch(fd, dbpath, IN_CLOSE_WRITE);

    while (1) {
	int i = 0;
	int read_count;
	int need_refresh = 0;
	// Wait for changes in the rpm database
	read_count = read(fd, buffer, EVENT_BUF_LEN);
	__pmNotifyErr(LOG_INFO, "rpm_inotify: read_count=%d", read_count);
	while (i < read_count) {
	    struct inotify_event *event =
		(struct inotify_event *) &buffer[i];
	    if (event->mask & IN_CLOSE_WRITE)
		need_refresh++;
	    i++;
	}
	__pmNotifyErr(LOG_INFO, "rpm_inotify: need_refresh=%d",
		      need_refresh);
	if (!need_refresh)
	    continue;
	rpm_update_indom(NULL);
	__pmNotifyErr(LOG_INFO, "rpm_inotify: refresh done");
    }
    return NULL;
}

/*
 * Initialize the daemon/.so agent.
 */

void
rpm_init(pmdaInterface * dp)
{
    if (isDSO) {
	char helppath[MAXPATHLEN];
	pmdaDSO(dp, PMDA_INTERFACE_5, "rpm DSO", helppath);
    }
    else {
	__pmSetProcessIdentity(username);
    }

    if (dp->status != 0)
	return;

    pthread_mutex_init(&indom_mutex, NULL);
    // Load rpms into instance table
    pthread_create(&indom_thread, NULL, rpm_update_indom, NULL);
    // Note changes to the rpm database
    pthread_create(&inotify_thread, NULL, rpm_inotify, NULL);

    dp->version.any.fetch = rpm_fetch;
    dp->version.any.instance = rpm_instance;

    pmdaSetFetchCallBack(dp, rpm_fetchCallBack);

    pmdaInit(dp, indomtab, sizeof(indomtab) / sizeof(indomtab[0]),
		metrictab, sizeof(metrictab) / sizeof(metrictab[0]));
}

static void
usage(void)
{
    fprintf(stderr, "Usage: %s [options]\n\n", pmProgname);
    fprintf(stderr, "Options:\n"
	  "  -d domain    use domain (numeric) for metrics domain of PMDA\n"
	  "  -l logfile   write log into logfile rather than using default log name\n"
	  "  -r path      path to directory containing rpm database (default %s)\n"
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
    int sep = __pmPathSeparator();
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
	    pmdaGetOpt(argc, argv, "D:d:i:l:pr:u:6:U:?", &dispatch,
		       &err)) != EOF) {
	switch (c) {
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
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);

    exit(0);
}
