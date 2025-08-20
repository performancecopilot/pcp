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

static int	oflag;
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
    fprintf(stderr, " -o               report byte offset to start of record\n");
    fprintf(stderr, " -v               verbose, more detail\n");
    fprintf(stderr, " -x               dump record in hex\n");
}

/*
 * preamble in buffer for PDU format (not required for archive record)
 * but needed to make offsets for vsets correct
 *
 * len:type:from:... for PDU cf len:... for archive record
 *
 * units for EXTRA are __int32_t words
 */
#define EXTRA 2

int
main(int argc, char *argv[])
{
    __int32_t	len;
    int		buflen = 0;
    __int32_t	*buf = NULL;
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

    while ((c = getopt(argc, argv, "D:ovx")) != EOF) {
	switch (c) {

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmGetProgname(), optarg);
		errflag++;
	    }
	    break;

	case 'o':	/* report byte offsets */
	    oflag = 1;
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
	if (oflag) {
	    if (nrec == 0)
		printf("[%d] len=%d magic=0x%x (version=%d) @ offset=%lld\n", nrec, len, label.magic, version, (long long)offset);
	    else
		printf("[%d] len=%d @ offset=%lld\n", nrec, len, (long long)offset);
	}
	rem = len - sizeof(len);
	if (len > buflen) {
	    if (buf)
		__pmUnpinPDUBuf(buf);
	    buflen = len;
	    buf = (__int32_t *)__pmFindPDUBuf(buflen + EXTRA*sizeof(__int32_t));
	    if (buf == NULL) {
		fprintf(stderr, "Arrgh: __pmFindPDUBuf(%zd) failed\n", EXTRA*sizeof(__int32_t));
		exit(1);
	    }
	}
	buf[EXTRA+0] = htonl(len);
	if ((nb = read(in, &buf[EXTRA+1], rem)) != rem) {
	    if (nb == 0)
		fprintf(stderr, "[%d] body read error: end of file\n", nrec);
	    else if (nb < 0)
		fprintf(stderr, "[%d] body read error: %s\n", nrec, strerror(errno));
	    else
		fprintf(stderr, "[%d] body read error: %d not %d as expected\n", nrec, rem, nb);
	    exit(1);
	}
	
	if (vflag && nrec > 0) {
	    __pmResult	*rp;
	    int		numpmid;
	    int		preamble;
	    int		i;
	    int		j;

	    if (version == PM_LOG_VERS03) {
		__pmTimestamp	stamp;
		__pmLoadTimestamp(&buf[EXTRA+1], &stamp);
		printf("Timestamp: ");
		__pmPrintTimestamp(stdout, &stamp);
		numpmid = ntohl(buf[EXTRA+4]);
		data = &buf[EXTRA+5];
		putchar('\n');
	    }
	    else {
		__pmTimestamp	stamp;
		__pmLoadTimeval(&buf[EXTRA+1], &stamp);
		printf("Timestamp: ");
		__pmPrintTimestamp(stdout, &stamp);
		numpmid = ntohl(buf[EXTRA+3]);
		data = &buf[EXTRA+4];
	    }
	    printf(" numpmid: %d", numpmid);
	    if (numpmid == 0) {
		printf(" <mark>\n");
		goto done;
	    }
	    putchar('\n');

	    if ((rp = __pmAllocResult(numpmid)) == NULL) {
		fprintf(stderr, "__pmAllocResult(%d) failed!\n", numpmid);
		exit(1);
	    }
	    rp->numpmid = numpmid;
	    preamble = (data - buf) * sizeof(data[0]);
	    sts = __pmDecodeValueSet((__pmPDU *)buf, len+sizeof(buf[0]), (__pmPDU *)data, (char *)&buf[((EXTRA*sizeof(__int32_t)+len)/sizeof(buf[0]))-1], numpmid, preamble, preamble, rp->vset);
	    if (sts < 0)
		printf("sts=%d (%s)\n", sts, pmErrStr(sts));
	    else {
		for (i = 0; i < numpmid; i++) {
		    printf("  <%d> %s numval: %d valfmt: %d\n", i, pmIDStr(rp->vset[i]->pmid), rp->vset[i]->numval, rp->vset[i]->valfmt);
		    for (j = 0; j < rp->vset[i]->numval; j++) {
			pmValue	*vp = &rp->vset[i]->vlist[j];
			printf("    inst[%d]: ", vp->inst);
			if (rp->vset[i]->valfmt == PM_VAL_INSITU)
			    pmPrintValue(stdout, PM_VAL_INSITU, PM_TYPE_UNKNOWN, vp, 1);
			else if (rp->vset[i]->valfmt == PM_VAL_DPTR || rp->vset[i]->valfmt == PM_VAL_SPTR)
			    pmPrintValue(stdout, rp->vset[i]->valfmt, (int)vp->value.pval->vtype, vp, 1);
			else
			    printf("bad valfmt %d", rp->vset[i]->valfmt);
			putchar('\n');
		    }
		}
	    }
	    __pmFreeResult(rp);
	}
done:
	
	if (xflag) {
	    int		i;
	    for (i = 0; i < len / sizeof(buf[0]); i++) {
		if ((i % 8) == 0) {
		    if (i > 0)
			putchar('\n');
		    printf("%4d", i);
		}
		printf(" %8x", buf[EXTRA+i]);
	    }
	    putchar('\n');
	}

    }

    __pmLogFreeLabel(&label);

    /* nrec == 0 is the label record */
    printf("%d data records found\n", nrec-1);
    exit(0);
}
