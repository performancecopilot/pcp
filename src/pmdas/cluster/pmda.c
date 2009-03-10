/*
 * Cluster PMDA
 *
 * Copyright (c) 2008-2009 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <stdio.h>
#include <unistd.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/pmda.h>
#include "domain.h"
#include "cluster.h"

static char* localCtxIbStr = "PMDA_LOCAL_IB=";

static char		mypath[MAXPATHLEN];
extern void		cluster_main_loop(pmdaInterface *dispatch);

static int
cluster_init_metrictab()
{
    int			e;
    int			c;
    int			i;
    __pmID_int		*mbits;		/* metric bits as uints */
    __pmInDom_int	*indom_int;

    /*
     * Set an environment variable so pmNewContext() loads the Infiniband
     * PMDA.  Keep going if unsuccessful.  Other PMDAs may be OK.
     */
    if (putenv(localCtxIbStr) != 0)
	fprintf(stderr, "pmclusterd: Warning: couldn't set environment "
		"to enable Infiniband support in local context (%s)\n",
		localCtxIbStr);

    if ((c = pmNewContext(PM_CONTEXT_LOCAL, NULL)) < 0) {
	fprintf(stderr,
		"cluster PMDA: failed to open a local context, err %s\n",
		pmErrStr(c));
	return c;
    }

    for (i=0; i < ncluster_mtab; i++) {
	unsigned int subdom;
	__pmInDom_int_subdomain *subindombits;
	pmID orig_pmid_cluster = cluster_mtab[i].m_desc.pmid;
	__pmID_int_subdomain *sbits;

	sbits = __pmid_int_subdomain(&subcluster_mtab[i]); 
	subdom = sbits->subdomain;

	mbits = __pmid_int(&cluster_mtab[i].m_desc.pmid);
	indom_int = (__pmInDom_int *)&(cluster_mtab[i].m_desc.indom);
	sbits = __pmid_int_subdomain(&cluster_mtab[i].m_desc.pmid);

	if (subdom || (subdom = sbits->subdomain) 
			>= num_subdom_dom_map) {
	    fprintf(stderr,
		    "cluster PMDA: warning: pmid for metric %s "
		    "has bad subdomain bits=%d\n",
		    (char *)cluster_mtab[i].m_user, subdom);
	    mbits->cluster = CLUSTER_BAD_CLUSTER;
	    continue;
	} 
	/*
	 * map PMID in cluster PMDA's namespace to sub-PMDA's real
	 * PMID and get descriptor for metric from sub-PMDA
	 */
	if ((e = pmLookupDesc(subcluster_mtab[i], &cluster_mtab[i].m_desc)) != 0) {
	    fprintf(stderr,
		    "cluster PMDA: warning: failed to lookup desc " 
		    "for cluster pmid %d.%d.%d : %s\n",
		    mbits->domain, mbits->cluster, mbits->item,
		    pmErrStr(e));
	}
	cluster_mtab[i].m_desc.pmid = orig_pmid_cluster;
	subindombits = __pmindom_int_subdomain(&cluster_mtab[i].m_desc.indom);
	if (cluster_mtab[i].m_desc.indom == PM_INDOM_NULL) {
	    subindombits->domain	= CLUSTER;
	    subindombits->serial	= CLUSTER_INDOM;
	} else if (subindombits->subdomain) {
	    fprintf(stderr,
		    "cluster PMDA: warning: indom for metric %s " 
		    "has non-zero subdomain bits=%d indom=%d\n",
		    (char *)cluster_mtab[i].m_user, subindombits->subdomain,
		    cluster_mtab[i].m_desc.indom);
	    mbits->cluster = CLUSTER_BAD_CLUSTER;
	} else {
	    subindombits->subdomain = subdom;
	} 
    }
    pmDestroyContext(c);
    return 0; /* success */
}

/*
 * callback provided to pmdaFetch
 */
static int
cluster_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    pmID		 pmid = mdesc->m_desc.pmid;
    __pmID_int		 *idp = (__pmID_int *)&pmid;
    pmID		 pmda_pmid = pmid;
    __pmID_int_subdomain *idsp = __pmid_int_subdomain(&pmda_pmid);
    __pmInDom_int	 *indom_int = (__pmInDom_int *)&mdesc->m_desc.indom;
    int			 monitoring_suspended = cluster_monitoring_suspended();
    int			 sts = 0;
    int			 i;
    int			 j;
    cluster_inst_t	 *instp;
    char		 *instname;
    cluster_client_t	 *tc = NULL;
    pmResult		 *r;
    pmValue		 *v;

    /*
     * If this metric has NOT been configured, then we can
     * return it's descriptor, etc, but there are no
     * instances or values available.
     */
    if (mdesc->m_user == NULL)
    	return PM_ERR_PMID;

    if (idp->cluster == CLUSTER_CLUSTER) {
	if (idp->item == 0) {  /* cluster.control.suspend_monitoring */
	    atom->ul = monitoring_suspended;
	    return 1;
	}
    }
    else if (monitoring_suspended ||	/* everything is stale */
	     idp->cluster == CLUSTER_BAD_CLUSTER)
					/* sub-PMDA using subdomain bits */
    {
	return PM_ERR_VALUE;
    }
    /*
     * find the name and client for this instance in the pmda cache
     */
    sts = pmdaCacheLookup(mdesc->m_desc.indom, inst, &instname, (void **)&instp);
    if (sts != PMDA_CACHE_ACTIVE) {
	fprintf(stderr, "Error: pmdaCacheLookup Error: %s, pmid=%d.%d.%d inst=0x%x\n",
		pmErrStr(sts), idp->domain, idp->cluster, idp->item, inst);
    	return sts < 0 ? sts : PM_ERR_INST;
    }
    if (idp->cluster == CLUSTER_CLUSTER) {
	switch (idp->item) { 
	case 1:/* cluster.control.delete */
	    atom->cp = 0;
	    return 1; 
	case 2: /* cluster.control.metrics */
	    atom->cp = cluster_client_metrics_get(instname);
	    return 1; 
	default:
	    return PM_ERR_PMID;
	}
    }
    if (instp->client < 0) {
	fprintf(stderr, "cluster_fetchCallBack: Error: client for this instance has disconnected\n");
	return PM_ERR_VALUE;
    }
    tc = &cluster_clients[instp->client];

    if (tc->fd == -1 || tc->pb == NULL)
    	return PM_ERR_VALUE;

    if (pmDebug & DBG_TRACE_APPL2) {
	fprintf(stderr, "cluster_fetchCallBack: found client=%d for pmid=%d.%d.%d input_inst=0x%x name=\"%s\"\n",
	    tc->fd, idp->domain, idp->cluster, idp->item, inst, instname);
    }

    if (tc->flags & CLUSTER_CLIENT_FLAG_STALE_RESULT) {
    	if (tc->result) {
	    pmFreeResult(tc->result);
	    tc->result = NULL;
	    tc->flags &= ~CLUSTER_CLIENT_FLAG_STALE_RESULT;
	}
    }

    if (tc->result == NULL) {
	sts = __pmDecodeResult(tc->pb, PDU_BINARY, &tc->result);
	if (sts < 0 || !tc->result) {
	    fprintf(stderr, "cluster_fetchCallback: Error: found client %d but pb == NULL\n", tc->fd);
	    return PM_ERR_VALUE;
	}
	if (pmDebug & DBG_TRACE_APPL2) {
	    fprintf(stderr, "cluster_fetchCallBack: client=%d decoded result, numpmid=%d\n",
		tc->fd, tc->result->numpmid);
	}
    }

    /*
     * Now find the pmid and instance in the cached result.  The domain and
     * cluster for each PMID in the result will be for the sub-PMDA that
     * returned it, so translate the pmDesc.pmID to match before comparing.
     */
    idsp->domain = subdom_dom_map[idsp->subdomain]; 
    idsp->subdomain = 0;
    sts = PM_ERR_PMID;
    for (i=0, r = tc->result; i < r->numpmid; i++) {
	if (pmid_domain( r->vset[i]->pmid) != pmid_domain( pmda_pmid) ||
	    pmid_cluster(r->vset[i]->pmid) != pmid_cluster(pmda_pmid) ||
	    pmid_item(   r->vset[i]->pmid) != pmid_item(   pmda_pmid) )
	    continue;
	/* found the pmid, now look for the instance */
	sts = PM_ERR_INST;
        for (j=0; j < r->vset[i]->numval; j++) {
            v = &r->vset[i]->vlist[j];
	    if (indom_int->serial == CLUSTER_INDOM || v->inst == instp->node_inst) {
		/*
		 * found
		 */
		if (r->vset[i]->valfmt == PM_VAL_INSITU)
		    memcpy(&atom->l, &v->value.lval, sizeof(atom->l));
		else
		    pmExtractValue(r->vset[i]->valfmt, v, v->value.pval->vtype, atom, v->value.pval->vtype);

		return 1;
	    }
        }
    }
    return sts;
}

static int
cluster_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    int e;

    e = pmdaFetch(numpmid, pmidlist, resp, pmda);

    return e;
}

static int
cluster_desc(pmID id, pmDesc *desc, pmdaExt *pmda)
{
    int                 e;

    e = pmdaDesc(id, desc, pmda);

    return e;
}

static int
cluster_profile(__pmProfile *prof, pmdaExt *pmda)
{
    int e;

    e = pmdaProfile(prof, pmda);

    return e;
}

static int
cluster_instance(pmInDom indom, int inst, char *name, __pmInResult **result, pmdaExt *pmda)
{
    int                 e;

    e = pmdaInstance(indom, inst, name, result, pmda);

    return e;
}

static int
cluster_store(pmResult *result, pmdaExt *pmda)
{
    int		ret = 0;
    int		i, j;

    for (i = 0; i < result->numpmid; i++) {
	pmValueSet	*vsp = result->vset[i];
	__pmID_int	*pmidp = (__pmID_int *)&vsp->pmid;
	int		del = 0;
	int		sts;

	if(pmidp->cluster != CLUSTER_CLUSTER) {
	    if (!ret)
		ret = -EACCES;

	    continue;
	}
	switch (pmidp->item) {
	case 0:
	    cluster_suspend_monitoring(vsp->vlist[0].value.lval);
	    break;
	case 1:  /* cluster.control.delete */
	    del = 1;
	    /*FALLTHRU*/
	case 2:  /* cluster.control.metrics */
	    /* Generally report first error discovered, but allow other
	     * errors to override PM_ERR_ISCONN.
	     */
	    for (j=0; j < vsp->numval; j++) {
		sts = cluster_client_metrics_set(&vsp->vlist[j], del);
		if (sts && (!ret || (ret == PM_ERR_ISCONN 
				     && sts != PM_ERR_ISCONN)))
		    ret = sts;
	    }
	    break;
	default:
	    ret = PM_ERR_PMID;
	    break;
	}
    }
    return ret;
}

/*
 * Initialise the agent (both daemon and DSO).
 */
void 
cluster_init(pmdaInterface *dp)
{
    int e;

    dp->version.two.instance = cluster_instance;
    dp->version.two.desc = cluster_desc;
    dp->version.two.fetch = cluster_fetch;
    dp->version.two.store = cluster_store;
    dp->version.two.profile = cluster_profile;
    pmdaSetFetchCallBack(dp, cluster_fetchCallBack);
    if ((e = cluster_init_metrictab()) < 0) {
    	fprintf(stderr, "cluster PMDA: cluster_init_metrictab failed, err = %d\n", e);
	exit(1);
    }
    pmdaInit(dp, NULL, 0, cluster_mtab, ncluster_mtab + CLUSTER_NUM_CONTROL_METRICS);
}

static void
usage(void)
{
    fprintf(stderr, "Usage: %s [options]\n\n", pmProgname);
    fputs("Options:\n"
	  "  -d domain    use domain (numeric) for metrics domain of PMDA\n"
	  "  -l logfile   write log into logfile rather than using default log name\n",
	      stderr);
    exit(1);
}

/*
 * Set up the agent if running as a daemon.
 */
int
main(int argc, char **argv)
{
    int			err = 0;
    pmdaInterface	desc;
    char		*p;
    int 		n = 0;

    /* trim cmd name of leading directory components */
    pmProgname = argv[0];
    for (p = pmProgname; *p; p++) {
	if (*p == '/')
	    pmProgname = p+1;
    }

    snprintf(mypath, sizeof(mypath),
		"%s/cluster/help", pmGetConfig("PCP_PMDAS_DIR"));
    pmdaDaemon(&desc, PMDA_INTERFACE_2, pmProgname, CLUSTER,
		"cluster.log", mypath);

    while ((n = pmdaGetOpt(argc, argv, "D:d:l:?",
                           &desc, &err)) != EOF) {
	fprintf(stderr, "%s: Unknown option \"-%c\"", pmProgname, (char)n);
	err++;
    }
    if (err)
    	usage();

    pmdaOpenLog(&desc);
    cluster_init(&desc);
    pmdaConnect(&desc);
    if (desc.status < 0) {
	fprintf(stderr, "%s: pmdaConnect failed\n", pmProgname);
	exit(1);
    }
    cluster_main_loop(&desc);

    exit(0);
    /*NOTREACHED*/
}
