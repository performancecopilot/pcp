/*
 * Scan an archive data file from disk, no libpcp layers in the
 * way.
 *
 * Copyright (c) 2018 Ken McDonell.  All Rights Reserved.
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pcp/pmapi.h>
#include "libpcp.h"

/* from internal.h ... */
#ifdef HAVE_NETWORK_BYTEORDER
#define __ntohpmID(a)           (a)
#else
#define __ntohpmID(a)           ntohl(a)
#endif

static int	vflag;
static int	xflag;
static int	nrec;
static int	version;

static off_t	offset;

static __pmLogLabel	label;

void
usage(void)
{
    fprintf(stderr, "Usage: %s [options] in.0\n", pmGetProgname());
    fprintf(stderr, "Options:\n");
    fprintf(stderr, " -D debug         set debug options\n");
    fprintf(stderr, " -v               verbose, more detail\n");
    fprintf(stderr, " -x               dump record in hex\n");
}

int
main(int argc, char *argv[])
{
    __int32_t	len;
    int		buflen;
    __int32_t	*buf;
    __int32_t	*data;
    int		in;
    int		nb;
    int		rem;
    int		c;
    int		sts;
    int		errflag = 0;
    __pmFILE	*f;

    pmSetProgname(argv[0]);
    setlinebuf(stdout);
    setlinebuf(stderr);

    /*
     * we don't have a context, so force reporting of timestamps
     * relative to UTC
     */
    putenv("TZ=UTC");

    while ((c = getopt(argc, argv, "D:vx")) != EOF) {
	switch (c) {

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmGetProgname(), optarg);
		errflag++;
	    }
	    break;

	case 'v':	/* more detail */
	    vflag = 1;
	    break;

	case 'x':	/* dump records in hex */
	    xflag = 1;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag || optind != argc-1) {
	usage();
	exit(1);
    }

    if ((in = open(argv[optind], O_RDONLY)) < 0) {
	fprintf(stderr, "Failed to open %s: %s\n", argv[optind], strerror(errno));
	exit(1);
    }
    buflen = 0;
    buf = (__int32_t *)malloc(buflen);
    if (buf == NULL) {
	fprintf(stderr, "Arrgh: buf malloc failed\n");
	exit(1);
    }

    if ((f = __pmFopen(argv[optind], "r")) == NULL) {
	fprintf(stderr, "Failed to __pmFopen %s: %s\n", argv[optind], strerror(errno));
	exit(1);
    }
    memset((void *)&label, 0, sizeof(label));
    if ((sts = __pmLogLoadLabel(f, &label)) < 0) {
	fprintf(stderr, "error: %s does not start with label record, not a PCP archive file?\n", argv[optind]);
	exit(1);
    }
    __pmFclose(f);
    version = label.magic & 0xff;

    for (nrec = 0; ; nrec++) {
	offset = lseek(in, 0, SEEK_CUR);
	if ((nb = read(in, &len, sizeof(len))) != sizeof(len)) {
	    if (nb == 0) {
		if (nrec == 0)
		    fprintf(stderr, "error: %s is empty file\n", argv[optind]);
		break;
	    }
	    if (nb < 0)
		fprintf(stderr, "[%d] header len read error: %s\n", nrec, strerror(errno));
	    else
		fprintf(stderr, "[%d] header len read error: %d not %zd as expected\n", nrec, nb, sizeof(len));
	    exit(1);
	}
	len = ntohl(len);
	if (vflag) {
	    if (nrec == 0)
		printf("[%d] len=%d magic=0x%x (version=%d) @ offset=%lld\n", nrec, len, label.magic, version, (long long)offset);
	    else
		printf("[%d] len=%d @ offset=%lld\n", nrec, len, (long long)offset);
	}
	rem = len - sizeof(len);
	if (len > buflen) {
	    buflen = len;
	    buf = (__int32_t *)realloc(buf, buflen);
	    if (buf == NULL) {
		fprintf(stderr, "Arrgh: buf realloc failed\n");
		exit(1);
	    }
	}
	buf[0] = htonl(len);
	if ((nb = read(in, &buf[1], rem)) != rem) {
	    if (nb == 0)
		fprintf(stderr, "[%d] body read error: end of file\n", nrec);
	    else if (nb < 0)
		fprintf(stderr, "[%d] body read error: %s\n", nrec, strerror(errno));
	    else
		fprintf(stderr, "[%d] body read error: %d not %d as expected\n", nrec, rem, nb);
	    exit(1);
	}
	
	if (vflag && nrec > 0) {
	    __pmResult	result;
	    int		preamble;
	    int		i;
	    int		j;
	    if (version == PM_LOG_VERS03) {
		__pmTimestamp	stamp;
		__pmLoadTimestamp(&buf[1], &stamp);
		printf("Timestamp: ");
		__pmPrintTimestamp(stdout, &stamp);
		result.numpmid = ntohl(buf[4]);
		data = &buf[5];
		putchar('\n');
	    }
	    else {
		__pmTimestamp	stamp;
		__pmLoadTimeval(&buf[1], &stamp);
		printf("Timestamp: ");
		__pmPrintTimestamp(stdout, &stamp);
		result.numpmid = ntohl(buf[3]);
		data = &buf[4];
	    }
	    printf(" numpmid: %d\n", result.numpmid);
	    preamble = (data - buf + 2) * sizeof(data[0]);
	    sts = __pmDecodeValueSet((__pmPDU *)&buf[-2], len+sizeof(buf[0]), (__pmPDU *)data, (char *)&buf[((len+2)/sizeof(buf[0]))-1], result.numpmid, preamble, preamble, result.vset);
	    if (sts < 0)
		printf("sts=%d (%s)\n", sts, pmErrStr(sts));
	    else {
		for (i = 0; i < result.numpmid; i++) {
		    printf("[%d] %s numval: %d valfmt: %d\n", i, pmIDStr(result.vset[i]->pmid), result.vset[i]->numval, result.vset[i]->valfmt);
		    for (j = 0; j < result.vset[i]->numval; j++) {
			pmValue	*vp = &result.vset[i]->vlist[j];
			printf("    inst[%d]: ", vp->inst);
			if (result.vset[i]->valfmt == PM_VAL_INSITU)
			    pmPrintValue(stdout, PM_VAL_INSITU, PM_TYPE_UNKNOWN, vp, 1);
			else if (result.vset[i]->valfmt == PM_VAL_DPTR || result.vset[i]->valfmt == PM_VAL_SPTR)
			    pmPrintValue(stdout, result.vset[i]->valfmt, (int)vp->value.pval->vtype, vp, 1);
			else
			    printf("bad valfmt %d", result.vset[i]->valfmt);
			putchar('\n');
		    }
		}
		__pmFreeResult(&result);
	    }
	}
	
	if (xflag) {
	    int		i;
	    for (i = 0; i < len / sizeof(buf[0]); i++) {
		if ((i % 8) == 0) {
		    if (i > 0)
			putchar('\n');
		    printf("%4d", i);
		}
		printf(" %8x", buf[i]);
	    }
	    putchar('\n');
	}

    }

    __pmLogFreeLabel(&label);

    printf("%d data records found\n", nrec);
    exit(0);
}
