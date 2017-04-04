/*
 * Copyright (c) 2012 Red Hat.  All Rights Reserved.
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

/*
 * ping pmcd 4 times, kill off pmcd, ping 4 more times, restart pmcd,
 * ping 4 more times ... and some reconnect attempts for good measure ...
 * and some more pings ...
 * sleep 10 seconds after change of state for pmcd
 *
 * Has to be run as root to control pmcd
 */

#include <ctype.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>

#include "localconfig.h"

static pmResult	*_store;
static pmDesc	_desc;
static int	_numinst;
static int	*_instlist;
static char	**_inamelist;
static int	_text;
static int	_indom_text;
static int	ctlport;

static void
_ConnectLogger(void)
{
    int		n;
    int		pid = PM_LOG_PRIMARY_PID;
    int		port = PM_LOG_NO_PORT;

    if ((n = __pmConnectLogger("localhost", &pid, &port)) < 0) {
	printf("Cannot connect to primary pmlogger on \"localhost\": %s\n", pmErrStr(n));
	exit(1);
    }
    ctlport = n;
}

static int
x_indom(int xpecterr)
{
    int		err = 0;
    int		n;
    int		j;
    int		numinst;
    int		*instlist;
    char	**inamelist;
    char	*name = NULL;

    if ((numinst = pmGetInDom(_desc.indom, &instlist, &inamelist)) < 0) {
	fprintf(stderr, "pmGetInDom: %s", pmErrStr(numinst));
	if (xpecterr)
	    fprintf(stderr, " -- error expected\n");
	else {
	    fprintf(stderr, "\n");
	    err++;
	}
    }
    else {
	free(instlist);
	free(inamelist);
    }

    j = _numinst / 2;

    if ((n = pmNameInDom(_desc.indom, _instlist[j], &name)) < 0) {
	fprintf(stderr, "pmNameInDom: %s", pmErrStr(n));
	if (xpecterr)
	    fprintf(stderr, " -- error expected\n");
	else {
	    fprintf(stderr, "\n");
	    err++;
	}
    }

    if ((n = pmLookupInDom(_desc.indom, name)) < 0) {
	fprintf(stderr, "pmLookupInDom: %s", pmErrStr(n));
	if (xpecterr)
	    fprintf(stderr, " -- error expected\n");
	else {
	    fprintf(stderr, "\n");
	    err++;
	}
    }
    else {
	if (n != _instlist[j]) {
	    err++;
	    fprintf(stderr, "botch: pmLookupInDom returns 0x%x, expected 0x%x\n",
		n, _instlist[j]);
	}
	free(name);
    }

    return err;
}

static int
exer(int numpmid, pmID *pmidlist, int xpecterr)
{
    int		i;
    int		j;
    int		n;
    int		err = 0;
    pmDesc	desc;
    char	*buf;
    pmResult	*resp, *lresp;

    for (i = 0; i < 4; i++) {
	if (!xpecterr)
	    fputc('.', stderr);

	if ((n = pmLookupDesc(pmidlist[0], &desc)) < 0) {
	    fprintf(stderr, "pmLookupDesc: %s", pmErrStr(n));
	    if (xpecterr)
		fprintf(stderr, " -- error expected\n");
	    else {
		fprintf(stderr, "\n");
		err++;
	    }
	}
	else if (xpecterr)
	    err++;

	err += x_indom(xpecterr);

	if ((n = pmLookupText(pmidlist[1], PM_TEXT_HELP, &buf)) < 0) {
	    fprintf(stderr, "pmLookupText: %s", pmErrStr(n));
	    if (xpecterr)
		fprintf(stderr, " -- error expected\n");
	    else {
		fprintf(stderr, "\n");
		err++;
	    }
	}
	else {
	    if (xpecterr)
		err++;
	    if (!_text) {
		fprintf(stderr, "Text: %s\n", buf);
		_text = 1;
	    }
	    free(buf);
	}

	if ((n = pmLookupInDomText(_desc.indom, PM_TEXT_HELP, &buf)) < 0) {
	    fprintf(stderr, "pmLookupInDomText: %s", pmErrStr(n));
	    if (xpecterr)
		fprintf(stderr, " -- error expected\n");
	    else {
		fprintf(stderr, "\n");
		err++;
	    }
	}
	else {
	    if (xpecterr)
		err++;
	    if (!_indom_text) {
		fprintf(stderr, "InDomText: %s\n", buf);
		_indom_text = 1;
	    }
	    free(buf);
	}

	if ((n = pmFetch(numpmid, pmidlist, &resp)) < 0) {
	    fprintf(stderr, "pmFetch: %s", pmErrStr(n));
	    if (xpecterr)
		fprintf(stderr, " -- error expected\n");
	    else {
		fprintf(stderr, "\n");
		err++;
	    }
	}
	else {
	    if (xpecterr)
		err++;
	    if (resp->numpmid != numpmid) {
		err++;
		__pmDumpResult(stderr, resp);
	    }
	    else for (j = 0; j < resp->numpmid; j++) {
		if (resp->vset[j]->numval < 1) {
		    err++;
		    __pmDumpResult(stderr, resp);
		    break;
		}
	    }
	    pmFreeResult(resp);
	}

	if ((n = pmStore(_store)) < 0) {
	    fprintf(stderr, "pmStore: %s", pmErrStr(n));
	    if (xpecterr)
		fprintf(stderr, " -- error expected\n");
	    else {
		fprintf(stderr, "\n");
		err++;
	    }
	}
	else if (xpecterr)
	    err++;

	if ((n = __pmControlLog(ctlport, _store, PM_LOG_ENQUIRE, 0, 0, &lresp)) < 0) {
	    fprintf(stderr, "__pmControlLog: %s", pmErrStr(n));
	    if (xpecterr)
		fprintf(stderr, " -- error expected\n");
	    else {
		fprintf(stderr, "\n");
		err++;
	    }
	}
	else {
	    pmFreeResult(lresp);
	}

    }
    if (!xpecterr)
	fputc('\n', stderr);


    return err;
}

int
main(int argc, char **argv)
{
    int		err = 0;
    int		sts;
    int		i;
    int		j;
    char	*namelist[4];
    pmID	pmidlist[4];
    int		n;
    int		numpmid;
    int		ctx0;
    int		ctx1;
    int		c;
    int		errflag = 0;
    char	*binadm = pmGetConfig("PCP_BINADM_DIR");
    char	path[MAXPATHLEN];

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:")) != EOF) {
	switch (c) {
	case 'D':	/* debug flag */
	    sts = __pmParseDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug flag specification (%s)\n",
		    pmProgname, optarg);
		errflag++;
	    }
	    else
		pmDebug |= sts;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag) {
	fprintf(stderr,
"Usage: %s options ...\n\
\n\
Options:\n\
  -D N		set pmDebug debugging flag to N\n",
		pmProgname);
	exit(1);
    }

#ifndef IS_MINGW
    if ((n = geteuid()) != 0) {
	fprintf(stderr, "pmcdgone: Must be run as root, not uid %d!\n", n);
	exit(1);
    }
#endif

    if ((n = pmLoadNameSpace(PM_NS_DEFAULT)) < 0) {
	fprintf(stderr, "pmLoadNameSpace: %s\n", pmErrStr(n));
	exit(1);
    }

    if ((ctx0 = pmNewContext(PM_CONTEXT_HOST, "localhost")) < 0) {
	fprintf(stderr, "pmNewContext: %s\n", pmErrStr(ctx0));
	exit(1);
    }
    if ((ctx1 = pmNewContext(PM_CONTEXT_HOST, "localhost")) < 0) {
	fprintf(stderr, "pmNewContext: %s\n", pmErrStr(ctx1));
	exit(1);
    }
    _ConnectLogger();

    i = 0;
    namelist[i++] = "sample.long.write_me";
    namelist[i++] = "sample.colour";
    namelist[i++] = "sampledso.bin";
    numpmid = i;
    n = pmLookupName(numpmid, namelist, pmidlist);
    if (n < 0) {
	fprintf(stderr, "pmLookupName: %s\n", pmErrStr(n));
	for (i = 0; i < numpmid; i++) {
	    if (pmidlist[i] == PM_ID_NULL)
		fprintf(stderr, "	%s - not known\n", namelist[i]);
	}
	exit(1);
    }
    if ((n = pmFetch(1, pmidlist, &_store)) < 0) {
	fprintf(stderr, "initial pmFetch failed: %s\n", pmErrStr(n));
	exit(1);
    }
    if ((n = pmLookupDesc(pmidlist[1], &_desc)) < 0) {
	fprintf(stderr, "initial pmLookupDesc failed: %s\n", pmErrStr(n));
	exit(1);
    }
    if ((_numinst = pmGetInDom(_desc.indom, &_instlist, &_inamelist)) < 0) {
	fprintf(stderr, "initial pmGetInDom failed: %s\n", pmErrStr(n));
	exit(1);
    }

    err += exer(numpmid, pmidlist, 0);

    fprintf(stderr, "Kill off pmcd ...\n");
    sts = system(". $PCP_DIR/etc/pcp.env; $PCP_RC_DIR/pcp stop");
    if (sts != 0)
	fprintf(stderr, "Warning: stop script returns %d\n", sts);
    sleep(10);
    _text = _indom_text = 0;
    __pmCloseSocket(ctlport);

    err += exer(numpmid, pmidlist, 1);

    for (j = 0; j < 2; j++) {
	if (j == 0)
	    i = ctx1;
	else
	    i = ctx0;;
	fprintf(stderr, "Reconnect to pmcd context %d ...\n", i);
	if ((n = pmReconnectContext(i)) < 0) {
	    fprintf(stderr, "pmReconnectContext: %s -- error expected\n", pmErrStr(n));
	}
	else {
	    fprintf(stderr, "pmReconnectContext: success after pmcd killed!?\n");
	    err++;
	}

	err += exer(numpmid, pmidlist, 1);
    }

    /*
     * tricky part begins ...
     * 1. get rid of the archive folio to avoid timestamp clashes with last
     *    created folio
     * 2. re-start pmcd
     * 3. wait for pmcd to be accepting connections
     * 4. wait for pmlogger to be accepting connections from pmlc
     * 5. connect to pmlogger
     */
    fprintf(stderr, "Restart pmcd ...\n");
    sts = system(". $PCP_DIR/etc/pcp.env; path_opt=''; if [ $PCP_PLATFORM = linux ]; then path_opt=pmlogger/; fi; pmafm $PCP_LOG_DIR/$path_opt`hostname`/Latest remove 2>/dev/null | sh");
    if (sts != 0)
	fprintf(stderr, "Warning: folio removal script %d\n", sts);
    __pmCloseSocket(ctlport);
    sts = system(". $PCP_DIR/etc/pcp.env; $PCP_RC_DIR/pcp start");
    if (sts != 0)
	fprintf(stderr, "Warning: stop script returns %d\n", sts);

    sprintf(path, "%s/pmcd_wait", binadm);
    if(access(path, X_OK) == 0) {
        sts = system(". $PCP_DIR/etc/pcp.env; [ -x $PCP_BINADM_DIR/pmcd_wait ] && $PCP_BINADM_DIR/pmcd_wait");
	if (sts != 0)
	    fprintf(stderr, "Warning: pmcd_wait script returns %d\n", sts);
    }

    sts = system(". $PCP_DIR/etc/pcp.env; ( cat common.check; echo _wait_for_pmlogger -P $PCP_LOG_DIR/pmlogger/`hostname`/pmlogger.log ) | sh");
    if (sts != 0)
	fprintf(stderr, "Warning: _wait_for_pmlogger script returns %d\n", sts);
    _ConnectLogger();

    err += exer(numpmid, pmidlist, 1);

    /*
     * more trickery ...
     * need to sleep here for at least 1 second so that timestamp of
     * IPC failure above is more than 1 second ago, so we wait long enough
     * that pmReconnectContext() really does the connect() attempt, rather
     * than returning -ETIMEDOUT immediately ... but sleep(1) is unreliable,
     * hence ...
     */
    sleep(3);

    for (j = 0; j < 2; j++) {
	if (j == 0)
	    i = ctx1;
	else
	    i = ctx0;
	fprintf(stderr, "Reconnect to pmcd context %d ...\n", i);
	if ((n = pmReconnectContext(i)) < 0) {
	    fprintf(stderr, "pmReconnectContext: %s\n", pmErrStr(n));
	    err++;
	}

	err += exer(numpmid, pmidlist, 0);
    }

    fprintf(stderr, "%d unexpected errors.\n", err);
    exit(0);
}
