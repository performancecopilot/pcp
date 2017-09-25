
/*
 * Copyright (c) 2010 Ken McDonell.  All Rights Reserved.
 *
 * Make pmAtomStr jump through hoops
 */

#include <ctype.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/pmda.h>

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		errflag = 0;
    static char	*usage = "[-D debugspec]";
    pmAtomValue	atom;
    char	aggr[] = {
	    '\00', '\01', '\02', '\03', '\04', '\05', '\06', '\07',
	    '\10', '\11', '\12', '\13', '\14', '\15', '\16', '\17'
    			 };
    int		hdl;
    struct timeval	stamp = { 123, 456 };
    struct timespec	hrstamp = { 123456, 78901234 };

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:")) != EOF) {
	switch (c) {

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmProgname, optarg);
		errflag++;
	    }
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag || optind != argc) {
	printf("Usage: %s %s\n", pmProgname, usage);
	exit(1);
    }

    atom.l = -42;
    printf("%d -> %s\n", atom.l, pmAtomStr(&atom, PM_TYPE_32));

    atom.ul = 0x80000000;
    printf("%u -> %s\n", atom.ul, pmAtomStr(&atom, PM_TYPE_U32));

    atom.ll = -1234567890123LL;
    printf("%lld -> %s\n", (long long)atom.ll, pmAtomStr(&atom, PM_TYPE_64));

    atom.ull = 0x8000000000000000LL;
    printf("%llu -> %s\n", (unsigned long long)atom.ull, pmAtomStr(&atom, PM_TYPE_U64));

    atom.f = 123.456;
    printf("%.3f -> %s\n", atom.f, pmAtomStr(&atom, PM_TYPE_FLOAT));

    atom.d = 0.123456789;
    printf("%.9f -> %s\n", atom.d, pmAtomStr(&atom, PM_TYPE_DOUBLE));

    atom.cp = "mary had a little lamb";
    printf("%s-> %s\n", atom.cp, pmAtomStr(&atom, PM_TYPE_STRING));
    atom.cp = NULL;
    printf("%s-> %s\n", atom.cp, pmAtomStr(&atom, PM_TYPE_STRING));
    /* length = 37 */
    atom.cp = "abcdefghijklmnopqrstuvwxyz0123456789X";
    printf("%s -> %s\n", atom.cp, pmAtomStr(&atom, PM_TYPE_STRING));
    /* length = 39 */
    atom.cp = "abcdefghijklmnopqrstuvwxyz0123456789XYZ";
    printf("%s -> %s\n", atom.cp, pmAtomStr(&atom, PM_TYPE_STRING));

    hdl = pmdaEventNewArray();
    atom.vbp = (pmValueBlock *)pmdaEventGetAddr(hdl);
    printf("??? -> %s\n", pmAtomStr(&atom, PM_TYPE_EVENT));
    pmdaEventAddRecord(hdl, &stamp, 0);
    atom.l = -42;
    pmdaEventAddParam(hdl, PM_ID_NULL, PM_TYPE_32, &atom);
    atom.vbp = (pmValueBlock *)pmdaEventGetAddr(hdl);
    printf("??? -> %s\n", pmAtomStr(&atom, PM_TYPE_EVENT));
    pmdaEventAddRecord(hdl, &stamp, 0);
    atom.cp = "hullo world";
    pmdaEventAddParam(hdl, PM_ID_NULL, PM_TYPE_STRING, &atom);
    atom.vbp = (pmValueBlock *)pmdaEventGetAddr(hdl);
    printf("??? -> %s\n", pmAtomStr(&atom, PM_TYPE_EVENT));

    hdl = pmdaEventNewHighResArray();
    atom.vbp = (pmValueBlock *)pmdaEventGetAddr(hdl);
    printf("??? -> %s\n", pmAtomStr(&atom, PM_TYPE_HIGHRES_EVENT));
    pmdaEventAddHighResRecord(hdl, &hrstamp, 0);
    atom.l = -42;
    pmdaEventAddParam(hdl, PM_ID_NULL, PM_TYPE_32, &atom);
    atom.vbp = (pmValueBlock *)pmdaEventGetAddr(hdl);
    printf("??? -> %s\n", pmAtomStr(&atom, PM_TYPE_HIGHRES_EVENT));
    pmdaEventAddHighResRecord(hdl, &hrstamp, 0);
    atom.cp = "hullo world";
    pmdaEventAddParam(hdl, PM_ID_NULL, PM_TYPE_STRING, &atom);
    atom.vbp = (pmValueBlock *)pmdaEventGetAddr(hdl);
    printf("??? -> %s\n", pmAtomStr(&atom, PM_TYPE_HIGHRES_EVENT));

    atom.vbp = (pmValueBlock *)malloc(PM_VAL_HDR_SIZE + sizeof(aggr));
    atom.vbp->vlen = PM_VAL_HDR_SIZE + sizeof(aggr);
    atom.vbp->vtype = PM_TYPE_AGGREGATE_STATIC;
    memcpy(atom.vbp->vbuf, (void *)aggr, sizeof(aggr));
    printf("??? -> %s\n", pmAtomStr(&atom, PM_TYPE_AGGREGATE_STATIC));
    atom.vbp->vtype = PM_TYPE_AGGREGATE;
    for (atom.vbp->vlen = PM_VAL_HDR_SIZE; atom.vbp->vlen <= PM_VAL_HDR_SIZE + sizeof(aggr); atom.vbp->vlen += 2) {
	printf("??? [len=%d] -> %s\n", atom.vbp->vlen - PM_VAL_HDR_SIZE, pmAtomStr(&atom, PM_TYPE_AGGREGATE));
    }
    free(atom.vbp);
    atom.vbp = NULL;
    printf("NULL -> %s\n", pmAtomStr(&atom, PM_TYPE_AGGREGATE));

    printf("\nAnd now some error cases ...\n");
    printf("bad type -> %s\n", pmAtomStr(&atom, 123));
    printf("no support type -> %s\n", pmAtomStr(&atom, PM_TYPE_NOSUPPORT));
    printf("unknown type -> %s\n", pmAtomStr(&atom, PM_TYPE_UNKNOWN));

    exit(0);
}
