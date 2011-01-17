/*
 * Copyright (c) 1995-2001,2004 Silicon Graphics, Inc.  All Rights Reserved.
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
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
 */

#include "pmapi.h"
#include "impl.h"

/*
 * if you modify this table at all, be sure to remake qa/006
 */
static const struct {
    int  	err;
    char	*symb;
    char	*errmess;
} errtab[] = {
    { PM_ERR_GENERIC,		"PM_ERR_GENERIC",
	"Generic error, already reported above" },
    { PM_ERR_PMNS,		"PM_ERR_PMNS",
	"Problems parsing PMNS definitions" },
    { PM_ERR_NOPMNS,		"PM_ERR_NOPMNS",
	"PMNS not accessible" },
    { PM_ERR_DUPPMNS,		"PM_ERR_DUPPMNS",
	"Attempt to reload the PMNS" },
    { PM_ERR_TEXT,		"PM_ERR_TEXT",
	"One-line or help text is not available" },
    { PM_ERR_APPVERSION,	"PM_ERR_APPVERSION",
	"Metric not supported by this version of monitored application" },
    { PM_ERR_VALUE,		"PM_ERR_VALUE",
	"Missing metric value(s)" },
    { PM_ERR_LICENSE,		"PM_ERR_LICENSE",
	"Current PCP license does not permit this operation" },
    { PM_ERR_TIMEOUT,		"PM_ERR_TIMEOUT",
	"Timeout waiting for a response from PMCD" },
    { PM_ERR_NODATA,		"PM_ERR_NODATA",
	"Empty archive log file" },
    { PM_ERR_RESET,		"PM_ERR_RESET",
	"PMCD reset or configuration change" },
    { PM_ERR_FILE,		"PM_ERR_FILE",
	"Cannot locate a file" },
    { PM_ERR_NAME,		"PM_ERR_NAME",
	"Unknown metric name" },
    { PM_ERR_PMID,		"PM_ERR_PMID",
	"Unknown or illegal metric identifier" },
    { PM_ERR_INDOM,		"PM_ERR_INDOM",
	"Unknown or illegal instance domain identifier" },
    { PM_ERR_INST,		"PM_ERR_INST",
	"Unknown or illegal instance identifier" },
    { PM_ERR_TYPE,		"PM_ERR_TYPE",
	"Unknown or illegal metric type" },
    { PM_ERR_UNIT,		"PM_ERR_UNIT",
	"Illegal pmUnits specification" },
    { PM_ERR_CONV,		"PM_ERR_CONV",
	"Impossible value or scale conversion" },
    { PM_ERR_TRUNC,		"PM_ERR_TRUNC",
	"Truncation in value conversion" },
    { PM_ERR_SIGN,		"PM_ERR_SIGN",
	"Negative value in conversion to unsigned" },
    { PM_ERR_PROFILE,		"PM_ERR_PROFILE",
	"Explicit instance identifier(s) required" },
    { PM_ERR_IPC,		"PM_ERR_IPC",
	"IPC protocol failure" },
    { PM_ERR_NOASCII,		"PM_ERR_NOASCII",
	"ASCII format not supported for this PDU (no longer used)" },
    { PM_ERR_EOF,		"PM_ERR_EOF",
	"IPC channel closed" },
    { PM_ERR_NOTHOST,		"PM_ERR_NOTHOST",
	"Operation requires context with host source of metrics" },
    { PM_ERR_EOL,		"PM_ERR_EOL",
	"End of PCP archive log" },
    { PM_ERR_MODE,		"PM_ERR_MODE",
	"Illegal mode specification" },
    { PM_ERR_LABEL,		"PM_ERR_LABEL",
	"Illegal label record at start of a PCP archive log file" },
    { PM_ERR_LOGREC,		"PM_ERR_LOGREC",
	"Corrupted record in a PCP archive log" },
    { PM_ERR_LOGFILE,		"PM_ERR_LOGFILE",
	"Missing PCP archive log file" },
    { PM_ERR_NOTARCHIVE,	"PM_ERR_NOTARCHIVE",
	"Operation requires context with archive source of metrics" },
    { PM_ERR_NOCONTEXT,		"PM_ERR_NOCONTEXT",
	"Attempt to use an illegal context" },
    { PM_ERR_PROFILESPEC,	"PM_ERR_PROFILESPEC",
	"NULL pmInDom with non-NULL instlist" },
    { PM_ERR_PMID_LOG,		"PM_ERR_PMID_LOG",
	"Metric not defined in the PCP archive log" },
    { PM_ERR_INDOM_LOG,		"PM_ERR_INDOM_LOG",
	"Instance domain identifier not defined in the PCP archive log" },
    { PM_ERR_INST_LOG,		"PM_ERR_INST_LOG",
	"Instance identifier not defined in the PCP archive log" },
    { PM_ERR_NOPROFILE,		"PM_ERR_NOPROFILE",
	"Missing profile - protocol botch" },
    { PM_ERR_NOAGENT,		"PM_ERR_NOAGENT",
	"No PMCD agent for domain of request" },
    { PM_ERR_PERMISSION,	"PM_ERR_PERMISSION",
	"No permission to perform requested operation" },
    { PM_ERR_CONNLIMIT,		"PM_ERR_CONNLIMIT",
	"PMCD connection limit for this host exceeded" },
    { PM_ERR_AGAIN,		"PM_ERR_AGAIN",
	"Try again. Information not currently available" },
    { PM_ERR_ISCONN,		"PM_ERR_ISCONN",
	"Already Connected" },
    { PM_ERR_NOTCONN,		"PM_ERR_NOTCONN",
	"Not Connected" },
    { PM_ERR_NEEDPORT,		"PM_ERR_NEEDPORT",
	"A non-null port name is required" },
    { PM_ERR_WANTACK,		"PM_ERR_WANTACK",
	"Cannot send due to pending acknowledgements" },
    { PM_ERR_NONLEAF,		"PM_ERR_NONLEAF",
	"Metric name is not a leaf in PMNS" },
    { PM_ERR_PMDANOTREADY,	"PM_ERR_PMDANOTREADY",
	"PMDA is not yet ready to respond to requests" },
    { PM_ERR_PMDAREADY,		"PM_ERR_PMDAREADY",
	"PMDA is now responsive to requests" },
    { PM_ERR_OBJSTYLE,		"PM_ERR_OBJSTYLE",
	"Caller does not match object style of running kernel" },
    { PM_ERR_PMCDLICENSE,	"PM_ERR_PMCDLICENSE",
	"PMCD is not licensed to accept client connections" },
    { PM_ERR_TOOSMALL,		"PM_ERR_TOOSMALL",
	"Insufficient elements in list" },
    { PM_ERR_TOOBIG,		"PM_ERR_TOOBIG",
	"Result size exceeded" },
    { PM_ERR_NYI,		"PM_ERR_NYI",
	"Functionality not yet implemented" },
#ifdef ASYNC_API
    { PM_ERR_CTXBUSY,		"PM_ERR_CTXBUSY",
        "Current context is used by asynchronous operation" },
#endif /*ASYNC_API*/
    { 0,			"",
	"" }
};


#define BADCODE "No such PMAPI error code (%d)"
static char	barf[45];

const char *
pmErrStr(int code)
{
    static int	first = 1;
    static char	*unknown;
    char	*msg;
    int		i;

    if (code == 0)
	return "No error";

    if (first) {
	/*
	 * reference message for an unrecognized error code.
	 * For IRIX, strerror() returns NULL in this case.
	 */
	if ((msg = strerror(-1)) != NULL) {
	    /*
	     * For Linux et al, strip the last word, expected to be the
	     * error number as in ...
	     *    Unknown error -1
	     */
	    char *sp = strrchr(msg, ' ');
	    char *endp = NULL;
            long long ec = strtoll(sp+1, &endp, 0);

            if ((endp != NULL) && (*endp == '\0') && (endp != sp+1)) {
		if ((ec == -1LL) || (ec == (unsigned long long)-1LL)) {
		    if ((unknown = strdup (msg)) != NULL) {
			unknown[sp - msg] = '\0';
		    }
                }
	    }
	}
	first = 0;
    }
    if (code < 0 && code > -PM_ERR_BASE) {
	/* intro(2) errors, maybe */
	msg = strerror(-code);
	if (unknown == NULL) {
	    if (msg != NULL)
		return(msg);
	}
	else {
	    /* The intention here is to catch invariants of "Unknown
	     * error XXX" - in this case we're going to return pcp
	     * error message and not the system one */
	    if (msg != NULL && strncmp(msg, unknown, strlen(unknown)) != 0)
		return(msg);
	}
    }

    for (i = 0; errtab[i].err; i++)
	if (errtab[i].err == code)
	    return errtab[i].errmess;

    /* failure */
    snprintf(barf, sizeof(barf), BADCODE,  code);
    return barf;
}

void
__pmDumpErrTab(FILE *f)
{
    int	i;

    fprintf(f, "  Code  Symbolic Name        Message\n");
    for (i = 0; errtab[i].err; i++)
	fprintf(f, "%6d  %-20s %s\n",
	    errtab[i].err, errtab[i].symb, errtab[i].errmess);
}
