/*
 * Unpack an array of event records
 * Free space from unpack
 *
 * Copyright (c) 2010 Ken McDonell.  All Rights Reserved.
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */

#include "pmapi.h"
#include "impl.h"
#include "logger.h"

/*
 * Handle event records.
 *
 * Walk the packed array of events using similar logic to
 * pmUnpackEventRecords() but we don't need any allocations.
 *
 * For each embedded event parameter, make sure the metadata for
 * the associated metric is added to the archive.
 */
int
do_events(pmValueSet *vsp)
{
    pmEventArray	*eap;
    char		*base;
    pmEventRecord	*erp;
    pmEventParameter	*epp;
    int			r;	/* records */
    int			p;	/* parameters in a record ... */
    int			i;	/* instances ... */
    int			sts;
    pmDesc		desc;

    for (i = 0; i < vsp->numval; i++) {
	if ((sts = __pmCheckEventRecords(vsp, i)) < 0) {
	    __pmDumpEventRecords(stderr, vsp, i);
	    return sts;
	}
	eap = (pmEventArray *)vsp->vlist[i].value.pval;
	if (eap->ea_nrecords == 0)
	    return 0;
	base = (char *)&eap->ea_record[0];
	for (r = 0; r < eap->ea_nrecords; r++) {
	    erp = (pmEventRecord *)base;
	    base += sizeof(erp->er_timestamp) + sizeof(erp->er_flags) + sizeof(erp->er_nparams);
	    if (erp->er_flags & PM_EVENT_FLAG_MISSED) {
		/*
		 * no event "parameters" here, just a missed records count
		 * in er_nparams
		 */
		continue;
	    }
	    for (p = 0; p < erp->er_nparams; p++) {
		epp = (pmEventParameter *)base;
		base += sizeof(epp->ep_pmid) + PM_PDU_SIZE_BYTES(epp->ep_len);
		sts = __pmLogLookupDesc(&logctl, epp->ep_pmid, &desc);
		if (sts < 0) {
		    int	numnames;
		    char	**names;
		    numnames = pmNameAll(epp->ep_pmid, &names);
		    if (numnames < 0) {
			/*
			 * Event parameter metric not defined in the PMNS.
			 * This should not happen, but is probably not fatal, so
			 * issue a warning and make up a name based on the pmid
			 * event_param.<domain>.<cluster>.<item>
			 */
			char	*name;
                        size_t   name_size = strlen("event_param")+3+1+4+1+4+1;
                        names = (char **)malloc(sizeof(char*) + name_size);
			if (names == NULL)
			    return -oserror();
			name = (char *)&names[1];
			names[0] = name;
			snprintf(name, name_size, "event_param.%s", pmIDStr(epp->ep_pmid));
			fprintf(stderr, "Warning: metric %s has no name, using %s\n", pmIDStr(epp->ep_pmid), name);
		    }
		    sts = pmLookupDesc(epp->ep_pmid, &desc);
		    if (sts < 0) {
			/* Event parameter metric does not have a pmDesc.
			 * This should not happen, but is probably not entirely
			 * fatal (although more serious than not having a metric
			 * name), issue a warning and construct a minimalist
			 * pmDesc
			 */
			desc.pmid = epp->ep_pmid;
			desc.type = PM_TYPE_AGGREGATE;
			desc.indom = PM_INDOM_NULL;
			desc.sem = PM_SEM_DISCRETE;
			memset(&desc.units, '\0', sizeof(desc.units));
			fprintf(stderr, "Warning: metric %s (%s) has no descriptor, using a default one\n", names[0], pmIDStr(epp->ep_pmid));
		    }
		    if ((sts = __pmLogPutDesc(&logctl, &desc, numnames, names)) < 0) {
			fprintf(stderr, "__pmLogPutDesc: %s\n", pmErrStr(sts));
			exit(1);
		    }
		    free(names);
		}
	    }
	}
    }
    return 0;
}
