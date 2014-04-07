/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2004 Ken McDonell.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/pmda.h>

static pmdaInterface dispatch;

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		errflag = 0;
    int		id = 0;
    int		port = -1;
    char	*sockname = NULL;

    /* trim cmd name of leading directory components */
    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "c:p:s:?")) != EOF) {
	switch (c) {

	case 'c':	/* case number */
	    id = atoi(optarg);
	    break;

	case 's':	/* socket name */
	    sockname = optarg;
	    break;

	case 'p':	/* port number */
	    port = atoi(optarg);
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag) {
	fprintf(stderr,
"Usage: %s [options]\n\
\n\
Options:\n\
  -c number     case number (default 0)\n\
  -s name       socket name\n\
  -p number     port number\n",
                pmProgname);
        exit(1);
    }

    switch (id) {
	case 0:
	    fprintf(stderr, "--- case 0 pmdaConnect() ---\n");
	    pmdaConnect(&dispatch);
	    sts = dispatch.status;
	    if (sts < 0)
		fprintf(stderr, "pmdaConnect: Error: %s\n", pmErrStr(sts));
	    else
		fprintf(stderr, "pmdaConnect: OK\n");
	    break;

	case 1:
	    fprintf(stderr, "--- case 1 pmdaExt alloc + pmdaConnect() ---\n");
	    dispatch.version.any.ext = (pmdaExt *)calloc(1, sizeof(pmdaExt));
	    pmdaConnect(&dispatch);
	    sts = dispatch.status;
	    if (sts < 0)
		fprintf(stderr, "pmdaConnect: Error: %s\n", pmErrStr(sts));
	    else
		fprintf(stderr, "pmdaConnect: OK\n");
	    break;

	case 2:
	    fprintf(stderr, "--- case 2 pmdaDaemon() + pmdaInet + pmdaConnect() ---\n");
	    pmdaDaemon(&dispatch, PMDA_INTERFACE_LATEST, "badpmda", 123, NULL, NULL);
	    sts = dispatch.status;
	    if (sts < 0)
		fprintf(stderr, "pmdaDaemon: Error: %s\n", pmErrStr(sts));
	    else
		fprintf(stderr, "pmdaDaemon: OK\n");
	    dispatch.version.any.ext->e_io = pmdaInet;
	    dispatch.version.any.ext->e_port = port;
	    pmdaConnect(&dispatch);
	    sts = dispatch.status;
	    if (sts < 0)
		fprintf(stderr, "pmdaConnect: Error: %s\n", pmErrStr(sts));
	    else
		fprintf(stderr, "pmdaConnect: OK\n");
	    break;

	case 3:
	    fprintf(stderr, "--- case 3 pmdaDaemon() + pmdaInet + pmdaConnect() + pmdaConnect() ---\n");
	    pmdaDaemon(&dispatch, PMDA_INTERFACE_LATEST, "badpmda", 123, NULL, NULL);
	    sts = dispatch.status;
	    if (sts < 0)
		fprintf(stderr, "pmdaDaemon: Error: %s\n", pmErrStr(sts));
	    else
		fprintf(stderr, "pmdaDaemon: OK\n");
	    dispatch.version.any.ext->e_io = pmdaUnix;
	    dispatch.version.any.ext->e_sockname = sockname;
	    pmdaConnect(&dispatch);
	    sts = dispatch.status;
	    if (sts < 0)
		fprintf(stderr, "pmdaConnect: Error: %s\n", pmErrStr(sts));
	    else
		fprintf(stderr, "pmdaConnect: OK\n");
	    pmdaConnect(&dispatch);
	    sts = dispatch.status;
	    if (sts < 0)
		fprintf(stderr, "pmdaConnect: Error: %s\n", pmErrStr(sts));
	    else
		fprintf(stderr, "pmdaConnect: OK\n");
	    break;

    }

    return 0;
}
