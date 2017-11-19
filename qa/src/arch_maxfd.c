/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 *
 * Exercise pmNewContext() for archives close to the NOFILE max fd limit.
 * For incident: 504616
 */

#include <pcp/pmapi.h>
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#include "localconfig.h"

#if PCP_VER < 2200
#define PRINTF_P_PFX ""
#endif

static char	*sfx[] = { "0", "index", "meta" };

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		errflag = 0;
    int		i;
    int		j;
    int		max_ctx;
    int		max_nofile;
    int		numopen = 0;			/* pander to gcc */
    int		ctx = -1;			/* pander to gcc */
    int		last_ctx;
    char	buf[100];
    char	lbuf[100];
    struct rlimit	top;
    char	*start = NULL;
    char	*end;

    pmSetProgname(pmGetProgname());

    while ((c = getopt(argc, argv, "D:?")) != EOF) {
	switch (c) {

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmGetProgname(), optarg);
		errflag++;
	    }
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    sts = getrlimit(RLIMIT_NOFILE, &top);

    for (max_nofile = 30; max_nofile > 20; max_nofile--) {
	top.rlim_cur = max_nofile;
	sts = setrlimit(RLIMIT_NOFILE, &top);
	if (sts < 0) {
	    fprintf(stderr, "setrlimit(NOFILE=%d) failed: %s\n", max_nofile, strerror(errno));
	    exit(1);
	}
	sts = dup(0);
	if (sts < 0) {
	    fprintf(stderr, "dup(0) failed: %s\n", strerror(errno));
	    exit(1);
	}
	max_ctx = (max_nofile + 2 - sts) / 3;
	close(sts);

	printf("max fd: %d max ctx#: %d\n", max_nofile, max_ctx);
	last_ctx = -1;

	for (i = 0; i <= max_ctx; i++) {

	    for (j = 0; j < 3; j++) {
		pmsprintf(lbuf, sizeof(lbuf), "qa-tmp-%d.%s", i, sfx[j]);
		pmsprintf(buf, sizeof(buf), "%s.%s", argv[optind], sfx[j]);
		sts = link(buf, lbuf);
		if (sts < 0) {
		    fprintf(stderr, "link %s -> %s failed: %s\n",
			lbuf, buf, strerror(errno));
		    break;
		}
	    }

	    pmsprintf(lbuf, sizeof(lbuf), "qa-tmp-%d", i);
	    ctx = pmNewContext(PM_CONTEXT_ARCHIVE, lbuf);

	    for (j = 0; j < 3; j++) {
		pmsprintf(lbuf, sizeof(lbuf), "qa-tmp-%d.%s", i, sfx[j]);
		sts = unlink(lbuf);
		if (sts < 0) {
		    fprintf(stderr, "unlink %s failed: %s\n",
			lbuf, strerror(errno));
		    break;
		}
	    }

	    if (ctx < 0) {
		printf("pmNewContext(): %s\n", pmErrStr(ctx));
		if (i != max_ctx && i != max_ctx-1)
		    printf("Error: failure after ctx# %d, expected after %d or %d\n", last_ctx, max_ctx, max_ctx-1);
		break;
	    }
	    else
		numopen++;
	    last_ctx = ctx;
	}

	if (ctx >= 0)
	    printf("Error: pmNewContext() did not fail?\n");

	for (i = 0; i <= last_ctx; i++)
	    pmDestroyContext(i);

	if (start == NULL) {
	    start = sbrk(0);
	    numopen = 0;
	}
    }

    end = sbrk(0);

    if (end - start > 16*1024) {
	printf("Memory leak? after first pass, %ld bytes per archive open-close\n",
	    (long)((end - start) / numopen));
	printf("start: " PRINTF_P_PFX "%p end: " PRINTF_P_PFX "%p diff: %ld numopen: %d\n", start, end,
		(long)(end - start), numopen);
    }
    
    return 0;
}
