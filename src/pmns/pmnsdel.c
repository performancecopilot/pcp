/*
 * Cull subtree(s) from a PCP PMNS
 *
 * Copyright (c) 2014 Red Hat.
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
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

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    PMOPT_NAMESPACE,
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "dD:n:?",
    .long_options = longopts,
    .short_usage = "[options] metricpath [...]",
};

static void
delpmns(__pmnsNode *base, char *name)
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
	/* no match ... */
	fprintf(stderr, "%s: Error: metricpath \"%s\" not defined in the PMNS\n",
		pmProgname, fullname);
	exit(1);
    }
    else if (*tail == '\0') {
	/* complete match */
	if (np == base->first) {
	    /* deleted node is first at this level */
	    base->first = np->next;
	    /*
	     * remove predecessors that only exist to connect
	     * deleted node to the rest of the PMNS
	     */
	    np = base;
	    while (np->first == NULL && np->parent != NULL) {
		if (np->parent->first == np) {
		    /* victim is the only one at this level ... */
		    np->parent->first = np->next;
		    np = np->parent;
		}
		else {
		    /* victim has at least one sibling at this level */
		    lastp = np->parent->first;
		    while (lastp->next != np)
			lastp = lastp->next;
		    lastp->next = np->next;
		    break;
		}
	    }
	}
	else
	    /* link around deleted node */
	    lastp->next = np->next;
	return;
    }

    /* descend */
    delpmns(np, tail+1);
}

int
main(int argc, char **argv)
{
    int		sep = __pmPathSeparator();
    int		sts;
    int		c;
    char	*p;
    char	pmnsfile[MAXPATHLEN];
    char	outfname[MAXPATHLEN];
    struct stat	sbuf;

    if ((p = getenv("PMNS_DEFAULT")) != NULL) {
	strncpy(pmnsfile, p, MAXPATHLEN);
        pmnsfile[MAXPATHLEN-1]= '\0';

    } else {
	snprintf(pmnsfile, sizeof(pmnsfile), "%s%c" "pmns" "%c" "root",
		pmGetConfig("PCP_VAR_DIR"), sep, sep);
    }

    while ((c = pmgetopt_r(argc, argv, &opts)) != EOF) {
	switch (c) {

	case 'd':	/* duplicate PMIDs are OK */
	    fprintf(stderr, "%s: Warning: -d deprecated, duplicate PMNS names allowed by default\n", pmProgname);
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

	case 'n':	/* alternative name space file */
	    strncpy(pmnsfile, opts.optarg, MAXPATHLEN);
	    pmnsfile[MAXPATHLEN-1]= '\0';
	    break;

	case '?':
	default:
	    opts.errors++;
	    break;
	}
    }

    if (opts.errors || opts.optind > argc - 1) {
	pmUsageMessage(&opts);
	exit(1);
    }

    if ((sts = pmLoadASCIINameSpace(pmnsfile, 1)) < 0) {
	fprintf(stderr, "%s: Error: pmLoadASCIINameSpace(%s, 1): %s\n",
		pmProgname, pmnsfile, pmErrStr(sts));
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
        root = t->root;
    }


    while (opts.optind < argc) {
	delpmns(root, fullname = argv[opts.optind]);
	opts.optind++;
    }

    /*
     * from here on, ignore SIGHUP, SIGINT and SIGTERM to protect
     * the integrity of the new ouput file
     */
    __pmSetSignalHandler(SIGHUP, SIG_IGN);
    __pmSetSignalHandler(SIGINT, SIG_IGN);
    __pmSetSignalHandler(SIGTERM, SIG_IGN);

    snprintf(outfname, sizeof(outfname), "%s.new", pmnsfile);
    if ((outf = fopen(outfname, "w")) == NULL) {
	fprintf(stderr, "%s: Error: cannot open PMNS file \"%s\" for writing: %s\n",
		pmProgname, outfname, osstrerror());
	exit(1);
    }
    if (stat(pmnsfile, &sbuf) == 0) {
	/*
	 * preserve the mode and ownership of any existing PMNS file
	 */
	chmod(outfname, sbuf.st_mode & ~S_IFMT);
#if defined(HAVE_CHOWN)
	if (chown(outfname, sbuf.st_uid, sbuf.st_gid) < 0)
	    fprintf(stderr, "%s: chown(%s, ...) failed: %s\n",
		    pmProgname, outfname, osstrerror());
#endif
    }

    pmns_output(root, outf);
    fclose(outf);

    /* rename the PMNS */
    if (rename2(outfname, pmnsfile) == -1) {
	fprintf(stderr, "%s: cannot rename \"%s\" to \"%s\": %s\n",
		pmProgname, outfname, pmnsfile, osstrerror());
	/* remove the new PMNS */
	unlink(outfname);
	exit(1);
    }

    exit(0);
}
