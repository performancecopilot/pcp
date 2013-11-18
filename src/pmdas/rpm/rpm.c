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

// corresponds to metrictab to give the tag rpm uses to fetch the given value

static int metric_tags[] = {
    /* arch */	    	RPMTAG_ARCH,
    /* basenames */	RPMTAG_BASENAMES,
    /* buildtime */    	RPMTAG_BUILDTIME,
    /* description */	RPMTAG_DESCRIPTION,
    /* dirnames */	RPMTAG_DIRNAMES,
    /* distribution */	RPMTAG_DISTRIBUTION,
    /* evr */    	RPMTAG_EVR,
    /* file.class */    RPMTAG_FILECLASS,
    /* file.linktos */  RPMTAG_FILELINKTOS,
    /* file.md5s */    	RPMTAG_FILEMD5S,
    /* file.modes */    RPMTAG_FILEMODES,
    /* file.names */    RPMTAG_FILENAMES,
    /* file.require */  RPMTAG_FILEREQUIRE,
    /* file.sizes */    RPMTAG_FILESIZES,
    /* group */    	RPMTAG_GROUP,
    /* license */    	RPMTAG_LICENSE,
    /* name */      	RPMTAG_NAME,
    /* obsoletes */    	RPMTAG_OBSOLETES,
    /* packageid */	RPMTAG_PKGID,
    /* platform */    	RPMTAG_PLATFORM,
    /* provideversion */ RPMTAG_PROVIDEVERSION,
    /* provides */    	RPMTAG_PROVIDES,
    /* release */   	RPMTAG_RELEASE,
    /* requires */    	RPMTAG_REQUIRES,
    /* rpmversion */	RPMTAG_RPMVERSION,
    /* size */	    	RPMTAG_SIZE,
    /* sourcerpm */ 	RPMTAG_SOURCERPM,
    /* summary */	RPMTAG_SUMMARY,
    /* url */		RPMTAG_URL,
    /* version */	RPMTAG_VERSION,
};


#define METRICTAB_ENTRY(M,N,T) (&metric_tags[N]), { PMDA_PMID (M, N), T, 0, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },

static pmdaMetric metrictab[] = {
    /* refresh.count */
    {NULL, { PMDA_PMID (0, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },  },
    /* refresh.time */
    {NULL, { PMDA_PMID (0, 1), PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },  },
    /* arch */
    { METRICTAB_ENTRY (1, __COUNTER__, PM_TYPE_STRING) },
    /* basenames */
    { METRICTAB_ENTRY (1, __COUNTER__, PM_TYPE_STRING) },
    /* buildtime */
    { METRICTAB_ENTRY (1, __COUNTER__, PM_TYPE_U32) },
    /* description */
    { METRICTAB_ENTRY (1, __COUNTER__, PM_TYPE_STRING) },
    /* dirnames */
    { METRICTAB_ENTRY (1, __COUNTER__, PM_TYPE_STRING) },
    /* distribution */
    { METRICTAB_ENTRY (1, __COUNTER__, PM_TYPE_STRING) },
    /* evr */
    { METRICTAB_ENTRY (1, __COUNTER__, PM_TYPE_STRING) },
    /* file.class */
    { METRICTAB_ENTRY (2, __COUNTER__, PM_TYPE_STRING) },
    /* file.linktos */
    { METRICTAB_ENTRY (2, __COUNTER__, PM_TYPE_STRING) },
    /* file.md5s */
    { METRICTAB_ENTRY (2, __COUNTER__, PM_TYPE_STRING) },
    /* file.modes */
    { METRICTAB_ENTRY (2, __COUNTER__, PM_TYPE_STRING) },
    /* file.names */
    { METRICTAB_ENTRY (2, __COUNTER__, PM_TYPE_STRING) },
    /* file.require */
    { METRICTAB_ENTRY (2, __COUNTER__, PM_TYPE_STRING) },
    /* file.sizes */
    { METRICTAB_ENTRY (2, __COUNTER__, PM_TYPE_U32) },
    /* group */
    { METRICTAB_ENTRY (1, __COUNTER__, PM_TYPE_STRING) },
    /* license */
    { METRICTAB_ENTRY (1, __COUNTER__, PM_TYPE_STRING) },
    /* name */
    { METRICTAB_ENTRY (1, __COUNTER__, PM_TYPE_STRING) },
    /* obsoletes */
    { METRICTAB_ENTRY (1, __COUNTER__, PM_TYPE_STRING) },
    /* packageid */
    { METRICTAB_ENTRY (1, __COUNTER__, PM_TYPE_STRING) },
    /* platform */
    { METRICTAB_ENTRY (1, __COUNTER__, PM_TYPE_STRING) },
    /* provideversion */
    { METRICTAB_ENTRY (1, __COUNTER__, PM_TYPE_STRING) },
    /* provides */
    { METRICTAB_ENTRY (1, __COUNTER__, PM_TYPE_STRING) },
    /* release */
    { METRICTAB_ENTRY (1, __COUNTER__, PM_TYPE_STRING) },
    /* requires */
    { METRICTAB_ENTRY (1, __COUNTER__, PM_TYPE_STRING) },
    /* rpmversion */
    { METRICTAB_ENTRY (1, __COUNTER__, PM_TYPE_STRING) },
    /* size */
    { METRICTAB_ENTRY (1, __COUNTER__, PM_TYPE_U32) },
    /* sourcerpm */
    { METRICTAB_ENTRY (1, __COUNTER__, PM_TYPE_STRING) },
    /* summary */
    { METRICTAB_ENTRY (1, __COUNTER__, PM_TYPE_STRING) },
    /* url */
    { METRICTAB_ENTRY (1, __COUNTER__, PM_TYPE_STRING) },
    /* version */
    { METRICTAB_ENTRY (1, __COUNTER__, PM_TYPE_STRING) }
};


// To load the instances
static pthread_t indom_thread;
// To notice when the rpm database changes
static pthread_t inotify_thread;
static long numrefresh = 0;


pthread_rwlock_t indom_lock;

// Load the instances dynamically
pmdaIndom indomtab[] = {
#define RPM_INDOM	0
		{ RPM_INDOM, 0, NULL },
};
static pmInDom	*rpm_indom = &indomtab[0].it_indom;

// Invoked as a .so or as a daemon?
static int	isDSO = 1;
static char	*username;

#define RPM_BUFSIZE		256

static char	mypath[MAXPATHLEN];

/*
 * Callback provided to pmdaFetch to fetch values from rpm db corresponding to metric_querytags
 */

static int
rpm_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    static int is_rpm_init = 0;
    Header h;
    rpmdbMatchIterator mi;
    rpmtd tn;
    rpmts ts;
    int rpmtag = 0;
    int idx;

    __pmID_int		*idp = (__pmID_int *)&(mdesc->m_desc.pmid);
    if (idp->cluster == 0) {
    	if (idp->item == 0) {			/* rpm.refresh.count */
    		atom->ll = numrefresh;
    		return PMDA_FETCH_STATIC;
    	}
    	else if (idp->item== 1) {
    		double	usr, sys;
    		__pmProcessRunTimes(&usr, &sys);
    		atom->d = usr + sys;
    		return PMDA_FETCH_STATIC;
    	}
    }

    rpmtag = *((int*)mdesc->m_user);

    // Fire up rpm only the first time
    if (! is_rpm_init) {
    	rpmReadConfigFiles( NULL, NULL );
    	is_rpm_init = 1;
    }
    tn = rpmtdNew();
    ts = rpmtsCreate();

    // Get the instance name for this instance
    pthread_rwlock_rdlock(&indom_lock);
    char * rpm_inst_name;
    idx = pmdaCacheLookup(*rpm_indom, inst, &rpm_inst_name, NULL);
    if (idx < 0) {
    	__pmNotifyErr(LOG_ERR, "pmdaCacheLookup failed: inst=%d: %s", inst, pmErrStr(idx));
    	return PM_ERR_INST;
    }
    pthread_rwlock_unlock(&indom_lock);

    // Setup the rpm iterator for this instance
    mi = rpmtsInitIterator(ts, RPMTAG_NAME, rpm_inst_name, 0);

    int ret_type = PMDA_FETCH_STATIC;
    while (NULL != (h = rpmdbNextIterator(mi))) {
    	h = headerLink(h);
    	headerGet(h, rpmtag, tn, HEADERGET_EXT);
    	switch(tn->type) {
    	// The rpm value is a string
    	case RPM_STRING_TYPE:
    	{
    		if (mdesc->m_desc.type != PM_TYPE_STRING) {
    			__pmNotifyErr(LOG_ERR, "Expected string type %d got %d.", PM_TYPE_STRING, mdesc->m_desc.type);
    			return EINVAL;
    		}
    		const char* rpmval = rpmtdGetString(tn);
            if ((atom->cp = strdup(rpmval)) == NULL)
            	ret_type = -ENOMEM;
            else
            	ret_type = PMDA_FETCH_DYNAMIC;
    		break;
    	}
    	// The rpm value is an array of strings
    	case RPM_STRING_ARRAY_TYPE:
    	{
    		if (mdesc->m_desc.type != PM_TYPE_STRING) {
    			__pmNotifyErr(LOG_ERR, "Expected string type %d got %d.", PM_TYPE_STRING, mdesc->m_desc.type);
    			return EINVAL;
    		}
    		char ** strings;
    		strings = tn->data;
    		for (idx = 0; idx < tn->count; idx++) {
    			if (idx == 0) {
    				atom->cp = malloc(strlen(strings[idx]) + 1);
    				strcpy(atom->cp, strings[idx]);
    			}
    			else {
    				atom->cp = realloc(atom->cp, strlen(atom->cp) + strlen(strings[idx]) + 2);
    				strcat (atom->cp, "\n");
    				strcat (atom->cp, strings[idx]);
    			}
    		}
    		free (tn->data);
    		ret_type = PMDA_FETCH_DYNAMIC;
    		break;
    	}
    	// The rpm value is an int
    	// ?? Handle array of ints
    	case RPM_INT8_TYPE:
    		if (mdesc->m_desc.type != PM_TYPE_U32) {
    			__pmNotifyErr(LOG_ERR, "Expected integer type %d got %d.", PM_TYPE_U32, mdesc->m_desc.type);
    			return EINVAL;
    		}
    		atom->l = ((char*)(tn->data))[0];
    		break;
    	case RPM_INT16_TYPE:
    		if (mdesc->m_desc.type != PM_TYPE_U32) {
    			__pmNotifyErr(LOG_ERR, "Expected integer type %d got %d.", PM_TYPE_U32, mdesc->m_desc.type);
    			return EINVAL;
    		}
    		atom->l = ((short*)(tn->data))[0];
    		break;
    	case RPM_INT32_TYPE:
    		if (mdesc->m_desc.type != PM_TYPE_U32) {
    			__pmNotifyErr(LOG_ERR, "Expected integer type %d got %d.", PM_TYPE_U32, mdesc->m_desc.type);
    			return EINVAL;
    		}
    		atom->l = ((int*)(tn->data))[0];
    		break;
    	case RPM_INT64_TYPE:
    		if (mdesc->m_desc.type != PM_TYPE_U64) {
    			__pmNotifyErr(LOG_ERR, "Expected integer type %d got %d.", PM_TYPE_U32, mdesc->m_desc.type);
    			return EINVAL;
    		}
    		atom->ll = ((long long*)(tn->data))[0];
    	}
        rpmtdReset(tn);
        headerFree(h);
    }

    rpmdbFreeIterator(mi);
    rpmtsFree(ts);

    return ret_type;
}

/*
 * Called once for each pmFetch(3) operation.  Join the indom loading thread here.
 */

static int
rpm_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
	__pmNotifyErr(LOG_NOTICE, "in rpm_fetch %d", numpmid);
	pthread_join (indom_thread, NULL);
    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}


/*
 * Load the rpm module names into the instance table
 */

void*
rpm_update_indom (void* ptr)
{
    int		sts;
    rpmts ts = NULL;
    Header h;
    rpmdbMatchIterator mi;
    rpmtd tn;

    numrefresh++;

    int cache_entry_count = pmdaCacheOp(*rpm_indom, PMDA_CACHE_SIZE_ACTIVE);

    sts = pmdaCacheOp(*rpm_indom, PMDA_CACHE_INACTIVE);
    if (sts < 0)
    	__pmNotifyErr(LOG_ERR, "pmdaCacheOp(INACTIVE) failed: indom=%s: %s",
			pmInDomStr(*rpm_indom), pmErrStr(sts));
    else
    __pmNotifyErr(LOG_INFO, "pmdaCacheOp(INACTIVE) success: indom=%s",
			pmInDomStr(*rpm_indom));

    tn = rpmtdNew();
    ts = rpmtsCreate();
    __pmNotifyErr(LOG_INFO, "rpm_update_indom hello1!");

    rpmReadConfigFiles( NULL, NULL );
    __pmNotifyErr(LOG_INFO, "rpm_update_indom hello2!");

    // Iterate through the entire list of rpms
    mi = rpmtsInitIterator( ts, RPMDBI_PACKAGES, NULL, 0);
    __pmNotifyErr(LOG_INFO, "rpm_update_indom hello3!");

    while ((h = rpmdbNextIterator(mi)) != NULL) {
    	h = headerLink(h);
    	headerGet(h, RPMTAG_NAME, tn, HEADERGET_EXT);
    	const char* rpm_name = rpmtdGetString(tn);

    	// If we are doing an initial load then do not bother looking up names
    	if (0 && cache_entry_count != 0) {
    		sts = pmdaCacheLookupName(*rpm_indom, rpm_name, NULL, NULL);
    		if (sts < 0)
    			__pmNotifyErr(LOG_ERR, "pmdaCacheLookupName failed: indom=%s: %s",
    					pmInDomStr(*rpm_indom), pmErrStr(sts));
    	}
    	pthread_rwlock_wrlock(&indom_lock);
    	sts = pmdaCacheStore(*rpm_indom, PMDA_CACHE_ADD, rpm_name, NULL);
    	if (sts < 0)
    		__pmNotifyErr(LOG_ERR, "pmdaCacheStore failed: indom=%s: %s",
    				pmInDomStr(*rpm_indom), pmErrStr(sts));
    	pthread_rwlock_unlock(&indom_lock);
    	rpmtdReset(tn);
    	headerFree(h);
    }
    __pmNotifyErr(LOG_INFO, "rpm_update_indom hello4!");
    sts = pmdaCacheLookupName(*rpm_indom, "gcc", NULL, NULL);
    __pmNotifyErr(LOG_NOTICE, "%d", sts);
    __pmNotifyErr(LOG_NOTICE, "updated %d instances from rpm database", pmdaCacheOp(*rpm_indom, PMDA_CACHE_SIZE_ACTIVE));

    rpmdbFreeIterator(mi);

	return NULL;
}

/*
 * Notice when the rpm database changes and reload the instances.
 */

void*
rpm_inotify (void* ptr)
{
#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define EVENT_BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )
	char buffer[EVENT_BUF_LEN];
	int fd;

	fd = inotify_init();

	// ?? parameterize the path, check return code
	inotify_add_watch( fd, "/var/lib/rpm/", IN_CLOSE_WRITE);
	while (1) {
		int i = 0;
		int read_count;
		int need_refresh = 0;
		// Wait for changes in the rpm database
		__pmNotifyErr(LOG_INFO, "rpm_inotify: awaiting...");
		read_count = read( fd, buffer, EVENT_BUF_LEN );
		__pmNotifyErr(LOG_INFO, "rpm_inotify: read_count=%d", read_count);
		while (i < read_count ) {
			struct inotify_event *event = (struct inotify_event *) &buffer[ i ];
			if (event->mask & IN_CLOSE_WRITE)
				need_refresh++;
			i++;
		}
		__pmNotifyErr(LOG_INFO, "rpm_inotify: need_refresh=%d", need_refresh);
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
rpm_init(pmdaInterface *dp)
{
    if (isDSO) {
    	pmdaDSO(dp, PMDA_INTERFACE_5, "rpm DSO", mypath);
    }
    else {
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


    pmdaInit(dp, indomtab, 1, metrictab, sizeof(metrictab)/sizeof(metrictab[0]));

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
	int			c, err = 0;
    int			sep = __pmPathSeparator();
    pmdaInterface	dispatch;

    isDSO = 0;
    __pmSetProgname(argv[0]);
    __pmGetUsername(&username);

    snprintf(mypath, sizeof(mypath), "%s%c" "rpm" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_5, pmProgname, RPM,
		"rpm.log", mypath);

    while ((c = pmdaGetOpt(argc, argv, "D:d:i:l:pu:6:U:?", &dispatch, &err)) != EOF) {
    	switch(c) {
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
