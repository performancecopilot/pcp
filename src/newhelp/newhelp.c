/*
 * Copyright (c) 1995-2001,2003 Silicon Graphics, Inc.  All Rights Reserved.
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

/*
 * newhelp file
 *
 * in the model of newaliases, build ndbm data files for a PMDA's help file
 * -- given the bloat of the ndbm files, version 2 uses a much simpler
 * file access method, but preserves the file name conventions from
 * version 1 that is based on ndbm.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include "pmapi.h"
#include "impl.h"

#define DEFAULT_HELP_VERSION 2

/* maximum bytes per line and bytes per entry */
#define MAXLINE	128
#define MAXENTRY 1024

static int	verbose = 0;
static int	ln = 0;
static char	*filename;
static int	status = 0;
static int	version = DEFAULT_HELP_VERSION;
static FILE	*f = NULL;

typedef struct {
    pmID	pmid;
    __uint32_t	off_oneline;
    __uint32_t	off_text;
} help_idx_t;

static help_idx_t	*hindex = NULL;
static int		numindex = 0;
static int		thisindex = -1;

static void
newentry(char *buf)
{
    int		n;
    char	*p;
    char	*end_name;
    char	end_c;
    char	*name;
    pmID	pmid;
    char	*start;
    int		warn = 0;
    int		i;

    /* skip leading white space ... */
    for (p = buf; isspace((int)*p); p++)
	;
    /* skip over metric name or indom spec ... */
    name = p;
    for (p = buf; *p != '\n' && !isspace((int)*p); p++)
	;
    end_c = *p;
    *p = '\0';	/* terminate metric name */
    end_name = p;

    if ((n = pmLookupName(1, &name, &pmid)) < 0) {
	/* apparently not a metric name */
	int	domain;
	int	cluster;
	int	item;
	int	serial;
	pmID	*pmidp;
	if (sscanf(buf, "%d.%d.%d", &domain, &cluster, &item) == 3) {
	    /* a numeric pmid */
	    __pmID_int	ii;
	    ii.domain = domain;
	    ii.cluster = cluster;
	    ii.item = item;
	    ii.flag = 0;
	    pmidp = (pmID *)&ii;
	    pmid = *pmidp;
	}
	else if (sscanf(buf, "%d.%d", &domain, &serial) == 2) {
	    /* an entry for an instance domain */
	    __pmInDom_int	ii;
	    ii.domain = domain;
	    ii.serial = serial;
	    /* set a bit here to disambiguate pmInDom from pmID */
	    ii.flag = 1;
	    pmidp = (pmID *)&ii;
	    pmid = *pmidp;
	}
	else {
	    fprintf(stderr, "%s: [%s:%d] %s: %s, entry abandoned\n",
		    pmProgname, filename, ln, buf, pmErrStr(n));
	    status = 2;
	    return;
	}
    }
    else {
	if (pmid == PM_ID_NULL) {
	    fprintf(stderr, "%s: [%s:%d] %s: unknown metric, entry abandoned\n",
		    pmProgname, filename, ln, name);
	    status = 2;
	    return;
	}
    }

    for (i = 0; i < thisindex; i++) {
	if (hindex[thisindex].pmid == pmid) {
	    __pmInDom_int	*kp = (__pmInDom_int *)&pmid;
	    fprintf(stderr, "%s: [%s:%d] duplicate key (", 
		    pmProgname, filename, ln);
	    if (kp->flag == 0)
		fprintf(stderr, "%s", pmIDStr(pmid));
	    else {
		kp->flag = 0;
		fprintf(stderr, "%s", pmInDomStr((pmInDom)pmid));
	    }
	    fprintf(stderr, ") entry abandoned\n");
	    status = 2;
	    return;
	}
    }

    if (++thisindex >= numindex) {
	if (numindex == 0)
	    numindex = 128;
	else
	    numindex *= 2;
	if ((hindex = (help_idx_t *)realloc(hindex, numindex * sizeof(hindex[0]))) == NULL) {
	    __pmNoMem("newentry", numindex * sizeof(hindex[0]), PM_FATAL_ERR);
	}
    }

    fprintf(f, "\n@ %s ", name);

    hindex[thisindex].pmid = pmid;
    hindex[thisindex].off_oneline = ftell(f);

    /* skip white space ... to start of oneline */
    *p = end_c;
    if (*p != '\n')
	p++;
    for ( ; *p != '\n' && isspace((int)*p); p++)
	;
    start = p;

    /* skip to end of line ... */
    for ( ; *p != '\n'; p++)
	;
    *p = '\0';
    p++;
    
    if (p - start == 1 && verbose) {
	fprintf(stderr, "%s: [%s:%d] %s: warning, null oneline\n",
	    pmProgname, filename, ln, name);
	warn = 1;
	if (!status) status = 1;
    }

    if (fwrite(start, sizeof(*start), p - start, f) != sizeof(*start)
	|| ferror(f)) {
	fprintf(stderr, "%s: [%s:%d] %s: write oneline failed, entry abandoned\n",
		pmProgname, filename, ln, name);
	thisindex--;
	status = 2;
	return;
    }

    hindex[thisindex].off_text = ftell(f);

    /* trim all but last newline ... */
    i = (int)strlen(p) - 1;
    while (i >= 0 && p[i] == '\n')
	i--;
    if (i < 0)
	i = 0;
    else {
	/* really have text ... p[i] is last non-newline char */
	i++;
	if (version == 1)
	    p[i++] = '\n';
    }
    p[i] = '\0';

    if (i == 0 && verbose) {
	fprintf(stderr, "%s: [%s:%d] %s: warning, null help\n",
	    pmProgname, filename, ln, name);
	warn = 1;
	if (!status) status = 1;
    }

    if (fwrite(p, sizeof(*p), i+1, f) != sizeof(*p)
	|| ferror(f)) {
	fprintf(stderr, 
		"%s: [%s:%d] %s: write help failed, entry abandoned\n",
		pmProgname, filename, ln, name);
	thisindex--;
	status = 2;
	return;
    }

    if (verbose && warn == 0) {
	*end_name = '\0';
	fprintf(stderr, "%s\n", name);
	*end_name = end_c;
    }
}

static int
idcomp(const void *a, const void *b)
{
    /*
     * comparing 32-bit keys here ... want PMIDs to go first the
     * InDoms, sort by low order bits ... serial from InDom is easier
     * than cluster and item from PMID, so use InDom format
     */
    __pmInDom_int	*iiap, *iibp;

    iiap = (__pmInDom_int *)(&((help_idx_t *)a)->pmid);
    iibp = (__pmInDom_int *)(&((help_idx_t *)b)->pmid);

    if (iiap->flag == iibp->flag)
	/* both of the same type, use serial to order */
	return iiap->serial - iibp->serial;
    else if (iiap->flag == 0)
	/* a is the PMID, b is an InDom */
	return -1;
    else
	/* b is the PMID, a is an InDom */
	return 1;
}

int
main(int argc, char **argv)
{
    int		n;
    int		c;
    int		i;
    int		sts;
    char	*pmnsfile = PM_NS_DEFAULT;
    int		errflag = 0;
    char	*fname = NULL;
    char	pathname[MAXPATHLEN];
    FILE	*inf;
    char	buf[MAXENTRY+MAXLINE];
    char	*endnum;
    char	*bp;
    char	*p;
    int		skip;
    help_idx_t	hdr;

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:n:o:Vv:?")) != EOF) {
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

	case 'n':	/* alternative namespace file */
	    pmnsfile = optarg;
	    break;

	case 'o':	/* alternative output file name */
	    fname = optarg;
	    break;

	case 'V':	/* more chit-chat */
	    verbose++;
	    break;

	case 'v':	/* version 2 only these days */
	    version = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0') {
		fprintf(stderr, "%s: -v requires numeric argument\n",
			pmProgname);
		errflag++;
	    }
	    if (version != 2) {
		fprintf(stderr 
                       ,"%s: deprecated option - only version 2 is supported\n"
                       , pmProgname);
		errflag++;
	    }
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag) {
	fprintf(stderr,
"Usage: %s [options] [file ...]\n\
\n\
Options:\n\
  -n pmnsfile    use an alternative PMNS\n\
  -o outputfile  base name for output files\n\
  -V             verbose/diagnostic output\n\
  -v version     deprecated (only version 2 format supported)\n",
		pmProgname);
	exit(2);
    }

    if ((n = pmLoadNameSpace(pmnsfile)) < 0) {
	fprintf(stderr, "%s: pmLoadNameSpace: %s\n",
		pmProgname, pmErrStr(n));
	exit(2);
    }

    do {
	if (optind < argc) {
	    filename = argv[optind];
	    if ((inf = fopen(filename, "r")) == NULL) {
		perror(filename);
		exit(2);
	    }
	    if (fname == NULL)
		fname = filename;
	}
	else {
	    if (fname == NULL) {
		fprintf(stderr, 
			"%s: need either a -o option or a filename "
			"argument to name the output file\n", pmProgname);
		exit(2);
	    }
	    filename = "<stdin>";
	    inf = stdin;
	}

	if (version == 2 && f == NULL) {
	    sprintf(pathname, "%s.pag", fname);
	    if ((f = fopen(pathname, "w")) == NULL) {
		fprintf(stderr, "%s: fopen(\"%s\", ...) failed: %s\n",
		    pmProgname, pathname, osstrerror());
		exit(2);
	    }
	    /* header: 2 => pag cf 1 => dir */
	    fprintf(f, "PcPh2%c\n", '0' + version);
	}

	bp = buf;
	skip = 1;
	for ( ; ; ) {
	    if (fgets(bp, MAXLINE, inf) == NULL) {
		skip = -1;
		*bp = '@';
	    }
	    ln++;
	    if (bp[0] == '#')
		continue;
	    if (bp[0] == '@') {
		/* start of a new entry */
		if (bp > buf) {
		    /* really have a prior entry */
		    p = bp - 1;
		    while (p > buf && *p == '\n')
			p--;
		    *++p = '\n';
		    *++p = '\0';
		    newentry(buf);
		}
		if (skip == -1)
		    break;
		skip = 0;
		bp++;	/* skip '@' */
		while (*bp && isspace((int)*bp))
		    bp++;
		if (bp[0] == '\0') {
		    if (verbose)
			fprintf(stderr, "%s: [%s:%d] null entry?\n", 
				pmProgname, filename, ln);
		    skip = 1;
		    bp = buf;
		    if (!status) status = 1;
		}
		else {
		    for (p = bp; *p; p++)
			;
		    memmove(buf, bp, p - bp + 1);
		    for (bp = buf; *bp; bp++)
			;
		}
	    }
	    if (skip)
		continue;
	    for (p = bp; *p; p++)
		;
	    if (bp > buf && p[-1] != '\n') {
		*p++ = '\n';
		*p = '\0';
		fprintf(stderr, "%s: [%s:%d] long line split after ...\n%s",
			    pmProgname, filename, ln, buf);
		ln--;
		if (!status) status = 1;
	    }
	    bp = p;
	    if (bp > &buf[MAXENTRY]) {
		bp = &buf[MAXENTRY];
		bp[-1] = '\0';
		bp[-2] = '\n';
		fprintf(stderr, "%s: [%s:%d] entry truncated after ... %s",
			    pmProgname, filename, ln, &bp[-64]);
		skip = 1;
		if (!status) status = 1;
	    }
	}

	fclose(inf);
	optind++;
    } while (optind < argc);

    if (f != NULL) {
	fclose(f);

	/* do the directory index ... */
	sprintf(pathname, "%s.dir", fname);
	if ((f = fopen(pathname, "w")) == NULL) {
	    fprintf(stderr, "%s: fopen(\"%s\", ...) failed: %s\n",
		pmProgname, pathname, osstrerror());
	    exit(2);
	}

	/* index header */
	hdr.pmid = 0x50635068;		/* "PcPh" */
	/* "1" => dir, next char is version */
	hdr.off_oneline = 0x31000000 | (('0' + version) << 16);
	hdr.off_text = thisindex + 1;	/* # entries */
	if (fwrite(&hdr, sizeof(hdr), 1, f) != sizeof(hdr)
	    || ferror(f)) {
	     fprintf(stderr, "%s: fwrite index failed: %s\n",
		     pmProgname, osstrerror());
	     exit(2);
	}

	/* sort and write index */
	qsort((void *)hindex, thisindex+1, sizeof(hindex[0]), idcomp);
	for (i = 0; i <= thisindex; i++) {
	    if (fwrite(&hindex[i], sizeof(hindex[0]), 1, f) != sizeof(hindex[0])
		|| ferror(f)) {
		 fprintf(stderr, "%s: fwrite index failed: %s\n",
			 pmProgname, osstrerror());
		 exit(2);
	    }
	}
    }

    exit(status);
}
