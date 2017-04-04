/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2015 Ken McDonell.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		errflag = 0;
    char	*str_ver;
    char	buf[12];	/* enough for XXX.XXX.XXX */

    /* trim cmd name of leading directory components */
    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:?")) != EOF) {
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

#if PM_VERSION_CURRENT < PM_VERSION(3,10,5)
    fprintf(stderr, "PCP version 0x%x too old, should be >= 0x30a05\n", PM_VERSION_CURRENT);
#else
    sts = pmGetVersion();
    if (sts != PM_VERSION_CURRENT)
	fprintf(stderr, "Botch: header version 0x%x != pmGetVersion() 0x%x\n", PM_VERSION_CURRENT, sts);
    else
	fprintf(stderr, "pmGetVersion check OK\n");
#endif

    str_ver = pmGetConfig("PCP_VERSION");
    snprintf(buf, sizeof(buf), "%d.%d.%d", (sts&0xff0000)>>16, (sts&0xff00)>>8, (sts&0xff));
    if (strcmp(str_ver, buf) != 0)
	fprintf(stderr, "Botch: pmGetConfig version %s != pmGetVersion() %s\n", str_ver, buf);
    else
	fprintf(stderr, "pmGetConfig check OK\n");

    return 0;
}
