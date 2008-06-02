/*
 * Copyright (c) 2008 Aconex.  All Rights Reserved.
 * Copyright (c) 2004 Silicon Graphics, Inc.  All Rights Reserved.
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

#include "hypnotoad.h"

static char	*lastbuf;

int
windows_help(int ident, int type, char **buf)
{
    int			i;
    pmID		pmid;
    PDH_STATUS  	pdhsts;
    static LPSTR	info = NULL;
    static DWORD	info_sz = 0;
    static DWORD	result_sz;
    PDH_COUNTER_INFO_A	*infop;
    int			sts = PM_ERR_TEXT;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2) {
	fprintf(stderr, "help(ident=%s, type=%d)", pmIDStr((pmID)ident), type);
    }
#endif
    if ((type & PM_TEXT_PMID) != PM_TEXT_PMID)
	/*
	 * cannot do anything other than metric help text from PDH,
	 * so no instance domain help text I'm afraid
	 */
	return sts;

    if (type & PM_TEXT_ONELINE)
	/*
	 * no one line help text I'm afraid ...
	 */
	return sts;

    pmid = (pmID)ident;
    for (i = 0; i < metricdesc_sz; i++) {
	if (pmid == metricdesc[i].desc.pmid) {
	    pdh_value_t *pvp = metricdesc[i].vals;
	    if (metricdesc[i].num_vals < 1)
		/* no counter handle, no help text */
		return sts;
	    if (info_sz == 0) {
		/*
		 * We've observed the initial call to PdhGetCounterInfoA
		 * hang with a zero sized buffer ... pander to this with
		 * an intial buffer allocation ... size is a 100% guess.
		 */
	    	info_sz = 256;
		if ((info = (LPSTR)malloc(info_sz)) == NULL) {
		    __pmNotifyErr(LOG_ERR, "windows_help: PdhGetCounterInfoA "
			    		"malloc (%d) failed @ metric %s: ",
				(int)info_sz, pmIDStr(metricdesc[i].desc.pmid));
		    errmsg();
		    return sts;
		}
	    }
	    result_sz = info_sz;
	    pdhsts = PdhGetCounterInfoA(pvp->hdl, TRUE, &result_sz,
					(PDH_COUNTER_INFO_A *)info);
	    if (pdhsts == PDH_MORE_DATA) {
		info_sz = result_sz;
		if ((info = (LPSTR)realloc(info, info_sz)) == NULL) {
		    __pmNotifyErr(LOG_ERR, "windows_help: PdhGetCounterInfoA "
			    		"failed @ metric %s: ", pmIDStr(pmid));
		    errmsg();
		    return sts;
		}
		pdhsts =  PdhGetCounterInfoA(pvp->hdl, TRUE, &result_sz,
					     (PDH_COUNTER_INFO_A *)info);
	    }
	    if (pdhsts != ERROR_SUCCESS) {
		__pmNotifyErr(LOG_ERR, "windows_help: PdhGetCounterInfoA "
				"failed @ metric %s: %s\n",
				pmIDStr(pmid), pdherrstr(pdhsts));
		return sts;
	    }
	    infop = (PDH_COUNTER_INFO_A *)info;
	    if (infop->szExplainText == NULL) {
		__pmNotifyErr(LOG_ERR, "windows_help: PdhGetCounterInfoA "
				"NULL help text @ metric %s\n", pmIDStr(pmid));
		return sts;
	    }
	    if (lastbuf != NULL) {
		free(lastbuf);
		lastbuf = NULL;
	    }
	    lastbuf = *buf = strdup(infop->szExplainText);
	    if (*buf != NULL)
		sts = 0;
	    return sts;
	}
    }
    return PM_ERR_PMID;
}
