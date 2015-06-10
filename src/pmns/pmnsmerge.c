/*
 * pmnsmerge [-adfv] infile [...] outfile
 *
 * Merge PCP PMNS files
 *
 * Copyright (c) 1995-2002 Silicon Graphics, Inc.  All Rights Reserved.
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
#include <sys/stat.h>
#include "pmapi.h"
#include "impl.h"
#include "pmnsutil.h"

static FILE		*outf;		/* output */
static __pmnsNode	*root;		/* result so far */
static char		*fullname;	/* full PMNS pathname for newbie */
static int		verbose;

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    { "", 0, 'a', 0, "process files in order, ignoring embedded _DATESTAMP control lines" },
    { "dupok", 0, 'd', 0, "duplicate names for the same PMID are allowed [default]" },
    { "force", 0, 'f', 0, "force overwriting of the output file if it exists" },
    { "nodups", 0, 'x', 0, "duplicate names for the same PMID are not allowed" },
    { "verbose", 0, 'v', 0, "verbose, echo input file names as processed" },
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "aD:dfvx?",
    .long_options = longopts,
    .short_usage = "[options] infile [...] outfile",
};

typedef struct {
    char	*fname;
    char	*date;
} datestamp_t;

#define STAMP	"_DATESTAMP"

static int
sortcmp(const void *a, const void *b)
{
    datestamp_t	*pa = (datestamp_t *)a;
    datestamp_t	*pb = (datestamp_t *)b;

    if (pa->date == NULL) return -1;
    if (pb->date == NULL) return 1;
    return strcmp(pa->date, pb->date);
}

/*
 * scan for #define _DATESTAMP and re-order args accordingly
 */
static void
sortargs(char **argv, int argc)
{
    FILE	*f;
    datestamp_t	*tab;
    char	*p;
    char	*q;
    int		i;
    char	lbuf[40];

    tab = (datestamp_t *)malloc(argc * sizeof(datestamp_t));

    for (i = 0; i <argc; i++) {
	if ((f = fopen(argv[i], "r")) == NULL) {
	    fprintf(stderr, "%s: Error: cannot open input PMNS file \"%s\"\n",
		pmProgname, argv[i]);
	    exit(1);
	}
	tab[i].fname = strdup(argv[i]);
	tab[i].date = NULL;
	while (fgets(lbuf, sizeof(lbuf), f) != NULL) {
	    if (strncmp(lbuf, "#define", 7) != 0)
		continue;
	    p = &lbuf[7];
	    while (*p && isspace((int)*p))
		p++;
	    if (*p == '\0' || strncmp(p, STAMP, strlen(STAMP)) != 0)
		continue;
	    p += strlen(STAMP);
	    while (*p && isspace((int)*p))
		p++;
	    q = p;
	    while (*p && !isspace((int)*p))
		p++;
	    *p = '\0';
	    tab[i].date = strdup(q);
	    break;
	}
	fclose(f);
    }

    qsort(tab, argc, sizeof(tab[0]), sortcmp);
    for (i = 0; i <argc; i++) {
	argv[i] = tab[i].fname;
	if (verbose > 1)
	    printf("arg[%d] %s _DATESTAMP=%s\n", i, tab[i].fname, tab[i].date);
    }

    free(tab);
}

static void
addpmns(__pmnsNode *base, char *name, __pmnsNode *p)
{
    char	*tail;
    ptrdiff_t	nch;
    __pmnsNode	*np;
    __pmnsNode	*lastp = NULL;

    for (tail = name; *tail && *tail != '.'; tail++)
	;
    nch = tail - name;

    for (np = base->first; np != NULL; np = np->next) {
	if (strlen(np->name) == nch && strncmp(name, np->name, (int)nch) == 0)
	    break;
	lastp = np;
    }

    if (np == NULL) {
	/* no match ... add here */
	np = (__pmnsNode *)malloc(sizeof(__pmnsNode));
	if (base->first) {
	    lastp->next = np;
	    np->parent = lastp->parent;
	}
	else {
	    base->first = np;
	    np->parent = base;
	}
	np->first = np->next = NULL;
	np->hash = NULL;		/* we do not need this here */
	np->name = (char *)malloc(nch+1);
	strncpy(np->name, name, nch);
	np->name[nch] = '\0';
	if (*tail == '\0') {
	    np->pmid = p->pmid;
	    return;
	}
	np->pmid = PM_ID_NULL;
    }
    else if (*tail == '\0') {
	/* complete match */
	if (np->pmid != p->pmid) {
	    fprintf(stderr, "%s: Warning: performance metric \"%s\" has multiple PMIDs.\n... using PMID %s and ignoring PMID",
		pmProgname, fullname, pmIDStr(np->pmid));
	    fprintf(stderr, " %s\n",
		pmIDStr(p->pmid));
	}
	return;
    }

    /* descend */
    addpmns(np, tail+1, p);
}


/*
 * merge, adding new nodes if required
 */
static void
merge(__pmnsNode *p, int depth, char *path)
{
    char	*name;

    if (depth < 1 || p->pmid == PM_ID_NULL || p->first != NULL)
	return;
    name = (char *)malloc(strlen(path)+strlen(p->name)+2);
    if (*path == '\0')
	strcpy(name, p->name);
    else {
	strcpy(name, path);
	strcat(name, ".");
	strcat(name, p->name);
    }
    fullname = name;
    addpmns(root, name, p);
    free(name);
}

int
main(int argc, char **argv)
{
    int		sts;
    int		first = 1;
    int		c;
    int		j;
    int		force = 0;
    int		asis = 0;
    int		dupok = 1;
    __pmnsNode	*tmp;

    umask((mode_t)022);		/* anything else is pretty silly */

    while ((c = pmgetopt_r(argc, argv, &opts)) != EOF) {
	switch (c) {

	case 'a':
	    asis = 1;
	    break;

	case 'd':	/* duplicate PMIDs are OK */
	    fprintf(stderr, "%s: Warning: -d deprecated, duplicate PMNS names allowed by default\n", pmProgname);
	    dupok = 1;
		break;

	case 'D':	/* debug flag */
	    if ((sts = __pmParseDebug(opts.optarg)) < 0) {
		fprintf(stderr, "%s: unrecognized debug flag specification (%s)\n",
		    pmProgname, opts.optarg);
		opts.errors++;
	    } else {
		pmDebug |= sts;
	    }
	    break;

	case 'f':	/* force ... clobber output file if it exists */
	    force = 1;
	    break;

	case 'x':	/* duplicate PMIDs are NOT OK */
	    dupok = 0;
	    break;

	case 'v':
	    verbose++;
	    break;

	case '?':
	default:
	    opts.errors++;
	    break;
	}
    }

    if (opts.errors || opts.optind > argc - 2) {
	pmUsageMessage(&opts);
	exit(1);
    }

    if (force) {
	/* try to unlink, but if this fails it is OK because
	 * we truncate the file in the fopen() below and check for
	 * errors there
	 */
	unlink(argv[argc-1]);
    }
    else if (access(argv[argc-1], F_OK) == 0) {
	fprintf(stderr, "%s: Error: output PMNS file \"%s\" already exists!\nYou must either remove it first, or use -f\n",
		pmProgname, argv[argc-1]);
	exit(1);
    }

    /*
     * from here on, ignore SIGHUP, SIGINT and SIGTERM to protect
     * the integrity of the new ouput file
     */
    __pmSetSignalHandler(SIGHUP, SIG_IGN);
    __pmSetSignalHandler(SIGINT, SIG_IGN);
    __pmSetSignalHandler(SIGTERM, SIG_IGN);

    if ((outf = fopen(argv[argc-1], "w+")) == NULL) {
	fprintf(stderr, "%s: Error: cannot create output PMNS file \"%s\": %s\n", pmProgname, argv[argc-1], osstrerror());
	exit(1);
    }

    if (!asis)
	sortargs(&argv[opts.optind], argc - opts.optind - 1);

    j = opts.optind;
    while (j < argc-1) {
	if (verbose)
	    printf("%s:\n", argv[j]);

	if ((sts = pmLoadASCIINameSpace(argv[j], dupok)) < 0) {
	    fprintf(stderr, "%s: Error: pmLoadASCIINameSpace(%s, %d): %s\n",
		pmProgname, argv[j], dupok, pmErrStr(sts));
	    exit(1);
	}
	{
	    __pmnsTree *t;
	    t = __pmExportPMNS();
	    if (t == NULL) {
	       /* sanity check - shouldn't ever happen */
	       fprintf(stderr, "Exported PMNS is NULL !");
	       exit(1);
	    }
	    tmp = t->root;
	}

	if (first) {
	    root = tmp;
	    first = 0;
	}
	else {
	    pmns_traverse(tmp, 0, "", merge);
	}
	j++;
    }

    pmns_output(root, outf);
    fclose(outf);

    /*
     * now load the merged PMNS to check for errors ...
     */
    if ((sts = pmLoadASCIINameSpace(argv[argc-1], dupok)) < 0) {
	fprintf(stderr, "%s: Error: pmLoadASCIINameSpace(%s, %d): %s\n",
	    pmProgname, argv[argc-1], dupok, pmErrStr(sts));
	exit(1);
    }

    exit(0);
}
