/*
 * Copyright (c) 1995,2003,2004 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "pmapi.h"
#include "impl.h"

#ifdef HAVE_VALUES_H
#include <values.h>
#endif
#include <float.h>

#include "./dbpmda.h"
#include "./lex.h"
#include "./gram.h"

static pid_t		pmda_pid;
static __pmTimeval	now = { 0, 0 };

int			infd;
int			outfd;
char			*pmdaName = 0;

extern int		_creds_timeout;
extern int		errno;

#ifndef HAVE_STRTOLL
/*
 * cheap hack ...won't work for large values!
 */
static __int64_t
strtoll(char *p, char **endp, int base)
{
    return (__int64_t)strtol(p, endp, base);
}
#endif

#ifndef HAVE_STRTOULL
/*
 * cheap hack ...won't work for large values!
 */
static __uint64_t
strtoull(char *p, char **endp, int base)
{
    return (__uint64_t)strtoul(p, endp, base);
}
#endif

/* version exchange - get a credentials PDU from 2.0 agents */

static int
agent_creds(__pmPDU *pb)
{
    int			i;
    int			sts = 0;
    int			credcount = 0;
    int			sender = 0;
    int			vflag = 0;
    __pmCred		*credlist = NULL;
    __pmIPC		ipc = { UNKNOWN_VERSION, NULL };

    if ((sts = __pmDecodeCreds(pb, PDU_BINARY, &sender, &credcount, &credlist)) < 0)
	return sts;

    for (i = 0; i < credcount; i++) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_CONTEXT)
	    fprintf(stderr, "agent_creds: doing cred #%d from PID %d\n", i+1, sender);
#endif
	switch(credlist[i].c_type) {
	case CVERSION:
	    ipc.version = credlist[i].c_vala;
	    vflag = 1;
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_CONTEXT)
		fprintf(stderr, "agent_creds: version cred (%u)\n", ipc.version);
#endif
	    break;
	}
    }

    if (credlist)
	free(credlist);

    if (((sts = __pmAddIPC(infd, ipc)) < 0) ||
	((sts = __pmAddIPC(outfd, ipc)) < 0))
	return sts;

    if (vflag) {	/* complete the version exchange - respond to agent */
        __pmCred	handshake[1];

	handshake[0].c_type = CVERSION;
	handshake[0].c_vala = PDU_VERSION;
	handshake[0].c_valb = 0;
	handshake[0].c_valc = 0;
	if ((sts = __pmSendCreds(outfd, PDU_BINARY, 1, handshake)) < 0)
	    return sts;
    }

    return 0;
}

static void
pmdaversion(void)
{
    int		sts;
    __pmPDU	*ack;

    sts = __pmGetPDU(infd, PDU_BINARY, _creds_timeout, &ack);
    if (sts == PDU_CREDS) {
	if ((sts = agent_creds(ack)) < 0) {
	    fprintf(stderr, "Warning: version exchange failed "
		"for PMDA %s: %s\n", pmdaName, pmErrStr(sts));
	    return;
	}
    }
    else {
	__pmIPC	ipc = { PDU_VERSION1, NULL };
	if (sts < 0)
	    fprintf(stderr, "__pmGetPDU(%d): %s\n", infd, pmErrStr(sts));
	fprintf(stderr, "Warning: no version exchange with PMDA %s: "
			"assuming PCP 1.x PMDA.\n", pmdaName);
	__pmAddIPC(infd, ipc);
	__pmAddIPC(outfd, ipc);
    }
}

void
openpmda(char *fname)
{
    int		in[2];
    int		out[2];
    int		i;
    struct stat	buf;

    if (stat(fname, &buf) < 0) {
	fprintf(stderr, "openpmda: %s: %s\n", fname, strerror(errno));
	return;
    }
   
    closepmda();
    free(param.argv[0]);
    param.argv[0] = strdup(basename(fname));
    param.argc--;
    printf("Start %s PMDA: %s", param.argv[0], fname);
    for (i = 1; i < param.argc; i++)
	printf(" %s", param.argv[i]);
    putchar('\n');
    pipe(in);
    pipe(out);
    pmda_pid = fork();
    if (pmda_pid < 0) {
	fprintf(stderr, "openpmda: fork: %s\n", strerror(errno));
	return;
    }
    else if (pmda_pid) {
	/* dbpmda */
	close(in[0]);
	close(out[1]);
	outfd = in[1];
	infd = out[0];
	connmode = PDU_BINARY;
	reset_profile();
    }
    else {
	/* pmda */
	close(in[1]);
	close(out[0]);
	if (in[0] != 0) {
	    close(0);
	    dup2(in[0], 0);
	    close(in[0]);
	}
	if (out[1] != 1) {
	    close(1);
	    dup2(out[1], 1);
	    close(out[1]);
	}
	execvp(fname, param.argv);
	fprintf(stderr, "openpmda: exec: %s\n", strerror(errno));
	exit(1);
    }
    if (pmdaName != NULL)
	free(pmdaName);
    pmdaName = strdup(fname);
    pmdaversion();
}

void
closepmda(void)
{
    if (connmode != PDU_NOT) {
	close(outfd);
	close(infd);
	__pmResetIPC(infd);
	connmode = PDU_NOT;
	if (pmdaName != NULL) {
	    free(pmdaName);
	    pmdaName = NULL;
	}
    }
}


int
dopmda_desc(pmID pmid, pmDesc *desc, int print)
{
    int sts;
    __pmPDU *pb;
    int i;

    if ((sts = __pmSendDescReq(outfd, connmode, pmid)) >= 0) {
	if ((sts = __pmGetPDU(infd, connmode, TIMEOUT_NEVER, &pb)) == PDU_DESC) {
	    if ((sts = __pmDecodeDesc(pb, connmode, desc)) >= 0) {
		if (print)
		    __pmPrintDesc(stdout, desc);
#ifdef PCP_DEBUG
		    else if (pmDebug & DBG_TRACE_PDU)
			__pmPrintDesc(stdout, desc);
#endif
            }
	    else
		printf("Error: __pmDecodeDesc() failed: %s\n", pmErrStr(sts));
	}
	else if (sts == PDU_ERROR) {
	    if ((i = __pmDecodeError(pb, connmode, &sts)) >= 0)
		printf("Error PDU: %s\n", pmErrStr(sts));
	    else
		printf("Error: __pmDecodeError() failed: %s\n", pmErrStr(i));
	}
	else if (sts == 0)
	    printf("Error: __pmGetPDU() failed: PDU empty, PMDA may have died\n");
	else
	    printf("Error: __pmGetPDU() failed: wrong PDU (%x)\n", sts);
    }
    else
	printf("Error: __pmSendDescReq() failed: %s\n", pmErrStr(sts));

    return sts;
}

void
dopmda(int pdu)
{
    int			sts;
    pmDesc		desc;
    pmDesc		*desc_list;
    pmResult		*result;
    __pmInResult		*inresult;
    __pmPDU		*pb;
    int			i;
    int			j;
    int			ident;
    char		*buffer;
    struct timeval	start;
    struct timeval	end;

    if (timer != 0)
	gettimeofday(&start, NULL);
  
    switch (pdu) {

	case PDU_DESC_REQ:
	    printf("PMID: %s\n", pmIDStr(param.pmid));
            sts = dopmda_desc(param.pmid, &desc, 1);
	    break;

	case PDU_FETCH:
	    printf("PMID(s):");
	    for (i = 0; i < param.numpmid; i++)
		printf(" %s", pmIDStr(param.pmidlist[i]));
	    putchar('\n');

	    if (get_desc) {
		desc_list = (pmDesc *)malloc(param.numpmid * sizeof(pmDesc));
		if (desc_list == NULL) {
	            printf("Error: PDU fetch() failed: %s\n", pmErrStr(ENOMEM));
                    return;
                }
	    	for (i = 0; i < param.numpmid; i++) {
            	    if ((sts = dopmda_desc(param.pmidlist[i], &desc_list[i], 0)) < 0) {
			free(desc_list);
			return;
                    }
		} 
            }/*get_desc*/

	    sts = 0;
	    if (profile_changed) {
		if ((sts = __pmSendProfile(outfd, connmode, 0, profile)) < 0)
		    printf("Error: __pmSendProfile() failed: %s\n", pmErrStr(sts));
		else
		    profile_changed = 0;
	    }
	    if (sts >= 0) {
		if ((sts = __pmSendFetch(outfd, connmode, 0, NULL, param.numpmid, param.pmidlist)) >= 0) {
		    if ((sts = __pmGetPDU(infd, connmode, TIMEOUT_NEVER, &pb)) == PDU_RESULT) {
			if ((sts = __pmDecodeResult(pb, connmode, &result)) >= 0) {
			    if (get_desc) 
				_dbDumpResult(stdout, result, desc_list);
			    else
				__pmDumpResult(stdout, result);
			    pmFreeResult(result);
			}
			else
			    printf("Error: __pmDecodeResult() failed: %s\n", pmErrStr(sts));
		    }
		    else if (sts == PDU_ERROR) {
			if ((i = __pmDecodeError(pb, connmode, &sts)) >= 0)
			    printf("Error PDU: %s\n", pmErrStr(sts));
			else
			    printf("Error: __pmDecodeError() failed: %s\n", pmErrStr(i));
		    }
		    else if (sts == 0)
			printf("Error: __pmGetPDU() failed: PDU empty, PMDA may have died\n");
		    else
			printf("Error: __pmGetPDU() failed: wrong PDU (%x)\n", sts);
		}
		else
		    printf("Error: __pmSendFetch() failed: %s\n", pmErrStr(sts));
	    }
	    break;

	case PDU_INSTANCE_REQ:
	    printf("pmInDom: %s\n", pmInDomStr(param.indom));
	    if ((sts = __pmSendInstanceReq(outfd, connmode, &now, param.indom, param.number, param.name)) >= 0) {
		if ((sts = __pmGetPDU(infd, connmode, TIMEOUT_NEVER, &pb)) == PDU_INSTANCE) {
		    if ((sts = __pmDecodeInstance(pb, connmode, &inresult)) >= 0)
			printindom(stdout, inresult);
		    else
			printf("Error: __pmDecodeInstance() failed: %s\n", pmErrStr(sts));
		}
		else if (sts == PDU_ERROR) {
		    if ((i = __pmDecodeError(pb, connmode, &sts)) >= 0)
			printf("Error PDU: %s\n", pmErrStr(sts));
		    else
			printf("Error: __pmDecodeError() failed: %s\n", pmErrStr(i));
		}
		else
		    printf("Error: __pmGetPDU() failed: %s\n", pmErrStr(sts));
	    }
	    else
		printf("Error: __pmSendInstanceReq() failed: %s\n", pmErrStr(sts));
	    break;

	case PDU_RESULT:
	    printf("PMID: %s\n", pmIDStr(param.pmid));

	    printf("Getting description...\n");

            if ((sts = dopmda_desc(param.pmid, &desc, 0)) < 0)
		return;

    	    if (profile_changed) {
		printf("Sending Profile...\n");
		if ((sts = __pmSendProfile(outfd, connmode, 0, profile)) < 0) {
		    printf("Error: __pmSendProfile() failed: %s\n", pmErrStr(sts));
		    return;
		}
		else
		    profile_changed = 0;
	    }

	    printf("Getting Result Structure...\n");
	    if ((sts = __pmSendFetch(outfd, connmode, 0, NULL, 
				    1, &(desc.pmid))) >= 0) {
		if ((sts = __pmGetPDU(infd, connmode, TIMEOUT_NEVER, 
				     &pb)) == PDU_RESULT) {
		    if ((sts = __pmDecodeResult(pb, connmode, &result)) < 0)
			printf("Error: __pmDecodeResult() failed: %s\n", 
			       pmErrStr(sts));
#ifdef PCP_DEBUG
		    else if (pmDebug & DBG_TRACE_FETCH)
			__pmDumpResult(stdout, result);
#endif
		}
		else if (sts == PDU_ERROR) {
		    if ((i = __pmDecodeError(pb, connmode, &sts)) >= 0)
			printf("Error PDU: %s\n", pmErrStr(sts));
		    else
			printf("Error: __pmDecodeError() failed: %s\n", pmErrStr(i));
		}
		else
		    printf("Error: __pmGetPDU() failed: %s\n", pmErrStr(sts));
	    }
	    else
		printf("Error: __pmSendFetch() failed: %s\n", pmErrStr(sts));

	    if (sts < 0)
		return;

	    sts = fillResult(result, desc.type);

	    if (sts < 0) {
		pmFreeResult(result);
		return;
	    }

	    printf("Sending Result...\n");
	    if ((sts = __pmSendResult(outfd, connmode, result)) >= 0) {
		if ((sts = __pmGetPDU(infd, connmode, TIMEOUT_NEVER, 
				     &pb)) == PDU_ERROR) {
		    if ((i = __pmDecodeError(pb, connmode, &sts)) >= 0) {
			if (sts < 0)
			    printf("Error PDU: %s\n", pmErrStr(sts));
		    }
		    else
			printf("Error: __pmDecodeError() failed: %s\n", pmErrStr(i));
		}
		else
		    printf("Error: __pmGetPDU() failed: %s\n", pmErrStr(sts));
	    }
	    else
		printf("Error: __pmSendResult() failed: %s\n", pmErrStr(sts));

	    pmFreeResult(result);	
	    break;

	case PDU_TEXT_REQ:
	    if (param.number == PM_TEXT_PMID) {
		printf("PMID: %s\n", pmIDStr(param.pmid));
		ident = param.pmid;
	    }
	    else {
		printf("pmInDom: %s\n", pmInDomStr(param.indom));
		ident = param.indom;
	    }

	    for (j = 0; j < 2; j++) {

		if (j == 0)
		    param.number |= PM_TEXT_ONELINE;
		else {
		    param.number &= ~PM_TEXT_ONELINE;
		    param.number |= PM_TEXT_HELP;
		}

		if ((sts = __pmSendTextReq(outfd, connmode, ident, param.number)) >= 0) {
		    if ((sts = __pmGetPDU(infd, connmode, TIMEOUT_NEVER, &pb)) == PDU_TEXT) {
			if ((sts = __pmDecodeText(pb, connmode, &i, &buffer)) >= 0) {
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
			else
			    printf("Error: __pmDecodeText() failed: %s\n", pmErrStr(sts));
		    }
		    else if (sts == PDU_ERROR) {
			if ((i = __pmDecodeError(pb, connmode, &sts)) >= 0)
			    printf("Error PDU: %s\n", pmErrStr(sts));
			else
			    printf("Error: __pmDecodeError() failed: %s\n", pmErrStr(i));
		    }
		    else
			printf("Error: __pmGetPDU() failed: %s\n", pmErrStr(sts));
		}
		else
		    printf("Error: __pmSendTextReq() failed: %s\n", pmErrStr(sts));
	    
	    }
	    break;
	}

    if (sts >= 0 && timer != 0) {
	gettimeofday(&end, NULL);
	printf("Timer: %f seconds\n", __pmtimevalSub(&end, &start));
    }
}

int
fillResult(pmResult *result, int type)
{
    int		i;
    int		sts = 0;
    int		nbyte;
    pmAtomValue	atom;
    pmValueSet	*vsp;
    char	*endbuf = NULL;

    switch(type) {
    case PM_TYPE_32:
	atom.l = (int)strtol(param.name, &endbuf, 10);
	nbyte = sizeof(atom.l);
	break;
    case PM_TYPE_U32:
	atom.ul = (unsigned int)strtoul(param.name, &endbuf, 10);
	nbyte = sizeof(atom.ul);
	break;
    case PM_TYPE_64:
	atom.ll = strtoll(param.name, &endbuf, 10);
	nbyte = sizeof(atom.ll);
	break;
    case PM_TYPE_U64:
	atom.ull = strtoull(param.name, &endbuf, 10);
	nbyte = sizeof(atom.ull);
	break;
    case PM_TYPE_FLOAT:
	atom.d = strtod(param.name, &endbuf);
	if (atom.d < FLT_MIN || atom.d > FLT_MAX)
	    sts = ERANGE;
	else {
	    atom.f = atom.d;
	    nbyte = sizeof(atom.f);
	}
	break;
    case PM_TYPE_DOUBLE:
	atom.d = strtod(param.name, &endbuf);
	nbyte = sizeof(atom.d);
	break;
    case PM_TYPE_STRING:
	nbyte = (int)strlen(param.name);
	atom.cp = (char *)malloc(nbyte * sizeof(char));
	if (atom.cp == NULL)
	    sts = ENOMEM;
	else {
	    strcpy(atom.cp, param.name);
	    endbuf = "";
	}
	break;
    default:
	printf("Error: dbpmda does not support storing into aggregate metrics");
	sts = PM_ERR_VALUE;
    }
    
    if (sts < 0)
	printf("Error: Decoding value: %s\n", pmErrStr(sts));
    
    if (*endbuf != '\0') {
	printf("Error: Value \"%s\" is incompatible with metric type (PM_TYPE_%s)\n",
	       param.name, pmTypeStr(type));
	sts = PM_ERR_VALUE;
    }

    if (sts >= 0) {
	vsp = result->vset[0];

	if (vsp->numval == 0) {
	    printf("Error: %s not available!\n", pmIDStr(param.pmid));
	    return PM_ERR_VALUE;
	}

	if (vsp->numval < 0) {
	    printf("Error: %s: %s\n", pmIDStr(param.pmid), pmErrStr(vsp->numval));
	    return vsp->numval;
	}

	for (i = 0; i < vsp->numval; i++) {
	    if (vsp->numval > 1)
		printf("%s [%d]: ", pmIDStr(param.pmid), i);		
	    else
		printf("%s: ", pmIDStr(param.pmid));
	    
	    pmPrintValue(stdout, vsp->valfmt, type, &vsp->vlist[i], 1);
	    vsp->valfmt = __pmStuffValue(&atom, nbyte, &vsp->vlist[i], type); 
	    printf(" -> ");
	    pmPrintValue(stdout, vsp->valfmt, type, &vsp->vlist[i], 1);
	    putchar('\n');
	}
    }

    return sts;
}

