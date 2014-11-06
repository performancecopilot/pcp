#include "pmapi.h"
#include "impl.h"
#include "pmda.h"

#include "clusters.h"

#include <ctype.h>

#define MAX_CLUSTERS 64

typedef struct {
    int	    item;
    int	    cluster;
    char    *name;
} dynproc_metric_t;

static __pmnsTree *dynamic_proc_tree;

enum {
    DYNPROC_GROUP_PSINFO = 0,
    DYNPROC_GROUP_FD = 1,
    DYNPROC_GROUP_ID = 2,
    DYNPROC_GROUP_MEMORY = 3,
    DYNPROC_GROUP_IO = 4,
    DYNPROC_GROUP_SCHEDSTAT = 5,
    NUM_DYNPROC_GROUPS
};

enum {
    DYNPROC_PROC = 0,
    NUM_DYNPROC_TREES
};

typedef struct {
    char		*name;
    dynproc_metric_t	*metrics;
    int			nmetrics;
} dynproc_group_t;


static const char *dynproc_members[] = {
	[DYNPROC_PROC]	    = "proc",
	//[DYNPROC_HOTPROC]   = "hotproc",
};

static dynproc_metric_t psinfo_metrics[] = {
	{ .name = "pid",	    .cluster = CLUSTER_PID_STAT,	.item=0 },
	{ .name = "cmd",	    .cluster = CLUSTER_PID_STAT,	.item=1 },
	{ .name = "sname",	    .cluster = CLUSTER_PID_STAT,	.item=2 },
	{ .name = "ppid",	    .cluster = CLUSTER_PID_STAT,	.item=3 },
	{ .name = "pgrp",	    .cluster = CLUSTER_PID_STAT,	.item=4 },
	{ .name = "session",	    .cluster = CLUSTER_PID_STAT,	.item=5 },
	{ .name = "tty",	    .cluster = CLUSTER_PID_STAT,	.item=6 },
	{ .name = "tty_pgrp",	    .cluster = CLUSTER_PID_STAT,	.item=7 },
	{ .name = "flags",	    .cluster = CLUSTER_PID_STAT,	.item=8 },
	{ .name = "minflt",	    .cluster = CLUSTER_PID_STAT,	.item=9 },
	{ .name = "cmin_flt",	    .cluster = CLUSTER_PID_STAT,	.item=10 },
	{ .name = "maj_flt",	    .cluster = CLUSTER_PID_STAT,	.item=11 },
	{ .name = "cmaj_flt",	    .cluster = CLUSTER_PID_STAT,	.item=12 },
	{ .name = "utime",	    .cluster = CLUSTER_PID_STAT,	.item=13 },
	{ .name = "stime",	    .cluster = CLUSTER_PID_STAT,	.item=14 },
	{ .name = "cutime",	    .cluster = CLUSTER_PID_STAT,	.item=15 },
	{ .name = "cstime",	    .cluster = CLUSTER_PID_STAT,	.item=16 },
	{ .name = "priority",	    .cluster = CLUSTER_PID_STAT,	.item=17 },
	{ .name = "nice",	    .cluster = CLUSTER_PID_STAT,	.item=18 },
	{ .name = "it_real_value",  .cluster = CLUSTER_PID_STAT,	.item=20 },
	{ .name = "start_time",	    .cluster = CLUSTER_PID_STAT,	.item=21 },
	{ .name = "vsize",	    .cluster = CLUSTER_PID_STAT,	.item=22 },
	{ .name = "rss",	    .cluster = CLUSTER_PID_STAT,	.item=23 },
	{ .name = "rss_rlim",	    .cluster = CLUSTER_PID_STAT,	.item=24 },
	{ .name = "start_code",	    .cluster = CLUSTER_PID_STAT,	.item=25 },
	{ .name = "end_code",	    .cluster = CLUSTER_PID_STAT,	.item=26 },
	{ .name = "start_stack",    .cluster = CLUSTER_PID_STAT,	.item=27 },
	{ .name = "esp",	    .cluster = CLUSTER_PID_STAT,	.item=28 },
	{ .name = "eip",	    .cluster = CLUSTER_PID_STAT,	.item=29 },
	{ .name = "signal",	    .cluster = CLUSTER_PID_STAT,	.item=30 },
	{ .name = "blocked",	    .cluster = CLUSTER_PID_STAT,	.item=31 },
	{ .name = "sigignore",	    .cluster = CLUSTER_PID_STAT,	.item=32 },
	{ .name = "sigcatch",	    .cluster = CLUSTER_PID_STAT,	.item=33 },
	{ .name = "wchan",	    .cluster = CLUSTER_PID_STAT,	.item=34 },
	{ .name = "nswap",	    .cluster = CLUSTER_PID_STAT,	.item=35 },
	{ .name = "cnswap",	    .cluster = CLUSTER_PID_STAT,	.item=36 },
	{ .name = "exit_signal",    .cluster = CLUSTER_PID_STAT,	.item=37 },
	{ .name = "processor",	    .cluster = CLUSTER_PID_STAT,	.item=38 },
	{ .name = "ttyname",	    .cluster = CLUSTER_PID_STAT,	.item=39 },
	{ .name = "wchan_s",	    .cluster = CLUSTER_PID_STAT,	.item=40 },
	{ .name = "psargs",	    .cluster = CLUSTER_PID_STAT,	.item=41 },
	{ .name = "rt_priority",    .cluster = CLUSTER_PID_STAT,	.item=42 },
	{ .name = "policy",	    .cluster = CLUSTER_PID_STAT,	.item=43 },
	{ .name = "delayacct_blkio_time",   .cluster = CLUSTER_PID_STAT,.item=44 },
	{ .name = "guest_time",	    .cluster = CLUSTER_PID_STAT,	.item=45 },
	{ .name = "cguest_time",    .cluster = CLUSTER_PID_STAT,	.item=46 },
	{ .name = "signal_s",	    .cluster = CLUSTER_PID_STATUS,	.item=16 },
	{ .name = "blocked_s",	    .cluster = CLUSTER_PID_STATUS,	.item=17 },
	{ .name = "sigignore_s",    .cluster = CLUSTER_PID_STATUS,	.item=18 },
	{ .name = "sigcatch_s",	    .cluster = CLUSTER_PID_STATUS,	.item=19 },
	{ .name = "threads",	    .cluster = CLUSTER_PID_STATUS,	.item=28 },
	{ .name = "cgroups",	    .cluster = CLUSTER_PID_CGROUP,	.item=0 },
	{ .name = "labels",	    .cluster = CLUSTER_PID_LABEL,	.item=0 },
	{ .name = "vctxsw",	    .cluster = CLUSTER_PID_STATUS,	.item=29 },
	{ .name = "nvctxsw",	    .cluster = CLUSTER_PID_STATUS,	.item=30 },
};

static dynproc_metric_t id_metrics[] = {
        { .name = "uid",	.cluster = CLUSTER_PID_STATUS,  .item=0 },
        { .name = "euid",	.cluster = CLUSTER_PID_STATUS,  .item=1 },
        { .name = "suid",	.cluster = CLUSTER_PID_STATUS,  .item=2 },
        { .name = "fsuid",	.cluster = CLUSTER_PID_STATUS,  .item=3 },
        { .name = "gid",	.cluster = CLUSTER_PID_STATUS,  .item=4 },
        { .name = "egid",	.cluster = CLUSTER_PID_STATUS,  .item=5 },
        { .name = "sgid",	.cluster = CLUSTER_PID_STATUS,  .item=6 },
        { .name = "fsgid",	.cluster = CLUSTER_PID_STATUS,  .item=7 },
        { .name = "uid_nm",	.cluster = CLUSTER_PID_STATUS,  .item=8 },
        { .name = "euid_nm",	.cluster = CLUSTER_PID_STATUS,  .item=9 },
        { .name = "suid_nm",	.cluster = CLUSTER_PID_STATUS,  .item=10 },
        { .name = "fsuid_nm",   .cluster = CLUSTER_PID_STATUS,  .item=11 },
        { .name = "gid_nm",	.cluster = CLUSTER_PID_STATUS,  .item=12 },
        { .name = "egid_nm",	.cluster = CLUSTER_PID_STATUS,  .item=13 },
        { .name = "sgid_nm",	.cluster = CLUSTER_PID_STATUS,  .item=14 },
        { .name = "fsgid_nm",   .cluster = CLUSTER_PID_STATUS,  .item=15 },
};

static dynproc_metric_t memory_metrics[] = {
        { .name = "size",   .cluster = CLUSTER_PID_STATM,  .item=0 },
        { .name = "rss",    .cluster = CLUSTER_PID_STATM,  .item=1 },
        { .name = "share",  .cluster = CLUSTER_PID_STATM,  .item=2 },
        { .name = "textrss",.cluster = CLUSTER_PID_STATM,  .item=3 },
        { .name = "librss", .cluster = CLUSTER_PID_STATM,  .item=4 },
        { .name = "datrss", .cluster = CLUSTER_PID_STATM,  .item=5 },
        { .name = "dirty",  .cluster = CLUSTER_PID_STATM,  .item=6 },
        { .name = "maps",   .cluster = CLUSTER_PID_STATM,  .item=7 },
        { .name = "vmsize", .cluster = CLUSTER_PID_STATUS,  .item=20 },
        { .name = "vmlock", .cluster = CLUSTER_PID_STATUS,  .item=21 },
        { .name = "vmrss",  .cluster = CLUSTER_PID_STATUS,  .item=22 },
        { .name = "vmdata", .cluster = CLUSTER_PID_STATUS,  .item=23 },
        { .name = "vmstack",.cluster = CLUSTER_PID_STATUS,  .item=24 },
        { .name = "vmexe",  .cluster = CLUSTER_PID_STATUS,  .item=25 },
        { .name = "vmlib",  .cluster = CLUSTER_PID_STATUS,  .item=26 },
        { .name = "vmswap", .cluster = CLUSTER_PID_STATUS,  .item=27 },
};

static dynproc_metric_t io_metrics[] = {
        { .name = "rchar",		    .cluster = CLUSTER_PID_IO,  .item=0 },
        { .name = "wchar",		    .cluster = CLUSTER_PID_IO,  .item=1 },
        { .name = "syscr",		    .cluster = CLUSTER_PID_IO,  .item=2 },
        { .name = "syscw",		    .cluster = CLUSTER_PID_IO,  .item=3 },
        { .name = "read_bytes",		    .cluster = CLUSTER_PID_IO,  .item=4 },
        { .name = "write_bytes",	    .cluster = CLUSTER_PID_IO,  .item=5 },
        { .name = "cancelled_write_bytes",  .cluster = CLUSTER_PID_IO,  .item=6 },
};

static dynproc_metric_t fd_metrics[] = {
        { .name = "count",   .cluster = 51,  .item=0 },
};

static dynproc_metric_t schedstat_metrics[] = {
	{ .name = "cpu_time",	.cluster = CLUSTER_PID_SCHEDSTAT,	.item=0 },
	{ .name = "run_delay",	.cluster = CLUSTER_PID_SCHEDSTAT,	.item=1 },
	{ .name = "pcount",	.cluster = CLUSTER_PID_SCHEDSTAT,	.item=2 },
};

static dynproc_group_t dynproc_groups[] = {
	[DYNPROC_GROUP_PSINFO]    = { .name = "psinfo",	    .metrics = psinfo_metrics,	    .nmetrics = sizeof(psinfo_metrics)/sizeof(dynproc_metric_t)},
	[DYNPROC_GROUP_ID]	  = { .name = "id",	    .metrics = id_metrics,	    .nmetrics = sizeof(id_metrics)/sizeof(dynproc_metric_t)},
	[DYNPROC_GROUP_MEMORY]    = { .name = "memory",	    .metrics = memory_metrics,	    .nmetrics = sizeof(memory_metrics)/sizeof(dynproc_metric_t)},
	[DYNPROC_GROUP_IO]	  = { .name = "io",	    .metrics = io_metrics,	    .nmetrics = sizeof(io_metrics)/sizeof(dynproc_metric_t)},
	[DYNPROC_GROUP_FD]	  = { .name = "fd",	    .metrics = fd_metrics,	    .nmetrics = sizeof(fd_metrics)/sizeof(dynproc_metric_t)},
	[DYNPROC_GROUP_SCHEDSTAT] = { .name = "schedstat",  .metrics = schedstat_metrics,   .nmetrics = sizeof(schedstat_metrics)/sizeof(dynproc_metric_t) },
};

/*
 * Utility function to give a set of clusters used by a dynproc_metric_t array
 */

int
get_clusters_used( dynproc_group_t dyngroup, int *clusters ){

    int numclusters = 0;
    int j, i;

    for(i=0; i< dyngroup.nmetrics; i++){
	int curcluster = dyngroup.metrics[i].cluster;
	int skip = 0;
	for(j=0; j<numclusters;j++){
	    if( clusters[j] == curcluster ){
		skip = 1;
		break; //already collected this one
	    }
	}  
	if( !skip ){
	    clusters[numclusters] = curcluster;
	    numclusters++;
	    if( numclusters == MAX_CLUSTERS ){
		fprintf(stderr, "Increase MAX_CLUSTERS in proc_dynamic.  Data is missing\n");
		return numclusters;
	    }
	} 

    }
    return numclusters;
}

/*
 * Map proc cluster id's to new hotproc varients that don't conflict
 */
/*
static int
get_hotproc_cluster( int cluster )
{
    switch(cluster){
        case CLUSTER_PID_STAT:
                return CLUSTER_HOTPROC_PID_STAT;
        case CLUSTER_PID_STATM:
                return CLUSTER_HOTPROC_PID_STATM;
        case CLUSTER_PID_CGROUP:
                return CLUSTER_HOTPROC_PID_CGROUP;
        case CLUSTER_PID_LABEL:
                return CLUSTER_HOTPROC_PID_LABEL;
        case CLUSTER_PID_STATUS:
                return CLUSTER_HOTPROC_PID_STATUS;
        case CLUSTER_PID_SCHEDSTAT:
                return CLUSTER_HOTPROC_PID_SCHEDSTAT;
        case CLUSTER_PID_IO:
                return CLUSTER_HOTPROC_PID_IO;
        case CLUSTER_PID_FD:
                return CLUSTER_HOTPROC_PID_FD;
        default:
                return -1;
        }
}
*/

static void
build_dynamic_proc_tree( int domain )
{

    fprintf(stderr, "proc_dynamic_buildtree\n");

    char entry[128];
    pmID pmid;
    //int hotcluster = -1;

    unsigned int tree, group, metric;

    int num_dynproc_trees = sizeof(dynproc_members)/sizeof(char*);
    int num_dynproc_groups = sizeof(dynproc_groups)/sizeof(dynproc_group_t);

    fprintf(stderr, "trees, groups: %d, %d\n", num_dynproc_trees, num_dynproc_groups);

    for( tree = 0; tree < num_dynproc_trees; tree++){
	for( group = 0; group < num_dynproc_groups; group++ ){

	    dynproc_metric_t * cur_metrics = dynproc_groups[group].metrics;
	    int num_cur_metrics = dynproc_groups[group].nmetrics;

	    fprintf(stderr, "metrics: %d\n", num_cur_metrics);

	    for( metric = 0; metric < num_cur_metrics; metric++){

		snprintf(entry, sizeof(entry), "%s.%s.%s", dynproc_members[tree], dynproc_groups[group].name, cur_metrics[metric].name);

		fprintf(stderr, "Adding: %s\n", entry);

		int cluster =  cur_metrics[metric].cluster;
		int item =  cur_metrics[metric].item;

		pmid = pmid_build(domain, cluster, item);
		__pmAddPMNSNode(dynamic_proc_tree, pmid, entry);

		/* This should always be true */
		//if( (hotcluster = get_hotproc_cluster( cluster )) != -1  ){
		    //pmid = pmid_build(domain, hotcluster, item);
		    //__pmAddPMNSNode(dynamic_proc_tree, pmid, entry);
		 //   }
		//else{
		 //   fprintf(stderr, "Got non hotproc member while building tree: %d %d %d\n", domain, cluster, item);
		//}
	    }
	}
    }

    fprintf(stderr, "build tree end\n");

}

/*
 * Create a new metric table entry based on an existing one.
 * Will use the templates we have in pmda.c, modifying cluster values
 * In this case we assume the only 2 metric groups are proc and hotproc
 *
 * I assume id=0 is proc and id=1 is hotproc.
 *
 * This should be pretty simple.  Only metrics that are supposed to
 * be dynamic should flow thorugh here (correct?) so we don't do any checking
 *
 */
static void
refresh_metrictable(pmdaMetric *source, pmdaMetric *dest, int id)
{

    fprintf(stderr, "proc_dynamic_refresh_metrictable\n");

    int domain = pmid_domain(source->m_desc.pmid);
    int cluster = pmid_cluster(source->m_desc.pmid);
    //int hotcluster = -1;

    memcpy(dest, source, sizeof(pmdaMetric));


    /*

    if( id == 1 ){
	hotcluster = get_hotproc_cluster(cluster);
	if( hotcluster != -1 ){
	    dest->m_desc.pmid = pmid_build(domain, hotcluster, id);
	}
	else{
	    fprintf(stderr, "Got bad hotproc cluster for %d %d %d\n", domain, cluster, id);
	}
    }

    */

        fprintf(stderr, "dynamic_proc refresh_metrictable: (%p -> %p) "
                        "metric ID dup: %d.%d.%d -> %d.%d.%d\n",
                source, dest, domain, cluster,
                pmid_item(source->m_desc.pmid), domain, cluster, id);

}


/* Add the pmns entries based on appropriate cluster and namespace information 
 *
 * A little more complicated. Need to build up the full tree from all the structs at the top.
 *
 */

static int
refresh_dynamic_proc(pmdaExt *pmda, __pmnsTree **tree)
{

    fprintf(stderr, "proc_dynamic_refresh\n");

    int sts, dom = pmda->e_domain;

    if (dynamic_proc_tree) {
        *tree = dynamic_proc_tree;
    } else if ((sts = __pmNewPMNS(&dynamic_proc_tree)) < 0) {
        __pmNotifyErr(LOG_ERR, "%s: failed to create dynamic_proc names: %s\n",
                        pmProgname, pmErrStr(sts));
        *tree = NULL;
    } else {
	/* Call something that constructs the PMNS by multiple calls to __pmAddPMNSNode */
	build_dynamic_proc_tree(dom);
	*tree = dynamic_proc_tree;
        return 1;
    }
    return 0;

}

/* 
 * I think this is the total number of entries, not just the additional
 * after the first set.
 */

static void
size_metrictable(int *total, int *trees)
{

    fprintf(stderr, "proc_dynamic_size\n");

    int i, num_leaf_nodes = 0;

    int num_dyn_groups =  sizeof(dynproc_groups)/sizeof(dynproc_group_t);

    for( i = 0; i < num_dyn_groups; i++){
	num_leaf_nodes += dynproc_groups[i].nmetrics;
    }

    *total = num_leaf_nodes; /* calc based on all the structs above.  Total number of leaf nodes right ??? */
    *trees = sizeof(dynproc_members)/sizeof(char*); /* will be 2 for proc and hotproc */

    if (pmDebug & DBG_TRACE_LIBPMDA)
        printf(stderr, "interrupts size_metrictable: %d total x %d trees\n",
                *total, *trees);
}

char *dummy_text;

static int
dynamic_proc_text(pmdaExt *pmda, pmID pmid, int type, char **buf)
{
    fprintf(stderr, "proc_dynamic_text\n");

    //int item = pmid_item(pmid);
    //int cluster = pmid_cluster(pmid);

    static int started = 0;

    if (!started){
	dummy_text = strdup("dummy help text");
	started = 1;
    }

    *buf = dummy_text;
    return 0;

}


/*
 * Build up the dynamic infrastructure. Call pmdaDynamicPMNS for each unique [hot]proc.foo
 * grouping.
 *
 * But share everything else as much as possible.
 *
 */

void
proc_dynamic_init(pmdaMetric *metrics, int nmetrics)
{
    //int set[] = { CLUSTER_PID_SCHEDSTAT };

    int clusters[MAX_CLUSTERS];
    int nclusters;

    char treename[128];

    fprintf(stderr, "proc_dynamic_init\n");

    int i,j;
    int num_dyngroups = sizeof(dynproc_groups)/sizeof(dynproc_group_t);
    int num_dyntrees = sizeof(dynproc_members)/sizeof(char*);

    for(i=0; i< num_dyngroups; i++){

	nclusters = get_clusters_used( dynproc_groups[i], clusters );

	for(j=0; j< num_dyntrees; j++){

	    sprintf(treename, "%s.%s", dynproc_members[j], dynproc_groups[i].name);
	    fprintf(stderr, "Adding tree: %s, with %d clusters\n", treename,nclusters);

	    // Should the strdup/memcpy be inside pmdaDynamicPMNS? currently it just assignes the pointer, assuming a string literal or other static type.
	    pmdaDynamicPMNS(strdup(treename),
                    clusters, nclusters,
                    refresh_dynamic_proc, dynamic_proc_text,
                    refresh_metrictable, size_metrictable,
                    metrics, nmetrics);
	}
    }
}

