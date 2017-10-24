/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
 */

/*
 * grind_conv - exercise pmConvScale, pmAtomStr, pmUnitsStr
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/pmda.h>

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		errflag = 0;
    static char	*usage = "[-D debug] type value iunit ounit\n"
"\n"
"iunit and ounit are in the 6-integer format:\n"
"dimspace:dimtime:dimcount:scalespace:scaletime:scalecount\n";
    pmUnits	iu;
    pmUnits	ou;
    int		type;
    int		vbase;
    pmAtomValue	iv;
    pmAtomValue	ov;
    char	*vp;
    char	*q;

    __pmSetProgname(argv[0]);

    /* stop at type arg, so value may have leading "-" */
    putenv("POSIXLY_CORRECT=yes");

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

    if (errflag || argc - optind != 4) {
	fprintf(stderr, "Usage: %s %s\n", pmProgname, usage);
	exit(1);
    }



    /* non-flag args are argv[optind] ... argv[argc-1] */
    type = atoi(argv[optind]);
    optind++;

    if (strncmp(argv[optind], "0x", 2) == 0) {
	vp = &argv[optind][2];
	vbase = 16;
    }
    else {
	vp = argv[optind];
	vbase = 10;
    }

    q = vp;
    switch (type) {
	case PM_TYPE_32:
	    iv.l = strtol(vp, &q, vbase);
	    break;
	case PM_TYPE_U32:
	    iv.ul = strtoul(vp, &q, vbase);
	    break;
	case PM_TYPE_64:
	    iv.ll = strtoll(vp, &q, vbase);
	    break;
	case PM_TYPE_U64:
	    iv.ull = strtoull(vp, &q, vbase);
	    break;
	case PM_TYPE_FLOAT:
	    sts = sscanf(vp, "%f", &iv.f);
	    if (sts == 1) q = "";
	    break;
	case PM_TYPE_DOUBLE:
	    sts = sscanf(vp, "%lf", &iv.d);
	    if (sts == 1) q = "";
	    break;
	default:
	case PM_TYPE_STRING:
	    iv.cp = vp;
	    q = "";
	    break;
	case PM_TYPE_AGGREGATE:
	case PM_TYPE_AGGREGATE_STATIC:
	    iv.vbp = (pmValueBlock *)malloc(PM_VAL_HDR_SIZE+strlen(vp));
	    iv.vbp->vlen = PM_VAL_HDR_SIZE+strlen(vp);
	    iv.vbp->vtype = type;
	    strncpy(iv.vbp->vbuf, vp, strlen(vp));
	    q = "";
	    break;
	case PM_TYPE_EVENT:	// ignore the value, force 0 event records
	    iv.vbp = (pmValueBlock *)malloc(sizeof(pmEventArray)-sizeof(pmEventRecord));
	    iv.vbp->vlen = sizeof(pmEventArray)-sizeof(pmEventRecord);
	    iv.vbp->vtype = type;
	    memset((void *)iv.vbp->vbuf, 0, sizeof(int));
	    q = "";
	    break;
	case PM_TYPE_HIGHRES_EVENT:	// ignore the value, force 0 event records
	    iv.vbp = (pmValueBlock *)malloc(sizeof(pmHighResEventArray)-sizeof(pmHighResEventRecord));
	    iv.vbp->vlen = sizeof(pmHighResEventArray)-sizeof(pmHighResEventRecord);
	    iv.vbp->vtype = type;
	    memset((void *)iv.vbp->vbuf, 0, sizeof(int));
	    q = "";
	    break;
    }
    optind++;

    if (*q != '\0') {
	fprintf(stderr, "Value botched @ %s\n", q);
	exit(1);
    }

    vp = argv[optind];
    iu.dimSpace = strtol(vp, &q, 10);
    if (*q != ':') goto bad_in;
    vp = ++q;
    iu.dimTime = strtol(vp, &q, 10);
    if (*q != ':') goto bad_in;
    vp = ++q;
    iu.dimCount = strtol(vp, &q, 10);
    if (*q != ':') goto bad_in;
    vp = ++q;
    iu.scaleSpace = strtol(vp, &q, 10);
    if (*q != ':') goto bad_in;
	vp = ++q;
    iu.scaleTime = strtol(vp, &q, 10);
    if (*q != ':') goto bad_in;
	vp = ++q;
    iu.scaleCount = strtol(vp, &q, 10);
    if (*q != '\0') goto bad_in;
    optind++;

    vp = argv[optind];
    ou.dimSpace = strtol(vp, &q, 10);
    if (*q != ':') goto bad_out;
    vp = ++q;
    ou.dimTime = strtol(vp, &q, 10);
    if (*q != ':') goto bad_out;
    vp = ++q;
    ou.dimCount = strtol(vp, &q, 10);
    if (*q != ':') goto bad_out;
    vp = ++q;
    ou.scaleSpace = strtol(vp, &q, 10);
    if (*q != ':') goto bad_out;
	vp = ++q;
    ou.scaleTime = strtol(vp, &q, 10);
    if (*q != ':') goto bad_out;
	vp = ++q;
    ou.scaleCount = strtol(vp, &q, 10);
    if (*q != '\0') goto bad_out;

    printf("type=%d input units=%s value=%s\n", type, pmUnitsStr(&iu), pmAtomStr(&iv, type));

    if ((sts = pmConvScale(type, &iv, &iu, &ov, &ou)) < 0)
	printf("pmConvScale Error: %s\n", pmErrStr(sts));
    else
	printf("output units=%s value=%s\n", pmUnitsStr(&ou), pmAtomStr(&ov, type));

    exit(0);

bad_in:
    fprintf(stderr, "Input units botch @ %s\n", q);
    exit(1);

bad_out:
    fprintf(stderr, "Output units botch @ %s\n", q);
    exit(1);
}

