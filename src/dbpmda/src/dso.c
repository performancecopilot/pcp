/*
 * Copyright (c) 1995,2004 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include "./dbpmda.h"
#include "pmapi.h"

#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#endif

static char	*dsoname;
static void	*handle;
pmdaInterface	dispatch;

void
opendso(char *dso, char *init, int domain)
{
#ifdef HAVE_DLOPEN
    struct stat		buf;
    unsigned int	challenge;

    dispatch.status = -1;

    if (stat(dso, &buf) < 0) {
	fprintf(stderr, "opendso: %s: %s\n", dso, strerror(errno));
	return;
    }
   
    closedso();
    /*
     * RTLD_NOW would be better in terms of detecting unresolved symbols
     * now, rather than taking a SEGV later ... but various combinations
     * of dynamic and static libraries used to create the DSO PMDA,
     * combined with hiding symbols in the DSO PMDA may result in benign
     * unresolved symbols remaining and the dlopen() would fail under
     * these circumstances.
     */
    handle = dlopen(dso, RTLD_LAZY);
    if (handle == NULL) {
	printf("Error attaching DSO \"%s\"\n", dso);
	printf("%s\n\n", dlerror());
    }
    else {
	void	(*initp)(pmdaInterface *);
	initp = (void (*)(pmdaInterface *))dlsym(handle, init);
	if (initp == NULL) {
	    printf("Error: couldn't find init function \"%s\" in DSO \"%s\"\n",
		    init, dso);
	    dlclose(handle);
	}
	else {
	    /*
	     * the PMDA interface / PMAPI version discovery as a "challenge" ...
	     * for pmda_interface it is all the bits being set,
	     * for pmapi_version it is the complement of the one you are
	     * using now
	     */
	    challenge = 0xff;
	    dispatch.comm.pmda_interface = challenge;
	    dispatch.comm.pmapi_version = ~PMAPI_VERSION;
	    dispatch.comm.flags = 0;
	    dispatch.status = 0;
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_PDU)
		fprintf(stderr, "DSO init %s->"PRINTF_P_PFX"%p() domain=%d challenge: pmda_interface=0x%x pmapi_version=%d\n",
			init, initp, dispatch.domain,
			dispatch.comm.pmda_interface,
			(~dispatch.comm.pmapi_version) & 0xff);
#endif
	    dispatch.domain = domain;

	    (*initp)(&dispatch);

	    if (dispatch.status != 0) {
		printf("Error: initialization routine \"%s\" failed in DSO \"%s\": %s\n", 
		    init, dso, pmErrStr(dispatch.status));
		dispatch.status = -1;
		dlclose(handle);
	    }
	    else {
		if (dispatch.comm.pmda_interface == challenge) {
		    /*
		     * DSO did not change pmda_interface, assume PMAPI
		     * version 1 from PCP 1.x and PMDA_INTERFACE_1
		     */
		    dispatch.comm.pmda_interface = PMDA_INTERFACE_1;
		    dispatch.comm.pmapi_version = PMAPI_VERSION_1;
		}
		else {
		    /*
		     * gets a bit tricky ... 
		     * interface_version (8-bits) used to be version (4-bits),
		     * so it is possible that only the bottom 4 bits were
		     * changed and in this case the PMAPI version is 1 for
		     * PCP 1.x
		     */
		    if ((dispatch.comm.pmda_interface & 0xf0) == (challenge & 0xf0)) {
			dispatch.comm.pmapi_version = PMAPI_VERSION_1;
			dispatch.comm.pmda_interface &= 0x0f;
		    }
		    if (dispatch.comm.pmda_interface < PMDA_INTERFACE_1 ||
			dispatch.comm.pmda_interface > PMDA_INTERFACE_LATEST) {

			printf("Error: Unsupported PMDA interface version %d returned by DSO \"%s\"\n",
			       dispatch.comm.pmda_interface, dso);
			dispatch.status = -1;
			dlclose(handle);
		    }
		    if (dispatch.comm.pmapi_version != PMAPI_VERSION_1 &&
			dispatch.comm.pmapi_version != PMAPI_VERSION_2) {
			printf("Error: Unsupported PMAPI version %d returned by DSO \"%s\"\n",
			       dispatch.comm.pmapi_version, dso);
			dispatch.status = -1;
			dlclose(handle);
		    }
		}
	    }

	    if (dispatch.status == 0) {
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_PDU) {
		    fprintf(stderr, "DSO has domain=%d", dispatch.domain);
		    fprintf(stderr, " pmda_interface=%d pmapi_version=%d\n", 
				dispatch.comm.pmda_interface,
				dispatch.comm.pmapi_version);
		}
#endif
		dsoname = strdup(dso);
		connmode = PDU_DSO;
		reset_profile();

		if (pmdaName != NULL)
		    free(pmdaName);
		pmdaName = strdup(dso);
	    }
	}
    }
#else /* ! HAVE_DLOPEN */
    dispatch.status = -1;

    fprintf(stderr, "opendso: %s: No dynamic DSO/DLL support on this platform\n", dso);
#endif
}

void
closedso(void)
{
    if (dsoname != NULL) {
#ifdef HAVE_DLOPEN
	dlclose(handle);
#endif
	free(dsoname);
	dsoname = NULL;
	connmode = PDU_NOT;
    }
}

/*
 * Do a descriptor pdu.
 * Abstracted here for several calls.
 */
int
dodso_desc(pmID pmid, pmDesc *desc)
{
    int sts;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PDU)
	fprintf(stderr, "DSO desc()\n");
#endif
    if (dispatch.comm.pmda_interface == PMDA_INTERFACE_1)
	sts = dispatch.version.one.desc(pmid, desc);
    else
	sts = dispatch.version.two.desc(pmid, desc, dispatch.version.two.ext);
    if (sts < 0 && dispatch.comm.pmapi_version == PMAPI_VERSION_1)
	sts = XLATE_ERR_1TO2(sts);

#ifdef PCP_DEBUG
    if (sts >= 0  && (pmDebug & DBG_TRACE_PDU))
	__pmPrintDesc(stdout, desc);
#endif

    return sts;
}/*dodso_desc*/


void
dodso(int pdu)
{
    int			sts;
    pmDesc		desc;
    pmDesc		*desc_list;
    pmResult		*result;
    __pmInResult		*inresult;
    int			i;
    int			j;
    char		*buffer;
    struct timeval	start;
    struct timeval	end;

    if (timer != 0)
	gettimeofday(&start, NULL);

    switch (pdu) {

	case PDU_DESC_REQ:
            printf("PMID: %s\n", pmIDStr(param.pmid));
            if ((sts = dodso_desc(param.pmid, &desc)) >= 0)
		__pmPrintDesc(stdout, &desc);
            else
	        printf("Error: DSO desc() failed: %s\n", pmErrStr(sts));
	    break;

	case PDU_FETCH:
	    printf("PMID(s):");
	    for (i = 0; i < param.numpmid; i++)
		printf(" %s", pmIDStr(param.pmidlist[i]));
	    putchar('\n');

	    if (get_desc) {
		desc_list = (pmDesc *)malloc(param.numpmid * sizeof(pmDesc));
		if (desc_list == NULL) {
	            printf("Error: DSO fetch() failed: %s\n", pmErrStr(ENOMEM));
                    return;
                }
	    	for (i = 0; i < param.numpmid; i++) {
            	    if ((sts = dodso_desc(param.pmidlist[i], &desc_list[i])) < 0) {
	                printf("Error: DSO desc() failed: %s\n", pmErrStr(sts));
			free(desc_list);
			return;
                    }
		} 
            }/*get_desc*/
	    sts = 0;
	    if (profile_changed) {
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_PDU)
		    fprintf(stderr, "DSO profile()\n");
#endif
		if (dispatch.comm.pmda_interface == PMDA_INTERFACE_1)
		    sts = dispatch.version.one.profile(profile);
		else
		    sts = dispatch.version.two.profile(profile, dispatch.version.two.ext);

		if (sts < 0) {
		    if (dispatch.comm.pmapi_version == PMAPI_VERSION_1)
			sts = XLATE_ERR_1TO2(sts);
		    printf("Error: DSO profile() failed: %s\n", pmErrStr(sts));
		}
		else
		    profile_changed = 0;
	    }
	    if (sts >= 0) {
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_PDU)
		    fprintf(stderr, "DSO fetch()\n");
#endif
		if (dispatch.comm.pmda_interface == PMDA_INTERFACE_1)
		    sts = dispatch.version.one.fetch(param.numpmid, param.pmidlist, 
						     &result);
		else
		    sts = dispatch.version.two.fetch(param.numpmid, param.pmidlist, 
						     &result, dispatch.version.two.ext);

		if (sts >= 0) {
		    if (result != NULL &&
			dispatch.comm.pmapi_version == PMAPI_VERSION_1) {
			for (j = 0; j < result->numpmid; j++)
			    result->vset[j]->numval =
				    XLATE_ERR_1TO2(result->vset[j]->numval);
		    }
		    if (get_desc) {
		        _dbDumpResult(stdout, result, desc_list);
			free(desc_list);
		    }
                    else
		        __pmDumpResult(stdout, result);
                }
		else {
		    if (dispatch.comm.pmapi_version == PMAPI_VERSION_1)
			sts = XLATE_ERR_1TO2(sts);
		    printf("Error: DSO fetch() failed: %s\n", pmErrStr(sts));
		}
	    }
	    break;

	case PDU_INSTANCE_REQ:
	    printf("pmInDom: %s\n", pmInDomStr(param.indom));
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_PDU)
		fprintf(stderr, "DSO instance()\n");
#endif

	    if (dispatch.comm.pmda_interface == PMDA_INTERFACE_1)
		sts = dispatch.version.one.instance(param.indom, param.number, 
						    param.name, &inresult);
	    else
		sts = dispatch.version.two.instance(param.indom, param.number, 
						    param.name, &inresult,
						    dispatch.version.two.ext);

	    if (sts >= 0)
		printindom(stdout, inresult);
	    else {
		if (dispatch.comm.pmapi_version == PMAPI_VERSION_1)
		    sts = XLATE_ERR_1TO2(sts);
		printf("Error: DSO instance() failed: %s\n", pmErrStr(sts));
	    }
	    break;

	case PDU_RESULT:
            
            printf("PMID: %s\n", pmIDStr(param.pmid));
	    printf("Getting description...\n");
            if ((sts = dodso_desc(param.pmid, &desc)) < 0) {
	    	printf("Error: DSO desc() failed: %s\n", pmErrStr(sts));
                return;
            }

	    if (profile_changed) {
		printf("Sending Profile...\n");
		if (dispatch.comm.pmda_interface == PMDA_INTERFACE_1)
		    sts = dispatch.version.one.profile(profile);
		else
		    sts = dispatch.version.two.profile(profile, dispatch.version.two.ext);

		if (sts < 0) {
		    if (dispatch.comm.pmapi_version == PMAPI_VERSION_1)
			sts = XLATE_ERR_1TO2(sts);
		    printf("Error: DSO profile() failed: %s\n", pmErrStr(sts));
		    return;
		}
		else
		    profile_changed = 0;
	    }

	    printf("Getting Result Structure...\n");
	    if (dispatch.comm.pmda_interface == PMDA_INTERFACE_1)
		sts = dispatch.version.one.fetch(1, &(desc.pmid), &result); 
	    else
		sts = dispatch.version.two.fetch(1, &(desc.pmid), &result,
						    dispatch.version.two.ext);
	    
	    if (sts < 0) {
		if (dispatch.comm.pmapi_version == PMAPI_VERSION_1)
		    sts = XLATE_ERR_1TO2(sts);
		printf("Error: DSO fetch() failed: %s\n", pmErrStr(sts));
		return;
	    }
	    if (result != NULL &&
		dispatch.comm.pmapi_version == PMAPI_VERSION_1) {
		for (j = 0; j < result->numpmid; j++)
		    result->vset[j]->numval =
			    XLATE_ERR_1TO2(result->vset[j]->numval);
	    }

#ifdef PCP_DEBUG
	    else if (pmDebug & DBG_TRACE_FETCH)
		_dbDumpResult(stdout, result, &desc);
#endif
	 
	    sts = fillResult(result, desc.type);

	    if (sts < 0) {
		pmFreeResult(result);
		return;
	    }

	    if (dispatch.comm.pmda_interface == PMDA_INTERFACE_1)
		sts = dispatch.version.one.store(result);
	    else
		sts = dispatch.version.two.store(result, dispatch.version.two.ext);
	    
	    if (sts < 0) {
		if (dispatch.comm.pmapi_version == PMAPI_VERSION_1)
		    sts = XLATE_ERR_1TO2(sts);
		printf("Error: DSO store() failed: %s\n", pmErrStr(sts));
	    }

	    break;

	case PDU_TEXT_REQ:
	    if (param.number == PM_TEXT_PMID) {
		printf("PMID: %s\n", pmIDStr(param.pmid));
		i = param.pmid;
	    }
	    else {
		printf("pmInDom: %s\n", pmInDomStr(param.indom));
		i = param.indom;
	    }

	    for (j = 0; j < 2; j++) {

		if (j == 0)
		    param.number |= PM_TEXT_ONELINE;
		else {
		    param.number &= ~PM_TEXT_ONELINE;
		    param.number |= PM_TEXT_HELP;
		}

		if (dispatch.comm.pmda_interface == PMDA_INTERFACE_1)
		    sts = dispatch.version.one.text(i, param.number, &buffer);
		else
		    sts = dispatch.version.two.text(i, param.number, &buffer, dispatch.version.two.ext);

		if (sts >= 0) {
		    if (j == 0) {
			if (*buffer != '\0')
			    printf("[%s]\n", buffer);
			else
			    printf("[<no one line help text specified>]\n");
		    }
		    else if (*buffer != '\0')
			printf("%s\n", buffer);
		    else
			printf("<no help text specified>\n");
		    free(buffer);
		}
		else {
		    if (dispatch.comm.pmapi_version == PMAPI_VERSION_1)
			sts = XLATE_ERR_1TO2(sts);
		    printf("Error: DSO text() failed: %s\n", pmErrStr(sts));
		}
	    }
	    break;

	}
	   
    if (sts >= 0 && timer != 0) {
	gettimeofday(&end, NULL);
	printf("Timer: %f seconds\n", __pmtimevalSub(&end, &start));
    }
}
