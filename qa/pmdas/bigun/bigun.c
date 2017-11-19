/*
 * bigun PMDA ... one big value for QA
 *
 * Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
 * 
 */

#include <pcp/pmapi.h>
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
    if (pmID_cluster(mdesc->m_desc.pmid) != 0 ||
        pmID_item(mdesc->m_desc.pmid) != 0)
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
    int		sep = pmPathSeparator();
    char	helppath[MAXPATHLEN];

    /*
     * Note: helpfile is only available if the PMDA has been installed
     * from $PCP_VAR_DIR/testsuite/pmdas/bigun ... when the PMDA install
     * comes from QA run from some other directory, the helpfile may not
     * be found ... fortunately nothing in QA depends on the helpfile
     * being available for the bigun PMDA.
     */
    pmsprintf(helppath, sizeof(helppath),
		"%s%c" "testsuite" "%c" "pmdas" "%c" "bigun" "%c" "help",
		pmGetConfig("PCP_VAR_DIR"), sep, sep, sep, sep);
    pmdaDSO(dp, PMDA_INTERFACE_4, "bigun DSO", helppath);
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
