/*
 * Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
 *
 * exercise low-level libpcp locking primitives
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>

static void
timeout(int i, void *j)
{
  fprintf(stderr, "Timeout!\n");
  exit(2);
}

static void
_usage()
{
    fprintf(stderr, "Usage: exerlock reqs\n");
    fprintf(stderr, "reqs is a string of: i (initialize), l (lock) and u (unlock)\n");
    exit(1);
}

int
main(int argc, char **argv)
{
    char		*p;
    struct timeval	delta = { 1, 0 };

    if (argc != 2) _usage();

    __pmAFregister(&delta, NULL, timeout);
#ifdef DBG_TRACE_DESPERATE
    pmDebug |= DBG_TRACE_DESPERATE;
#endif

    for (p = argv[1]; *p; p++) {
	switch (*p) {
	    case 'i':
	    	fprintf(stderr, "initialize\n");
		PM_INIT_LOCKS();
		break;
	    case 'l':
	    	fprintf(stderr, "lock\n");
		PM_LOCK(__pmLock_libpcp);
		break;
	    case 'u':
	    	fprintf(stderr, "unlock\n");
		PM_UNLOCK(__pmLock_libpcp);
		break;
	    default:
		_usage();
	}
    }

    return 0;
}
