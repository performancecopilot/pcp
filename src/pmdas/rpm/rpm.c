/*
 * RPM PMDA
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
#include <assert.h>
#include <stdio.h>
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

// List of metrics corresponding to rpm --querytags

// corresponds to metrictab to give the tag rpm uses to fetch the given
// value

static struct metrics {
    int tag;
    int type;
} metrics[] = {
    /*
     * arch 
     */  {
    RPMTAG_ARCH, 0},
	/*
	 * basenames 
	 */  {
    RPMTAG_BASENAMES, 0},
	/*
	 * buildtime 
	 */  {
    RPMTAG_BUILDTIME, 0},
	/*
	 * description 
	 */  {
    RPMTAG_DESCRIPTION, 0},
	/*
	 * dirnames 
	 */  {
    RPMTAG_DIRNAMES, 0},
	/*
	 * distribution 
	 */  {
    RPMTAG_DISTRIBUTION, 0},
	/*
	 * evr 
	 */  {
    RPMTAG_EVR, 0},
	/*
	 * file.class 
	 */  {
    RPMTAG_FILECLASS, 0},
	/*
	 * file.linktos 
	 */  {
    RPMTAG_FILELINKTOS, 0},
	/*
	 * file.md5s 
	 */  {
    RPMTAG_FILEMD5S, 0},
	/*
	 * file.modes 
	 */  {
    RPMTAG_FILEMODES, 0},
	/*
	 * file.names 
	 */  {
    RPMTAG_FILENAMES, 0},
	/*
	 * file.require 
	 */  {
    RPMTAG_FILEREQUIRE, 0},
	/*
	 * file.sizes 
	 */  {
    RPMTAG_FILESIZES, 0},
	/*
	 * group 
	 */  {
    RPMTAG_GROUP, 0},
	/*
	 * license 
	 */  {
    RPMTAG_LICENSE, 0},
	/*
	 * name 
	 */  {
    RPMTAG_NAME, 0},
	/*
	 * obsoletes 
	 */  {
    RPMTAG_OBSOLETES, 0},
	/*
	 * packageid 
	 */  {
    RPMTAG_PKGID, 0},
	/*
	 * platform 
	 */  {
    RPMTAG_PLATFORM, 0},
	/*
	 * provideversion 
	 */  {
    RPMTAG_PROVIDEVERSION, 0},
	/*
	 * provides 
	 */  {
    RPMTAG_PROVIDES, 0},
	/*
	 * release 
	 */  {
    RPMTAG_RELEASE, 0},
	/*
	 * requires 
	 */  {
    RPMTAG_REQUIRES, 0},
	/*
	 * rpmversion 
	 */  {
    RPMTAG_RPMVERSION, 0},
	/*
	 * size 
	 */  {
    RPMTAG_SIZE, 0},
	/*
	 * sourcerpm 
	 */  {
    RPMTAG_SOURCERPM, 0},
	/*
	 * summary 
	 */  {
    RPMTAG_SUMMARY, 0},
	/*
	 * url 
	 */  {
    RPMTAG_URL, 0},
	/*
	 * version 
	 */  {
    RPMTAG_VERSION, 0}
};


#define METRICTAB_ENTRY(M,N,T) (&metrics[N].tag), { PMDA_PMID (M, N), T, 0, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },

static pmdaMetric metrictab[] = {
    /*
     * refresh.count 
     */
    {NULL,
     {PMDA_PMID(0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(0, 0, 0, 0, 0, 0)},},
    /*
     * refresh.time 
     */
    {NULL,
     {PMDA_PMID(0, 1), PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(0, 0, 0, 0, 0, 0)},},
    /*
     * arch 
     */
    {METRICTAB_ENTRY(1, __COUNTER__, PM_TYPE_STRING)},
    /*
     * basenames 
     */
    {METRICTAB_ENTRY(1, __COUNTER__, PM_TYPE_STRING)},
    /*
     * buildtime 
     */
    {METRICTAB_ENTRY(1, __COUNTER__, PM_TYPE_U32)},
    /*
     * description 
     */
    {METRICTAB_ENTRY(1, __COUNTER__, PM_TYPE_STRING)},
    /*
     * dirnames 
     */
    {METRICTAB_ENTRY(1, __COUNTER__, PM_TYPE_STRING)},
    /*
     * distribution 
     */
    {METRICTAB_ENTRY(1, __COUNTER__, PM_TYPE_STRING)},
    /*
     * evr 
     */
    {METRICTAB_ENTRY(1, __COUNTER__, PM_TYPE_STRING)},
    /*
     * file.class 
     */
    {METRICTAB_ENTRY(2, __COUNTER__, PM_TYPE_STRING)},
    /*
     * file.linktos 
     */
    {METRICTAB_ENTRY(2, __COUNTER__, PM_TYPE_STRING)},
    /*
     * file.md5s 
     */
    {METRICTAB_ENTRY(2, __COUNTER__, PM_TYPE_STRING)},
    /*
     * file.modes 
     */
    {METRICTAB_ENTRY(2, __COUNTER__, PM_TYPE_STRING)},
    /*
     * file.names 
     */
    {METRICTAB_ENTRY(2, __COUNTER__, PM_TYPE_STRING)},
    /*
     * file.require 
     */
    {METRICTAB_ENTRY(2, __COUNTER__, PM_TYPE_STRING)},
    /*
     * file.sizes 
     */
    {METRICTAB_ENTRY(2, __COUNTER__, PM_TYPE_U32)},
    /*
     * group 
     */
    {METRICTAB_ENTRY(1, __COUNTER__, PM_TYPE_STRING)},
    /*
     * license 
     */
    {METRICTAB_ENTRY(1, __COUNTER__, PM_TYPE_STRING)},
    /*
     * name 
     */
    {METRICTAB_ENTRY(1, __COUNTER__, PM_TYPE_STRING)},
    /*
     * obsoletes 
     */
    {METRICTAB_ENTRY(1, __COUNTER__, PM_TYPE_STRING)},
    /*
     * packageid 
     */
    {METRICTAB_ENTRY(1, __COUNTER__, PM_TYPE_STRING)},
    /*
     * platform 
     */
    {METRICTAB_ENTRY(1, __COUNTER__, PM_TYPE_STRING)},
    /*
     * provideversion 
     */
    {METRICTAB_ENTRY(1, __COUNTER__, PM_TYPE_STRING)},
    /*
     * provides 
     */
    {METRICTAB_ENTRY(1, __COUNTER__, PM_TYPE_STRING)},
    /*
     * release 
     */
    {METRICTAB_ENTRY(1, __COUNTER__, PM_TYPE_STRING)},
    /*
     * requires 
     */
    {METRICTAB_ENTRY(1, __COUNTER__, PM_TYPE_STRING)},
    /*
     * rpmversion 
     */
    {METRICTAB_ENTRY(1, __COUNTER__, PM_TYPE_STRING)},
    /*
     * size 
     */
    {METRICTAB_ENTRY(1, __COUNTER__, PM_TYPE_U32)},
    /*
     * sourcerpm 
     */
    {METRICTAB_ENTRY(1, __COUNTER__, PM_TYPE_STRING)},
    /*
     * summary 
     */
    {METRICTAB_ENTRY(1, __COUNTER__, PM_TYPE_STRING)},
    /*
     * url 
     */
    {METRICTAB_ENTRY(1, __COUNTER__, PM_TYPE_STRING)},
    /*
     * version 
     */
    {METRICTAB_ENTRY(1, __COUNTER__, PM_TYPE_STRING)}
};

// RPM cache information.  A linked list of pcp pmAtonValue structures.
// RPMTAG_NAME will yield the name.  The type for each metric is stored and
// saved in metrics.

typedef struct cache_entry {
    pmAtomValue entry[(RPMTAG_NVRA - RPMTAG_NAME) + 1 +
		      (RPMTAG_FIRSTFREE_TAG - RPMTAG_FILENAMES)];
    struct cache_entry *next;
} cache_entry;
cache_entry *cache;
cache_entry *current_cache_entry;

// To load the instances
static pthread_t indom_thread;
// To notice when the rpm database changes
static pthread_t inotify_thread;
static long numrefresh = 0;

// hash table info used by hsearch_r

#define HASH_HBOUND 5000
struct hsearch_data htab;

pthread_rwlock_t indom_lock;

// Load the instances dynamically
pmdaIndom indomtab[] = {
#define RPM_INDOM	0
    {RPM_INDOM, 0, NULL},
};
static pmInDom *rpm_indom = &indomtab[0].it_indom;

// Invoked as a .so or as a daemon?
static int isDSO = 1;
static char *username;

#define RPM_BUFSIZE		256

static char mypath[MAXPATHLEN];

/*
 * Callback provided to pmdaFetch to fetch values from rpm db corresponding to metric_querytags
 */

static int
rpm_fetchCallBack(pmdaMetric * mdesc, unsigned int inst,
		  pmAtomValue * atom)
{
    int ret_type;
    ENTRY e,
    *ep;
    __pmID_int *idp = (__pmID_int *) & (mdesc->m_desc.pmid);
    pmAtomValue this_atom;
    int type;
    int tag;
    // Get the instance name for this instance
    char *rpm_inst_name;
    int cacheidx = pmdaCacheLookup(*rpm_indom, inst, &rpm_inst_name, NULL);
    if (cacheidx < 0) {
	__pmNotifyErr(LOG_ERR, "pmdaCacheLookup failed: inst=%d: %s", inst,
		      pmErrStr(cacheidx));
	return PM_ERR_INST;
    }

    if (idp->cluster == 0) {
	if (idp->item == 0) {	/* rpm.refresh.count */
	    atom->ll = numrefresh;
	    return PMDA_FETCH_STATIC;
//	} else if (idp->item == 1) { /* rpm.refresh.time */
	    double usr,
	     sys;
	    __pmProcessRunTimes(&usr, &sys);
	    atom->d = usr + sys;
	    return PMDA_FETCH_STATIC;
	}
    }

    type = ((struct metrics *) mdesc->m_user)->type;
    tag = ((struct metrics *) mdesc->m_user)->tag;
    int metric_idx;	// index of this rpms' metric
    for (metric_idx = 0;
	 metric_idx < (sizeof(metrics) / sizeof(struct metrics));
	 metric_idx++)
	if (metrics[metric_idx].tag == tag)
	    break;

    // search for this rpm
    pthread_rwlock_rdlock(&indom_lock);
    e.key = rpm_inst_name;
    e.data = NULL;
    if (hsearch_r(e, FIND, &ep, &htab) == 0)
	return PM_ERR_INST;

    // the hash table points to the metrics for this rpm
    this_atom = ((cache_entry *) (ep->data))->entry[metric_idx];

    switch (type) {
    case PM_TYPE_STRING:
	atom->cp = this_atom.cp;
	ret_type = PMDA_FETCH_DYNAMIC;
	break;
    case PM_TYPE_32:
	atom->l = this_atom.l;
	ret_type = PMDA_FETCH_STATIC;
	break;
    case PM_TYPE_64:
	atom->ll = this_atom.ll;
	ret_type = PMDA_FETCH_STATIC;
	break;
    }

    pthread_rwlock_unlock(&indom_lock);

    return ret_type;
}

/*
 * Called once for each pmFetch(3) operation.  Join the indom loading thread here.
 */

static int
rpm_fetch(int numpmid, pmID pmidlist[], pmResult ** resp, pmdaExt * pmda)
{
    pthread_join(indom_thread, NULL);
    return pmdaFetch(numpmid, pmidlist, resp, pmda);
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
    double usr1,
     usr2,
     sys1,
     sys2;

    numrefresh++;

    __pmProcessRunTimes(&usr1, &sys1);
    pthread_rwlock_wrlock(&indom_lock);

    td = rpmtdNew();
    ts = rpmtsCreate();

    rpmReadConfigFiles(NULL, NULL);

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
	    ENTRY e,
	    *ep;
	    e.key = rpmname;
	    e.data = 0;
	    // If we already have this rpm then grab the next one
	    // ?? we need to check the rpm version and reload if it is a newer rpm version
	    if (hsearch_r(e, FIND, &ep, &htab) != 0)
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
			return -ENOMEM;
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
		    char *ccecp;
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
			} else {
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
		    /*
		     * printf 
		     */
		    /*
		     * ("STRING_ARRAY count=%d[%#lx][%d]=%s/%s\n", 
		     */
		    /*
		     * td->count, current_cache_entry, i, 
		     */
		    /*
		     * current_cache_entry->entry[i].cp, ccecp); 
		     */
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
	    }
	    rpmtdReset(td);
	}
	// headerFree(h);
    }
    rpmdbFreeIterator(mi);
    rpmtsFree(ts);

    ENTRY e, *ep;
    int code;
    // run through the cache and load the hash table
    for (this_cache_entry = cache; this_cache_entry != NULL;
	 this_cache_entry = this_cache_entry->next) {
	e.key = this_cache_entry->entry[16].cp;
	if (!e.key)
	    continue;
	e.data = this_cache_entry;
	if (hsearch_r(e, ENTER, &ep, &htab) == 0)
	    return PM_ERR_INST;
    }

    __pmProcessRunTimes(&usr2, &sys2);
    __pmNotifyErr(LOG_NOTICE, "user/%g sys/%g", (usr2 - usr1) / 100,
		  (sys2 - sys1) / 100);

    pthread_rwlock_unlock(&indom_lock);
    return 0;
}


/*
 * Load the rpm module names into the instance table
 */

void *
rpm_update_indom(void *ptr)
{
    int sts;
    cache_entry *this_cache_entry;
    double usr1,
     usr2,
     sys1,
     sys2;

    numrefresh++;

    // For rebuilding the hash table
    hdestroy_r(&htab);
    memset((void *) &htab, 0, sizeof(htab));
    hcreate_r(HASH_HBOUND, &htab);

    rpm_update_cache(NULL);

    __pmProcessRunTimes(&usr1, &sys1);
    pthread_rwlock_wrlock(&indom_lock);
    sts = pmdaCacheOp(*rpm_indom, PMDA_CACHE_INACTIVE);
    if (sts < 0)
	__pmNotifyErr(LOG_ERR,
		      "pmdaCacheOp(INACTIVE) failed: indom=%s: %s",
		      pmInDomStr(*rpm_indom), pmErrStr(sts));

    // Iterate through the entire list of rpms.  The pmda cache are the rpm module names

    for (this_cache_entry = cache; this_cache_entry != NULL;
	 this_cache_entry = this_cache_entry->next) {
	ENTRY e;
	e.key = this_cache_entry->entry[16].cp;
	if (!e.key)
	    continue;
	sts =
	    pmdaCacheStore(*rpm_indom, PMDA_CACHE_ADD, e.key,
			   (void *) e.data);
	if (sts < 0)
	    __pmNotifyErr(LOG_ERR, "pmdaCacheStore failed: indom=%s: %s",
			  pmInDomStr(*rpm_indom), pmErrStr(sts));
    }

    __pmProcessRunTimes(&usr2, &sys2);
    __pmNotifyErr(LOG_NOTICE, "user/%g sys/%g", (usr2 - usr1) / 100,
		  (sys2 - sys1) / 100);
    pthread_rwlock_unlock(&indom_lock);

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
    inotify_add_watch(fd, "/var/lib/rpm/", IN_CLOSE_WRITE);

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
	pmdaDSO(dp, PMDA_INTERFACE_5, "rpm DSO", mypath);
    } else {
	__pmSetProcessIdentity(username);
    }

    if (dp->status != 0)
	return;

    // Load rpms into instance table
    pthread_create(&indom_thread, NULL, rpm_update_indom, NULL);
    // Note changes to the rpm database
    pthread_create(&inotify_thread, NULL, rpm_inotify, NULL);

    dp->version.any.fetch = rpm_fetch;

    pmdaSetFetchCallBack(dp, rpm_fetchCallBack);


    pmdaInit(dp, indomtab, 1, metrictab,
	     sizeof(metrictab) / sizeof(metrictab[0]));

    if (dp->status != 0)
	__pmNotifyErr(LOG_ERR, "pmdaInit failed %d", dp->status);
}

static void
usage(void)
{
    fprintf(stderr, "Usage: %s [options]\n\n", pmProgname);
    fputs("Options:\n"
	  "  -d domain    use domain (numeric) for metrics domain of PMDA\n"
	  "  -l logfile   write log into logfile rather than using default log name\n"
	  "  -U username  user account to run under (default \"pcp\")\n"
	  "\nExactly one of the following options may appear:\n"
	  "  -i port      expect PMCD to connect on given inet port (number or name)\n"
	  "  -p           expect PMCD to supply stdin/stdout (pipe)\n"
	  "  -u socket    expect PMCD to connect on given unix domain socket\n"
	  "  -6 port      expect PMCD to connect on given ipv6 port (number or name)\n",
	  stderr);
    exit(1);
}

/*
 * Set up the agent if running as a daemon.
 */

int
main(int argc, char **argv)
{
    int c,
     err = 0;
    int sep = __pmPathSeparator();
    pmdaInterface dispatch;

    isDSO = 0;
    __pmSetProgname(argv[0]);
    __pmGetUsername(&username);

    snprintf(mypath, sizeof(mypath), "%s%c" "rpm" "%c" "help",
	     pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_5, pmProgname, RPM,
	       "rpm.log", mypath);

    while ((c =
	    pmdaGetOpt(argc, argv, "D:d:i:l:pu:6:U:?", &dispatch,
		       &err)) != EOF) {
	switch (c) {
	case 'U':
	    username = optarg;
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
