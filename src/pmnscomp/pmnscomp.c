/*
 * Construct a compiled PMNS suitable for "fast" loading in pmLoadNameSpace
 *
 * Copyright (c) 2014 Red Hat.
 * Copyright (c) 1995-2006 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#include <ctype.h>
#include <inttypes.h>
#include <sys/stat.h>
#include "pmapi.h"
#include "impl.h"

static int	nodecnt;	/* number of nodes */
static int	leafcnt;	/* number of leaf nodes */
static int	symbsize;	/* aggregate string table size */
static __pmnsNode	**_htab;	/* output htab[] */
static __pmnsNode	*_nodetab;	/* output nodes */
static char	*_symbol;	/* string table */
static char	*symp;		/* pointer into same */
static __pmnsNode	**_map;		/* point-to-ordinal map */
static int	i;		/* node counter for traversal */

static int	version = 2;	/* default output format version */

/*
 * 32-bit pointer version of __pmnsNode
 */
typedef struct {
    __int32_t	parent;
    __int32_t	next;
    __int32_t	first;
    __int32_t	hash;
    __int32_t	name;
    pmID	pmid;
} __pmnsNode32;
static __int32_t	*_htab32;
static __pmnsNode32	*_nodetab32;

/*
 * 64-bit pointer version of __pmnsNode
 */
typedef struct {
    __int64_t	parent;
    __int64_t	next;
    __int64_t	first;
    __int64_t	hash;
    __int64_t	name;
    pmID	pmid;
    __int32_t	__pad__;
} __pmnsNode64;
static __int64_t	*_htab64;
static __pmnsNode64	*_nodetab64;

static void
dumpmap(void)
{
    int		n;

    for (n = 0; n < nodecnt; n++) {
	if (n % 8 == 0) {
	    if (n)
		putchar('\n');
	    printf("map[%3d]", n);
	}
	printf(" " PRINTF_P_PFX "%p", _map[n]);
    }
    putchar('\n');
}

static long
nodemap(__pmnsNode *p)
{
    int		n;

    if (p == NULL)
	return -1;

    for (n = 0; n < nodecnt; n++) {
	if (_map[n] == p)
	    return n;
    }
    printf("%s: fatal error, cannot map node addr " PRINTF_P_PFX "%p\n", pmProgname, p);
    dumpmap();
    exit(1);
}

static void
traverse(__pmnsNode *p, void(*func)(__pmnsNode *this))
{
    if (p != NULL) {
	(*func)(p);
	traverse(p->first, func);
	traverse(p->next, func);
    }
}

#ifdef PCP_DEBUG
static void
chkascii(char *tag, char *p)
{
    int	i = 0;

    while (*p) {
	if (!isascii((int)*p) || !isprint((int)*p)) {
	    printf("chkascii: %s: non-printable char 0x%02x in \"%s\"[%d] @ " PRINTF_P_PFX "%p\n",
		tag, *p & 0xff, p, i, p);
	    exit(1);
	}
	i++;
	p++;
    }
}

static char *chktag;

static void
chknames(__pmnsNode *p)
{
    chkascii(chktag, p->name);
}
#endif

static void
pass1(__pmnsNode *p)
{
    nodecnt++;
    if (p->pmid != PM_ID_NULL && p->first == NULL)
	leafcnt++;
    symbsize += strlen(p->name)+1;
}

static void
pass2(__pmnsNode *p)
{
    ptrdiff_t	offset;
    _map[i] = p;
    _nodetab[i] = *p;	/* struct assignment */
    strcpy(symp, p->name);
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	chkascii("pass2 symtab", symp);
#endif
    offset = symp - _symbol;
    symp += strlen(p->name);
    *symp++ = '\0';
    _nodetab[i].name = (char *)offset;
    i++;
}

static void
pass3(__pmnsNode *p)
{
    _nodetab[i].parent = (__pmnsNode *)nodemap(_nodetab[i].parent);
    _nodetab[i].next = (__pmnsNode *)nodemap(_nodetab[i].next);
    _nodetab[i].first = (__pmnsNode *)nodemap(_nodetab[i].first);
    _nodetab[i].hash = (__pmnsNode *)nodemap(_nodetab[i].hash);
    i++;
}

#ifndef __htonll
void
__htonll(char *p)
{
    char        c;
    int         i;

    for (i = 0; i < 4; i++) {
        c = p[i];
        p[i] = p[7-i];
        p[7-i] = c;
    }
}
#endif

#ifdef HAVE_NETWORK_BYTEORDER
#define __ntohll(a) /* noop */
#else
#define __ntohll(v) __htonll(v)
#endif

/*
 * Promote a pointer to a 64 bit interger and do the endianess
 * conversion on a 64 bit integer.  Promotion should be sign
 * extending because we're actually dealing with integers and
 * not real pointers, so if promoting -1 from 32 to 64 bits, we
 * want -1 to appear on the other end as well.
 */
static __int64_t
to64_htonll(void *from)
{
    __int64_t to = (__psint_t)from; /* compiler sign extends */

    __htonll((char *)&to);
    return to;
}

/*
 * And this is the reverse - take something which purpots to be a pointer
 * and stuff it into an 32 bit integer. If cannot do it safely, then
 * complain and exit
 */
static __int32_t
to32_htonl(void *from)
{
    __int32_t to = (__int32_t)(__psint_t)from; /* compiler truncates */

    if (to !=  (__psint_t)from) {
	fprintf(stderr, "%s: loss of precision during the conversion\n", 
		pmProgname);
	exit(1);
    }
    return htonl(to);
}

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    { "duplicates", 0, 'd', 0, "duplicate PMIDs are allowed" },
    { "force", 0, 'f', 0, "force overwriting of the output file if it exists" },
    PMOPT_NAMESPACE,
    { "version", 1, 'v', "N", "alternate output format version [default 2]" },
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "dD:fn:v:?",
    .long_options = longopts,
    .short_usage = "[options] outfile",
};

int
main(int argc, char **argv)
{
    int		n;
    int		c;
    int		j;
    int		sts;
    int		force = 0;
    int		dupok = 0;
    char	*pmnsfile = PM_NS_DEFAULT;
    char	*endnum;
    FILE	*outf;
    __pmnsNode	*root;
    __pmnsNode	**htab;
    int		htabcnt;	/* count of the number of htab[] entries */
    __int32_t	tmp;
    __int32_t	sum;
    long	startsum = 0;	/* initialize to pander to gcc */

    while ((c = pmgetopt_r(argc, argv, &opts)) != EOF) {
	switch (c) {

	case 'd':	/* duplicate PMIDs are allowed */
	    dupok = 1;
	    break;

	case 'D':	/* debug flag */
	    sts = __pmParseDebug(opts.optarg);
	    if (sts < 0) {
		pmprintf("%s: unrecognized debug flag specification (%s)\n",
			pmProgname, opts.optarg);
		opts.errors++;
	    }
	    else
		pmDebug |= sts;
	    break;

	case 'f':	/* force ... unlink file first */
	    force = 1;
	    break;

	case 'n':	/* alternative namespace file */
	    pmnsfile = opts.optarg;
	    break;

	case 'v':	/* alternate version */
	    version = (int)strtol(opts.optarg, &endnum, 10);
	    if (*endnum != '\0') {
		pmprintf("%s: -v requires numeric argument\n", pmProgname);
		opts.errors++;
	    }
	    if (version < 1 || version > 2) {
		pmprintf("%s: output format version %d not supported\n",
			pmProgname, version);
		opts.errors++;
	    }
	    break;

	case '?':
	default:
	    opts.errors++;
	    break;
	}
    }

    if (opts.errors || opts.optind != argc-1) {
	pmUsageMessage(&opts);
	exit(1);
    }

    if (force) {
	struct stat	sbuf;

	if (stat(argv[opts.optind], &sbuf) == -1) {
	    if (oserror() != ENOENT) {
		fprintf(stderr, "%s: cannot stat \"%s\": %s\n",
		    pmProgname, argv[opts.optind], osstrerror());
		exit(1);
	    }
	}
	else {
	    /* stat is OK, so exists ... must be a regular file */
	    if (!S_ISREG(sbuf.st_mode)) {
		fprintf(stderr, "%s: \"%s\" is not a regular file\n",
		    pmProgname, argv[opts.optind]);
		exit(1);
	    }
	    if (unlink(argv[opts.optind]) == -1) {
		fprintf(stderr, "%s: cannot unlink \"%s\": %s\n",
		    pmProgname, argv[opts.optind], osstrerror());
		exit(1);
	    }
	}
    }

    if (access(argv[opts.optind], F_OK) == 0) {
	fprintf(stderr, "%s: \"%s\" already exists!\nYou must either remove it first, or use -f\n",
		pmProgname, argv[opts.optind]);
	exit(1);
    }

    if ((n = pmLoadASCIINameSpace(pmnsfile, dupok)) < 0) {
	fprintf(stderr, "pmLoadNameSpace: %s\n", pmErrStr(n));
	exit(1);
    }

    {
        __pmnsTree *t;
	t = __pmExportPMNS();
	if (t == NULL) {
	   /* sanity check - shouldn't ever happen */
	   fprintf(stderr, "%s: Exported PMNS is NULL!\n", pmProgname);
	   exit(1);
	}
	root = t->root;
	htabcnt = t->htabsize;
	htab = t->htab;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0) {
	chktag = "pmLoadASCIINameSpace";
	traverse(root, chknames);
    }
#endif

    traverse(root, pass1);

    _htab = (__pmnsNode **)malloc(htabcnt * sizeof(_htab[0]));
    _nodetab = (__pmnsNode *)malloc(nodecnt * sizeof(_nodetab[0]));
    symp = _symbol = (char *)malloc(symbsize);
    _map = (__pmnsNode **)malloc(nodecnt * sizeof(_map[0]));

    if (_htab == NULL || _nodetab == NULL ||
	symp == NULL || _map == NULL) {
	    __pmNoMem("pmnscomp", htabcnt * sizeof(_htab[0]) +
				 nodecnt * sizeof(_nodetab[0]) +
				 symbsize +
				 nodecnt * sizeof(_map[0]), PM_FATAL_ERR);
    }

    i = 0;
    traverse(root, pass2);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0) {
	chktag = "pass2";
	traverse(root, chknames);
    }
#endif

    memcpy(_htab, htab, htabcnt * sizeof(htab[0]));
    for (j = 0; j < htabcnt; j++)
	_htab[j] = (__pmnsNode *)nodemap(_htab[j]);

    i = 0;
    traverse(root, pass3);

    /*
     * from here on, ignore SIGHUP, SIGINT and SIGTERM to protect
     * the integrity of the new ouput file
     */
    __pmSetSignalHandler(SIGHUP, SIG_IGN);
    __pmSetSignalHandler(SIGINT, SIG_IGN);
    __pmSetSignalHandler(SIGTERM, SIG_IGN);

    if ((outf = fopen(argv[opts.optind], "w+")) == NULL) {
	fprintf(stderr, "%s: cannot create \"%s\": %s\n",
		pmProgname, argv[opts.optind], osstrerror());
	exit(1);
    }

    /*
     * Format verisons
     *	0	PmNs	- original PCP 1.0, 32-bit format
     * 	1	PmN1	- PCP 1.1 format with both 32 and 64-bit formats
     *			  for MIPS
     *  2       PmN2	- same as PmN1, but with initial checksum
     *
     * Note: all of this must be understood by pmLoadNameSpace() as well
     */
    if (version == 0)
	fprintf(outf, "PmNs");
    else if (version == 1)
	fprintf(outf, "PmN1");
    else if (version == 2)
	fprintf(outf, "PmN2");

    if (version == 2) {
	/* dummy at this stage, filled in later */
	fwrite(&sum, sizeof(sum), 1, outf);
	startsum = ftell(outf);
    }

    if(version == 1 || version == 2) {
	/*
	 * Version 1, after label, comes repetitions of, one for each "style"
	 *
	 * <symbsize>				| __int32_t
	 * <_symbol>				|
	 * <htabcnt><size of _htab[0]>		| style 1, __int32_t
	 * <nodecnt><size of _nodetab[0]>	| __int32_t
	 * <_htab>
	 * <_nodetab>
	 * <htabcnt><size of _htab[0]>		| style 2, __int32_t
	 * <nodecnt><size of _nodetab[0]>	| __int32_t
	 * <_htab>
	 * <_nodetab>
	 * ....					| style 3, ...
	 *
	 * Version 2 is similar, except the checksum follows the label,
	 * then as for Version 1.
	 */

	tmp = htonl((__int32_t)symbsize);
	fwrite(&tmp, sizeof(tmp), 1, outf);

	fwrite(_symbol, sizeof(_symbol[0]), symbsize, outf);

	tmp = htonl((__int32_t)htabcnt);
	fwrite(&tmp, sizeof(tmp), 1, outf);
	tmp = htonl((__int32_t)sizeof(_htab32[0]));
	fwrite(&tmp, sizeof(tmp), 1, outf);
	tmp = htonl((__int32_t)nodecnt);
	fwrite(&tmp, sizeof(tmp), 1, outf);
	tmp = htonl((__int32_t)sizeof(_nodetab32[0]));
	fwrite(&tmp, sizeof(tmp), 1, outf);

	_htab32 = (__int32_t *)malloc(htabcnt * sizeof(__int32_t));
	for (j = 0; j < htabcnt; j++)
	    _htab32[j] = to32_htonl(_htab[j]);
	fwrite(_htab32, sizeof(_htab32[0]), htabcnt, outf);
	_nodetab32 = (__pmnsNode32 *)malloc(nodecnt * sizeof(__pmnsNode32));
	for (j = 0; j < nodecnt; j++) {
	    _nodetab32[j].parent = to32_htonl(_nodetab[j].parent);
	    _nodetab32[j].next = to32_htonl(_nodetab[j].next);
	    _nodetab32[j].first = to32_htonl(_nodetab[j].first);
	    _nodetab32[j].hash = to32_htonl(_nodetab[j].hash);
	    _nodetab32[j].name = to32_htonl(_nodetab[j].name);
	    _nodetab32[j].pmid = htonl(_nodetab[j].pmid);
	}
	fwrite(_nodetab32, sizeof(_nodetab32[0]), nodecnt, outf);

	tmp = htonl((__int32_t)htabcnt);
	fwrite(&tmp, sizeof(tmp), 1, outf);
	tmp = htonl((__int32_t)sizeof(_htab64[0]));
	fwrite(&tmp, sizeof(tmp), 1, outf);
	tmp = htonl((__int32_t)nodecnt);
	fwrite(&tmp, sizeof(tmp), 1, outf);
	tmp = htonl((__int32_t)sizeof(_nodetab64[0]));
	fwrite(&tmp, sizeof(tmp), 1, outf);

	_htab64 = (__int64_t *)malloc(htabcnt * sizeof(__int64_t));
	for (j = 0; j < htabcnt; j++) {
	    /*
	     * Danger ahead ... serious cast games here to convert
	     * 32-bit ptrs to 64-bit integers _with_ sign extension
	     */
	    _htab64[j] = to64_htonll(_htab[j]);
	}
	fwrite(_htab64, sizeof(_htab64[0]), htabcnt, outf);
	_nodetab64 = (__pmnsNode64 *)malloc(nodecnt * sizeof(__pmnsNode64));
	for (j = 0; j < nodecnt; j++) {
	    /*
	     * Danger ahead ... serious cast games here to convert
	     * 32-bit ptrs to 64-bit integers _with_ sign extension
	     */
	    _nodetab64[j].parent = to64_htonll(_nodetab[j].parent);
	    _nodetab64[j].next = to64_htonll(_nodetab[j].next);
	    _nodetab64[j].first = to64_htonll(_nodetab[j].first);
	    _nodetab64[j].hash = to64_htonll(_nodetab[j].hash);
	    _nodetab64[j].name = to64_htonll(_nodetab[j].name);

	    _nodetab64[j].pmid = htonl(_nodetab[j].pmid);
	    _nodetab64[j].__pad__ = htonl(0xdeadbeef);
	}
	fwrite(_nodetab64, sizeof(_nodetab64[0]), nodecnt, outf);
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0) {
	    printf("32-bit Header: htab[%d] x %d bytes, nodetab[%d] x %d bytes\n",
		htabcnt, (int)sizeof(_htab32[0]),
		nodecnt, (int)sizeof(_nodetab32[0]));
	    printf("\n32-bit Hash Table\n");
	    for (j = 0; j < htabcnt; j++) {
		if (j % 10 == 0) {
		    if (j)
			putchar('\n');
		    printf("htab32[%3d]", j);
		}
		printf(" %5d", (int)ntohl(_htab32[j]));
	    }
	    printf("\n\n32-bit Node Table\n");
	    for (j = 0; j < nodecnt; j++) {
		if (j % 20 == 0)
		    printf("             Parent   Next  First   Hash Symbol           PMID\n");
		printf("node32[%4d] %6d %6d %6d %6d %-16.16s",
		    j, (int)ntohl(_nodetab32[j].parent), 
		       (int)ntohl(_nodetab32[j].next), 
	               (int)ntohl(_nodetab32[j].first),
		       (int)ntohl(_nodetab32[j].hash), 
		       _symbol+ntohl(_nodetab32[j].name));
		if (htonl(_nodetab32[j].first) == -1)
		    printf(" %s", pmIDStr(htonl(_nodetab32[j].pmid)));
		putchar('\n');
	    }
	    printf("\n64-bit Header: htab[%d] x %d bytes, nodetab[%d] x %d bytes\n",
		htabcnt, (int)sizeof(_htab64[0]),
		nodecnt, (int)sizeof(_nodetab64[0]));
	    printf("\n64-bit Hash Table\n");
	    for (j = 0; j < htabcnt; j++) {
		__int64_t k64 = _htab64[j];
		if (j % 10 == 0) {
		    if (j)
			putchar('\n');
		    printf("htab64[%3d]", j);
		}
		__ntohll((char *)&k64);	
		printf(" %5" PRIi64, k64); 
	    }
	    printf("\n\n64-bit Node Table\n");
	    for (j = 0; j < nodecnt; j++) {
		__pmnsNode64 t = _nodetab64[j]; /* struct copy */

		if (j % 20 == 0)
		    printf("             Parent   Next  First   Hash Symbol           PMID\n");

	        __ntohll ((char *)&t.name);
	        __ntohll ((char *)&t.parent);
	        __ntohll ((char *)&t.first);
	        __ntohll ((char *)&t.next);
	        __ntohll ((char *)&t.hash);

		printf("node64[%4d] "
		       "%6" PRIi64 " %6" PRIi64 " %6" PRIi64 " %6" PRIi64
		       " %-16.16s",
		    j, t.parent, t.next, t.first, t.hash, _symbol+t.name);
		if (t.first == -1) {
		    printf(" %s", pmIDStr(htonl(_nodetab64[j].pmid)));
		}	
		putchar('\n');
	    }
	}
#endif
    }

    if (version == 2) {
	fseek(outf, startsum, SEEK_SET);
	sum = __pmCheckSum(outf);
	fseek(outf, startsum - (long)sizeof(sum), SEEK_SET);
	tmp = htonl(sum);
	fwrite(&tmp, sizeof(sum), 1, outf);
    }


#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0) {
	if (version == 2)
	    printf("\nChecksum 0x%x\n", sum);
	printf("\nSymbol Table\n");
	for (j = 0; j < symbsize; j++) {
	    if (j % 50 == 0) {
		if (j)
		    putchar('\n');
		printf("symbol[%4d]  ", j);
	    }
	    if (_symbol[j])
		putchar(_symbol[j]);
	    else
		putchar('*');
	}
	putchar('\n');
    }
#endif

    printf("Compiled PMNS contains\n\t%5d hash table entries\n\t%5d leaf nodes\n\t%5d non-leaf nodes\n\t%5d bytes of symbol table\n",
	htabcnt, leafcnt, nodecnt - leafcnt, symbsize);

    exit(0);
}
