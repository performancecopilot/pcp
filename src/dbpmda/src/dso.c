/*
 * Copyright (c) 2013,2017 Red Hat.
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
 */

#include <sys/stat.h>
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
	fprintf(stderr, "opendso: %s: %s\n", dso, osstrerror());
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
	    /* set in 2 steps to avoid int to bitfield truncation warnings */
	    dispatch.comm.pmapi_version = PMAPI_VERSION;
	    dispatch.comm.pmapi_version = ~dispatch.comm.pmapi_version;
	    dispatch.comm.flags = 0;
	    dispatch.status = 0;
	    if (pmDebugOptions.pdu)
		fprintf(stderr, "DSO init %s->"PRINTF_P_PFX"%p() domain=%d challenge: pmda_interface=0x%x pmapi_version=%d\n",
			init, initp, dispatch.domain,
			dispatch.comm.pmda_interface,
			(~dispatch.comm.pmapi_version) & 0xff);
	    dispatch.domain = domain;

	    (*initp)(&dispatch);

	    if (dispatch.status != 0) {
		printf("Error: initialization routine \"%s\" failed in DSO \"%s\": %s\n", 
		    init, dso, pmErrStr(dispatch.status));
		dispatch.status = -1;
		dlclose(handle);
	    }
	    else {
		if (dispatch.comm.pmda_interface < PMDA_INTERFACE_2 ||
		    dispatch.comm.pmda_interface > PMDA_INTERFACE_LATEST) {

		    printf("Error: Unsupported PMDA interface version %d returned by DSO \"%s\"\n",
			   dispatch.comm.pmda_interface, dso);
		    dispatch.status = -1;
		    dlclose(handle);
		}
		if (dispatch.comm.pmapi_version != PMAPI_VERSION_2) {
		    printf("Error: Unsupported PMAPI version %d returned by DSO \"%s\"\n",
			   dispatch.comm.pmapi_version, dso);
		    dispatch.status = -1;
		    dlclose(handle);
		}
	    }

	    if (dispatch.status == 0) {
		if (pmDebugOptions.pdu) {
		    fprintf(stderr, "DSO has domain=%d", dispatch.domain);
		    fprintf(stderr, " pmda_interface=%d pmapi_version=%d\n", 
				dispatch.comm.pmda_interface,
				dispatch.comm.pmapi_version);
		}
		dsoname = strdup(dso);
		connmode = CONN_DSO;
		reset_profile();

		if (myPmdaName != NULL)
		    free(myPmdaName);
		myPmdaName = strdup(dso);

		/*
		 * set here once and used by all subsequent calls into the
		 * PMDA
		 */
		if (dispatch.comm.pmda_interface >= PMDA_INTERFACE_5)
		    dispatch.version.four.ext->e_context = 0;
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
	if (dispatch.comm.pmda_interface >= PMDA_INTERFACE_5) {
	    if (dispatch.version.four.ext->e_endCallBack != NULL) {
		(*(dispatch.version.four.ext->e_endCallBack))(0);
	    }
	}
#ifdef HAVE_DLOPEN
	dlclose(handle);
#endif
	free(dsoname);
	dsoname = NULL;
	connmode = NO_CONN;
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

    if (pmDebugOptions.pdu)
	fprintf(stderr, "DSO desc()\n");
    sts = dispatch.version.any.desc(pmid, desc, dispatch.version.four.ext);

    if (sts >= 0  && (pmDebugOptions.pdu))
	pmPrintDesc(stdout, desc);

    return sts;
}/*dodso_desc*/


void
dodso(int pdu)
{
    int			sts = 0;		/* initialize to pander to gcc */
    int			length;
    pmDesc		desc;
    pmDesc		*desc_list = NULL;
    pmResult		*result;
    pmLabelSet		*labelset = NULL;
    pmInResult	*inresult;
    int			i;
    int			j;
    char		*buffer;
    struct timeval	start;
    struct timeval	end;
    char		name[32];
    char		**namelist;
    int			*statuslist;
    pmID		pmid;

    if (timer != 0)
	pmtimevalNow(&start);

    switch (pdu) {

	case PDU_DESC_REQ:
            printf("PMID: %s\n", pmIDStr(param.pmid));
            if ((sts = dodso_desc(param.pmid, &desc)) >= 0)
		pmPrintDesc(stdout, &desc);
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
            }
	    sts = 0;
	    if (profile_changed) {
		if (pmDebugOptions.pdu)
		    fprintf(stderr, "DSO profile()\n");
		sts = dispatch.version.any.profile(profile, dispatch.version.any.ext);
		if (sts < 0)
		    printf("Error: DSO profile() failed: %s\n", pmErrStr(sts));
		else
		    profile_changed = 0;
	    }
	    if (sts >= 0) {
		if (pmDebugOptions.pdu)
		    fprintf(stderr, "DSO fetch()\n");
		sts = dispatch.version.any.fetch(param.numpmid, param.pmidlist, 
						&result, dispatch.version.any.ext);
		if (sts >= 0) {
		    if (desc_list)
		        _dbDumpResult(stdout, result, desc_list);
                    else
		        __pmDumpResult(stdout, result);
		    /*
		     * DSO PMDA will manage the pmResult skelton, but
		     * we need to free the pmValueSets and values here
		     */
		    __pmFreeResultValues(result);
                }
		else {
		    printf("Error: DSO fetch() failed: %s\n", pmErrStr(sts));
		}
	    }
	    if (desc_list)
		free(desc_list);
	    break;

	case PDU_INSTANCE_REQ:
	    printf("pmInDom: %s\n", pmInDomStr(param.indom));
	    if (pmDebugOptions.pdu)
		fprintf(stderr, "DSO instance()\n");

	    sts = dispatch.version.any.instance(param.indom, param.number, 
						    param.name, &inresult,
						    dispatch.version.any.ext);
	    if (sts >= 0)
		printindom(stdout, inresult);
	    else
		printf("Error: DSO instance() failed: %s\n", pmErrStr(sts));
	    break;

	case PDU_RESULT:
            
            printf("PMID: %s\n", pmIDStr(param.pmid));
	    printf("Getting description...\n");
	    desc_list = &desc;
            if ((sts = dodso_desc(param.pmid, desc_list)) < 0) {
	    	printf("Error: DSO desc() failed: %s\n", pmErrStr(sts));
                return;
            }

	    if (profile_changed) {
		printf("Sending Profile...\n");
		sts = dispatch.version.any.profile(profile, dispatch.version.any.ext);
		if (sts < 0) {
		    printf("Error: DSO profile() failed: %s\n", pmErrStr(sts));
		    return;
		}
		else
		    profile_changed = 0;
	    }

	    printf("Getting Result Structure...\n");
	    sts = dispatch.version.any.fetch(1, &(desc.pmid), &result,
						    dispatch.version.any.ext);
	    if (sts < 0) {
		printf("Error: DSO fetch() failed: %s\n", pmErrStr(sts));
		return;
	    }

	    else if (pmDebugOptions.fetch)
		_dbDumpResult(stdout, result, desc_list);
	 
	    sts = fillResult(result, desc.type);
	    if (sts < 0) {
		pmFreeResult(result);
		return;
	    }

	    sts = dispatch.version.any.store(result, dispatch.version.any.ext);
	    if (sts < 0)
		printf("Error: DSO store() failed: %s\n", pmErrStr(sts));

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

		sts = dispatch.version.any.text(i, param.number, &buffer, dispatch.version.any.ext);
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
		}
		else
		    printf("Error: DSO text() failed: %s\n", pmErrStr(sts));
	    }
	    break;

	case PDU_LABEL_REQ:
	    if (param.number & PM_LABEL_INDOM) {
		printf("pmInDom: %s\n", pmInDomStr(param.indom));
		i = param.indom;
	    }
	    else if (param.number & PM_LABEL_CLUSTER) {
		printf("Cluster: %s\n", strcluster(param.pmid));
		i = param.pmid;
	    }
	    else if (param.number & PM_LABEL_ITEM) {
		printf("PMID: %s\n", pmIDStr(param.pmid));
		i = param.pmid;
	    }
	    else if (param.number & PM_LABEL_INSTANCES) {
		printf("Instances of pmInDom: %s\n", pmInDomStr(param.indom));
		i = param.indom;
	    }
	    else /* param.number & (PM_LABEL_DOMAIN|PM_LABEL_CONTEXT) */
		i = PM_IN_NULL;

	    sts = dispatch.version.seven.label(i, param.number, &labelset, dispatch.version.any.ext);
	    if (sts > 0) {
		for (i = 0; i < sts; i++) {
		    if (labelset[i].inst != PM_IN_NULL)
			printf("[%3d] Labels inst: %d\n", i, labelset[i].inst);
		    else
			printf("Labels:\n");
		    for (j = 0; j < labelset[i].nlabels; j++) {
			pmLabel	*lp = &labelset[i].labels[j];
			char *name = labelset[i].json + lp->name;
			char *value = labelset[i].json + lp->value;
			printf("    %.*s=%.*s\n", lp->namelen, name, lp->valuelen, value);
		    }
		}
		pmFreeLabelSets(labelset, sts);
	    }
	    else if (sts == 0)
		printf("Info: DSO label() returns 0\n");
	    else
		printf("Error: DSO label() failed: %s\n", pmErrStr(sts));

	    break;

	case PDU_PMNS_IDS:
	    if (dispatch.comm.pmda_interface < PMDA_INTERFACE_4) {
		printf("Error: PMDA Interface %d does not support dynamic metric names\n", dispatch.comm.pmda_interface);
		break;
	    }
            printf("PMID: %s\n", pmIDStr(param.pmid));
	    sts = dispatch.version.four.name(param.pmid, &namelist, dispatch.version.four.ext);
	    if (sts > 0) {
		for (i = 0; i < sts; i++) {
		    printf("   %s\n", namelist[i]);
		}
		free(namelist);
	    }
	    else if (sts == 0)
		printf("Warning: DSO name() returns 0\n");
	    else
		printf("Error: DSO name() failed: %s\n", pmErrStr(sts));
	    break;

	case PDU_PMNS_NAMES:
	    if (dispatch.comm.pmda_interface < PMDA_INTERFACE_4) {
		printf("Error: PMDA Interface %d does not support dynamic metric names\n", dispatch.comm.pmda_interface);
		break;
	    }
            printf("Metric: %s\n", param.name);
	    sts = dispatch.version.four.pmid(param.name, &pmid, dispatch.version.four.ext);
	    if (sts >= 0)
		printf("   %s\n", pmIDStr(pmid));
	    else
		printf("Error: DSO pmid() failed: %s\n", pmErrStr(sts));
	    break;

	case PDU_PMNS_CHILD:
	    if (dispatch.comm.pmda_interface < PMDA_INTERFACE_4) {
		printf("Error: PMDA Interface %d does not support dynamic metric names\n", dispatch.comm.pmda_interface);
		break;
	    }
            printf("Metric: %s\n", param.name);
	    sts = dispatch.version.four.children(param.name, 0, &namelist, &statuslist, dispatch.version.four.ext);
	    if (sts > 0) {
		for (i = 0; i < sts; i++) {
		    printf("   %8.8s %s\n", statuslist[i] == 1 ? "non-leaf" : "leaf", namelist[i]);
		}
		free(namelist);
		free(statuslist);
	    }
	    else if (sts == 0)
		printf("Warning: DSO children() returns 0\n");
	    else
		printf("Error: DSO children() failed: %s\n", pmErrStr(sts));
	    break;

	case PDU_PMNS_TRAVERSE:
	    if (dispatch.comm.pmda_interface < PMDA_INTERFACE_4) {
		printf("Error: PMDA Interface %d does not support dynamic metric names\n", dispatch.comm.pmda_interface);
		break;
	    }
	    printf("Metric: %s\n", param.name);
	    sts = dispatch.version.four.children(param.name, 1, &namelist, &statuslist, dispatch.version.four.ext);
	    if (sts > 0) {
		for (i = 0; i < sts; i++) {
		    printf("   %8.8s %s\n", statuslist[i] == 1 ? "non-leaf" : "leaf", namelist[i]);
		}
		free(namelist);
		free(statuslist);
	    }
	    else if (sts == 0)
		printf("Warning: DSO children() returns 0\n");
	    else
		printf("Error: DSO children() failed: %s\n", pmErrStr(sts));
	    break;

	case PDU_AUTH:
	    if (dispatch.comm.pmda_interface < PMDA_INTERFACE_6) {
		printf("Error: PMDA Interface %d does not support authentication\n", dispatch.comm.pmda_interface);
		break;
	    }
	    j = param.number;			/* attribute key */
	    buffer = param.name;		/* attribute value */
	    if (buffer)
	        length = strlen(buffer) + 1;	/* length of value */
	    else
		length = 0;
	    i = 0;				/* client ID */

	    __pmAttrKeyStr_r(j, name, sizeof(name)-1);
	    name[sizeof(name)-1] = '\0';

	    printf("Attribute: %s=%s\n", name, buffer ? buffer : "''");
	    sts = dispatch.version.six.attribute(i, j, buffer, length, dispatch.version.six.ext);
	    if (sts >= 0)
		printf("Success\n");
	    else
		printf("Error: DSO attribute() failed: %s\n", pmErrStr(sts));
	    break;

	default:
	    printf("Error: DSO PDU (%s) botch!\n", __pmPDUTypeStr(pdu));
	    break;
    }
  
    if (sts >= 0 && timer != 0) {
	pmtimevalNow(&end);
	printf("Timer: %f seconds\n", pmtimevalSub(&end, &start));
    }
}
