/*
 * Copyright (c) 2012-2015 Red Hat.
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * Thread-safe notes
 *
 * locerr - no serious side-effects, most unlikely to be used, and
 * repeated calls are likely to produce the same result, so don't bother
 * to make thread-safe
 */

#include <sys/stat.h>
#include <stddef.h>
#include <assert.h>
#include <ctype.h>
#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "internal.h"
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

/* token types */
#define NAME	1
#define PATH	2
#define PMID	3
#define LBRACE	4
#define RBRACE	5
#define BOGUS	10

#define UNKNOWN_MARK_STATE -1           /* tree not all marked the same way */
/*
 * Note: bit masks below are designed to clear and set the "flag" field
 *       of a __pmID_int (i.e. a PMID)
 */
#define PMID_MASK	0x7fffffff	/* 31 bits of PMID */
#define MARK_BIT	0x80000000	/* mark bit */

/*
 * conditional controls
 */
#define NO_DUPS		0
#define DUPS_OK		1
#define NO_CPP		0
#define USE_CPP		1


static int	lineno;
static char	linebuf[256];
static char	*linep;
static char	fname[256];
static char	tokbuf[256];
static pmID	tokpmid;
static int	seenpmid;

static __pmnsNode *seen; /* list of pass-1 subtree nodes */

/* Last modification time for loading main_pmns file. */
#if defined(HAVE_STAT_TIMESTRUC)
static timestruc_t	last_mtim;
#elif defined(HAVE_STAT_TIMESPEC)
static struct timespec	last_mtim;
#elif defined(HAVE_STAT_TIMESPEC_T)
static timespec_t	last_mtim;
#elif defined(HAVE_STAT_TIME_T)
static time_t	last_mtim;
#else
!bozo!
#endif


/* The PM_TPD(curr_pmns) points to PMNS to use for API ops.
 * Curr_pmns will point to either the main_pmns or
 * a pmns from a version 2 archive context.
 */
#ifdef PM_MULTI_THREAD
#ifdef HAVE___THREAD
/* using a gcc construct here to make curcontext thread-private */
static __thread __pmnsTree*	curr_pmns = NULL;
static __thread int             useExtPMNS = 0;
#endif
#else
static __pmnsTree*              curr_pmns = NULL;
static int                      useExtPMNS = 0;
#endif


/* The main_pmns points to the single global loaded PMNS (not from
   archive).  It is not generally modified after being loaded. */
static __pmnsTree *main_pmns;


/* == 1 if PMNS loaded and __pmExportPMNS has been called */
static int export;

static int havePmLoadCall;

static int load(const char *, int, int);
static __pmnsNode *locate(const char *, __pmnsNode *);

/*
 * Helper routine to report all the names for a metric ...
 * numnames and names[] would typically by returned from
 * an earlier call to pmNameAll()
 */
void
__pmPrintMetricNames(FILE *f, int numnames, char **names, char *sep)
{
    int		j;
    
    if (numnames < 1)
	fprintf(f, "<nonames>");
    else {
	for (j = 0; j < numnames; j++) {
	    if (j == 0)
		fprintf(f, "%s", names[j]);
	    else
		fprintf(f, "%s%s", sep, names[j]);
	}
    }
}

/*
 * Set current pmns to an externally supplied PMNS.
 * Useful for testing the API routines during debugging.
 */
void
__pmUsePMNS(__pmnsTree *t)
{
    PM_INIT_LOCKS();
    PM_TPD(useExtPMNS) = 1;
    PM_TPD(curr_pmns) = t;
}

static char *
pmPMNSLocationStr(int location)
{
    if (location < 0) {
	/* see thread-safe note above */
	static char	locerr[PM_MAXERRMSGLEN];
	return pmErrStr_r(location, locerr, sizeof(locerr));
    }

    switch(location) {
    case PMNS_LOCAL:	return "Local";
    case PMNS_REMOTE:	return "Remote";
    case PMNS_ARCHIVE:	return "Archive";
    }
    return "Internal Error";
}


static int
LoadDefault(char *reason_msg, int use_cpp)
{
    if (main_pmns == NULL) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMNS) {
	    fprintf(stderr,
		"pmGetPMNSLocation: Loading local PMNS for %s PMAPI context\n",
		reason_msg);
	}
#endif
	/* duplicate names in the PMNS are OK now ... */
	if (load(PM_NS_DEFAULT, DUPS_OK, NO_CPP) < 0)
	    return PM_ERR_NOPMNS;
	else
	    return PMNS_LOCAL;
    }
    return PMNS_LOCAL;
}

/*
 * Return the pmns_location.  Possibly load the default PMNS.
 */
int 
pmGetPMNSLocation(void)
{
    int pmns_location = PM_ERR_NOPMNS;
    int n;
    int sts;

    PM_INIT_LOCKS();
    if (PM_TPD(useExtPMNS)) {
	pmns_location = PMNS_LOCAL;
	goto done;
    }

    /* 
     * Determine if we are to use PDUs or local PMNS file.
     * Load PMNS if necessary.
     */
    if (!havePmLoadCall) {
	__pmContext  	*ctxp;
	int		version;

	if ((n = pmWhichContext()) >= 0 && (ctxp = __pmHandleToPtr(n)) != NULL) {
	    switch(ctxp->c_type) {
		case PM_CONTEXT_HOST:
		    if (ctxp->c_pmcd->pc_fd == -1) {
			pmns_location = PM_ERR_IPC;
			goto done;
		    }
		    if ((sts = version = __pmVersionIPC(ctxp->c_pmcd->pc_fd)) < 0) {
			char	errmsg[PM_MAXERRMSGLEN];
			__pmNotifyErr(LOG_ERR, 
				"pmGetPMNSLocation: version lookup failed "
				"(context=%d, fd=%d): %s", 
				n, ctxp->c_pmcd->pc_fd, pmErrStr_r(sts, errmsg, sizeof(errmsg)));
			pmns_location = PM_ERR_NOPMNS;
		    }
		    else if (version == PDU_VERSION2) {
			pmns_location = PMNS_REMOTE;
		    }
		    else {
			__pmNotifyErr(LOG_ERR, 
				"pmGetPMNSLocation: bad host PDU version "
				"(context=%d, fd=%d, ver=%d)",
				n, ctxp->c_pmcd->pc_fd, version);
			pmns_location = PM_ERR_NOPMNS;
		    }
		    break;

		case PM_CONTEXT_LOCAL:
		    if (PM_MULTIPLE_THREADS(PM_SCOPE_DSO_PMDA))
			/* Local context requires single-threaded applications */
			pmns_location = PM_ERR_THREAD;
		    else
			pmns_location = LoadDefault("local", 0);
		    break;

		case PM_CONTEXT_ARCHIVE:
		    version = ctxp->c_archctl->ac_log->l_label.ill_magic & 0xff;
		    if (version == PM_LOG_VERS02) {
			pmns_location = PMNS_ARCHIVE;
			PM_TPD(curr_pmns) = ctxp->c_archctl->ac_log->l_pmns; 
		    }
		    else {
			__pmNotifyErr(LOG_ERR, "pmGetPMNSLocation: bad archive "
				"version (context=%d, fd=%d, ver=%d)",
				n, ctxp->c_pmcd->pc_fd, version); 
			pmns_location = PM_ERR_NOPMNS;
		    }
		    break;

		default: 
		    __pmNotifyErr(LOG_ERR, "pmGetPMNSLocation: bogus context "
				"type: %d", ctxp->c_type); 
		    pmns_location = PM_ERR_NOPMNS;
		    break;
	    }
	    PM_UNLOCK(ctxp->c_lock);
	}
	else {
	    pmns_location = PM_ERR_NOPMNS; /* no context for client */
	}
    }
    else { /* have explicit external load call */
	if (main_pmns == NULL)
	    pmns_location = PM_ERR_NOPMNS;
	else
	    pmns_location = PMNS_LOCAL;
    }

    PM_LOCK(__pmLock_libpcp);
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PMNS) {
	static int last_pmns_location = -1;

	if (pmns_location != last_pmns_location) {
	    fprintf(stderr, "pmGetPMNSLocation() -> %s\n", 
			    pmPMNSLocationStr(pmns_location));
	    last_pmns_location = pmns_location;
	}
    }
#endif

    /* fix up curr_pmns for API ops */
    if (pmns_location == PMNS_LOCAL)
	PM_TPD(curr_pmns) = main_pmns;
    PM_UNLOCK(__pmLock_libpcp);

done:
    return pmns_location;
}

/*
 * Our own PMNS locator.  Don't distinguish between ARCHIVE or LOCAL.
 */
static int
GetLocation(void)
{
    int	loc = pmGetPMNSLocation();

    if (loc == PMNS_ARCHIVE)
	return PMNS_LOCAL;
    return loc;
}

/*
 * For debugging, call via __pmDumpNameSpace() or __pmDumpNameNode()
 *
 * verbosity is 0 (name), 1 (names and pmids) or 2 (names, pmids and
 * linked-list structures)
 */
static void
dumptree(FILE *f, int level, __pmnsNode *rp, int verbosity)
{
    int		i;
    __pmID_int	*pp;

    if (rp != NULL) {
	if (verbosity > 1)
	    fprintf(f, "" PRINTF_P_PFX "%p", rp);
	for (i = 0; i < level; i++) {
	    fprintf(f, "    ");
	}
	fprintf(f, " %-16.16s", rp->name);
	pp = (__pmID_int *)&rp->pmid;
	if (verbosity > 0 && rp->first == NULL)
	    fprintf(f, " %d %d.%d.%d 0x%08x", rp->pmid,
		    pp->domain, pp->cluster, pp->item,
		    rp->pmid);
	if (verbosity > 1) {
	    fprintf(f, "\t[first: ");
	    if (rp->first) fprintf(f, "" PRINTF_P_PFX "%p", rp->first);
	    else fprintf(f, "<null>");
	    fprintf(f, " next: ");
	    if (rp->next) fprintf(f, "" PRINTF_P_PFX "%p", rp->next);
	    else fprintf(f, "<null>");
	    fprintf(f, " parent: ");
	    if (rp->parent) fprintf(f, "" PRINTF_P_PFX "%p", rp->parent);
	    else fprintf(f, "<null>");
	    fprintf(f, " hash: ");
	    if (rp->hash) fprintf(f, "" PRINTF_P_PFX "%p", rp->hash);
	    else fprintf(f, "<null>");
	}
	fputc('\n', f);
	dumptree(f, level+1, rp->first, verbosity);
	dumptree(f, level, rp->next, verbosity);
    }
}

static void
err(char *s)
{
    if (lineno > 0)
	pmprintf("[%s:%d] ", fname, lineno);
    pmprintf("Error Parsing ASCII PMNS: %s\n", s);
    if (lineno > 0) {
	char	*p;
	pmprintf("    %s", linebuf);
	for (p = linebuf; *p; p++)
	    ;
	if (p[-1] != '\n')
	    pmprintf("\n");
	if (linep) {
	    p = linebuf;
	    for (p = linebuf; p < linep; p++) {
		if (!isspace((int)*p))
		    *p = ' ';
	    }
	    *p++ = '^';
	    *p++ = '\n';
	    *p = '\0';
	    pmprintf("    %s", linebuf);
	}
    }
    pmflush();
}

/*
 * lexical analyser for loading the ASCII pmns
 * reset == 0 => get next token
 * reset == 1+NO_CPP => initialize to pre-process with pmcpp and popen()
 * reset == 1+USE_CPP => initialize to use fopen() not popen()
 */
static int
lex(int reset)
{
    static int	first = 1;
    static int	use_cpp;
    static FILE	*fin;
    static char	*lp;
    char	*tp;
    int		colon;
    int		type;
    int		d, c, i;
    __pmID_int	pmid_int;

    if (reset == 1+NO_CPP || reset == 1+USE_CPP) {
	/* reset/initialize */
	linep = NULL;
	first = 1;
	if (reset == 1+NO_CPP)
	    use_cpp = NO_CPP;
	else
	    /* else assume pmcpp(1) needed */
	    use_cpp = USE_CPP;
	return 0;
    }

    if (first) {
	if (use_cpp == USE_CPP) {
	    char	*alt;
	    char	cmd[80+MAXPATHLEN];

	    if ((alt = getenv("PCP_ALT_CPP")) != NULL) {
		/* $PCP_ALT_CPP used in the build before pmcpp installed */
		snprintf(cmd, sizeof(cmd), "%s %s", alt, fname);
	    }
	    else {
		/* the normal case ... */
		int	sep = __pmPathSeparator();
		char	*bin_dir = pmGetOptionalConfig("PCP_BINADM_DIR");

		if (bin_dir == NULL)
		    return PM_ERR_GENERIC;
		snprintf(cmd, sizeof(cmd), "%s%c%s %s", bin_dir, sep, "pmcpp" EXEC_SUFFIX, fname);
	    }

	    if ((fin = popen(cmd, "r")) == NULL)
		return -oserror();
	}
	else {
	    if ((fin = fopen(fname, "r")) == NULL)
		return -oserror();
	}

	first = 0;
	lp = linebuf;
	*lp = '\0';
    }

    while (*lp && isspace((int)*lp)) lp++;

    while (*lp == '\0') {
	for ( ; ; ) {
	    char	*p;
	    char	*q;
	    int		inspace = 0;

	    if (fgets(linebuf, sizeof(linebuf), fin) == NULL) {
		lineno = -1; /* We're outside of line counting range now */
		if (use_cpp == USE_CPP) {
		    if (pclose(fin) != 0) {
			err("pmcpp returned non-zero exit status");
			return PM_ERR_PMNS;
		    }
		}
		else {
		    if (fclose(fin) != 0) {
			err("fclose returned non-zero exit status");
			return PM_ERR_PMNS;
		    }
		}
		return 0;
	    }
	    for (q = p = linebuf; *p; p++) {
		if (isspace((int)*p)) {
		    if (!inspace) {
			if (q > linebuf && q[-1] != ':')
			    *q++ = *p;
			inspace = 1;
		    }
		}
		else if (*p == ':') {
		    if (inspace) {
			q--;
			inspace = 0;
		    }
		    *q++ = *p;
		}
		else {
		    *q++ = *p;
		    inspace = 0;
		}
	    }
	    if (p[-1] != '\n') {
		err("Absurdly long line, cannot recover");
		return PM_ERR_PMNS;
	    }
	    *q = '\0';
	    if (linebuf[0] == '#') {
		/* pmcpp line number control line */
		if (sscanf(linebuf, "# %d \"%s", &lineno, fname) != 2) {
		    err("Illegal line number control number");
		    return PM_ERR_PMNS;
		}
		--lineno;
		for (p = fname; *p; p++)
		    ;
		*--p = '\0';
		continue;
	    }
	    else
		lineno++;
	    lp = linebuf;
	    while (*lp && isspace((int)*lp)) lp++;
	    break;
	}
    }

    linep = lp;
    tp = tokbuf;
    while (!isspace((int)*lp))
	*tp++ = *lp++;
    *tp = '\0';

    if (tokbuf[0] == '{' && tokbuf[1] == '\0') return LBRACE;
    else if (tokbuf[0] == '}' && tokbuf[1] == '\0') return RBRACE;
    else if (isalpha((int)tokbuf[0])) {
	type = NAME;
	for (tp = &tokbuf[1]; *tp; tp++) {
	    if (*tp == '.')
		type = PATH;
	    else if (!isalpha((int)*tp) && !isdigit((int)*tp) && *tp != '_')
		break;
	}
	if (*tp == '\0') return type;
    }
    colon = 0;
    for (tp = tokbuf; *tp; tp++) {
	if (*tp == ':') {
	    if (++colon > 3) return BOGUS;
	}
	else if (!isdigit((int)*tp) && *tp != '*') return BOGUS;
    }

    /*
     * Internal PMID format
     * domain 9 bits
     * cluster 12 bits
     * item 10 bits
     */
    if (sscanf(tokbuf, "%d:%d:%d", &d, &c, &i) == 3) {
	if (d > 510) {
	    err("Illegal domain field in PMID");
	    return BOGUS;
	}
	else if (c > 4095) {
	    err("Illegal cluster field in PMID");
	    return BOGUS;
	}
	else if (i > 1023) {
	    err("Illegal item field in PMID");
	    return BOGUS;
	}
	pmid_int.flag = 0;
	pmid_int.domain = d;
	pmid_int.cluster = c;
	pmid_int.item = i;
    }
    else {
	for (tp = tokbuf; *tp; tp++) {
	    if (*tp == ':') {
		if (strcmp("*:*", ++tp) != 0) {
		    err("Illegal PMID");
		    return BOGUS;
		}
		break;
	    }
	}
	if (sscanf(tokbuf, "%d:", &d) != 1) {
	    err("Illegal PMID");
	    return BOGUS;
	}
	if (d > 510) {
	    err("Illegal domain field in dynamic PMID");
	    return BOGUS;
	}
	else {
	    /*
	     * this node is the base of a dynamic subtree in the PMNS
	     * ... identified by setting the domain field to the reserved
	     * value DYNAMIC_PMID and storing the real domain of the PMDA
	     * that can enumerate the subtree in the cluster field, while
	     * the item field is not used (and set to zero)
	     */
	    pmid_int.flag = 0;
	    pmid_int.domain = DYNAMIC_PMID;
	    pmid_int.cluster = d;
	    pmid_int.item = 0;
	}
    }
    tokpmid = *(pmID *)&pmid_int;

    return PMID;
}

/*
 * Remove the named node from the seen list and return it.
 * The seen-list is a list of subtrees from pass 1.
 */

static __pmnsNode *
findseen(char *name)
{
    __pmnsNode	*np;
    __pmnsNode	*lnp; /* last np */

    for (np = seen, lnp = NULL; np != NULL; lnp = np, np = np->next) {
	if (strcmp(np->name, name) == 0) {
	    if (np == seen)
		seen = np->next;
	    else
		lnp->next = np->next;
	    np->next = NULL;
	    return np;
	}
    }
    return NULL;
}

/*
 * Attach the subtrees from pass-1 to form a whole 
 * connected tree.
 */
static int
attach(char *base, __pmnsNode *rp)
{
    int		i;
    __pmnsNode	*np;
    __pmnsNode	*xp;
    char	*path;

    if (rp != NULL) {
	for (np = rp->first; np != NULL; np = np->next) {
	    if (np->pmid == PM_ID_NULL) {
		/* non-terminal node ... */
		if (*base == '\0') {
		    if ((path = (char *)malloc(strlen(np->name)+1)) == NULL)
			return -oserror();
		    strcpy(path, np->name);
		}
		else {
		    if ((path = (char *)malloc(strlen(base)+strlen(np->name)+2)) == NULL)
			return -oserror();
		    strcpy(path, base);
		    strcat(path, ".");
		    strcat(path, np->name);
		}
		if ((xp = findseen(path)) == NULL) {
		    snprintf(linebuf, sizeof(linebuf), "Cannot find definition for non-terminal node \"%s\" in name space",
		        path);
		    err(linebuf);
		    free(path);
		    return PM_ERR_PMNS;
		}
		np->first = xp->first;
		/* node xp and name no longer needed */
		free(xp->name);
		free(xp);
		seenpmid--;
		i = attach(path, np);
		free(path);
		if (i != 0)
		    return i;
	    }
	}
    }
    return 0;
}

/*
 * Create a fullpath name by walking from the current
 * tree node up to the root.
 */
static int
backname(__pmnsNode *np, char **name)
{
    __pmnsNode	*xp;
    char	*p;
    int		nch;

    nch = 0;
    xp = np;
    while (xp->parent != NULL) {
	nch += (int)strlen(xp->name)+1;
	xp = xp->parent;
    }

    if ((p = (char *)malloc(nch)) == NULL)
	return -oserror();

    p[--nch] = '\0';
    xp = np;
    while (xp->parent != NULL) {
	int	xl;

	xl = (int)strlen(xp->name);
	nch -= xl;
	strncpy(&p[nch], xp->name, xl);
	xp = xp->parent;
	if (xp->parent == NULL)
	    break;
	else
	    p[--nch] = '.';
    }
    *name = p;

    return 0;
}

/*
 * Fixup the parent pointers of the tree.
 * Fill in the hash table with nodes from the tree.
 * Hashing is done on pmid.
 */
static int
backlink(__pmnsTree *tree, __pmnsNode *root, int dupok)
{
    __pmnsNode	*np;
    int		status;

    for (np = root->first; np != NULL; np = np->next) {
	np->parent = root;
	if (np->pmid != PM_ID_NULL) {
	    int		i;
	    __pmnsNode	*xp;
	    i = np->pmid % tree->htabsize;
	    for (xp = tree->htab[i]; xp != NULL; xp = xp->hash) {
		if (xp->pmid == np->pmid && dupok == NO_DUPS) {
		    char	*nn, *xn;
		    char	strbuf[20];
		    backname(np, &nn);
		    backname(xp, &xn);
		    snprintf(linebuf, sizeof(linebuf), "Duplicate metric id (%s) in name space for metrics \"%s\" and \"%s\"\n",
		        pmIDStr_r(np->pmid, strbuf, sizeof(strbuf)), nn, xn);
		    err(linebuf);
		    free(nn);
		    free(xn);
		    return PM_ERR_PMNS;
		}
	    }
	    np->hash = tree->htab[i];
	    tree->htab[i] = np;
	}
	if ((status = backlink(tree, np, dupok)))
	    return status;
    }
    return 0;
}

/*
 * Build up the whole tree by attaching the subtrees
 * from the seen list.
 * Create the hash table keyed on pmid.
 *
 */
static int
pass2(int dupok)
{
    __pmnsNode	*np;
    int		status;

    lineno = -1;

    main_pmns = (__pmnsTree*)malloc(sizeof(*main_pmns));
    if (main_pmns == NULL) {
	return -oserror();
    }

    /* Get the root subtree out of the seen list */
    if ((main_pmns->root = findseen("root")) == NULL) {
	err("No name space entry for \"root\"");
	return PM_ERR_PMNS;
    }

    if (findseen("root") != NULL) {
	err("Multiple name space entries for \"root\"");
	return PM_ERR_PMNS;
    }

    /* Build up main tree from subtrees in seen-list */
    if ((status = attach("", main_pmns->root)))
	return status;

    /* Make sure all subtrees have been used in the main tree */
    for (np = seen; np != NULL; np = np->next) {
	snprintf(linebuf, sizeof(linebuf), "Disconnected subtree (\"%s\") in name space", np->name);
	err(linebuf);
	status = PM_ERR_PMNS;
    }
    if (status)
	return status;

    main_pmns->symbol = NULL;
    main_pmns->contiguous = 0;
    main_pmns->mark_state = UNKNOWN_MARK_STATE;

    return __pmFixPMNSHashTab(main_pmns, seenpmid, dupok);
}


/*
 * clear/set the "mark" bit used by pmTrimNameSpace, for all pmids
 */
static void
mark_all(__pmnsTree *pmns, int bit)
{
    int		i;
    __pmnsNode	*np;
    __pmnsNode	*pp;

    if (pmns->mark_state == bit)
	return;

    pmns->mark_state = bit;
    for (i = 0; i < pmns->htabsize; i++) {
	for (np = pmns->htab[i]; np != NULL; np = np->hash) {
	    for (pp = np ; pp != NULL; pp = pp->parent) {
		if (bit)
		    pp->pmid |= MARK_BIT;
		else
		    pp->pmid &= ~MARK_BIT;
	    }
	}
    }
}

/*
 * clear/set the "mark" bit used by pmTrimNameSpace, for one pmid, and
 * for all parent nodes on the path to the root of the PMNS
 */
static void
mark_one(__pmnsTree *pmns, pmID pmid, int bit)
{
    __pmnsNode	*np;

    if (pmns->mark_state == bit)
	return;

    pmns->mark_state = UNKNOWN_MARK_STATE;
    for (np = pmns->htab[pmid % pmns->htabsize]; np != NULL; np = np->hash) {
	if ((np->pmid & PMID_MASK) == (pmid & PMID_MASK)) {
	    /* mark nodes from leaf to root of the PMNS */
	    __pmnsNode	*lnp;
	    for (lnp = np; lnp != NULL; lnp = lnp->parent) {
		if (bit)
		    lnp->pmid |= MARK_BIT;
		else
		    lnp->pmid &= ~MARK_BIT;
	    }
	    /* keep going, may be more than one name with this PMID */
	}
    }
}


/*
 * Create a new empty PMNS for Adding nodes to.
 * Use with __pmAddPMNSNode() and __pmFixPMNSHashTab()
 */
int
__pmNewPMNS(__pmnsTree **pmns)
{
    __pmnsTree *t = NULL;
    __pmnsNode *np = NULL;

    t = (__pmnsTree*)malloc(sizeof(*main_pmns));
    if (t == NULL)
	return -oserror();

    /* Insert the "root" node first */
    if ((np = (__pmnsNode *)malloc(sizeof(*np))) == NULL) {
	free(t);
	return -oserror();
    }
    np->pmid = PM_ID_NULL;
    np->parent = np->first = np->hash = np->next = NULL;
    np->name = strdup("root");
    if (np->name == NULL) {
	free(t);
	free(np);
	return -oserror();
    }

    t->root = np;
    t->htab = NULL;
    t->htabsize = 0;
    t->symbol = NULL;
    t->contiguous = 0;
    t->mark_state = UNKNOWN_MARK_STATE;

    *pmns = t;
    return 0;
}

/*
 * Go through the tree and build a hash table.
 * Fix up parent links while we're there.
 * Unmark all nodes.
 */
int
__pmFixPMNSHashTab(__pmnsTree *tree, int numpmid, int dupok)
{
    int sts;
    int htabsize = numpmid/5;

    /*
     * make the average hash list no longer than 5, and the number
     * of hash table entries not a multiple of 2, 3 or 5
     */
    if (htabsize % 2 == 0) htabsize++;
    if (htabsize % 3 == 0) htabsize += 2;
    if (htabsize % 5 == 0) htabsize += 2;
    tree->htabsize = htabsize;
    tree->htab = (__pmnsNode **)calloc(htabsize, sizeof(__pmnsNode *));
    if (tree->htab == NULL)
	return -oserror();

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    if ((sts = backlink(tree, tree->root, dupok)) < 0) {
	PM_UNLOCK(__pmLock_libpcp);
	return sts;
    }
    mark_all(tree, 0);
    PM_UNLOCK(__pmLock_libpcp);
    return 0;
}


/*
 * Add a new node for fullpath, name, with pmid.
 * Does NOT update the hash table;
 * need to call __pmFixPMNSHashTab() for that.
 * Recursive routine.
 */

static int
AddPMNSNode(__pmnsNode *root, int pmid, const char *name)
{
    __pmnsNode *np = NULL;
    const char *tail;
    int nch;

    /* Traverse until '.' or '\0' */
    for (tail = name; *tail && *tail != '.'; tail++)
	;

    nch = (int)(tail - name);

    /* Compare name with all the child nodes */
    for (np = root->first; np != NULL; np = np->next) {
	if (strncmp(name, np->name, (int)nch) == 0 && np->name[(int)nch] == '\0')
	    break;
    }

    if (np == NULL) { /* no match with child */
	__pmnsNode *parent_np = root;
	const char *name_p = name;
	int is_first = 1;
 
	/* create nodes until reach leaf */

	for ( ; ; ) { 
	    if ((np = (__pmnsNode *)malloc(sizeof(*np))) == NULL)
		return -oserror();

	    /* fixup name */
	    if ((np->name = (char *)malloc(nch+1)) == NULL) {
		free(np);
		return -oserror();
	    }
	    strncpy(np->name, name_p, nch);
	    np->name[nch] = '\0';

	    /* fixup some links */
	    np->first = np->hash = np->next = NULL;
	    np->parent = parent_np;
	    if (is_first) {
		is_first = 0;
		if (root->first != NULL) {
		    /* chuck new node at front of list */
		    np->next = root->first;
		}
	    }
	    parent_np->first = np;

	    /* at this stage, assume np is a non-leaf */
	    np->pmid = PM_ID_NULL;

	    parent_np = np;
	    if (*tail == '\0')
		break;
	    name_p += nch+1; /* skip over node + dot */ 
	    for (tail = name_p; *tail && *tail != '.'; tail++)
		;
	    nch = (int)(tail - name_p);
	}

	np->pmid = pmid; /* set pmid of leaf node */
	return 0;
    }
    else if (*tail == '\0') { /* matched with whole path */
	if (np->pmid != pmid)
	    return PM_ERR_PMID;
	else 
	    return 0;
    }
    else {
	return AddPMNSNode(np, pmid, tail+1); /* try matching with rest of pathname */
    }

}


/*
 * Add a new node for fullpath, name, with pmid.
 * NOTE: Need to call __pmFixPMNSHashTab() to update hash table
 *       when have finished adding nodes.
 */
int
__pmAddPMNSNode(__pmnsTree *tree, int pmid, const char *name)
{
    if (tree->contiguous) {
	/* Cannot add node to contiguously allocated tree! */ 
	return -EINVAL;
    }

    return AddPMNSNode(tree->root, pmid, name);
}

/*
 * fsa for parser
 *
 *	old	token	new
 *	0	NAME	1
 *	0	PATH	1
 *	1	LBRACE	2
 *      2	NAME	3
 *	2	RBRACE	0
 *	3	NAME	3
 *	3	PMID	2
 *	3	RBRACE	0
 *
 * dupok
 *	NO_DUPS	duplicate names are not allowed
 *	DUPS_OK	duplicate names are allowed
 *
 * use_cpp
 *	NO_CPP	just fopen() the PMNS file
 *	USE_CPP	pre-process the PMNS file with pmcpp(1)
 *
 * PMNS file (fname) set up before we get here ...
 */
static int
loadascii(int dupok, int use_cpp)
{
    int		state = 0;
    int		type;
    __pmnsNode	*np = NULL;	/* pander to gcc */

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PMNS)
	fprintf(stderr, "loadascii(dupok=%d, use_cpp=%d) fname=%s\n", dupok, use_cpp, fname);
#endif


    /* reset the lexical scanner */
    lex(1+use_cpp);

    seen = NULL; /* make seen-list empty */
    seenpmid = 0;


    if (access(fname, R_OK) == -1) {
	snprintf(linebuf, sizeof(linebuf), "Cannot open \"%s\"", fname);
	err(linebuf);
	return -oserror();
    }
    lineno = 1;

    while ((type = lex(0)) > 0) {
	switch (state) {

	case 0:
	    if (type != NAME && type != PATH) {
		err("Expected NAME or PATH");
		return PM_ERR_PMNS;
	    }
	    state = 1;
	    break;

	case 1:
	    if (type != LBRACE) {
		err("{ expected");
		return PM_ERR_PMNS;
	    }
	    state = 2;
	    break;

	case 2:
	    if (type == NAME) {
		state = 3;
	    }
	    else if (type == RBRACE) {
		state = 0;
	    }
	    else {
		err("Expected NAME or }");
		return PM_ERR_PMNS;
	    }
	    break;

	case 3:
	    if (type == NAME) {
		state = 3;
	    }
	    else if (type == PMID) {
		np->pmid = tokpmid;
		state = 2;
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_PMNS) {
		    char	strbuf[20];
		    fprintf(stderr, "pmLoadNameSpace: %s -> %s\n",
			np->name, pmIDStr_r(np->pmid, strbuf, sizeof(strbuf)));
		}
#endif
	    }
	    else if (type == RBRACE) {
		state = 0;
	    }
	    else {
		err("Expected NAME, PMID or }");
		return PM_ERR_PMNS;
	    }
	    break;

	}

	if (state == 1 || state == 3) {
	    if ((np = (__pmnsNode *)malloc(sizeof(*np))) == NULL)
		return -oserror();
	    seenpmid++;
	    if ((np->name = (char *)malloc(strlen(tokbuf)+1)) == NULL) {
		free(np);
		return -oserror();
	    }
	    strcpy(np->name, tokbuf);
	    np->first = np->hash = np->next = np->parent = NULL;
	    np->pmid = PM_ID_NULL;
	    if (state == 1) {
		np->next = seen;
		seen = np;
	    }
	    else {
		if (seen->hash)
		    seen->hash->next = np;
		else
		    seen->first = np;
		seen->hash = np;
	    }
	}
	else if (state == 0) {
	    if (seen) {
		__pmnsNode	*xp;

		for (np = seen->first; np != NULL; np = np->next) {
		    for (xp = np->next; xp != NULL; xp = xp->next) {
			if (strcmp(xp->name, np->name) == 0) {
			    snprintf(linebuf, sizeof(linebuf), "Duplicate name \"%s\" in subtree for \"%s\"\n",
			        np->name, seen->name);
			    err(linebuf);
			    return PM_ERR_PMNS;
			}
		    }
		}
	    }
	}
    }

    if (type == 0)
	type = pass2(dupok);


    if (type == 0) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMNS)
	    fprintf(stderr, "Loaded ASCII PMNS\n");
#endif
    }

    return type;
}

static const char * 
getfname(const char *filename)
{
    /*
     * 0xffffffff is there to maintain backwards compatibility with PCP 1.0
     */
    if (filename == PM_NS_DEFAULT || (__psint_t)filename == 0xffffffff) {
	char	*def_pmns;

	def_pmns = getenv("PMNS_DEFAULT");
	if (def_pmns != NULL) {
	    /* get default PMNS name from environment */
	    return def_pmns;
	}
	else {
	    static char repname[MAXPATHLEN];
	    int sep = __pmPathSeparator();

	    if ((def_pmns = pmGetOptionalConfig("PCP_VAR_DIR")) == NULL)
		return NULL;
	    snprintf(repname, sizeof(repname), "%s%c" "pmns" "%c" "root",
		     def_pmns, sep, sep);
	    return repname;
	}
    }
    return filename;
}

int
__pmHasPMNSFileChanged(const char *filename)
{
    const char	*f;
    int		sts;

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);

    f = getfname(filename);
    if (f == NULL) {
	/* error encountered -> must have changed :) */
	sts = 1;
	goto done;
    }

    /* if still using same filename ... */
    if (strcmp(f, fname) == 0) {
	struct stat statbuf;

	if (stat(f, &statbuf) == 0) {
	    /* If the modification times have changed */
#if defined(HAVE_ST_MTIME_WITH_E) && defined(HAVE_STAT_TIME_T)
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_PMNS) {
		fprintf(stderr,
			"__pmHasPMNSFileChanged(%s) -> %s last=%d now=%d\n",
			filename == PM_NS_DEFAULT ||
			(__psint_t)filename == 0xffffffff ?
				"PM_NS_DEFAULT" : filename,
			f, (int)last_mtim, (int)statbuf.st_mtime);
	    }
#endif
	    sts = (statbuf.st_mtime == last_mtim) ? 0 : 1;
	    goto done;
#elif defined(HAVE_ST_MTIME_WITH_SPEC)
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_PMNS) {
		fprintf(stderr,
			"__pmHasPMNSFileChanged(%s) -> %s last=%d.%09ld now=%d.%09ld\n",
			filename == PM_NS_DEFAULT ||
			(__psint_t)filename == 0xffffffff ?
				"PM_NS_DEFAULT" : filename,
			f, (int)last_mtim.tv_sec, last_mtim.tv_nsec,
			(int)statbuf.st_mtimespec.tv_sec,
			statbuf.st_mtimespec.tv_nsec);
	    }
#endif
	    sts = (statbuf.st_mtimespec.tv_sec == last_mtim.tv_sec &&
		statbuf.st_mtimespec.tv_nsec == last_mtim.tv_nsec) ? 0 : 1;
	    goto done;
#elif defined(HAVE_STAT_TIMESTRUC) || defined(HAVE_STAT_TIMESPEC) || defined(HAVE_STAT_TIMESPEC_T)
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_PMNS) {
		fprintf(stderr,
			"__pmHasPMNSFileChanged(%s) -> %s last=%d.%09ld now=%d.%09ld\n",
			filename == PM_NS_DEFAULT ||
			(__psint_t)filename == 0xffffffff ?
				"PM_NS_DEFAULT" : filename,
			f, (int)last_mtim.tv_sec, last_mtim.tv_nsec,
			(int)statbuf.st_mtim.tv_sec, statbuf.st_mtim.tv_nsec);
	    }
#endif
	    sts = (statbuf.st_mtim.tv_sec == last_mtim.tv_sec &&
		    (statbuf.st_mtim.tv_nsec == last_mtim.tv_nsec)) ? 0 : 1;
	    goto done;
#else
!bozo!
#endif
	}
	else {
	    /* error encountered -> must have changed */
	    sts = 1;
	    goto done;
	}
    }
    /* different filenames at least */
    sts = 1;

done:
    PM_UNLOCK(__pmLock_libpcp);
    return sts;
}

static int
load(const char *filename, int dupok, int use_cpp)
{
    const char	*f;
    int 	i = 0;

    if (main_pmns != NULL) {
	if (export) {
	    export = 0;

	    /*
	     * drop the loaded PMNS ... huge memory leak, but it is
	     * assumed the caller has saved the previous PMNS after calling
	     * __pmExportPMNS() ... only user of this service is pmnsmerge
	     */
	    main_pmns = NULL;
	}
	else {
	    return PM_ERR_DUPPMNS;
	}
    }

    if ((f = getfname(filename)) == NULL)
	return PM_ERR_GENERIC;
    strncpy(fname, f, sizeof(fname));
    fname[sizeof(fname)-1] = '\0';
 
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PMNS)
	fprintf(stderr, "load(name=%s, dupok=%d, use_cpp=%d) lic case=%d fname=%s\n",
		filename, dupok, use_cpp, i, fname);
#endif

    /* Note modification time of pmns file */
    {
	struct stat statbuf;

	if (stat(fname, &statbuf) == 0) {
#if defined(HAVE_ST_MTIME_WITH_E)
	    last_mtim = statbuf.st_mtime; /* possible struct assignment */
#elif defined(HAVE_ST_MTIME_WITH_SPEC)
	    last_mtim = statbuf.st_mtimespec; /* possible struct assignment */
#else
	    last_mtim = statbuf.st_mtim; /* possible struct assignment */
#endif
	}
    }

    /*
     * use_cpp passed in is a hint ... if it is USE_CPP and filename
     * is PM_NS_DEFAULT (NULL) then we can safely change to NO_CPP
     * because the PMNS file contains no cpp-style controls or macros
     */
    if (use_cpp == USE_CPP && filename == PM_NS_DEFAULT)
	use_cpp = NO_CPP;

    /*
     * load ASCII PMNS
     */
    return loadascii(dupok, use_cpp);
}

/*
 * just for pmnsmerge to use
 */
__pmnsTree*
__pmExportPMNS(void)
{
    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    export = 1;
    PM_UNLOCK(__pmLock_libpcp);

    /*
     * Warning: this is _not_ thread-safe, and cannot be guarded/protected
     */
    return main_pmns;
}

/*
 * Find and return the named node in the tree, root.
 */
static __pmnsNode *
locate(const char *name, __pmnsNode *root)
{
    const char	*tail;
    ptrdiff_t	nch;
    __pmnsNode	*np;

    /* Traverse until '.' or '\0' */
    for (tail = name; *tail && *tail != '.'; tail++)
	;

    nch = tail - name;

    /* Compare name with all the child nodes */
    for (np = root->first; np != NULL; np = np->next) {
	if (strncmp(name, np->name, (int)nch) == 0 && np->name[(int)nch] == '\0' &&
	    (np->pmid & MARK_BIT) == 0)
	    break;
    }

    if (np == NULL) /* no match with child */
	return NULL;
    else if (*tail == '\0') /* matched with whole path */
	return np;
    else
	return locate(tail+1, np); /* try matching with rest of pathname */
}

/*
 * PMAPI routines from here down
 */

/*
 * As of PCP 3.6, there is _only_ the ASCII version of the PMNS.
 * As of PCP 3.10.3, the default is to allow duplicates in the PMNS.
 */
int
pmLoadNameSpace(const char *filename)
{
    int	sts;

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    havePmLoadCall = 1;
    sts = load(filename, DUPS_OK, NO_CPP);
    PM_UNLOCK(__pmLock_libpcp);
    return sts;
}

int
pmLoadASCIINameSpace(const char *filename, int dupok)
{
    int	sts;

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    havePmLoadCall = 1;
    sts = load(filename, dupok, USE_CPP);
    PM_UNLOCK(__pmLock_libpcp);
    return sts;
}

/*
 * Assume that each node has been malloc'ed separately.
 * This is the case for an ASCII loaded PMNS.
 * Traverse entire tree and free each node.
 */
static void
FreeTraversePMNS(__pmnsNode *this)
{
    __pmnsNode *np, *next;

    if (this == NULL)
	return;

    /* Free child sub-trees */
    for (np = this->first; np != NULL; np = next) {
	next = np->next;
	FreeTraversePMNS(np);
    }

    free(this->name);
    free(this);
}

void
__pmFreePMNS(__pmnsTree *pmns)
{
    if (pmns != NULL) {
	if (pmns->contiguous) {
	    free(pmns->root);
	    free(pmns->htab);
	    free(pmns->symbol);
	}
	else { 
	    free(pmns->htab);
	    FreeTraversePMNS(pmns->root); 
	}

	free(pmns);
    }
}

void
pmUnloadNameSpace(void)
{
    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    havePmLoadCall = 0;
    __pmFreePMNS(main_pmns);
    main_pmns = NULL;
    PM_UNLOCK(__pmLock_libpcp);
}

int
pmLookupName(int numpmid, char *namelist[], pmID pmidlist[])
{
    int pmns_location = GetLocation();
    int	sts = 0;
    __pmContext	*ctxp;
    int		c_type;
    int		lsts;
    int		ctx;
    int		i;
    int		nfail = 0;

    if (numpmid < 1) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMNS) {
	    fprintf(stderr, "pmLookupName(%d, ...) bad numpmid!\n", numpmid);
	}
#endif
	return PM_ERR_TOOSMALL;
    }

    PM_INIT_LOCKS();

    ctx = lsts = pmWhichContext();
    if (lsts >= 0) {
	ctxp = __pmHandleToPtr(ctx);
	c_type = ctxp->c_type;
    }
    else {
	ctxp = NULL;
	/*
	 * set c_type to be NONE of PM_CONTEXT_HOST, PM_CONTEXT_ARCHIVE
	 * nor PM_CONTEXT_LOCAL
	 */
	c_type = 0;
    }
    if (ctxp != NULL && c_type == PM_CONTEXT_LOCAL && PM_MULTIPLE_THREADS(PM_SCOPE_DSO_PMDA)) {
	/* Local context requires single-threaded applications */
	PM_UNLOCK(ctxp->c_lock);
	return PM_ERR_THREAD;
    }

    /*
     * Guarantee that derived metrics preparation is done, for all possible
     * paths through this routine which might end at "Try derived metrics.."
     * below.
     */
    memset(pmidlist, PM_ID_NULL, numpmid * sizeof(pmID));

    if (pmns_location < 0) {
	if (ctxp != NULL)
	    PM_UNLOCK(ctxp->c_lock);
	sts = pmns_location;
	/* only hope is derived metrics ... set up for this */
	nfail += numpmid;
    }
    else if (pmns_location == PMNS_LOCAL) {
	char		*xname;
	char		*xp;
	__pmnsNode	*np;

	if (ctxp != NULL)
	    PM_UNLOCK(ctxp->c_lock);
	for (i = 0; i < numpmid; i++) {
	    /*
	     * if we locate the name and it is a leaf in the PMNS
	     * this is good
	     */
	    np = locate(namelist[i], PM_TPD(curr_pmns)->root);
	    if (np != NULL ) {
		if (np->first == NULL) {
		    /* looks good from local PMNS */
		    pmidlist[i] = np->pmid;
		}
		else {
		    /* non-leaf ... no error unless numpmid == 1 */
		    if (numpmid == 1)
			sts = PM_ERR_NONLEAF;
		    nfail++;
		}
		continue;
	    }
	    nfail++;
	    /*
	     * did not match name in PMNS ... try for prefix matching
	     * the name to the root of a dynamic subtree of the PMNS,
	     * or possibly we're using a local context and then we may
	     * be able to ship request to PMDA
	     */
	    xname = strdup(namelist[i]);
	    if (xname == NULL) {
		__pmNoMem("pmLookupName", strlen(namelist[i])+1, PM_RECOV_ERR);
		sts = -oserror();
		continue;
	    }
	    while ((xp = rindex(xname, '.')) != NULL) {
		*xp = '\0';
		lsts = 0;
		np = locate(xname, PM_TPD(curr_pmns)->root);
		if (np != NULL && np->first == NULL &&
		    pmid_domain(np->pmid) == DYNAMIC_PMID &&
		    pmid_item(np->pmid) == 0) {
		    /* root of dynamic subtree */
		    if (c_type == PM_CONTEXT_LOCAL) {
			/* have PM_CONTEXT_LOCAL ... try to ship request to PMDA */
			int	domain = ((__pmID_int *)&np->pmid)->cluster;
			__pmDSO	*dp;
			if ((dp = __pmLookupDSO(domain)) == NULL) {
			    /* no PMDA ... no error unless numpmid == 1 */
			    if (numpmid == 1)
				sts = PM_ERR_NOAGENT;
			    pmidlist[i] = PM_ID_NULL;
			    break;
			}
			if (dp->dispatch.comm.pmda_interface >= PMDA_INTERFACE_5)
			    dp->dispatch.version.four.ext->e_context = ctx;
			if (dp->dispatch.comm.pmda_interface >= PMDA_INTERFACE_4) {
			    lsts = dp->dispatch.version.four.pmid(namelist[i], &pmidlist[i], dp->dispatch.version.four.ext);
			    if (lsts >= 0)
				nfail--;
			    else {
				/* return error if numpmid == 1 */
				if (numpmid == 1)
				    sts = lsts;
				pmidlist[i] = PM_ID_NULL;
			    }
			    break;
			}
		    }
		    else {
			/*
			 * The requested name is _below_ a DYNAMIC node
			 * in the PMNS, so return the DYNAMIC node's PMID
			 * (as set above)
			 * ... this is a little odd, but is the trigger
			 * that pmcd requires to try and reship the request
			 * to the associated PMDA.
			 */
			pmidlist[i] = np->pmid;
			nfail--;
			break;
		    }
		}
	    }
	    free(xname);
	}

    	sts = (sts == 0 ? numpmid - nfail : sts);

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMNS) {
	    int		i;
	    char	strbuf[20];
	    fprintf(stderr, "pmLookupName(%d, ...) using local PMNS returns %d and ...\n",
		numpmid, sts);
	    for (i = 0; i < numpmid; i++) {
		fprintf(stderr, "  name[%d]: \"%s\"", i, namelist[i]);
		if (sts >= 0)
		    fprintf(stderr, " PMID: 0x%x %s",
			pmidlist[i], pmIDStr_r(pmidlist[i], strbuf, sizeof(strbuf)));
		fputc('\n', stderr);
	    }
	}
#endif
    }
    else {
	/*
	 * PMNS_REMOTE so there must be a current host context
	 */
	assert(c_type == PM_CONTEXT_HOST);
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMNS) {
	    fprintf(stderr, "pmLookupName: request_names ->");
	    for (i = 0; i < numpmid; i++)
		fprintf(stderr, " [%d] %s", i, namelist[i]);
	    fputc('\n', stderr);
	}
#endif
	PM_LOCK(ctxp->c_pmcd->pc_lock);
	sts = __pmSendNameList(ctxp->c_pmcd->pc_fd, __pmPtrToHandle(ctxp),
		numpmid, namelist, NULL);
	if (sts < 0)
	    sts = __pmMapErrno(sts);
	else {
	    __pmPDU	*pb;
	    int		pinpdu;

	    pinpdu = sts = __pmGetPDU(ctxp->c_pmcd->pc_fd, ANY_SIZE,
				    ctxp->c_pmcd->pc_tout_sec, &pb);
	    if (sts == PDU_PMNS_IDS) {
		/* Note:
		 * pmLookupName may return an error even though
		 * it has a valid list of ids.
		 * This is why we need op_status.
		 */
		int op_status; 
		sts = __pmDecodeIDList(pb, numpmid, pmidlist, &op_status);
		if (sts >= 0)
		    sts = op_status;
	    }
	    else if (sts == PDU_ERROR) {
		__pmDecodeError(pb, &sts);
	    }
	    else if (sts != PM_ERR_TIMEOUT) {
		sts = PM_ERR_IPC;
	    }
	    if (pinpdu > 0)
		__pmUnpinPDUBuf(pb);
	    if (sts >= 0)
		nfail = numpmid - sts;
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_PMNS) {
		char	strbuf[20];
		char	errmsg[PM_MAXERRMSGLEN];
		fprintf(stderr, "pmLookupName: receive_names <-");
		if (sts >= 0) {
		    for (i = 0; i < numpmid; i++)
			fprintf(stderr, " [%d] %s", i, pmIDStr_r(pmidlist[i], strbuf, sizeof(strbuf)));
		    fputc('\n', stderr);
		}
	    else
		fprintf(stderr, " %s\n", pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	    }
#endif
	}
	PM_UNLOCK(ctxp->c_pmcd->pc_lock);
	PM_UNLOCK(ctxp->c_lock);
    }

    if (sts < 0 || nfail > 0) {
	/*
	 * Try derived metrics for any remaining unknown pmids.
	 * The return status is a little tricky ... prefer the status
	 * from above unless all of the remaining unknown PMIDs are
	 * resolved by __dmgetpmid() in which case success (numpmid)
	 * is the right return status
	 */
	nfail = 0;
	for (i = 0; i < numpmid; i++) {
	    if (pmidlist[i] == PM_ID_NULL) {
		lsts = __dmgetpmid(namelist[i], &pmidlist[i]);
		if (lsts < 0) {
		    nfail++;
		}
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_DERIVE) {
		    char	strbuf[20];
		    char	errmsg[PM_MAXERRMSGLEN];
		    fprintf(stderr, "__dmgetpmid: metric \"%s\" -> ", namelist[i]);
		    if (lsts < 0)
			fprintf(stderr, "%s\n", pmErrStr_r(lsts, errmsg, sizeof(errmsg)));
		    else
			fprintf(stderr, "PMID %s\n", pmIDStr_r(pmidlist[i], strbuf, sizeof(strbuf)));
		}
#endif
	    }
	}
	if (nfail == 0)
	    sts = numpmid;
    }

    /*
     * special case for a single metric, PM_ERR_NAME is more helpful than
     * returning 0 and having one PM_ID_NULL pmid
     */
    if (sts == 0 && numpmid == 1)
	sts = PM_ERR_NAME;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PMNS) {
	fprintf(stderr, "pmLookupName(%d, ...) -> ", numpmid);
	if (sts < 0) {
	    char	errmsg[PM_MAXERRMSGLEN];
	    fprintf(stderr, "%s\n", pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	}
	else
	    fprintf(stderr, "%d\n", sts);
    }
#endif

    return sts;
}

static int
GetChildrenStatusRemote(__pmContext *ctxp, const char *name,
			char ***offspring, int **statuslist)
{
    int n;

    PM_LOCK(ctxp->c_pmcd->pc_lock);
    n = __pmSendChildReq(ctxp->c_pmcd->pc_fd, __pmPtrToHandle(ctxp),
		name, statuslist == NULL ? 0 : 1);
    if (n < 0)
	n =  __pmMapErrno(n);
    else {
	__pmPDU		*pb;
	int		pinpdu;

	pinpdu = n = __pmGetPDU(ctxp->c_pmcd->pc_fd, ANY_SIZE, 
				ctxp->c_pmcd->pc_tout_sec, &pb);
	if (n == PDU_PMNS_NAMES) {
	    int numnames;
	    n = __pmDecodeNameList(pb, &numnames, offspring, statuslist);
	    if (n >= 0)
		n = numnames;
	}
	else if (n == PDU_ERROR)
	    __pmDecodeError(pb, &n);
	else if (n != PM_ERR_TIMEOUT)
	    n = PM_ERR_IPC;
	if (pinpdu > 0)
	    __pmUnpinPDUBuf(pb);
    }
    PM_UNLOCK(ctxp->c_pmcd->pc_lock);

    return n;
}

static void
stitch_list(int *num, char ***offspring, int **statuslist, int x_num, char **x_offspring, int *x_statuslist)
{
    /*
     * so this gets tricky ... need to stitch the additional metrics
     * (derived metrics or dynamic metrics) at the end of the existing
     * metrics (if any) after removing any duplicates (!) ... and honour
     * the bizarre pmGetChildren contract in terms of malloc'ing the
     * result arrays
     */
    int		n_num;
    char	**n_offspring;
    int		*n_statuslist = NULL;
    int		i;
    int		j;
    char	*q;
    size_t	need;

    if (x_num == 0) {
	/* nothing to do */
	return;
    }
    if (*num > 0) {
	/* appending */
	n_num = *num + x_num;
    }
    else {
	/* initializing */
	n_num = x_num;
    }

    for (i = 0; i < x_num; i++) {
	for (j = 0; j < *num; j++) {
	    if (strcmp(x_offspring[i], (*offspring)[j]) == 0) {
		/* duplicate ... bugger */
		n_num--;
		free(x_offspring[i]);
		x_offspring[i] = NULL;
		break;
	    }
	}
    }

    need = n_num*sizeof(char *);
    for (j = 0; j < *num; j++) {
	need += strlen((*offspring)[j]) + 1;
    }
    for (i = 0; i < x_num; i++) {
	if (x_offspring[i] != NULL) {
	    need += strlen(x_offspring[i]) + 1;
	}
    }
    if ((n_offspring = (char **)malloc(need)) == NULL) {
	__pmNoMem("pmGetChildrenStatus: n_offspring", need, PM_FATAL_ERR);
	/*NOTREACHED*/
    }
    if (statuslist != NULL) {
	if ((n_statuslist = (int *)malloc(n_num*sizeof(n_statuslist[0]))) == NULL) {
	    __pmNoMem("pmGetChildrenStatus: n_statuslist", n_num*sizeof(n_statuslist[0]), PM_FATAL_ERR);
	    /*NOTREACHED*/
	}
    }
    q = (char *)&n_offspring[n_num];
    for (j = 0; j < *num; j++) {
	n_offspring[j] = q;
	strcpy(q, (*offspring)[j]);
	q += strlen(n_offspring[j]) + 1;
	if (statuslist != NULL)
	    n_statuslist[j] = (*statuslist)[j];
    }
    for (i = 0; i < x_num; i++) {
	if (x_offspring[i] != NULL) {
	    n_offspring[j] = q;
	    strcpy(q, x_offspring[i]);
	    q += strlen(n_offspring[j]) + 1;
	    if (statuslist != NULL)
		n_statuslist[j] = x_statuslist[i];
	    j++;
	}
    }
    if (*num > 0) {
	free(*offspring);
	if (statuslist != NULL)
	    free(*statuslist);
    }
    *num = n_num;
    if (statuslist != NULL)
	*statuslist = n_statuslist;
    *offspring = n_offspring;
}

/*
 * It is allowable to pass in a statuslist arg of NULL. It is therefore
 * important to check that this is not NULL before accessing it.
 */
int
pmGetChildrenStatus(const char *name, char ***offspring, int **statuslist)
{
    int		*status = NULL;
    int		pmns_location = GetLocation();
    int		num;
    int		dm_num;
    char	**dm_offspring;
    int		*dm_statuslist;
    int		sts;
    int		ctx;
    __pmContext	*ctxp;

    if (pmns_location < 0)
	return pmns_location;

    if (name == NULL) 
	return PM_ERR_NAME;

    PM_INIT_LOCKS();

    ctx = sts = pmWhichContext();
    if (sts >= 0)
	ctxp = __pmHandleToPtr(sts);
    else
	ctxp = NULL;

    if (pmns_location == PMNS_LOCAL) {
	__pmnsNode	*np;
	__pmnsNode	*tnp;
	int		i;
	int		need;
	char		**result;
	char		*p;

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMNS) {
	    fprintf(stderr, "pmGetChildren(name=\"%s\") [local]\n", name);
	}
#endif

	/* avoids ambiguity, for errors and leaf nodes */
	*offspring = NULL;
	num = 0;
	if (statuslist)
	  *statuslist = NULL;

	PM_INIT_LOCKS();
	if (*name == '\0')
	    np = PM_TPD(curr_pmns)->root; /* use "" to name the root of the PMNS */
	else
	    np = locate(name, PM_TPD(curr_pmns)->root);
	if (np == NULL) {
	    if (ctxp != NULL && ctxp->c_type == PM_CONTEXT_LOCAL) {
		/*
		 * No match in PMNS and using PM_CONTEXT_LOCAL so for
		 * dynamic metrics, need to consider prefix matches back to
		 * the root on the PMNS to find a possible root of a dynamic
		 * subtree, and hence the domain of the responsible PMDA
		 */
		char	*xname = strdup(name);
		char	*xp;
		if (xname == NULL) {
		    __pmNoMem("pmGetChildrenStatus", strlen(name)+1, PM_RECOV_ERR);
		    num = -oserror();
		    goto report;
		}
		while ((xp = rindex(xname, '.')) != NULL) {
		    *xp = '\0';
		    np = locate(xname, PM_TPD(curr_pmns)->root);
		    if (np != NULL && np->first == NULL &&
			pmid_domain(np->pmid) == DYNAMIC_PMID &&
			pmid_item(np->pmid) == 0) {
			int		domain = ((__pmID_int *)&np->pmid)->cluster;
			__pmDSO		*dp;
			if ((dp = __pmLookupDSO(domain)) == NULL) {
			    num = PM_ERR_NOAGENT;
			    free(xname);
			    goto check;
			}
			if (dp->dispatch.comm.pmda_interface >= PMDA_INTERFACE_5)
			    dp->dispatch.version.four.ext->e_context = ctx;
			if (dp->dispatch.comm.pmda_interface >= PMDA_INTERFACE_4) {
			    char	**x_offspring = NULL;
			    int		*x_statuslist = NULL;
			    int		x_num;
			    x_num = dp->dispatch.version.four.children(
					name, 0, &x_offspring, &x_statuslist,
					dp->dispatch.version.four.ext);
			    if (x_num < 0)
				num = x_num;
			    else if (x_num > 0) {
				stitch_list(&num, offspring, statuslist,
					    x_num, x_offspring, x_statuslist);
				free(x_offspring);
				free(x_statuslist);
			    }
			    free(xname);
			    goto check;
			}
			else {
			    /* Not PMDA_INTERFACE_4 or later */
			    num = PM_ERR_NAME;
			    free(xname);
			    goto check;
			}
		    }
		}
		free(xname);
	    }
	   num = PM_ERR_NAME;
	   goto check;
	}

	if (np != NULL && np->first == NULL) {
	    /*
	     * this is a leaf node ... if it is the root of a dynamic
	     * subtree of the PMNS and we have an existing context
	     * of type PM_CONTEXT_LOCAL than we should chase the
	     * relevant PMDA to provide the details
	     */
	    if (pmid_domain(np->pmid) == DYNAMIC_PMID &&
		pmid_item(np->pmid) == 0) {
		if (ctxp != NULL && ctxp->c_type == PM_CONTEXT_LOCAL) {
		    int		domain = ((__pmID_int *)&np->pmid)->cluster;
		    __pmDSO	*dp;
		    if ((dp = __pmLookupDSO(domain)) == NULL) {
			num = PM_ERR_NOAGENT;
			goto check;
		    }
		    if (dp->dispatch.comm.pmda_interface >= PMDA_INTERFACE_5)
			dp->dispatch.version.four.ext->e_context = ctx;
		    if (dp->dispatch.comm.pmda_interface >= PMDA_INTERFACE_4) {
			char	**x_offspring = NULL;
			int	*x_statuslist = NULL;
			int	x_num;
			x_num = dp->dispatch.version.four.children(name, 0,
					&x_offspring, &x_statuslist,
					dp->dispatch.version.four.ext);
			if (x_num < 0)
			    num = x_num;
			else if (x_num > 0) {
			    stitch_list(&num, offspring, statuslist,
					x_num, x_offspring, x_statuslist);
			    free(x_offspring);
			    free(x_statuslist);
			}
			goto check;
		    }
		    else {
			/* Not PMDA_INTERFACE_4 or later */
			num = PM_ERR_NAME;
			goto check;
		    }
		}
	    }
	    num = 0;
	    goto check;
	}

	need = 0;
	num = 0;

	if (np != NULL) {
	    for (i = 0, tnp = np->first; tnp != NULL; tnp = tnp->next, i++) {
	        if ((tnp->pmid & MARK_BIT) == 0) {
		    num++;
		    need += sizeof(**offspring) + strlen(tnp->name) + 1;
	        }
	    }
	}

	if ((result = (char **)malloc(need)) == NULL) {
	    num = -oserror();
	    goto report;
	}

	if (statuslist != NULL) {
	    if ((status = (int *)malloc(num*sizeof(int))) == NULL) {
		num = -oserror();
		free(result);
		goto report;
	    }
	}

	p = (char *)&result[num];

	if (np != NULL) {
	    for (i = 0, tnp = np->first; tnp != NULL; tnp = tnp->next) {
	        if ((tnp->pmid & MARK_BIT) == 0) {
		    result[i] = p;
		    /*
		     * a name at the root of a dynamic metrics subtree
		     * needs some special handling ... they will have a
		     * "special" PMID, but need the status set to indicate
		     * they are not a leaf node of the PMNS
		     */
		    if (statuslist != NULL) {
			if (pmid_domain(tnp->pmid) == DYNAMIC_PMID &&
			    pmid_item(tnp->pmid) == 0) {
			    status[i] = PMNS_NONLEAF_STATUS;
			}
			else
			  /* node has children? */
			  status[i] = (tnp->first == NULL ? PMNS_LEAF_STATUS : PMNS_NONLEAF_STATUS);
		    }
		    strcpy(result[i], tnp->name);
		    p += strlen(tnp->name) + 1;
		    i++;
	        }
	    }
	}

	*offspring = result;
	if (statuslist != NULL)
	  *statuslist = status;
    }
    else {
	/*
	 * PMNS_REMOTE so there must be a current host context
	 */
	assert(ctxp != NULL && ctxp->c_type == PM_CONTEXT_HOST);
	num = GetChildrenStatusRemote(ctxp, name, offspring, statuslist);
    }

check:
    if (ctxp != NULL)
        PM_UNLOCK(ctxp->c_lock);
    

    /*
     * see if there are derived metrics that qualify
     */
    dm_num = __dmchildren(name, &dm_offspring, &dm_statuslist);
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_DERIVE) {
	char	errmsg[PM_MAXERRMSGLEN];
	if (num < 0)
	    fprintf(stderr, "pmGetChildren(name=\"%s\") no regular children (%s)", name, pmErrStr_r(num, errmsg, sizeof(errmsg)));
	else
	    fprintf(stderr, "pmGetChildren(name=\"%s\") %d regular children", name, num);
	if (dm_num < 0)
	    fprintf(stderr, ", no derived children (%s)\n", pmErrStr_r(dm_num, errmsg, sizeof(errmsg)));
	else if (dm_num == 0)
	    fprintf(stderr, ", derived leaf\n");
	else
	    fprintf(stderr, ", %d derived children\n", dm_num);
    }
#endif
    if (dm_num > 0) {
	stitch_list(&num, offspring, statuslist, dm_num, dm_offspring, dm_statuslist);
	free(dm_offspring);
	free(dm_statuslist);
    }
    else if (dm_num == 0 && num < 0) {
	/* leaf node and derived metric */
	num = 0;
    }

report:
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PMNS) {
	fprintf(stderr, "pmGetChildren(name=\"%s\") -> ", name);
	if (num == 0)
	    fprintf(stderr, "leaf\n");
	else if (num > 0) {
	    if (statuslist != NULL)
		__pmDumpNameAndStatusList(stderr, num, *offspring, *statuslist);
	    else
		__pmDumpNameList(stderr, num, *offspring);
	}
	else {
	    char	errmsg[PM_MAXERRMSGLEN];
	    fprintf(stderr, "%s\n", pmErrStr_r(num, errmsg, sizeof(errmsg)));
	}
    }
#endif

    return num;
}

int
pmGetChildren(const char *name, char ***offspring)
{
    return pmGetChildrenStatus(name, offspring, NULL);
}

static int
request_namebypmid(__pmContext *ctxp, pmID pmid)
{
    int n;

    n = __pmSendIDList(ctxp->c_pmcd->pc_fd, __pmPtrToHandle(ctxp), 1, &pmid, 0);
    if (n < 0)
	n = __pmMapErrno(n);
    return n;
}

static int
receive_namesbyid(__pmContext *ctxp, char ***namelist)
{
    int         n;
    __pmPDU	*pb;
    int		pinpdu;

    pinpdu = n = __pmGetPDU(ctxp->c_pmcd->pc_fd, ANY_SIZE, 
			    ctxp->c_pmcd->pc_tout_sec, &pb);
    
    if (n == PDU_PMNS_NAMES) {
	int numnames;

	n = __pmDecodeNameList(pb, &numnames, namelist, NULL);
	if (n >= 0)
	    n = numnames;
    }
    else if (n == PDU_ERROR)
	__pmDecodeError(pb, &n);
    else if (n != PM_ERR_TIMEOUT)
	n = PM_ERR_IPC;

    if (pinpdu > 0)
	__pmUnpinPDUBuf(pb);

    return n;
}

static int 
receive_a_name(__pmContext *ctxp, char **name)
{
    int n;
    char **namelist;

    if ((n = receive_namesbyid(ctxp, &namelist)) >= 0) {
	char *newname = strdup(namelist[0]);
	free(namelist);
	if (newname == NULL) {
	    n = -oserror();
	} else {
	    *name = newname;
	    n = 0;
	}
    }

    return n;
}

int
pmNameID(pmID pmid, char **name)
{
    int 	pmns_location = GetLocation();
    int		ctx;
    __pmContext	*ctxp;
    int		c_type;
    int		sts;
    int		lsts;

    PM_INIT_LOCKS();

    sts = ctx = pmWhichContext();
    if (ctx >= 0) {
	ctxp = __pmHandleToPtr(ctx);
	c_type = ctxp->c_type;
    }
    else {
	ctxp = NULL;
	/*
	 * set c_type to be NONE of PM_CONTEXT_HOST, PM_CONTEXT_ARCHIVE
	 * nor PM_CONTEXT_LOCAL
	 */
	c_type = 0;
    }
    if (ctxp != NULL && c_type == PM_CONTEXT_LOCAL && PM_MULTIPLE_THREADS(PM_SCOPE_DSO_PMDA)) {
	/* Local context requires single-threaded applications */
	PM_UNLOCK(ctxp->c_lock);
	return PM_ERR_THREAD;
    }

    if (pmns_location < 0) {
	if (ctxp != NULL)
	    PM_UNLOCK(ctxp->c_lock);
	sts = pmns_location;
	/* only hope is derived metrics ... */
    }
    else if (pmns_location == PMNS_LOCAL) {
    	__pmnsNode	*np;

	if (ctxp != NULL)
	    PM_UNLOCK(ctxp->c_lock);
	if (pmid_domain(pmid) == DYNAMIC_PMID && pmid_item(pmid) == 0) {
	    /* cannot return name for dynamic PMID from local PMNS */
	    return PM_ERR_PMID;
	}
	for (np = PM_TPD(curr_pmns)->htab[pmid % PM_TPD(curr_pmns)->htabsize];
             np != NULL;
             np = np->hash) {
	    if (np->pmid == pmid)
		return backname(np, name);
	}
	/* not found in PMNS ... try some other options */
	sts = PM_ERR_PMID;

	if (c_type == PM_CONTEXT_LOCAL) {
	    /* have PM_CONTEXT_LOCAL ... try to ship request to PMDA */
	    int		domain = pmid_domain(pmid);
	    __pmDSO	*dp;

	    if ((dp = __pmLookupDSO(domain)) == NULL)
		sts = PM_ERR_NOAGENT;
	    else {
		if (dp->dispatch.comm.pmda_interface >= PMDA_INTERFACE_5)
		    dp->dispatch.version.four.ext->e_context = ctx;
		if (dp->dispatch.comm.pmda_interface >= PMDA_INTERFACE_4) {
		    char	**names;
		    sts = dp->dispatch.version.four.name(pmid, &names,
				    dp->dispatch.version.four.ext);
		    if (sts > 0) {
			/* for pmNameID, pick just the first one */
			*name = strdup(names[0]);
			if (*name == NULL)
			    sts = -oserror();
			else
			    sts = 0;
			free(names);
		    }
		}
		else
		    /* Not PMDA_INTERFACE_4 or later */
		    sts = PM_ERR_PMID;
	    }
	}
    }
    else {
	/* assume PMNS_REMOTE */
	assert(c_type == PM_CONTEXT_HOST);
	PM_LOCK(ctxp->c_pmcd->pc_lock);
	if ((sts = request_namebypmid(ctxp, pmid)) >= 0) {
	    sts = receive_a_name(ctxp, name);
	}
	PM_UNLOCK(ctxp->c_pmcd->pc_lock);
	PM_UNLOCK(ctxp->c_lock);
    }

    if (sts >= 0) return sts;

    /*
     * failed everything else, try derived metric, but if this fails
     * return last error from above ...
     */
    lsts = __dmgetname(pmid, name);
    return lsts >= 0 ? lsts : sts;
}

int
pmNameAll(pmID pmid, char ***namelist)
{
    int		pmns_location = GetLocation();
    char	**tmp = NULL;
    int		len = 0;
    int		n = 0;
    char	*sp;
    int		ctx;
    __pmContext	*ctxp;
    int		c_type;
    int		sts;

    PM_INIT_LOCKS();

    sts = ctx = pmWhichContext();
    if (ctx >= 0) {
	ctxp = __pmHandleToPtr(ctx);
	c_type = ctxp->c_type;
    }
    else {
	ctxp = NULL;
	/*
	 * set c_type to be NONE of PM_CONTEXT_HOST, PM_CONTEXT_ARCHIVE
	 * nor PM_CONTEXT_LOCAL
	 */
	c_type = 0;
    }
    if (ctxp != NULL && c_type == PM_CONTEXT_LOCAL && PM_MULTIPLE_THREADS(PM_SCOPE_DSO_PMDA)) {
	/* Local context requires single-threaded applications */
	PM_UNLOCK(ctxp->c_lock);
	return PM_ERR_THREAD;
    }

    if (pmns_location < 0) {
	if (ctxp != NULL)
	    PM_UNLOCK(ctxp->c_lock);
	sts = pmns_location;
	/* only hope is derived metrics ... */
    }
    else if (pmns_location == PMNS_LOCAL) {
    	__pmnsNode	*np;
	int		i;

	if (ctxp != NULL)
	    PM_UNLOCK(ctxp->c_lock);
	if (pmid_domain(pmid) == DYNAMIC_PMID && pmid_item(pmid) == 0) {
	    /* cannot return name(s) for dynamic PMID from local PMNS */
	    return PM_ERR_PMID;
	}
	for (np = PM_TPD(curr_pmns)->htab[pmid % PM_TPD(curr_pmns)->htabsize];
             np != NULL;
             np = np->hash) {
	    if (np->pmid == pmid) {
		n++;
		if ((tmp = (char **)realloc(tmp, n * sizeof(tmp[0]))) == NULL) {
		    sts = -oserror();
		    break;
		}
		if ((sts = backname(np, &tmp[n-1])) < 0) {
		    /* error, ... free any partial allocations */
		    for (i = n-2; i >= 0; i--)
			free(tmp[i]);
		    free(tmp);
		    break;
		}
		len += strlen(tmp[n-1])+1;
	    }
	}
	if (sts < 0)
	    return sts;

	if (n > 0) {
	    /* all good ... rearrange to a contiguous allocation and return */
	    len += n * sizeof(tmp[0]);
	    if ((tmp = (char **)realloc(tmp, len)) == NULL)
		return -oserror();

	    sp = (char *)&tmp[n];
	    for (i = 0; i < n; i++) {
		strcpy(sp, tmp[i]);
		free(tmp[i]);
		tmp[i] = sp;
		sp += strlen(sp)+1;
	    }

	    *namelist = tmp;
	    return n;
	}
	/* not found in PMNS ... try some other options */
	sts = PM_ERR_PMID;

	if (c_type == PM_CONTEXT_LOCAL) {
	    /* have PM_CONTEXT_LOCAL ... try to ship request to PMDA */
	    int		domain = pmid_domain(pmid);
	    __pmDSO	*dp;

	    if ((dp = __pmLookupDSO(domain)) == NULL)
		sts = PM_ERR_NOAGENT;
	    else {
		if (dp->dispatch.comm.pmda_interface >= PMDA_INTERFACE_5)
		    dp->dispatch.version.four.ext->e_context = ctx;
		if (dp->dispatch.comm.pmda_interface >= PMDA_INTERFACE_4) {
		    n = dp->dispatch.version.four.name(pmid, &tmp,
				    dp->dispatch.version.four.ext);
		    if (n > 0) {
			*namelist = tmp;
			return n;
		    }
		}
		else
		    /* Not PMDA_INTERFACE_4 or later */
		    sts = PM_ERR_PMID;
	    }
	}
    }
    else {
	/* assume PMNS_REMOTE */
	assert(c_type == PM_CONTEXT_HOST);
	PM_LOCK(ctxp->c_pmcd->pc_lock);
	if ((sts = request_namebypmid (ctxp, pmid)) >= 0) {
	    sts = receive_namesbyid (ctxp, namelist);
	}
	PM_UNLOCK(ctxp->c_pmcd->pc_lock);
	PM_UNLOCK(ctxp->c_lock);
	if (sts > 0)
	    return sts;
    }

    /*
     * failed everything else, try derived metric, but if this fails
     * return last error from above ...
     */
    if ((tmp = (char **)malloc(sizeof(tmp[0]))) == NULL)
	return -oserror();
    n = __dmgetname(pmid, tmp);
    if (n < 0) {
	free(tmp);
	return sts < 0 ? sts : PM_ERR_PMID;
    }
    len = sizeof(tmp[0]) + strlen(tmp[0])+1;
    if ((tmp = (char **)realloc(tmp, len)) == NULL)
	return -oserror();
    sp = (char *)&tmp[1];
    strcpy(sp, tmp[0]);
    free(tmp[0]);
    tmp[0] = sp;
    *namelist = tmp;
    return 1;
}


/*
 * generic depth-first recursive descent of the PMNS
 */
static int
TraversePMNS_local(const char *name, void(*func)(const char *), void(*func_r)(const char *, void *), void *closure)
{
    int		sts = 0;
    int		nchildren;
    char	**enfants;

    if ((nchildren = pmGetChildren(name, &enfants)) < 0)
	return nchildren;

    if (nchildren > 0) {
	int	j;
	char	*newname;

	for (j = 0; j < nchildren; j++) {
	    size_t size = strlen(name) + 1 + strlen(enfants[j]) + 1;
	    if ((newname = (char *)malloc(size)) == NULL)
		__pmNoMem("pmTraversePMNS", size, PM_FATAL_ERR);
	    if (*name == '\0')
		strcpy(newname, enfants[j]);
	    else {
		strcpy(newname, name);
		strcat(newname, ".");
		strcat(newname, enfants[j]);
	    }
	    sts = TraversePMNS_local(newname, func, func_r, closure);
	    free(newname);
	    if (sts < 0)
		break;
	}
	free(enfants);
    }
    else {
	/* leaf node, name is full name of a metric */
	if (func_r == NULL)
	    (*func)(name);
	else
	    (*func_r)(name, closure);
    }

    return sts;
}

static int
TraversePMNS(const char *name, void(*func)(const char *), void(*func_r)(const char *, void *), void *closure)
{
    int	sts;
    int	pmns_location = GetLocation();

    if (pmns_location < 0)
	return pmns_location;

    if (name == NULL) 
	return PM_ERR_NAME;

    PM_INIT_LOCKS();

    if (pmns_location == PMNS_LOCAL) {
	PM_LOCK(__pmLock_libpcp);
	sts = TraversePMNS_local(name, func, func_r, closure);
	PM_UNLOCK(__pmLock_libpcp);
    }
    else {
	__pmPDU      *pb;
	__pmContext  *ctxp;

	/* As we have PMNS_REMOTE there must be a current host context */
	if ((sts = pmWhichContext()) < 0 || (ctxp = __pmHandleToPtr(sts)) == NULL)
	    return PM_ERR_NOCONTEXT;
	PM_LOCK(ctxp->c_pmcd->pc_lock);
	sts = __pmSendTraversePMNSReq(ctxp->c_pmcd->pc_fd, __pmPtrToHandle(ctxp), name);
	if (sts < 0) {
	    sts = __pmMapErrno(sts);
	    PM_UNLOCK(ctxp->c_pmcd->pc_lock);
	    PM_UNLOCK(ctxp->c_lock);
	}
	else {
	    int		numnames;
	    int		i;
	    int		xtra;
	    char	**namelist;
	    int		pinpdu;

	    pinpdu = sts = __pmGetPDU(ctxp->c_pmcd->pc_fd, ANY_SIZE, 
				      TIMEOUT_DEFAULT, &pb);
	    PM_UNLOCK(ctxp->c_pmcd->pc_lock);
	    PM_UNLOCK(ctxp->c_lock);
	    if (sts == PDU_PMNS_NAMES) {
		sts = __pmDecodeNameList(pb, &numnames, 
		                      &namelist, NULL);
		if (sts > 0) {
		    for (i=0; i<numnames; i++) {
			/*
			 * Do not process anonymous metrics here, we'll
			 * pick them up with the derived metrics later on
			 */
			if (strncmp(namelist[i], "anon.", 5) != 0) {
			    if (func_r == NULL)
				(*func)(namelist[i]);
			    else
				(*func_r)(namelist[i], closure);
			}
		    }
		    numnames = sts;
		    free(namelist);
		}
		else {
		    __pmUnpinPDUBuf(pb);
		    return sts;
		}
	    }
	    else if (sts == PDU_ERROR) {
		__pmDecodeError(pb, &sts);
		if (sts != PM_ERR_NAME) {
		    __pmUnpinPDUBuf(pb);
		    return sts;
		}
		numnames = 0;
	    }
	    else {
		if (pinpdu > 0)
		    __pmUnpinPDUBuf(pb);
		return (sts == PM_ERR_TIMEOUT) ? sts : PM_ERR_IPC;
	    }

	    if (pinpdu > 0)
		__pmUnpinPDUBuf(pb);

	    /*
	     * add any derived metrics that have "name" as
	     * their prefix
	     */
	    xtra = __dmtraverse(name, &namelist);
	    if (xtra > 0) {
		sts = 0;
		for (i=0; i<xtra; i++) {
		    if (func_r == NULL)
			(*func)(namelist[i]);
		    else
			(*func_r)(namelist[i], closure);
		}
		numnames += xtra;
		free(namelist);
	    }

	    if (sts > 0)
		return numnames;
	}
    }
    return sts;
}

int
pmTraversePMNS(const char *name, void(*func)(const char *))
{
    return TraversePMNS(name, func, NULL, NULL);
}

int
pmTraversePMNS_r(const char *name, void(*func)(const char *, void *), void *closure)
{
    return TraversePMNS(name, NULL, func, closure);
}

int
pmTrimNameSpace(void)
{
    int		i;
    __pmContext	*ctxp;
    __pmHashCtl	*hcp;
    __pmHashNode *hp;
    int		pmns_location = GetLocation();

    if (pmns_location < 0)
	return pmns_location;
    else if (pmns_location == PMNS_REMOTE)
	return 0;

    /* for PMNS_LOCAL ... */
    PM_INIT_LOCKS();

    if ((ctxp = __pmHandleToPtr(pmWhichContext())) == NULL)
	return PM_ERR_NOCONTEXT;

    if (ctxp->c_type != PM_CONTEXT_ARCHIVE) {
	/* unset all of the marks */
	mark_all(PM_TPD(curr_pmns), 0);
	PM_UNLOCK(ctxp->c_lock);
	return 0;
    }

    /* Don't do any trimming for archives.
     * Exception: if an explicit load PMNS call was made.
     */
    if (havePmLoadCall) {
	/*
	 * (1) set all of the marks, and
	 * (2) clear the marks for those metrics defined in the archive
	 */
	mark_all(PM_TPD(curr_pmns), 1);
	hcp = &ctxp->c_archctl->ac_log->l_hashpmid;

	for (i = 0; i < hcp->hsize; i++) {
	    for (hp = hcp->hash[i]; hp != NULL; hp = hp->next) {
		mark_one(PM_TPD(curr_pmns), (pmID)hp->key, 0);
	    }
	}
    }
    PM_UNLOCK(ctxp->c_lock);

    return 0;
}

void
__pmDumpNameSpace(FILE *f, int verbosity)
{
    int pmns_location = GetLocation();

    if (pmns_location < 0)
	fprintf(f, "__pmDumpNameSpace: Unable to determine PMNS location\n");
    else if (pmns_location == PMNS_REMOTE)
	fprintf(f, "__pmDumpNameSpace: Name Space is remote !\n");

    PM_INIT_LOCKS();
    dumptree(f, 0, PM_TPD(curr_pmns)->root, verbosity);
}

void
__pmDumpNameNode(FILE *f, __pmnsNode *node, int verbosity)
{
    dumptree(f, 0, node, verbosity);
}
