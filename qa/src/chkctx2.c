/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

#define SOURCE handle == 0 ? "host" : ( type == PM_CONTEXT_ARCHIVE ? "archive" : "host" )
#define HOST handle == 0 ? "localhost" : host

/*
 * context and profile exerciser
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>

static int inst_bin[] = { 100, 200, 300, 400, 500, 600, 700, 800, 900 };

static int xpect_bin[] = { 1 };
static int xpect_colour[] = { 0 };

static char *namelist[] = {
    "sampledso.bin",
    "sampledso.colour"
};

/*
 * handle	profile				values expected
 *   0		bin=500, no colour		1 bin	0 colour
 */

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		errflag = 0;
    int		type = 0;
    char	*host = "localhost";
    char	*namespace = PM_NS_DEFAULT;
    pmInDom	indom_bin, indom_colour;
    pmID	metrics[2];
    pmResult	*resp;
    pmDesc	desc;
    int		handle;
#ifdef PCP_DEBUG
    static char	*debug = "[-D N] ";
#else
    static char	*debug = "";
#endif
    static char	*usage = "[-a archive] [-h hostname] [-L] [-n namespace]";

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "a:D:h:Ln:")) != EOF) {
	switch (c) {

	case 'a':	/* archive name */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a, -h and -L allowed\n", pmProgname);
		errflag++;
	    }
	    type = PM_CONTEXT_ARCHIVE;
	    host = optarg;
	    break;

#ifdef PCP_DEBUG
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
#endif

	case 'h':	/* hostname for PMCD to contact */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a, -h and -L allowed\n", pmProgname);
		errflag++;
	    }
	    host = optarg;
	    type = PM_CONTEXT_HOST;
	    break;

	case 'L':	/* local mode, no PMCD */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a, -h and -L allowed\n", pmProgname);
		errflag++;
	    }
	    host = NULL;
	    type = PM_CONTEXT_LOCAL;
	    break;

	case 'n':	/* alternative name space file */
	    namespace = optarg;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag) {
	fprintf(stderr, "Usage: %s %s%s\n", pmProgname, debug, usage);
	exit(1);
    }

    if ((sts = pmLoadASCIINameSpace(namespace, 1)) < 0) {
	printf("%s: Cannot load namespace from \"%s\": %s\n", pmProgname, namespace, pmErrStr(sts));
	exit(1);
    }
    if ((sts = pmLookupName(2, namelist, metrics)) < 0) {
	fprintf(stderr, "pmLookupName: %s\n", pmErrStr(sts));
	fprintf(stderr, "pmids: 0x%x 0x%x\n", metrics[0], metrics[1]);
	exit(1);
    }

    /* make context 0 the default localhost one */
    if ((sts = pmNewContext(PM_CONTEXT_HOST, "localhost")) < 0) {
	fprintf(stderr, "pmNewContext(..., \"localhost\"): %s\n",
		pmErrStr(sts));
	exit(1);
    }

    if (type == 0)
	type = PM_CONTEXT_HOST;		/* default */

    if (type == PM_CONTEXT_HOST) {
	if ((sts = pmNewContext(PM_CONTEXT_HOST, host)) < 0) {
	    fprintf(stderr, "handle: pmNewContext(host=%s): %s\n", host, pmErrStr(sts));
	    exit(1);
	}
    }
    else if (type == PM_CONTEXT_LOCAL) {
	if ((sts = pmNewContext(PM_CONTEXT_LOCAL, host)) < 0) {
	    fprintf(stderr, "handle: pmNewContext(local): %s\n", pmErrStr(sts));
	    exit(1);
	}
    }
    else {
	if ((sts = pmNewContext(PM_CONTEXT_ARCHIVE, host)) < 0) {
	    fprintf(stderr, "handle: pmNewContext(archive=%s): %s\n", host, pmErrStr(sts));
	    exit(1);
	}
    }
    handle = sts;

    while (handle >= 0) {
	pmUseContext(handle);

	if ((sts = pmLookupDesc(metrics[0], &desc)) < 0) {
	    fprintf(stderr, "pmLookupDesc: context=%d %s=%s %s: %s\n",
		handle, SOURCE, HOST, namelist[0], pmErrStr(sts));
	    exit(1);
	}
	indom_bin = desc.indom;
	if ((sts = pmLookupDesc(metrics[1], &desc)) < 0) {
	    fprintf(stderr, "pmLookupDesc: context=%d %s=%s %s: %s\n",
		handle, SOURCE, HOST, namelist[1], pmErrStr(sts));
	    exit(1);
	}
	indom_colour = desc.indom;

	pmDelProfile(indom_bin, 0, (int *)0);
	pmAddProfile(indom_bin, 1, &inst_bin[4]);
	pmDelProfile(indom_colour, 0, (int *)0);

	sts = pmFetch(2, metrics, &resp);
	if (sts < 0) {
	    fprintf(stderr, "botch @ context=%d %s=%s: pmFetch: %s\n",
		    handle, SOURCE, HOST, pmErrStr(sts));
	}
	else {
	    if (resp->numpmid != 2) {
		fprintf(stderr, "botch @ context=%d %s=%s: numpmid %d != 2\n",
			handle, SOURCE, HOST, resp->numpmid);
	    }
	    else {
		if (resp->vset[0]->numval != xpect_bin[0]) {
		    fprintf(stderr, "botch @ context=%d %s=%s: [indom %s] numval got: %d expect: %d\n",
			handle, SOURCE, HOST, pmInDomStr(indom_bin),
			resp->vset[0]->numval, xpect_bin[0]);
		}
		if (resp->vset[1]->numval != xpect_colour[0]) {
		    fprintf(stderr, "botch @ context=%d %s=%s: [indom %s] numval got: %d expect: %d\n",
			handle, SOURCE, HOST, pmInDomStr(indom_colour),
			resp->vset[1]->numval, xpect_colour[0]);
		}
	    }
	    pmFreeResult(resp);
	}
	if (handle) {
	    if ((sts = pmDestroyContext(handle)) < 0)
		fprintf(stderr, "pmDestroyContext %d %s=%s: %s\n",
		    handle, SOURCE, HOST, pmErrStr(sts));
	}
	handle--;
    }

    exit(0);
}
