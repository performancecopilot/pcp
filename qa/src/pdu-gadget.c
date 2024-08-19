/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2017,2024 Ken McDonell.  All Rights Reserved.
 *
 * PDU swiss army knife.
 *
 * Read PDU specifications on stdin and either write binary PDUs to
 * stdout or call the assocated __pmDecode<foo>() routines in libpcp
 * directly (with -x flag).
 *
 * Input PDU specification syntax:
 * - lines beginning # are comments
 * - blank lines are ignored
 * - other lines are PDUs with one white-space separated "word" for
 *   each 32-bit value in the PDU, a "word" may be
 *   + a positive decimial integer [0-9]+
 *   + a negative decimial integer -[0-9]+
 *   + a hexadecimal integer 0x[0-9a-f]+
 *   + pmid(domain.cluster.item) {no whitespace allowed}
 *   + pmid(<metricname>) {no whitespace allowed}
 *   + indom(domain.serial) {no whitespace allowed}
 *   + PDU_... which is mapped to the associated PDU code from
 *     <libpcp.h>
 *   + str(somestring) {\x to escape x, e.g \), if no ) continue to
 *     next word(s), output is null padded to word boundary}
 * - for the first word in the PDU, the special value ? may be used
 *   to have pdu-gadget fill in the PDU length (in bytes) in word[0]
 *
 * Output (without -x) binary PDU is in network byte order.
 */

#include <pcp/pmapi.h>
#include "libpcp.h"
#include <ctype.h>

static pmLongOptions longopts[] = {
    PMOPT_DEBUG,	/* -D */
    { "verbose", 0, 'v', NULL, "output PDU in pmGetPDU-style on stderr" },
    { "execute", 0, 'x', NULL, "call libpcp [default: don't call emit binary PDUs]" },
    PMOPT_HELP,		/* -? */
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .flags = PM_OPTFLAG_BOUNDARIES | PM_OPTFLAG_STDOUT_TZ,
    .short_options = "D:vx?",
    .long_options = longopts,
    .short_usage = "[options] ...",
};

/*
 * PDU type table: name (stripped of PDU_ prefix) -> code
 */
struct {
    char	*name;
    int		code;
} pdu_types[] = {
    { "ERROR",		0x7000 },
    { "RESULT",		0x7001 },
    { "PROFILE",	0x7002 },
    { "FETCH",		0x7003 },
    { "DESC_REQ",	0x7004 },
    { "DESC",		0x7005 },
    { "INSTANCE_REQ",	0x7006 },
    { "INSTANCE",	0x7007 },
    { "TEXT_REQ",	0x7008 },
    { "TEXT",		0x7009 },
    { "CONTROL_REQ",	0x700a },
    { "CREDS",		0x700c },
    { "PMNS_IDS",	0x700d },
    { "PMNS_NAMES",	0x700e },
    { "PMNS_CHILD",	0x700f },
    { "PMNS_TRAVERSE",	0x7010 },
    { "ATTR",		0x7011 },
    { "AUTH",		0x7011 },
    { "LABEL_REQ",	0x7012 },
    { "LABEL",		0x7013 },
    { "HIGHRES_FETCH",	0x7014 },
    { "HIGHRES_RESULT",	0x7015 },
    { "DESC_IDS",	0x7016 },
    { "DESCS",		0x7017 },
};

int	npdu_types = sizeof(pdu_types)/sizeof(pdu_types[0]);

#define PDUBUF_SIZE 1024

int
main(int argc, char **argv)
{
    int		verbose = 0;
    int		execute = 0;
    int		c;
    int		sts;
    int		lsts;
    long	value;
    int		calc_len;
    int		save_len;
    int		len;
    int		type;
    int		out;
    int		w;
    int		nwr;
    int		j;
    char	buf[1024];
    __pmPDU	*pdubuf;
    char	*bp;
    char	*wp;
    char	*end;
    int		lineno = 1;
    int		newline;

    pmSetProgname(argv[0]);

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {

	case 'v':	/* verbose output */
	    verbose++;
	    break;	

	case 'x':	/* call libpcp __pmDecode<foo>() routines */
	    execute++;
	    break;	
	}
    }

    if (opts.flags & PM_OPTFLAG_EXIT) {
	pmflush();
	pmUsageMessage(&opts);
	exit(0);
    }

    if (opts.errors || opts.optind != argc) {
	pmUsageMessage(&opts);
	exit(EXIT_FAILURE);
    }

    pdubuf = __pmFindPDUBuf(PDUBUF_SIZE);

    if ((sts = pmNewContext(PM_CONTEXT_HOST, "local:")) < 0) {
	fprintf(stderr, "%s: Cannot connect to pmcd on local: %s\n",
		pmGetProgname(), pmErrStr(sts));
	exit(EXIT_FAILURE);
    }
    sts = 0;

    while (fgets(buf, sizeof(buf), stdin) != NULL) {
	if (buf[0] == '#') {
	    lineno++;
	    continue;
	}
	calc_len = 0;
	len = 0;
	w = 0;
	newline = 0;
	type = PDU_MAX + 1;
	for (bp = buf; *bp; bp++) {
	    if (*bp == '\n') {
		newline = 1;
		break;
	    }
	    if (*bp == ' ' || *bp == '\t')
		continue;
	    /* at the start of a word */
	    wp = bp;
	    for (wp = bp; *wp != '\0' && *wp != ' ' && *wp != '\t' && *wp != '\n'; wp++)
		;
	    c = *wp;
	    *wp = '\0';
	    if (bp[0] == '0' && bp <= &buf[sizeof(buf)-3] && bp[1] == 'x') {
		out = value = strtol(&bp[2], &end, 16);
		if (*end != ' ' && *end != '\t' && *end != '\n' && *end != '\0') {
		    fprintf(stderr, "%d: bad hex word @ %s\n", lineno, bp);
		    sts = 1;
		}
		else if (end == &bp[2]) {
		    fprintf(stderr, "%d: missing hex value @ %s\n", lineno, bp);
		    sts = 1;
		}
		else if (value & 0xffffffff00000000L) {
		    fprintf(stderr, "%d: truncated hex value 0x%x @ %s\n", lineno, out, bp);
		    sts = 1;
		}
	    }
	    else if (isdigit(*bp)) {
		out = value = strtol(bp, &end, 10);
		if (*end != ' ' && *end != '\t' && *end != '\n' && *end != '\0') {
		    fprintf(stderr, "%d: bad decimal word @ %s\n", lineno, bp);
		    sts = 1;
		}
		else if (out != value) {
		    fprintf(stderr, "%d: truncated decimal value %d @ %s\n", lineno, out, bp);
		    sts = 1;
		}
	    }
	    else if (*bp == '-' && bp <= &buf[sizeof(buf)-1] && isdigit(bp[1])) {
		value = strtol(&bp[1], &end, 10);
		value = -value;
		out = value;
		if (*end != ' ' && *end != '\t' && *end != '\n' && *end != '\0') {
		    fprintf(stderr, "%d: bad (negative) decimal word @ %s\n", lineno, bp);
		    sts = 1;
		}
		else if (out != value) {
		    fprintf(stderr, "%d: truncated (negative) decimal value %d @ %s\n", lineno, out, bp);
		    sts = 1;
		}
	    }
	    else if (strncmp(bp, "pmid(", 5) == 0) {
		char		*p = &bp[5];
		unsigned int	domain;
		unsigned int	cluster;
		unsigned int	item;
		out = PM_ID_NULL;
		if (isdigit(*p)) {
		    /*
		     * pmid(domain.cluster.item)
		     * domain is 9 bits, but 511 is special
		     * cluster is 12 bits (4095 max)
		     * item is 10 bits (1023 max)
		     */
		    domain = strtol(p, &end, 10);
		    if (*end == '.' && domain <= 510) {
			p = &end[1];
			cluster = strtol(p, &end, 10);
			if (*end == '.' && cluster <= 4095) {
			    p = &end[1];
			    item = strtol(p, &end, 10);
			    if (*end == ')' && end[1] == '\0' && item <= 1023) {
				out = (int)pmID_build(domain, cluster, item);
			    }
			}
		    }
		}
		else {
		    char		*q = &p[1];
		    pmID		pmid;
		    while (*q && *q != ')')
			q++;
		    if (*q == ')' && q[1] == '\0') {
			*q = '\0';
			if ((lsts = pmLookupName(1, (const char **)&p, &pmid)) < 0) {
			    fprintf(stderr, "%d: pmlookupname(%s) failed: %s\n", lineno, p, pmErrStr(lsts));
			    sts = 1;
			}
			else
			    out = pmid;
		    }
		}
		if (out == PM_INDOM_NULL) {
		    fprintf(stderr, "%d: illegal pmid() @ %s\n", lineno, bp);
		    sts = 1;
		}
	    }
	    else if (strncmp(bp, "indom(", 6) == 0) {
		char	*q = &bp[6];
		unsigned int	domain;
		unsigned int	serial;
		out = PM_INDOM_NULL;
		if (isdigit(*q)) {
		    /*
		     * indom(domain.serial)
		     * domain is 9 bits, but 511 is special
		     * serial is 22 bits (4194303 max)
		     * item is 10 bits (1023 max)
		     */
		    domain = strtol(q, &end, 10);
		    if (*end == '.' && domain <= 510) {
			q = &end[1];
			serial = strtol(q, &end, 10);
			if (*end == ')' && end[1] == '\0' && serial <= 4194303) {
			    out = (int)pmInDom_build(domain, serial);
			}
		    }
		}
		if (out == PM_INDOM_NULL) {
		    fprintf(stderr, "%d: illegal indom() @ %s\n", lineno, bp);
		    sts = 1;
		}
	    }
	    else if (strncmp(bp, "PDU_", 4) == 0) {
		int	i;
		out = 0;
		for (i = 0; i < npdu_types; i++) {
		    if (strcmp(&bp[4], pdu_types[i].name) == 0) {
			out = pdu_types[i].code;
			break;
		    }
		}
		if (out == 0) {
		    fprintf(stderr, "%d: unknown PDU type @ %s\n", lineno, bp);
		    sts = 1;
		}
	    }
	    else if (strncmp(bp, "str(", 4) == 0) {
		char	*op = (char *)&pdubuf[w];
		int	esc = 0;
		int	nch = 0;
		*wp = c;	/* reinstate end of word */
		bp += 4;
		while (*bp && *bp != '\n') {
		    if (esc == 0) {
			/* process this char, no preceding \ */
			if (*bp == '\\') {
			    esc++;
			    bp++;
			    continue;
			}
			if (*bp == ')') {
			    break;
			}
		    }
		    if ((nch % sizeof(pdubuf[0])) == 0) {
			/* zero fill next element of pdubuf[] */
			pdubuf[w++] = 0;
		    }
		    *op++ = *bp++;
		    len++;
		    nch++;
		    esc = 0;
		}
		if (*bp == ')') {
		    /* pdubuf[] stuffing already done, onto next input word */
		    continue;
		}
		fprintf(stderr, "%d: no closing ) for str(...\n", lineno);
		sts = 1;
	    }
	    else if (w == 0 && strcmp(bp, "?") == 0) {
		calc_len = 1;
		out = -1;
	    }
	    else {
		fprintf(stderr, "%d: unrecognized word @ %s\n", lineno, bp);
		sts = 1;
	    }
	    if (sts == 0) {
		if (w >= (int)(PDUBUF_SIZE/sizeof(pdubuf[0]))) {
		    fprintf(stderr, "%d: output buffer overrun\n", lineno);
		    sts = 1;
		}
		else {
		    if (w == 0) {
			/* save PDU len before htonl() conversion */
			save_len = out;
		    }
		    else if (w == 1) {
			/* save PDU type before htonl() conversion */
			type = out;
		    }
		    pdubuf[w++] = htonl(out);
		    len += sizeof(pdubuf[0]);
		}
	    }
	    *wp = c;
	    bp = wp - 1;
	}
	if (!newline) {
	    fprintf(stderr, "%d: input line too long (limit=%d chars including \\n)\n", lineno, (int)sizeof(buf)-1);
	    sts = 1;
	}
	/* fill-in len field or integrity check on len field */
	if (sts == 0 && w > 0) {
	    if (calc_len) {
		pdubuf[0] = htonl(len);
	    }
	    else if (save_len != len) {
		fprintf(stderr, "%d: PDU length %d != byte count %d\n", lineno, save_len, len);
		sts = 1;
	    }
	}
	if (sts == 0 && w > 0) {
	    if (verbose > 0) {
		for (j = 0; j < w; j++) {
		    if ((j % 8) == 0) {
			if (j > 0)
			    fputc('\n', stderr);
			fprintf(stderr, "%03d: ", j);
		    }
		    fprintf(stderr, "%08x ", pdubuf[j]);
		}
		fputc('\n', stderr);
	    }
	    if (execute) {
		/*
		 * swab header, a la __pmGetPDU() rest of PDU
		 * remains in network byte order
		 */
		__pmPDUHdr  *php = (__pmPDUHdr *)pdubuf;
		php->len = ntohl(php->len);
		php->type = ntohl((unsigned int)php->type);
		php->from = ntohl((unsigned int)php->from);

		switch (type) {

		    case PDU_CREDS:
			{
			    int		sender;
			    int		credcount;
			    __pmCred	*credlist;
			    lsts = __pmDecodeCreds(pdubuf, &sender, &credcount, &credlist);
			    if (lsts < 0)
				fprintf(stderr, "%d: __pmDecodeCreds failed: %s\n", lineno, pmErrStr(lsts));
			    else {
				fprintf(stderr, "%d: __pmDecodeCreds: sts=%d sender=%d credcount=%d ...\n", lineno, lsts, sender, credcount);
				for (j = 0; j < credcount; j++) {
				    fprintf(stderr, "    #%d = { type=0x%x a=0x%x b=0x%x c=0x%x }\n",
					j, credlist[j].c_type, credlist[j].c_vala,
					credlist[j].c_valb, credlist[j].c_valc);
				}
				free(credlist);
			    }
			}
			break;

		    case PDU_PROFILE:
			{
			    int		ctxnum;
			    pmProfile	*result;
			    lsts = __pmDecodeProfile(pdubuf, &ctxnum, &result);
			    if (lsts < 0)
				fprintf(stderr, "%d: __pmDecodeProfile failed: %s\n", lineno, pmErrStr(lsts));
			    else {
				fprintf(stderr, "%d: __pmDecodeProfile: sts=%d ctxnum=%d ...\n", lineno, lsts, ctxnum);
				__pmDumpProfile(stderr, PM_INDOM_NULL, result);
				__pmFreeProfile(result);
			    }
			}
			break;

		    case PDU_FETCH:
			{
			    int		ctxnum;
			    int		numpmid;
			    pmTimeval	unused;
			    pmID	*pmidlist;
			    lsts = __pmDecodeFetch(pdubuf, &ctxnum, &unused, &numpmid, &pmidlist);
			    if (lsts < 0)
				fprintf(stderr, "%d: __pmDecodeesFetch failed: %s\n", lineno, pmErrStr(lsts));
			    else {
				fprintf(stderr, "%d: __pmDecodeesFetch: sts=%d ctxnum=%d unused=%d.%d numpmid=%d pmids:", lineno, lsts, ctxnum, (int)unused.tv_sec, (int)unused.tv_usec, numpmid);
				for (j = 0; j < numpmid; j++) {
				    fprintf(stderr, " %s", pmIDStr(pmidlist[j]));
				}
				fputc('\n', stderr);
				/* pmidlist[] is in pdubuf[]! */
				__pmUnpinPDUBuf(pmidlist);
			    }
			}
			break;

		    case PDU_HIGHRES_FETCH:
			{
			    int		ctxnum;
			    int		numpmid;
			    pmID	*pmidlist;
			    lsts = __pmDecodeHighResFetch(pdubuf, &ctxnum, &numpmid, &pmidlist);
			    if (lsts < 0)
				fprintf(stderr, "%d: __pmDecodeHighResFetch failed: %s\n", lineno, pmErrStr(lsts));
			    else {
				fprintf(stderr, "%d: __pmDecodeHighResFetch: sts=%d ctxnum=%d numpmid=%d pmids:", lineno, lsts, ctxnum, numpmid);
				for (j = 0; j < numpmid; j++) {
				    fprintf(stderr, " %s", pmIDStr(pmidlist[j]));
				}
				fputc('\n', stderr);
				/* pmidlist[] is in pdubuf[]! */
				__pmUnpinPDUBuf(pmidlist);
			    }
			}
			break;

		    case PDU_RESULT:
			{
			    __pmResult	*result;
			    lsts = __pmDecodeResult(pdubuf, &result);
			    if (lsts < 0)
				fprintf(stderr, "%d: __pmDecodeResult failed: %s\n", lineno, pmErrStr(lsts));
			    else {
				fprintf(stderr, "%d: __pmDecodeResult: sts=%d ...\n", lineno, lsts);
				__pmPrintResult(stderr, result);
				__pmFreeResult(result);
			    }
			}
			break;

		    case PDU_PMNS_IDS:
			{
			    int		asts;
			    pmID	pmid;
			    lsts = __pmDecodeIDList(pdubuf, 1, &pmid, &asts);
			    if (lsts < 0)
				fprintf(stderr, "%d: __pmDecodeIDList failed: %s\n", lineno, pmErrStr(lsts));
			    else
				fprintf(stderr, "%d: __pmDecodeIDList: sts=%d pmid=%s asts=%d\n", lineno, lsts, pmIDStr(pmid), asts);
			}
			break;

		    case PDU_ATTR:
			{
			    int		attr;
			    char	*val;
			    int		vlen;
			    lsts = __pmDecodeAttr(pdubuf, &attr, &val, &vlen);
			    if (lsts < 0)
				fprintf(stderr, "%d: __pmDecodeAttr failed: %s\n", lineno, pmErrStr(lsts));
			    else
				fprintf(stderr, "%d: __pmDecodeAttr: sts=%d attr=%d vlen=%d value=\"%s\"\n", lineno, lsts, attr, vlen, val);
			}
			break;

		    case PDU_LABEL_REQ:
			{
			    int		ident;
			    int		otype;
			    lsts = __pmDecodeLabelReq(pdubuf, &ident, &otype);
			    if (lsts < 0)
				fprintf(stderr, "%d: __pmDecodeLabelReq failed: %s\n", lineno, pmErrStr(lsts));
			    else
				fprintf(stderr, "%d: __pmDecodeLabelReq: sts=%d ident=%d otype=%d\n", lineno, sts, ident, otype);
			}
			break;

		    default:
			fprintf(stderr, "%d: execute unavailable (yet) for PDU_%s\n", lineno, __pmPDUTypeStr(type));
			break;
		}
	    }
	    else {
		nwr = write(fileno(stdout), pdubuf, w * sizeof(pdubuf[0]));
		if (nwr != w * sizeof(pdubuf[0])) {
		    fprintf(stderr, "%d: write failed: returned %d: %s\n", lineno, nwr, strerror(errno));
		    sts = 1;
		}
	    }
	}
	lineno++;
    }

    return sts;
}

