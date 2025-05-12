/*
 * Scan an archive metadata file from disk, no libpcp layers in the
 * way.
 *
 * Optionally warn about:
 * - same indom records with duplicated timestamps
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
#define __ntohpmInDom(a)        (a)
#define __ntohpmID(a)           (a)
#define __ntohpmUnits(a)        (a)
#define __ntohll(a)             /* noop */
#else
#define __ntohpmInDom(a)        ntohl(a)
#define __ntohpmID(a)           ntohl(a)
#endif

/* from endian.c ... */
#ifndef __ntohpmUnits
pmUnits
__ntohpmUnits(pmUnits units)
{
    unsigned int	x;

    x = ntohl(*(unsigned int *)&units);
    units = *(pmUnits *)&x;

    return units;
}
#endif

static int	hflag;
static int	iflag;
static int	lflag;
static int	mflag;
static int	oflag;
static int	tflag;
static int	wflag;
static int	xflag;
static int	nrec;
static int	version;

static off_t	offset;

static __pmLogLabel	label;

void
usage(void)
{
    fprintf(stderr, "Usage: %s [options] in.meta\n", pmGetProgname());
    fprintf(stderr, "Options:\n");
    fprintf(stderr, " -a               report all, i.e. -hilm\n");
    fprintf(stderr, " -D debug         set debug options\n");
    fprintf(stderr, " -h               report (help) text records\n");
    fprintf(stderr, " -i               report instance domain records\n");
    fprintf(stderr, " -l               report label records\n");
    fprintf(stderr, " -m               report metric records [default]\n");
    fprintf(stderr, " -o               report byte offset to start of record\n");
    fprintf(stderr, " -t               report metadata record types\n");
    fprintf(stderr, " -w               only warn about badness\n");
    fprintf(stderr, " -W               only warn verbosely about badness\n");
    fprintf(stderr, " -x               dump record in hex\n");
    fprintf(stderr, " -z               set reporting timezone to pmcd from archive\n");
    fprintf(stderr, " -Z timezone      set reporting timezone\n");
}

/*
 * linked list of indoms at the same time
 */
typedef struct elt {
    struct elt	*next;
    pmInDom	indom;
    int		numinst;
    int		*inst;
    char	**iname;
} elt_t;

void
unpack_indom(elt_t *ep, __pmLogInDom *lidp)
{
    int		j;

    if (ep->numinst > 0) {
	ep->inst = (int *)malloc(lidp->numinst * sizeof(int));
	if (ep->inst == NULL) {
	    fprintf(stderr, "Arrgh: inst[] malloc failed for indom %s\n", pmInDomStr(lidp->indom));
	    exit(1);
	}
	ep->iname = (char **)malloc(lidp->numinst * sizeof(char *));
	if (ep->iname == NULL) {
	    fprintf(stderr, "Arrgh: iname[] malloc failed for indom %s\n", pmInDomStr(lidp->indom));
	    exit(1);
	}
	for (j = 0; j < lidp->numinst; j++) {
	    ep->inst[j] = lidp->instlist[j];
	    if (lidp->namelist[j] != NULL) {
		ep->iname[j] = strdup(lidp->namelist[j]);
		if (ep->iname[j] == NULL) {
		    fprintf(stderr, "Arrgh: iname[%d] malloc failed for indom %s\n", j, pmInDomStr(lidp->indom));
		    exit(1);
		}
		if (pmDebugOptions.appl0) {
		    fprintf(stderr, "unpack (%d) inst=%d iname=%s\n", j, ep->inst[j], ep->iname[j]);
		}
	    }
	    else {
		/* assume TYPE_INDOM_DELTA */
		ep->iname[j] = NULL;
		if (pmDebugOptions.appl0) {
		    fprintf(stderr, "unpack (%d) inst=%d\n", j, ep->inst[j]);
		}
	    }
	}
    }
}

void
free_elt_fields(elt_t *ep)
{
    if (ep->inst != NULL)
	free(ep->inst);
    if (ep->iname != NULL) {
	int	j;
	for (j = 0; j < ep->numinst; j++) {
	    if (ep->iname[j] != NULL)
		free(ep->iname[j]);
	}
	free(ep->iname);
    }
}

void
do_indom(__int32_t *buf, int type)
{
    int				sts;
    static __pmTimestamp	prior_stamp = { 0, 0 };
    static elt_t		*head = NULL;
    static elt_t		dup = { NULL, 0, 0, NULL, NULL };
    static int			ndup = 0;
    __pmLogInDom		lid;
    int				warn;
    elt_t			*ep = NULL;	/* pander to gcc */
    elt_t			*tp;
    elt_t			*dp = &dup;

    if ((sts = __pmLogLoadInDom(NULL, 0, type, &lid, &buf)) < 0) {
	fprintf(stderr, "__pmLoadLoadInDom: failed: %s\n", pmErrStr(sts));
	return;
    }

    if (pmDebugOptions.appl0) {
	int	i;
	fprintf(stderr, "indom type=%s (%d) numinst=%d\n", __pmLogMetaTypeStr(type), type, lid.numinst);
	for (i = 0; i < lid.numinst; i++) {
	    if (lid.namelist[i] != NULL)
		fprintf(stderr, "(%d) inst=%d name=\"%s\"\n", i, lid.instlist[i], lid.namelist[i]);
	    else
		fprintf(stderr, "(%d) inst=%d\n", i, lid.instlist[i]);
	}
    }

    warn = 0;
    if (prior_stamp.sec != 0) {
	/*
	 * Not the first indom record, so check for duplicate timestamps
	 * for the same indom.
	 * Use a linked list of indoms previously seen.
	 */
	if (__pmTimestampSub(&lid.stamp, &prior_stamp) == 0) {
	    /* same timestamp as previous indom */
	    for (ep = head; ep != NULL; ep = ep->next) {
		if (ep->indom == lid.indom) {
		    /* indom match => duplicate */
		    ndup++;
		    warn++;
		    break;
		}
	    }
	    if (ep == NULL) {
		/*
		 * indom not seen before at this timestamp, add
		 * to head of linked list
		 */
		ep = (elt_t *)malloc(sizeof(elt_t));
		if (ep == NULL) {
		    fprintf(stderr, "Arrgh: elt malloc failed for indom %s @ ", pmInDomStr(lid.indom));
		    __pmPrintTimestamp(stderr, &lid.stamp);
		    exit(1);
		}
		ep->next = head;
		head = ep;
	    }
	}
	else {
	    /* new timestamp, clear linked list */
	    for (ep = head; ep != NULL; ) {
		free_elt_fields(ep);
		tp = ep->next;
		free(ep);
		ep = tp;
	    }
	    ep = head = (elt_t *)malloc(sizeof(elt_t));
	    if (head == NULL) {
		fprintf(stderr, "Arrgh: head malloc failed for indom %s @ ", pmInDomStr(lid.indom));
		__pmPrintTimestamp(stderr, &lid.stamp);
		exit(1);
	    }
	    ep->next = NULL;
	    ndup = 0;
	}
	ep->indom = lid.indom;
	ep->numinst = lid.numinst;
	ep->inst = NULL;
	ep->iname = NULL;
	if (wflag > 0) {
	    /* -w or -W, so unpack indom */
	    unpack_indom(ep, &lid);
	}
    }
    if (iflag || (warn && wflag > 0)) {
	/* if warn is set, ep must have been assigned a value */
	printf("[%d] ", nrec);
	if (tflag)
	    printf("<%s> ", __pmLogMetaTypeStr(type));
	if (oflag)
	    printf("+%ld ", (long)offset);
	printf("@ ");
	__pmPrintTimestamp(stdout, &lid.stamp);
	if (type == TYPE_INDOM_DELTA)
	    printf(" delta");
	printf(" indom %s numinst %d", pmInDomStr(lid.indom), lid.numinst);
	if (warn && wflag > 0) {
	    int	o, d;
	    int	diffs = 0;
	    printf(" duplicate #%d\n", ndup);
	    dp->indom = lid.indom;
	    dp->numinst = lid.numinst;
	    dp->inst = NULL;
	    unpack_indom(dp, &lid);
	    if (type == TYPE_INDOM_DELTA) {
		for (d = 0; d < dp->numinst; d++) {
		    if (dp->iname[d] == NULL)
			printf("  inst %d: delta dropped\n", -dp->inst[d]);
		    else
			printf("  inst %d: delta added (\"%s\")\n", dp->inst[d], dp->iname[d]);
		}
	    }
	    else {
		if (ep->numinst != dp->numinst)
		    printf("  numinst changed from %d to %d\n", ep->numinst, dp->numinst);
		for (o = 0; o < ep->numinst; o++) {
		    for (d = 0; d < dp->numinst; d++) {
			if (ep->inst[o] == dp->inst[d]) {
			    if (strcmp(ep->iname[o], dp->iname[d]) != 0) {
				printf("  inst %d: changed ext name from \"%s\" to \"%s\"\n", ep->inst[o], ep->iname[o], dp->iname[d]);
				diffs++;
			    }
#ifdef DEBUG
			    else
				printf("  inst %d: same\n", ep->inst[o]);
#endif
			    dp->inst[d] = -1;
			    break;
			}
		    }
		    if (d == dp->numinst) {
			printf("  inst %d: dropped (\"%s\")\n", ep->inst[o], ep->iname[o]);
			diffs++;
		    }
		}
		for (d = 0; d < dp->numinst; d++) {
		    if (dp->inst[d] != -1) {
			printf("  inst %d: added (\"%s\")\n", dp->inst[d], dp->iname[d]);
			diffs++;
		    }
		}
		if (diffs == 0)
		    printf(" no differences\n");
	    }
	    free_elt_fields(dp);
	}
	else
	    putchar('\n');
    }
    prior_stamp = lid.stamp;

    __pmFreeLogInDom(&lid);
}

void
do_desc(__int32_t *buf)
{
    pmDesc	*dp;
    int		numnames;
    int		i;
    int		len;
    char	*cp;
    char	**names;

    dp = (pmDesc *)buf;
    dp->type = ntohl(dp->type);

    dp->sem = ntohl(dp->sem);
    dp->indom = __ntohpmInDom(dp->indom);
    dp->units = __ntohpmUnits(dp->units);
    dp->pmid = __ntohpmID(dp->pmid);
    printf("[%d] ", nrec);
    if (tflag)
	printf("<%s> ", __pmLogMetaTypeStr(TYPE_DESC));
    if (oflag)
	printf("+%ld ", (long)offset);
    printf("metric %s (",  pmIDStr(dp->pmid));
    numnames = ntohl(buf[sizeof(pmDesc)/sizeof(int)]);
    names = (char **)malloc(numnames*sizeof(char *));
    if (names == NULL) {
	fprintf(stderr, "Arrgh: names x %d malloc failed\n", numnames);
	exit(1);
    }
    for (i = 0; i < numnames; i++) {
	names[i] = NULL;
    }
    cp = (char *)&buf[sizeof(pmDesc)/sizeof(int) + 1];
    for (i = 0; i < numnames; i++) {
	memmove(&len, cp, sizeof(len));
	len = htonl(len);
	cp += sizeof(len);
	names[i] = (char *)malloc(len + 1);
	if (names == NULL) {
	    fprintf(stderr, "Arrgh: names[%d] malloc failed\n", i);
	    exit(1);
	}
	memcpy(names[i], cp, len);
	names[i][len] = '\0';
	cp += len;
    }
    __pmPrintMetricNames(stdout, numnames, names, " or ");
    printf(")\n");
    pmPrintDesc(stdout, dp);
    for (i = 0; i < numnames; i++) {
	free(names[i]);
    }
    free(names);
}

void
do_help(__int32_t *buf)
{
    int		type;
    char	*verbosity;
    pmID	pmid;
    pmInDom	indom;
    char	*p;

    printf("[%d] ", nrec);
    if (tflag)
	printf("<%s> ", __pmLogMetaTypeStr(TYPE_TEXT));
    if (oflag)
	printf("+%ld ", (long)offset);
    type = ntohl(buf[0]);
    if ((type & PM_TEXT_ONELINE) == PM_TEXT_ONELINE)
	verbosity = "oneline";
    else if ((type & PM_TEXT_HELP) == PM_TEXT_HELP)
	verbosity = "help";
    else
	verbosity = "unknown";
    printf("%s text ", verbosity);
    if ((type & PM_TEXT_INDOM) == PM_TEXT_INDOM) {
	indom = __ntohpmInDom(buf[1]);
	printf("indom %s ", pmInDomStr(indom));
    }
    else {
	pmid = __ntohpmID(buf[1]);
	printf("pmid %s ", pmIDStr(pmid));
    }
    printf("\n    ");
    for (p = (char *)&buf[2]; *p; p++) {
	putchar(*p);
	if (*p == '\n')
	    printf("    ");
    }
    putchar('\n');
}

void
do_metric_label(__int32_t *buf, int type)
{
    __pmTimestamp	stamp;
    int			i;
    int			k;
    struct {
	int	type;
	char	*type_str;
    } type_map[] = {
	{ PM_LABEL_CONTEXT,	"context" },
	{ PM_LABEL_DOMAIN,	"domain" },
	{ PM_LABEL_INDOM,	"indom" },
	{ PM_LABEL_CLUSTER,	"cluster" },
	{ PM_LABEL_ITEM,	"item" },
	{ PM_LABEL_INSTANCES,	"instances" },
	{ -1,			NULL }
    };
    printf("[%d] ", nrec);
    if (tflag)
	printf("<%s> ", __pmLogMetaTypeStr(type));
    if (oflag)
	printf("+%ld ", (long)offset);
    printf("metric label @ ");
    if (type == TYPE_LABEL) {
	__pmLoadTimestamp(&buf[0], &stamp);
	k = 3; 
    }
    else {
	__pmLoadTimeval(&buf[0], &stamp);
	k = 2; 
    }
    __pmPrintTimestamp(stdout, &stamp);
    for (i = 0; type_map[i].type != -1; i++) {
	if (ntohl(buf[k]) == type_map[i].type)
	    break;
    }
    if (type_map[i].type != -1)
	printf(" type=%s", type_map[i].type_str);
    else
	printf(" type=%d (unknown)", ntohl(buf[k]));
    printf(" ident=%d nsets=%d", ntohl(buf[k+1]), ntohl(buf[k+2]));
    putchar('\n');
}

void
do_archive_label(int archversion)
{
    printf("[%d] ", nrec);
    if (tflag)
	printf("<V%d> ", archversion);
    printf("archive label @ ");
    __pmPrintTimestamp(stdout, &label.start);
    putchar('\n');
}

int
main(int argc, char *argv[])
{
    int		len;
    int		buflen;
    __int32_t	*buf ;
    int		in;
    int		nb;
    int		c;
    int		sts;
    int		errflag = 0;
    int		tzh;				/* initial timezone handle */
    int		zflag = 0;			/* for -z */
    char 	*tz = NULL;			/* for -Z timezone */
    __pmFILE	*f;
    __pmLogHdr	hdr;

    pmSetProgname(argv[0]);
    setlinebuf(stdout);
    setlinebuf(stderr);

    while ((c = getopt(argc, argv, "aD:hilmotwWxzZ:")) != EOF) {
	switch (c) {

	case 'a':	/* report all */
	    hflag = iflag = lflag = mflag = 1;
	    break;

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmGetProgname(), optarg);
		errflag++;
	    }
	    break;

	case 'h':	/* report help text */
	    hflag = 1;
	    break;

	case 'i':	/* report indoms */
	    iflag = 1;
	    break;

	case 'l':	/* report labels */
	    lflag = 1;
	    break;

	case 'm':	/* report metrics */
	    mflag = 1;
	    break;

	case 'o':	/* report byte offsets */
	    oflag = 1;
	    break;

	case 't':	/* report metadata record types */
	    tflag = 1;
	    break;

	case 'w':	/* report warnings */
	    wflag = 1;
	    break;

	case 'W':	/* verbosely report warnings */
	    wflag = 2;
	    break;

	case 'x':	/* dump records in hex */
	    xflag = 1;
	    break;

	case 'z':	/* timezone from archive */
	    if (tz != NULL) {
		fprintf(stderr, "%s: at most one of -Z and/or -z allowed\n", pmGetProgname());
		errflag++;
	    }
	    zflag++;
	    break;

	case 'Z':	/* $TZ timezone */
	    if (zflag) {
		fprintf(stderr, "%s: at most one of -Z and/or -z allowed\n", pmGetProgname());
		errflag++;
	    }
	    tz = optarg;
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

    if (hflag + iflag + lflag + mflag + wflag == 0)
	mflag = 1;	/* default */

    if ((sts = pmNewContext(PM_CONTEXT_ARCHIVE, argv[optind])) < 0) {
	fprintf(stderr, "%s: Warning: pmNewContext failed: %s\n",
	    pmGetProgname(), pmErrStr(sts));
    }

    if (zflag) {
	if (sts >= 0) {
	    if ((tzh = pmNewContextZone()) < 0) {
		fprintf(stderr, "%s: Cannot set context timezone: %s\n",
		    pmGetProgname(), pmErrStr(tzh));
		exit(1);
	    }
	    printf("Note: timezone set to local timezone of pmcd from archive\n");
	}
	else
	    fprintf(stderr, "No context, skipping -z\n");
    }
    else if (tz != NULL) {
	if (sts >= 0) {
	    if ((tzh = pmNewZone(tz)) < 0) {
		fprintf(stderr, "%s: Cannot set timezone to \"%s\": %s\n",
		    pmGetProgname(), tz, pmErrStr(tzh));
		exit(1);
	    }
	    printf("Note: timezone set to \"TZ=%s\"\n", tz);
	}
	else
	    fprintf(stderr, "No context, skipping -Z\n");
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

    for (nrec = 0; ; nrec++) {
	offset = lseek(in, 0, SEEK_CUR);
	if ((nb = read(in, &hdr, sizeof(hdr))) != sizeof(hdr)) {
	    if (nb == 0) {
		if (nrec == 0)
		    fprintf(stderr, "error: %s is empty file\n", argv[optind]);
		break;
	    }
	    if (nb < 0) {
		fprintf(stderr, "[%d] ", nrec);
		if (oflag)
		    fprintf(stderr, "+%ld ", (long)offset);
		fprintf(stderr, "hdr read error: %s\n", strerror(errno));
	    }
	    else {
		/* Strange eof logic here ... from __pmLogLoadMeta(), a short
		 * read is treated as end-of-file
		 */
		break;
	    }
	    exit(1);
	}
	hdr.len = ntohl(hdr.len);
	hdr.type = ntohl(hdr.type);
	if (nrec == 0)
	    version = hdr.type & 0xff;
	if (pmDebugOptions.log) {
	    if (nrec == 0)
		fprintf(stderr, "read: len=%d magic=0x%x (version=%d) @ offset=%lld\n", hdr.len, hdr.type, version, (long long)offset);
	    else
		fprintf(stderr, "read: len=%d type=%s (%d) @ offset=%lld\n", hdr.len, __pmLogMetaTypeStr(hdr.type), hdr.type, (long long)offset);
	}
	len = hdr.len - sizeof(hdr);
	if (len > buflen) {
	    buflen = len;
	    buf = (__int32_t *)realloc(buf, buflen);
	    if (buf == NULL) {
		fprintf(stderr, "Arrgh: buf realloc failed\n");
		exit(1);
	    }
	}
	if ((nb = read(in, buf, len)) != len) {
	    fprintf(stderr, "[%d] " , nrec);
	    if (oflag)
		fprintf(stderr, "+%ld ", (long)offset);
	    if (nb == 0)
		fprintf(stderr, "body read error: end of file\n");
	    else if (nb < 0)
		fprintf(stderr, "body read error: %s\n", strerror(errno));
	    else
		fprintf(stderr, "body read error: expected %d bytes, got %d\n",
			len, nb);
	    exit(1);
	}
	
	if (xflag) {
	    int		i;
	    if (nrec == 0)
		printf("[%d] len=%d magic=0x%x (version=%d) @ offset=%lld\n", nrec, hdr.len, hdr.type, version, (long long)offset);
	    else
		printf("[%d] len=%d type=%s (%d) @ offset=%lld\n", nrec, hdr.len, __pmLogMetaTypeStr(hdr.type), hdr.type, (long long)offset);
	    for (i = 0; i < len; i++) {
		if ((i % 8) == 0) {
		    if (i > 0)
			putchar('\n');
		    printf("%4d", i);
		}
		printf(" %8x", buf[i]);
	    }
	    putchar('\n');
	}

	switch (hdr.type) {
	    case PM_LOG_MAGIC|PM_LOG_VERS03:
	    case PM_LOG_MAGIC|PM_LOG_VERS02:
		do_archive_label(hdr.type & 0xf);
		break;

	    case TYPE_INDOM:
	    case TYPE_INDOM_DELTA:
	    case TYPE_INDOM_V2:
		if (!iflag)
		    break;
		do_indom(buf, hdr.type);
		break;

	    case TYPE_LABEL:
	    case TYPE_LABEL_V2:
		if (!lflag)
		    break;
		do_metric_label(buf, hdr.type);
		break;

	    case TYPE_DESC:
		if (!mflag)
		    break;
		do_desc(buf);
		break;

	    case TYPE_TEXT:
		if (!hflag)
		    break;
		do_help(buf);
		break;

	    default:
		fprintf(stderr, "[%d] ", nrec);
		if (oflag)
		    fprintf(stderr, "+%ld ", (long)offset);
		fprintf(stderr, "error bad type %d\n", hdr.type);
		exit(1);
	}
    }

    __pmLogFreeLabel(&label);
    return 0;
}
