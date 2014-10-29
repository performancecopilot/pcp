#include "pmapi.h"
#include "impl.h"
#include "pmda.h"

#include "clusters.h"

#include <ctype.h>

typedef struct {
    int	    item;
    int	    cluster;
    char    *name;
} dynproc_metric_t;

static __pmnsTree *dynamic_proc_tree;

enum {
    DYNPROC_GROUP_SCHEDSTAT = 0,
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

//static dynproc_metric_t psinfo_metrics[] = {
//    {	.name = "pid",		.cluster = 8,	    .item = 0	},

static dynproc_metric_t schedstat_metrics[] = {
	{ .name = "cpu_time",	.cluster = 31,	.item=0 },
	{ .name = "run_delay",	.cluster = 31,	.item=1 },
	{ .name = "pcount",	.cluster = 31,  .item=2 },
};

static dynproc_group_t dynproc_groups[] = {
//	[DYNPROC_GROUP_PSINFO]    = "psinfo",
//	[DYNPROC_GROUP_ID]	  = "id",
//	[DYNPROC_GROUP_MEMORY]    = "memory",
//	[DYNPROC_GROUP_IO]	  = "io",
//	[DYNPROC_GROUP_FD]	  = "fd",
	[DYNPROC_GROUP_SCHEDSTAT] = { .name = "schedstat",   .metrics = schedstat_metrics,  .nmetrics = sizeof(schedstat_metrics)/sizeof(dynproc_metric_t) },
};

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

    /* Instead of this do the cluster num change */
    /* dest->m_desc.pmid = pmid_build(domain, cluster, id); */

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

    *total = 3; /* will be calc based on all the structs above.  Total number of leaf nodes right ??? */
    *trees = 1; /* will be 2 for proc and hotproc */

    if (pmDebug & DBG_TRACE_LIBPMDA)
        fprintf(stderr, "interrupts size_metrictable: %d total x %d trees\n",
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

    /*
    switch (cluster) {
        case CLUSTER_INTERRUPT_LINES:
            if (item > lines_count)
                return PM_ERR_PMID;
            if (interrupt_lines[item].text == NULL)
                return PM_ERR_TEXT;
            *buf = interrupt_lines[item].text;
            return 0;
        case CLUSTER_INTERRUPT_OTHER:
            if (item > other_count)
                return PM_ERR_PMID;
            if (interrupt_other[item].text == NULL)
                return PM_ERR_TEXT;
            *buf = interrupt_other[item].text;
            return 0;
    }
    return PM_ERR_PMID;
    */
}


/*
 * Build up the dynamic infrastructure. Call pmdaDynamicPMNS for each unique [hot]proc.foo<CLUSTER>
 * grouping.
 *
 * i.e - psinfo will have 8 calls.
 *
 * But share everything else as much as possible.
 *
 */

void
proc_dynamic_init(pmdaMetric *metrics, int nmetrics)
{
    int set[] = { CLUSTER_PID_SCHEDSTAT };

    fprintf(stderr, "proc_dynamic_init\n");

    // Loop over dynproc_groups ??? instead
    // inner loop of clusters in each group.  indexd by group into table of clusters?
    // index of group gives you pointer to a group that holds clusters for that group
    pmdaDynamicPMNS("proc.schedstat",
                    set, sizeof(set)/sizeof(int),
                    refresh_dynamic_proc, dynamic_proc_text,
                    refresh_metrictable, size_metrictable,
                    metrics, nmetrics);
}

