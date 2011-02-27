/*
 * bigun PMDA ... one big value for QA
 *
 * Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
 * 
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/pmda.h>

static pmdaMetric metrics[] = {
    { NULL, 
      { PMDA_PMID(0,0), PM_TYPE_AGGREGATE_STATIC, PM_INDOM_NULL, PM_SEM_DISCRETE, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } }
};

#define MYSIZE (1024*1024)
static pmValueBlock *vbp;

/*
 * callback provided to pmdaFetch
 */
static int
bigun_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    if (pmid_cluster(mdesc->m_desc.pmid) != 0 ||
        pmid_item(mdesc->m_desc.pmid) != 0)
	return PM_ERR_PMID;
    if (inst != PM_IN_NULL)
	return PM_ERR_INST;

    atom->vbp = vbp;
    return 1;
}

/* Initialise the DSO agent */
void 
bigun_init(pmdaInterface *dp)
{
    int		i;

    pmdaDSO(dp, PMDA_INTERFACE_4, "bigun DSO", NULL);
    if (dp->status != 0)
	return;

    pmdaSetFetchCallBack(dp, bigun_fetchCallBack);
    pmdaInit(dp, NULL, 0, metrics, sizeof(metrics)/sizeof(metrics[0]));

    vbp = (pmValueBlock *)malloc(PM_VAL_HDR_SIZE+MYSIZE);
    if (vbp == NULL) {
	fprintf(stderr, "bigun_init: malloc failed: %s\n", pmErrStr(-errno));
	exit(1);
    }
    vbp->vtype = PM_TYPE_AGGREGATE_STATIC;
    vbp->vlen = PM_VAL_HDR_SIZE+MYSIZE;
    for (i = 0; i < MYSIZE; i += sizeof(int))
	memcpy((void *)&vbp->vbuf[i], (void *)&i, sizeof(int));
}
