/*
 * Help text support for shim.exe.
 *
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
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#include "./shim.h"

static char	*lastbuf = NULL;

int
help(int ident, int type, char **buf)
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
	goto done;

    if (type & PM_TEXT_ONELINE)
	/*
	 * no one line help text I'm afraid ...
	 */
	goto done;

    pmid = (pmID)ident;
    for (i = 0; i < metrictab_sz; i++) {
	if (pmid == shm_metrictab[i].m_desc.pmid) {
	    shim_ctr_t		*wcp;
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL2) {
		fprintf(stderr, " num_ctrs=%d", shim_metrictab[i].m_num_ctrs);
	    }
#endif
	    if (shim_metrictab[i].m_num_ctrs < 1)
		/* no counter handle, no help text */
		goto done;
	    wcp = shim_metrictab[i].m_ctrs;
	    if (info_sz == 0) {
		/*
		 * on hugh.melbourne.sgi.com running SFU 3.5 on Windows NT
		 * the first call to PdhGetCounterInfoA() hung with a zero
		 * sized buffer ... pander to this with an intial buffer
		 * allocation ... the size is a 100% guess
		 */
	    	info_sz = 256;
		if ((info = (LPSTR)malloc(info_sz)) == NULL) {
		    fprintf(stderr, "help: Warning: PdhGetCounterInfoA malloc (%d) failed @ metric %s: ",
			(int)info_sz, pmIDStr(shm_metrictab[i].m_desc.pmid));
		    errmsg();
		    goto done;
		}
	    }
	    result_sz = info_sz;
	    pdhsts = PdhGetCounterInfoA(wcp->c_hdl, TRUE, &result_sz, (PDH_COUNTER_INFO_A *)info);
	    if (pdhsts == PDH_MORE_DATA) {
		info_sz = result_sz;
		if ((info = (LPSTR)realloc(info, info_sz)) == NULL) {
		    fprintf(stderr, "help: Warning: PdhGetCounterInfoA realloc failed @ metric %s: ",
			pmIDStr(pmid));
		    errmsg();
		    goto done;
		}
		pdhsts =  PdhGetCounterInfoA(wcp->c_hdl, TRUE, &result_sz, (PDH_COUNTER_INFO_A *)info);
	    }
	    if (pdhsts != ERROR_SUCCESS) {
		fprintf(stderr, "help: Warning: PdhGetCounterInfoA failed @ metric %s: %s\n",
		    pmIDStr(pmid), pdherrstr(pdhsts));
		goto done;
	    }
	    infop = (PDH_COUNTER_INFO_A *)info;
	    if (infop->szExplainText == NULL) {
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_APPL2) {
		    fprintf(stderr, " no text from PdhGetCounterInfoA");
		}
#endif
		goto done;
	    }
	    if (lastbuf != NULL) {
		free(lastbuf);
		lastbuf = NULL;
	    }
	    lastbuf = *buf = strdup(infop->szExplainText);
	    if (*buf != NULL)
		sts = 0;
	    goto done;
	}
    }
    sts = PM_ERR_PMID;

done:
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2) {
	fprintf(stderr, " -> %d\n", sts);
    }
#endif
    fflush(stderr);
    return sts;
}
