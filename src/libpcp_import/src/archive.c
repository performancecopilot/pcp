#include "pmapi.h"
#include "impl.h"
#include "import.h"
#include "private.h"

static __pmTimeval	stamp;

int
_pmi_put_result(pmi_context *current, pmResult *result)
{
    int		sts;
    char	*host;
    char	myname[MAXHOSTNAMELEN];
    __pmPDU	*pb;
    __pmLogCtl	*lcp = &current->logctl;
    int		k;
    int		i;
    int		m;
    int		needti;

    stamp.tv_sec = result->timestamp.tv_sec;
    stamp.tv_usec = result->timestamp.tv_usec;

    if (current->state == CONTEXT_START) {
	// TODO
	if (current->hostname == NULL) {
	    (void)gethostname(myname, MAXHOSTNAMELEN);
	    myname[MAXHOSTNAMELEN-1] = '\0';
	    host = myname;
	}
	else
	    host = current->hostname;

	sts = __pmLogCreate(host, current->archive, PM_LOG_VERS02, lcp);
	if (sts < 0)
	    return sts;

	if (current->timezone == NULL)
	    strcpy(lcp->l_label.ill_tz, __pmTimezone());
	else
	    strcpy(lcp->l_label.ill_tz, current->timezone);
	pmNewZone(lcp->l_label.ill_tz);
	current->state = CONTEXT_ACTIVE;

	// do the label records (it is too late when __pmLogPutResult
	// is called as we've already output some metadata) ... this
	// code is stolen from __pmLogPutResult
	//
	lcp->l_label.ill_start.tv_sec = stamp.tv_sec;
	lcp->l_label.ill_start.tv_usec = stamp.tv_usec;
	lcp->l_label.ill_vol = PM_LOG_VOL_TI;
	__pmLogWriteLabel(lcp->l_tifp, &lcp->l_label);
	lcp->l_label.ill_vol = PM_LOG_VOL_META;
	__pmLogWriteLabel(lcp->l_mdfp, &lcp->l_label);
	lcp->l_label.ill_vol = 0;
	__pmLogWriteLabel(lcp->l_mfp, &lcp->l_label);
	lcp->l_state = PM_LOG_STATE_INIT;
	__pmLogPutIndex(&current->logctl, &stamp);
    }

    __pmOverrideLastFd(fileno(lcp->l_mfp));
    if ((sts = __pmEncodeResult(fileno(lcp->l_mfp), result, &pb)) < 0)
	return sts;

    needti = 0;
    for (k = 0; k < result->numpmid; k++) {
	for (m = 0; m < current->nmetric; m++) {
	    if (result->vset[k]->pmid != current->metric[m].pmid)
		continue;
	    if (current->metric[m].meta_done == 0) {
		char	**namelist = &current->metric[m].name;

		if ((sts = __pmLogPutDesc(lcp, &current->metric[m].desc, 1, namelist)) < 0)
		    return sts;
		current->metric[m].meta_done = 1;
		needti = 1;
	    }
	    if (current->metric[m].desc.indom != PM_INDOM_NULL) {
		for (i = 0; i < current->nindom; i++) {
		    if (current->metric[m].desc.indom == current->indom[i].indom) {
			if (current->indom[i].meta_done == 0) {
			    if ((sts = __pmLogPutInDom(lcp, current->indom[i].indom, &stamp, current->indom[i].ninstance, current->indom[i].inst, current->indom[i].name)) < 0)
				return sts;
			    current->indom[i].meta_done = 1;
			    needti = 1;
			}
		    }
		}
	    }
	    break;
	}
    }
    if (needti) {
	__pmLogPutIndex(lcp, &stamp);
    }

    if ((sts = __pmLogPutResult(lcp, pb)) < 0)
	return sts;

    return 0;
}

int
_pmi_end(pmi_context *current)
{
    // TODO flushing and end label processing
    __pmLogPutIndex(&current->logctl, &stamp);

    current->state = CONTEXT_END;
    return 0;
}
