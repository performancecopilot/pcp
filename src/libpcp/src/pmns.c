/*
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
 */

#include <sys/stat.h>
#include <stddef.h>
#include <assert.h>
#include <ctype.h>
#include "pmapi.h"
#include "impl.h"

/*
 *  %s fields in CPP_FMT
 *
 *  cpp-cmd from cpp_path[]
 *  cpp_simple_args (assumed to apply for all, eg. -U... -U... -P -undef -...)
 *  /var/pcp or similar
 *  /usr/pcp or similar
 *  input pmns file name
 */
#define CPP_FMT "%s %s -I. -I%s%cpmns -I%s%cpmns %s"

static char	*cpp_path[] = {
    CPP_SIMPLE EXEC_SUFFIX,
    "/lib/cpp" EXEC_SUFFIX,
    "/usr/lib/cpp" EXEC_SUFFIX,
    "/usr/bin/cpp" EXEC_SUFFIX,
    NULL
};


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


static int	lineno;
static char	linebuf[256];
static char	*linep;
static char	fname[256];
static char	tokbuf[256];
static pmID	tokpmid;
static int	numpmid;

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

/* The curr_pmns points to PMNS to use for API ops.
 * Curr_pmns will point to either the main_pmns or
 * a pmns from a version 2 archive context.
 */
static __pmnsTree *curr_pmns; 

/* The main_pmns points to the loaded PMNS (not from archive). */
static __pmnsTree *main_pmns; 


/* == 1 if PMNS loaded and __pmExportPMNS has been called */
static int export;

static int havePmLoadCall;
static int useExtPMNS;	/* set by __pmUsePMNS() */

static int load(const char *filename, int binok, int dupok);
static __pmnsNode *locate(const char *name, __pmnsNode *root);


/*
 * Set current pmns to an externally supplied PMNS.
 * Useful for testing the API routines during debugging.
 */
void
__pmUsePMNS(__pmnsTree *t)
{
    useExtPMNS = 1;
    curr_pmns = t;
}


static const char *
pmPMNSLocationStr(int location)
{
    if (location < 0)
	return pmErrStr(location);

    switch(location) {
      case PMNS_LOCAL: return "Local";
      case PMNS_REMOTE: return "Remote";
      case PMNS_ARCHIVE: return "Archive";
      default: return "Internal Error";
    }
}


static int
LoadDefault(char *reason_msg)
{
  if (main_pmns == NULL) {
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PMNS) {
	fprintf(stderr, "pmGetPMNSLocation: Loading local PMNS for %s PMAPI context\n",
                reason_msg);
    }
#endif
    if (load(PM_NS_DEFAULT, 1, 0) < 0)
      return PM_ERR_NOPMNS;
    else
      return PMNS_LOCAL;
  }
  else
    return PMNS_LOCAL;
}

/*
 * Return the pmns_location.
 * Possibly load the default PMNS.
 */
int 
pmGetPMNSLocation(void)
{
  int pmns_location = PM_ERR_NOPMNS;
  int n;
  int sts;
  __pmContext  *ctxp;
  int version;

  if (useExtPMNS) {
      return PMNS_LOCAL;
  }

  /* 
   * Determine if we are to use PDUs or local PMNS file.
   * Load PMNS if necessary.
   */
  if (!havePmLoadCall) {
    if ((n = pmWhichContext()) >= 0) {
      ctxp = __pmHandleToPtr(n);
      switch(ctxp->c_type) {
        case PM_CONTEXT_HOST:
	    if (ctxp->c_pmcd->pc_fd == -1)
		return PM_ERR_IPC;
	    if ((sts = version = __pmVersionIPC(ctxp->c_pmcd->pc_fd)) < 0) {
	      __pmNotifyErr(LOG_ERR, 
			"pmGetPMNSLocation: version lookup failed (context=%d, fd=%d): %s", 
			n, ctxp->c_pmcd->pc_fd, pmErrStr(sts));
	      pmns_location = PM_ERR_NOPMNS;
	    }
            else if (version == PDU_VERSION1) {
	      pmns_location = LoadDefault("PMCD (version 1)");
	    }
	    else if (version == PDU_VERSION2) {
	      pmns_location = PMNS_REMOTE;
	    }
	    else {
            	__pmNotifyErr(LOG_ERR, 
			"pmGetPMNSLocation: bad host PDU version (context=%d, fd=%d, ver=%d)",
			n, ctxp->c_pmcd->pc_fd, version);
	      	pmns_location = PM_ERR_NOPMNS;
	    }
	    break;
        case PM_CONTEXT_LOCAL:
	    pmns_location = LoadDefault("local");
	    break;
        case PM_CONTEXT_ARCHIVE:
            version = ctxp->c_archctl->ac_log->l_label.ill_magic & 0xff;
	    if (version == PM_LOG_VERS01) {
	    	pmns_location = LoadDefault("archive (version 1)");
            }
	    else if (version == PM_LOG_VERS02) {
		pmns_location = PMNS_ARCHIVE;
                curr_pmns = ctxp->c_archctl->ac_log->l_pmns; 
            }
	    else {
	        __pmNotifyErr(LOG_ERR, "pmGetPMNSLocation: bad archive version (context=%d, fd=%d, ver=%d)",
			n, ctxp->c_pmcd->pc_fd, version); 
	        pmns_location = PM_ERR_NOPMNS;
	    }
	    break;
        default: 
	    __pmNotifyErr(LOG_ERR, "pmGetPMNSLocation: bogus context type: %d", ctxp->c_type); 
            pmns_location = PM_ERR_NOPMNS;
      }
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
    curr_pmns = main_pmns;
  else if (pmns_location != PMNS_ARCHIVE)
    curr_pmns = NULL;

  return pmns_location;
}/*pmGetPMNSLocation*/


/*
 * Our own PMNS locator.
 * Don't distinguish between ARCHIVE or LOCAL.
 */
static
int GetLocation(void)
{
  int loc = pmGetPMNSLocation();
  if (loc == PMNS_ARCHIVE) return PMNS_LOCAL;
  return loc;
}

/*
 * for debugging, and visible via __pmDumpNameSpace()
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
 */
static int
lex(int reset)
{
    static int	first = 1;
    static FILE	*fin;
    static char	*lp;
    char	*tp;
    int		colon;
    int		type;
    int		d, c, i;
    __pmID_int	pmid_int;

    if (reset) {
	/* reset! */
	linep = NULL;
	first = 1;
	return 0;
    }

    if (first) {
	int	i, sep = __pmPathSeparator();
	char	*var_dir = pmGetConfig("PCP_VAR_DIR");
	char	*share_dir = pmGetConfig("PCP_SHARE_DIR");

	first = 0;
	for (i = 0; cpp_path[i] != NULL; i++) {
	    if (i != 0 && access(cpp_path[i], X_OK) != 0)
		continue;
	    if ((lp = (char *)malloc(1 + strlen(CPP_FMT)
		+ strlen(cpp_path[i]) + strlen(CPP_SIMPLE_ARGS)
		+ strlen(var_dir) + strlen(share_dir) 
		+ strlen(fname))) == NULL) {
		return -errno;
	    }

/* safe */  sprintf(lp, CPP_FMT, cpp_path[i], CPP_SIMPLE_ARGS, var_dir, 
		    sep, share_dir, sep, fname);

	    fin = popen(lp, "r");
	    free(lp);
	    if (fin == NULL)
		return -errno;
	    break;
	}
	if (cpp_path[i] == NULL) {
	    pmprintf("pmLoadNameSpace: Unable to find an executable cpp at any of ...\n");
	    for (i = 0; cpp_path[i] != NULL; i++)
		pmprintf("    %s\n", cpp_path[i]);
	    pmprintf("Sorry, but this is fatal\n");
	    pmflush();
	    exit(1);
	}

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
		if ( pclose(fin) != 0 ) {
		    lineno = -1; /* We're outside of line counting range now */
                    err("cpp returned non-zero exit status");
                    return PM_ERR_PMNS;
		} else {
		    return 0;
		}
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
		pclose(fin);	/* wait for cpp to finish */
		exit(1);
	    }
	    *q = '\0';
	    if (linebuf[0] == '#') {
#if defined(IS_DARWIN)
		if (sscanf(linebuf, "#pragma GCC set_debug_pwd \"%s", fname) == 1)
			goto skipline;
#endif
		/* cpp control line */
		if ( sscanf(linebuf, "# %d \"%s", &lineno, fname) != 2 ) {
		    err ("Illegal cpp construction");
                    return PM_ERR_PMNS;
		}
#if defined(IS_DARWIN)
skipline:
#endif
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
	     * the item field is not used
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
			return -errno;
		    strcpy(path, np->name);
		}
		else {
		    if ((path = (char *)malloc(strlen(base)+strlen(np->name)+2)) == NULL)
			return -errno;
		    strcpy(path, base);
		    strcat(path, ".");
		    strcat(path, np->name);
		}
		if ((xp = findseen(path)) == NULL) {
		    snprintf(linebuf, sizeof(linebuf), "Cannot find definition for non-terminal node \"%s\" in name space",
		        path);
		    err(linebuf);
		    return PM_ERR_PMNS;
		}
		np->first = xp->first;
		free(xp);
		numpmid--;
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
	return -errno;

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
		if (xp->pmid == np->pmid && !dupok) {
		    char *nn, *xn;
		    backname(np, &nn);
		    backname(xp, &xn);
		    snprintf(linebuf, sizeof(linebuf), "Duplicate metric id (%s) in name space for metrics \"%s\" and \"%s\"\n",
		        pmIDStr(np->pmid), nn, xn);
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
        return -errno;
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
    if ( (status = attach("", main_pmns->root)) )
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

    return __pmFixPMNSHashTab(main_pmns, numpmid, dupok);

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
	    for ( ; np != NULL; np = np->parent) {
		if (bit)
		    np->pmid |= MARK_BIT;
		else
		    np->pmid &= ~MARK_BIT;
	    }
	    return;
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
    if (t == NULL) {
        return -errno;
    }

    /* Insert the "root" node first */
    if ((np = (__pmnsNode *)malloc(sizeof(*np))) == NULL)
	return -errno;
    np->pmid = PM_ID_NULL;
    np->parent = np->first = np->hash = np->next = NULL;
    np->name = strdup("root");
    if (np->name == NULL) {
	free(np);
	return -errno;
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
	return -errno;

    if ((sts = backlink(tree, tree->root, dupok)) < 0)
        return sts;
    mark_all(tree, 0);
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
		return -errno;


            /* fixup name */
	    if ((np->name = (char *)malloc(nch+1)) == NULL)
		return -errno;
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

        }/*loop*/

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
       pmprintf("Cannot add node to contiguously allocated tree!\n"); 
       pmflush();
       exit(1);
    }

    return AddPMNSNode(tree->root, pmid, name);
}


/*
 * 32-bit and 64-bit dependencies ... there are TWO external format,
 * both created by pmnscomp ... choose the correct one based upon
 * how big pointer is ...
 *
 * Magic cookies in the binary format file
 *	PmNs	- old 32-bit (Version 0)
 *	PmN1	- new 32-bit and 64-bit (Version 1)
 *	PmN2	- new 32-bit and 64-bit (Version 1 + checksum)
 *
 * File format:
 *
 *   Version 0
 *     htab
 *     tree-nodes
 *     symbols
 *
 *
 *
 *   Version 1/2
 *     symbols
 *     list of binary-format PMNS (see below)
 *
 *   Binary-format PMNS
 *     htab size, htab entry size
 *     tree-node-tab size, tree-node-tab entry size
 *     htab
 *     tree-nodes
 *     
 */
static int
loadbinary(void)
{
    FILE	*fbin;
    char	magic[4];
    int		nodecnt;
    int		symbsize;
    int		htabsize;
    int		i;
    int		try;
    int		version;
    __int32_t	sum;
    __int32_t	chksum;
    long	endsum;
    __psint_t	ord;
    __pmnsNode	*root;
    __pmnsNode	**htab;
    char	*symbol;

    for (try = 0; try < 2; try++) {
	if (try == 0) {
	    strcpy(linebuf, fname);
	    strcat(linebuf, ".bin");
	}
	else
	    strcpy(linebuf, fname);

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMNS)
	    fprintf(stderr, "loadbinary(file=%s)\n", linebuf);
#endif
	if ((fbin = fopen(linebuf, "r")) == NULL)
	    continue;

	if (fread(magic, sizeof(magic), 1, fbin) != 1) {
	    fclose(fbin);
	    continue;
	}
	version = -1;
	if (strncmp(magic, "PmNs", 4) == 0) {
#if !defined(HAVE_32BIT_PTR)
	    __pmNotifyErr(LOG_WARNING, "pmLoadNameSpace: old 32-bit format binary file \"%s\"", linebuf);
	    fclose(fbin);
	    continue;
#else
	    version = 0;
#endif
	}
	else if (strncmp(magic, "PmN1", 4) == 0)
	    version= 1;
	else if (strncmp(magic, "PmN2", 4) == 0) {
	    version= 2;
	    if (fread(&sum, sizeof(sum), 1, fbin) != 1) {
		fclose(fbin);
		continue;
	    }
	    sum = ntohl(sum);
	    endsum = ftell(fbin);
	    chksum = __pmCheckSum(fbin);
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_PMNS)
		fprintf(stderr, "Version 2 Binary PMNS Checksums: got=%x expected=%x\n", chksum, sum);
#endif
	    if (chksum != sum) {
		__pmNotifyErr(LOG_WARNING, "pmLoadNameSpace: checksum failure for binary file \"%s\"", linebuf);
		fclose(fbin);
		continue;
	    }
	    fseek(fbin, endsum, SEEK_SET);
	}
	if (version == -1) {
	    fclose(fbin);
	    continue;
	}

	if (version == 0) {
	    /*
	     * Expunge support for Version 0 binary PMNS format.
	     * It can never work on anything but 32-bit int and 32-bit ptrs.
	     */
	    goto bad;
	}
	else if (version == 1 || version == 2) {
	    int		sz_htab_ent;
	    int		sz_nodetab_ent;

	    if (fread(&symbsize, sizeof(symbsize), 1, fbin) != 1) goto bad;
	    symbsize = ntohl(symbsize);
	    symbol = (char *)malloc(symbsize);
	    if (symbol == NULL) {
		__pmNoMem("loadbinary-symbol", symbsize, PM_FATAL_ERR);
	    }
	    if (fread(symbol, sizeof(symbol[0]), 
	        symbsize, fbin) != symbsize) goto bad;


	    /* once for each style ... or until EOF */
	    for ( ; ; ) {
		long	skip;

		if (fread(&htabsize, sizeof(htabsize), 1, fbin) != 1) goto bad;
		htabsize = ntohl(htabsize);
		if (fread(&sz_htab_ent, sizeof(sz_htab_ent), 1, fbin) != 1) goto bad;
		sz_htab_ent = ntohl(sz_htab_ent);
		if (fread(&nodecnt, sizeof(nodecnt), 1, fbin) != 1) goto bad;
		nodecnt = ntohl(nodecnt);
		if (fread(&sz_nodetab_ent, sizeof(sz_nodetab_ent), 1, fbin) != 1) goto bad;
		sz_nodetab_ent = ntohl(sz_nodetab_ent);
		if (sz_htab_ent == sizeof(htab[0]) && sz_nodetab_ent == sizeof(*root))
		   break; /* found correct one */

		/* skip over hash-table and node-table */
		skip = htabsize * sz_htab_ent + nodecnt * sz_nodetab_ent;
		fseek(fbin, skip, SEEK_CUR);
            }

	    /* the structure elements are all the right size */
	    main_pmns = (__pmnsTree*)malloc(sizeof(*main_pmns));
	    htab = (__pmnsNode **)malloc(htabsize * sizeof(htab[0]));
	    root = (__pmnsNode *)malloc(nodecnt * sizeof(*root));

	    if (main_pmns == NULL || htab == NULL || root == NULL) {
		__pmNoMem("loadbinary-1",
			 sizeof(*main_pmns) +
			 htabsize * sizeof(htab[0]) + 
			 nodecnt * sizeof(*root),
			 PM_FATAL_ERR);
	    }

	    if (fread(htab, sizeof(htab[0]), htabsize, fbin) != htabsize) goto bad;
	    if (fread(root, sizeof(*root), nodecnt, fbin) != nodecnt) goto bad;

#if defined(HAVE_32BIT_PTR)
	    /* swab htab : pointers are 32 bits */
	    for (i=0; i < htabsize; i++) {
		htab[i] = (__pmnsNode *)ntohl((__uint32_t)htab[i]);
	    }

	    /* swab all nodes : pointers are 32 bits */
	    for (i=0; i < nodecnt; i++) {
		__pmnsNode *p = &root[i];
		p->pmid = __ntohpmID(p->pmid);
		p->parent = (__pmnsNode *)ntohl((__uint32_t)p->parent);
		p->next = (__pmnsNode *)ntohl((__uint32_t)p->next);
		p->first = (__pmnsNode *)ntohl((__uint32_t)p->first);
		p->hash = (__pmnsNode *)ntohl((__uint32_t)p->hash);
		p->name = (char *)ntohl((__uint32_t)p->name);
	    }
#elif defined(HAVE_64BIT_PTR)
	    /* swab htab : pointers are 64 bits */
	    for (i=0; i < htabsize; i++) {
		__ntohll((char *)&htab[i]);
	    }

	    /* swab all nodes : pointers are 64 bits */
	    for (i=0; i < nodecnt; i++) {
		__pmnsNode *p = &root[i];
		p->pmid = __ntohpmID(p->pmid);
		__ntohll((char *)&p->parent);
		__ntohll((char *)&p->next);
		__ntohll((char *)&p->first);
		__ntohll((char *)&p->hash);
		__ntohll((char *)&p->name);
	    }
#else
!bozo!
#endif

#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_PMNS)
		fprintf(stderr, "Loaded Version 1 or 2 Binary PMNS, nodetab ent = %d bytes\n", sz_nodetab_ent);
#endif
	}

	fclose(fbin);

	/* relocate */
	for (i = 0; i < htabsize; i++) {
	    ord = (ptrdiff_t)htab[i];
	    if (ord == (__psint_t)-1)
		htab[i] = NULL;
	    else
		htab[i] = &root[ord];
	}

	for (i = 0; i < nodecnt; i++) {
	    ord = (__psint_t)root[i].parent;
	    if (ord == (__psint_t)-1)
		root[i].parent = NULL;
	    else
		root[i].parent = &root[ord];
	    ord = (__psint_t)root[i].next;
	    if (ord == (__psint_t)-1)
		root[i].next = NULL;
	    else
		root[i].next = &root[ord];
	    ord = (__psint_t)root[i].first;
	    if (ord == (__psint_t)-1)
		root[i].first = NULL;
	    else
		root[i].first = &root[ord];
	    ord = (__psint_t)root[i].hash;
	    if (ord == (__psint_t)-1)
		root[i].hash = NULL;
	    else
		root[i].hash = &root[ord];
	    ord = (__psint_t)root[i].name;
	    root[i].name = &symbol[ord];
	}

        /* set the pmns tree fields */
	main_pmns->root = root;
	main_pmns->htab = htab;
        main_pmns->htabsize = htabsize;
	main_pmns->symbol = symbol;
	main_pmns->contiguous = 1;
	main_pmns->mark_state = UNKNOWN_MARK_STATE;
	return 1;
	
bad:
	__pmNotifyErr(LOG_WARNING, "pmLoadNameSpace: bad binary file, \"%s\"", linebuf);
	fclose(fbin);
	return 0;
    }

    /* failed to open and/or find magic cookie */
    return 0;
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
 */
static int
loadascii(int dupok)
{
    int		state = 0;
    int		type;
    __pmnsNode	*np;


    /* do some resets */
    lex(1);      /* reset analyzer */
    seen = NULL; /* make seen-list empty */
    numpmid = 0;


    if (access(fname, R_OK) == -1) {
	snprintf(linebuf, sizeof(linebuf), "Cannot open \"%s\"", fname);
	err(linebuf);
	return -errno;
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
		    fprintf(stderr, "pmLoadNameSpace: %s -> 0x%0x\n",
					np->name, (int)np->pmid);
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

	default:
	    err("Internal botch");
	    abort();

	}

	if (state == 1 || state == 3) {
	    if ((np = (__pmnsNode *)malloc(sizeof(*np))) == NULL)
		return -errno;
	    numpmid++;
	    if ((np->name = (char *)malloc(strlen(tokbuf)+1)) == NULL)
		return -errno;
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
	    snprintf(repname, sizeof(repname), "%s%c" "pmns" "%c" "root",
		     pmGetConfig("PCP_VAR_DIR"), sep, sep);
	    return repname;
	}
    }
    return filename;
}

int
__pmHasPMNSFileChanged(const char *filename)
{
  static const char *f = NULL;

  f = getfname(filename);
  if (f == NULL)
     return 1; /* error encountered -> must have changed :) */
 
  /* if still using same filename ... */
  if (strcmp(f, fname) == 0) {
     struct stat statbuf;

     if (stat(f, &statbuf) == 0) {
         /* If the modification times have changed */
#if defined(HAVE_ST_MTIME_WITH_E) && defined(HAVE_STAT_TIME_T)
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMNS) {
	    fprintf(stderr, "__pmHasPMNSFileChanged(%s) -> %s last=%d now=%d\n",
		filename == PM_NS_DEFAULT || (__psint_t)filename == 0xffffffff ? "PM_NS_DEFAULT" : filename,
		f, (int)last_mtim, (int)statbuf.st_mtime);
	}
#endif
         return ((statbuf.st_mtime == last_mtim) ? 0 : 1);
#elif defined(HAVE_ST_MTIME_WITH_SPEC)
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMNS) {
	    fprintf(stderr, "__pmHasPMNSFileChanged(%s) -> %s last=%d.%09ld now=%d.%09ld\n",
		filename == PM_NS_DEFAULT || (__psint_t)filename == 0xffffffff ? "PM_NS_DEFAULT" : filename,
		f, (int)last_mtim.tv_sec, last_mtim.tv_nsec,
		(int)statbuf.st_mtimespec.tv_sec, statbuf.st_mtimespec.tv_nsec);
	}
#endif
	return ((statbuf.st_mtimespec.tv_sec == last_mtim.tv_sec &&
	    statbuf.st_mtimespec.tv_nsec == last_mtim.tv_nsec) ? 0 : 1);
#elif defined(HAVE_STAT_TIMESTRUC) || defined(HAVE_STAT_TIMESPEC) || defined(HAVE_STAT_TIMESPEC_T)
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMNS) {
	    fprintf(stderr, "__pmHasPMNSFileChanged(%s) -> %s last=%d.%09ld now=%d.%09ld\n",
		filename == PM_NS_DEFAULT || (__psint_t)filename == 0xffffffff ? "PM_NS_DEFAULT" : filename,
		f, (int)last_mtim.tv_sec, last_mtim.tv_nsec,
		(int)statbuf.st_mtim.tv_sec, statbuf.st_mtim.tv_nsec);
	}
#endif
	return ((statbuf.st_mtim.tv_sec == last_mtim.tv_sec &&
	    statbuf.st_mtim.tv_nsec == last_mtim.tv_nsec) ? 0 : 1);
#else
!bozo!
#endif
     }
     else {
         return 1; /* error encountered -> must have changed :) */
     }
  }
  return 1; /* different filenames atleast */
}

static int
load(const char *filename, int binok, int dupok)
{
    int 	i = 0;

    if (main_pmns != NULL) {
	if (export) {
	    export = 0;

	    /*
	     * drop the loaded PMNS ... huge memory leak, but it is
	     * assumed the caller has saved the previous PMNS after calling
	     * __pmExportPMNS()
	     */
	    main_pmns = NULL;
	}
	else {
	    return PM_ERR_DUPPMNS;
        }
    }

    strcpy(fname, getfname(filename));
 
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PMNS)
	fprintf(stderr, "load(name=%s, binok=%d, dupok=%d) lic case=%d fname=%s\n",
		filename, binok, dupok, i, fname);
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

    /* try the easy way, c/o pmnscomp */
    if (binok && loadbinary()) {
	mark_all(main_pmns, 0);
	return 0;
    }

    /*
     * the hard way, compiling as we go ...
     */
    return loadascii(dupok);
}

/*
 * just for pmnscomp to use
 */
__pmnsTree*
__pmExportPMNS(void)
{
    export = 1;
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

int
pmLoadNameSpace(const char *filename)
{
    havePmLoadCall = 1;
    return load(filename, 1, 0);
}

int
pmLoadASCIINameSpace(const char *filename, int dupok)
{
    havePmLoadCall = 1;
    return load(filename, 0, dupok);
}

/*
 * Assume that each node has been malloc'ed separately.
 * This is the case for an ASCII loaded PMNS.
 * Traverse entire tree and free each node.
 */
static void
FreeTraversePMNS(__pmnsNode *parent)
{
  __pmnsNode *np, *next;

  if (!parent) return;

  /* Free child sub-trees */
  for (np = parent->first; np != NULL; np = next) {
    next = np->next;
    FreeTraversePMNS(np);
  }

  free(parent->name);
  free(parent);

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
    havePmLoadCall = 0;
    __pmFreePMNS(main_pmns);
    main_pmns = NULL;
}

static int
request_names (__pmContext *ctxp, int numpmid, char *namelist[])
{
    int n;

    if (ctxp->c_pmcd->pc_curpdu != 0) {
	return (PM_ERR_CTXBUSY);
    }

    n = __pmSendNameList(ctxp->c_pmcd->pc_fd, PDU_BINARY, 
			 numpmid, namelist, NULL);
    if (n < 0) {
	n = __pmMapErrno(n);
    }

    return (n);
}

int
pmRequestNames (int ctxid, int numpmid, char *namelist[])
{
    int n;
    __pmContext *ctxp;

    if ((n =__pmGetHostContextByID(ctxid, &ctxp)) >= 0) {
	if ((n = request_names(ctxp, numpmid, namelist)) >= 0) {
	    ctxp->c_pmcd->pc_curpdu = PDU_PMNS_NAMES;
	    ctxp->c_pmcd->pc_tout_sec = TIMEOUT_DEFAULT;
	}
    }

    return (n);
}

static int
receive_names (__pmContext *ctxp, int numpmid, pmID pmidlist[])
{
    int n;
    __pmPDU      *pb;

    n = __pmGetPDU(ctxp->c_pmcd->pc_fd, PDU_BINARY,
		   ctxp->c_pmcd->pc_tout_sec, &pb);
    if (n == PDU_PMNS_IDS) {
	/* Note:
	 * pmLookupName may return an error even though
	 * it has a valid list of ids.
	 * This is why we need op_status.
	 */
	int op_status; 
	n = __pmDecodeIDList(pb, PDU_BINARY, 
			       numpmid, pmidlist, &op_status);
	if (n >= 0)
	    n = op_status;
    }
    else if (n == PDU_ERROR) {
	__pmDecodeError(pb, PDU_BINARY, &n);
    }
    else if (n != PM_ERR_TIMEOUT) {
	n = PM_ERR_IPC;
    }

    return (n);
}

int
pmReceiveNames (int ctxid, int numpmid, pmID pmidlist[])
{
    int n;
    __pmContext *ctxp;

    if ((n =__pmGetBusyHostContextByID(ctxid, &ctxp, PDU_PMNS_NAMES)) >= 0) {
	n = receive_names(ctxp, numpmid, pmidlist);

	ctxp->c_pmcd->pc_curpdu = 0;
	ctxp->c_pmcd->pc_tout_sec = 0;


    }

    return (n);
}

int
pmLookupName(int numpmid, char *namelist[], pmID pmidlist[])
{
    int pmns_location;
    int	sts = 0;

    if (numpmid < 1) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMNS) {
	    fprintf(stderr, "pmLookupName(%d, ...) bad numpmid!\n", numpmid);
	}
#endif
	return PM_ERR_TOOSMALL;
    }

    pmns_location = GetLocation();
    
    if (pmns_location < 0) {
        sts = pmns_location;
    }
    else if (pmns_location == PMNS_LOCAL) {
        int		i;
        __pmnsNode	*np;

	for (i = 0; i < numpmid; i++) {
            /* if we locate it and its a leaf */
	    if ((np = locate(namelist[i], curr_pmns->root)) != NULL ) {
               if (np->first == NULL)
		  pmidlist[i] = np->pmid;
               else {
		  sts = PM_ERR_NONLEAF;
		  pmidlist[i] = PM_ID_NULL;
               }
            }
	    else {
		sts = PM_ERR_NAME;
		pmidlist[i] = PM_ID_NULL;
	    }
	}

    	sts = (sts == 0 ? i : sts);

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMNS) {
	    int	i;
	    fprintf(stderr, "pmLookupName(%d, ...) using local PMNS returns %d and ...\n",
		numpmid, sts);
	    for (i = 0; i < numpmid; i++) {
		fprintf(stderr, "  name[%d]: \"%s\"", i, namelist[i]);
		if (sts >= 0)
		    fprintf(stderr, " PMID: 0x%x %s",
			pmidlist[i], pmIDStr(pmidlist[i]));
		fputc('\n', stderr);
	    }
	}
#endif

    }
    else {
        /* assume PMNS_REMOTE */
	int         n;
	__pmContext  *ctxp;

        /* As we have PMNS_REMOTE there must be
         * a current host context.
         */
	n = pmWhichContext();
	assert(n >= 0);
	ctxp = __pmHandleToPtr(n);
	if ((sts = request_names (ctxp, numpmid, namelist)) >= 0) {
	    sts = receive_names(ctxp, numpmid, pmidlist);
	}
    }

    return sts;
}

static int
request_names_of_children(__pmContext *ctxp, const char *name, int wantstatus)
{
    int n;

    if (ctxp->c_pmcd->pc_curpdu != 0) {
	return (PM_ERR_CTXBUSY);
    }

    n = __pmSendChildReq(ctxp->c_pmcd->pc_fd, PDU_BINARY, name, wantstatus);
    if (n < 0) {
        n =  __pmMapErrno(n);
    }

    return (0);
}

int
pmRequestNamesOfChildern (int ctxid, const char *name, int wantstatus)
{
    int n;
    __pmContext *ctxp;

    if ((n = __pmGetHostContextByID (ctxid, &ctxp)) >= 0) {
	if ((n = request_names_of_children(ctxp, name, wantstatus)) >= 0) {
	    ctxp->c_pmcd->pc_curpdu = PDU_PMNS_CHILD;
	    ctxp->c_pmcd->pc_tout_sec = TIMEOUT_DEFAULT;
	}
    }

    return (n);
}

static int
receive_names_of_children (__pmContext *ctxp, char ***offspring,
			   int **statuslist)
{
    int n;
    __pmPDU      *pb;

    n = __pmGetPDU(ctxp->c_pmcd->pc_fd, PDU_BINARY, 
		   ctxp->c_pmcd->pc_tout_sec, &pb);
    if (n == PDU_PMNS_NAMES) {
	int numnames;

	n = __pmDecodeNameList(pb, PDU_BINARY, &numnames, 
			       offspring, statuslist);
	if (n >= 0) {
	    n = numnames;
	}
    }
    else if (n == PDU_ERROR) {
	__pmDecodeError(pb, PDU_BINARY, &n);
	    }
    else if (n != PM_ERR_TIMEOUT)
	n =  PM_ERR_IPC;

    return n;
}

int
pmReceiveNamesOfChildren (int ctxid, char ***offsprings, int **status)
{
    int n;
    __pmContext *ctxp;

    if ((n = __pmGetBusyHostContextByID (ctxid, &ctxp, PDU_PMNS_CHILD)) >= 0) {
	n = receive_names_of_children (ctxp, offsprings, status);

	ctxp->c_pmcd->pc_curpdu = 0;
	ctxp->c_pmcd->pc_tout_sec = 0;
    }

    return (n);
}

static int
GetChildrenStatusRemote(__pmContext *ctxp, const char *name,
			char ***offspring, int **statuslist)
{
    int n;

    if ((n = request_names_of_children(ctxp, name,
				       (statuslist==NULL) ? 0 : 1)) >= 0) {
	n = receive_names_of_children (ctxp, offspring, statuslist);
    }
    return (n);
}

/*
 * It is allowable to pass in a statuslist arg of NULL. It is therefore important
 * to check that this is not NULL before accessing it.
 */
int
pmGetChildrenStatus(const char *name, char ***offspring, int **statuslist)
{
    int *status = NULL;
    int pmns_location = GetLocation();

    if (pmns_location < 0 )
	return pmns_location;

    if (name == NULL) 
	return PM_ERR_NAME;

    if (pmns_location == PMNS_LOCAL) {

	__pmnsNode	*np;
	__pmnsNode	*tnp;
	int		i;
	int		j;
	int		need;
	int		num;
	char		**result;
	char		*p;

        int     num_xlch = 0;
        char    **xlch = NULL;
        int     *xlstatus = NULL;

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMNS) {
	    fprintf(stderr, "pmGetChildren(name=\"%s\") [local]\n", name);
	}
#endif

	/* avoids ambiguity, for errors and leaf nodes */
	*offspring = NULL;
	if (statuslist)
	  *statuslist = NULL;

	if (*name == '\0')
	    np = curr_pmns->root; /* use "" to name the root of the PMNS */

	else if ((np = locate(name, curr_pmns->root)) == NULL)
	   return PM_ERR_NAME;

        if (np != NULL && num_xlch == 0)
	    if (np->first == NULL)
	       /* this is a leaf node */
	       return 0;

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
        for(i = 0; i < num_xlch; i++) {
            num++;
            need += sizeof(**offspring) + strlen(xlch[i]) + 1;
        }

	if ((result = (char **)malloc(need)) == NULL)
	    return -errno;

        if (statuslist != NULL) {
          if ((status = (int *)malloc(num*sizeof(int))) == NULL)
            return -errno;
        }

	p = (char *)&result[num];

        if (np != NULL) {
	    for (i = 0, tnp = np->first; tnp != NULL; tnp = tnp->next) {
	        if ((tnp->pmid & MARK_BIT) == 0) {
		    result[i] = p;
		    if (statuslist != NULL) 
		      status[i] = (tnp->first != NULL); /* has children */
		    strcpy(result[i], tnp->name);
		    p += strlen(tnp->name) + 1;
		    i++;
	        }
	    }
	}
        else
            i = 0;

        for(j = 0; j < num_xlch; i++, j++) {
            result[i] = p;
            if (statuslist != NULL)
                status[i] = xlstatus[j];
            strcpy(result[i], xlch[j]);
            p += strlen(xlch[j]) + 1;
        }
        if (num_xlch > 0) {
            free(xlch);
            free(xlstatus);
        }

	*offspring = result;
	if (statuslist != NULL)
	  *statuslist = status;

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMNS) {
	    fprintf(stderr, "pmGetChildren -> ");
	    __pmDumpNameList(stderr, num, result);
	}
#endif

	return num;
    }

    else {
	/* assume PMNS_REMOTE */
	int         n;
	__pmContext *ctxp;

        /* As we have PMNS_REMOTE there must be
         * a current host context.
         */
	n = pmWhichContext();
	assert(n >= 0);
	ctxp = __pmHandleToPtr(n);
        return GetChildrenStatusRemote(ctxp, name, offspring, statuslist);
    }
}

int
pmGetChildren(const char *name, char ***offspring)
{
    return pmGetChildrenStatus(name, offspring, NULL);
}

static int
request_namebypmid (__pmContext *ctxp, pmID pmid)
{
    int n;

    if (ctxp->c_pmcd->pc_curpdu != 0)
	return PM_ERR_CTXBUSY;

    n = __pmSendIDList(ctxp->c_pmcd->pc_fd, PDU_BINARY, 1, &pmid, 0);
    if (n < 0)
	n = __pmMapErrno(n);
    return n;
}

int
pmRequestNameID (int ctxid, pmID pmid)
{
    int n;
    __pmContext *ctxp;

    if ((n = __pmGetHostContextByID(ctxid, &ctxp)) >= 0) {
	if ((n = request_namebypmid (ctxp, pmid)) >= 0) {
	    ctxp->c_pmcd->pc_curpdu = PDU_PMNS_IDS;
	    ctxp->c_pmcd->pc_tout_sec = TIMEOUT_DEFAULT;
	}
    }

    return n;
}

static int
receive_namesbyid (__pmContext *ctxp, char ***namelist)
{
    int         n;
    __pmPDU      *pb;

    n = __pmGetPDU(ctxp->c_pmcd->pc_fd, PDU_BINARY, 
                   ctxp->c_pmcd->pc_tout_sec, &pb);
    
    if (n == PDU_PMNS_NAMES) {
	int numnames;

	n = __pmDecodeNameList(pb, PDU_BINARY, &numnames, namelist, NULL);
	if (n >= 0)
	    n = numnames;
    }
    else if (n == PDU_ERROR)
	__pmDecodeError(pb, PDU_BINARY, &n);
    else if (n != PM_ERR_TIMEOUT)
	n = PM_ERR_IPC;

    return n;
}

static int 
receive_a_name (__pmContext *ctxp, char **name)
{
    int n;
    char **namelist;

    if ((n = receive_namesbyid(ctxp, &namelist)) >= 0) {
	char *newname = strdup(namelist[0]);
	free(namelist);
	if (newname == NULL) {
	    n =  -((errno) ? errno : ENOMEM);
	} else {
	    *name = newname;
	    n = 0;
	}
    }

    return n;
}

int
pmReceiveNameID (int ctxid, char **name)
{
    int n;
    __pmContext *ctxp;

    if ((n = __pmGetBusyHostContextByID(ctxid, &ctxp, PDU_PMNS_IDS)) >= 0) {
	n = receive_a_name (ctxp, name);

	ctxp->c_pmcd->pc_curpdu = 0;
	ctxp->c_pmcd->pc_tout_sec = 0;
    }

    return n;
}

int
pmReceiveNamesAll (int ctxid, char ***namelist)
{
    int n;
    __pmContext *ctxp;

    if ((n = __pmGetBusyHostContextByID(ctxid, &ctxp, PDU_PMNS_IDS)) >= 0) {
	n = receive_namesbyid (ctxp, namelist);

	ctxp->c_pmcd->pc_curpdu = 0;
	ctxp->c_pmcd->pc_tout_sec = 0;
    }

    return n;
}

int
pmNameID(pmID pmid, char **name)
{
    int pmns_location = GetLocation();

    if (pmns_location < 0)
	return pmns_location;

    else if (pmns_location == PMNS_LOCAL) {
    	__pmnsNode	*np;
	for (np = curr_pmns->htab[pmid % curr_pmns->htabsize]; np != NULL; np = np->hash) {
	    if (np->pmid == pmid)
		return backname(np, name);
	}
    	return PM_ERR_PMID;
    }

    else {
	/* assume PMNS_REMOTE */
	int         n;
	__pmContext  *ctxp;

        /* As we have PMNS_REMOTE there must be
         * a current host context.
         */
	n = pmWhichContext();
	assert(n >= 0);
	ctxp = __pmHandleToPtr(n);

	if ((n = request_namebypmid (ctxp, pmid)) >= 0) {
	    n = receive_a_name(ctxp, name);
	}
	return n;
    }
}

int
pmNameAll(pmID pmid, char ***namelist)
{
    int pmns_location = GetLocation();

    if (pmns_location < 0)
	return pmns_location;

    else if (pmns_location == PMNS_LOCAL) {
    	__pmnsNode	*np;
	int		sts;
	int		n = 0;
	int		len = 0;
	int		i;
	char	*sp;
	char	**tmp = NULL;

	for (np = curr_pmns->htab[pmid % curr_pmns->htabsize]; np != NULL; np = np->hash) {
	    if (np->pmid == pmid) {
		n++;
		if ((tmp = (char **)realloc(tmp, n * sizeof(tmp[0]))) == NULL)
		    return -errno;
		if ((sts = backname(np, &tmp[n-1])) < 0) {
		    /* error, ... free any partial allocations */
		    for (i = n-2; i >= 0; i--)
			free(tmp[i]);
		    free(tmp);
		    return sts;
		}
		len += strlen(tmp[n-1])+1;
	    }
	}

	if (n == 0)
	    return PM_ERR_PMID;

	len += n * sizeof(tmp[0]);
	if ((tmp = (char **)realloc(tmp, len)) == NULL)
	    return -errno;

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

    else {
	/* assume PMNS_REMOTE */
	int         n;
	__pmContext  *ctxp;

        /* As we have PMNS_REMOTE there must be
         * a current host context.
         */
	n = pmWhichContext();
	assert(n >= 0);
	ctxp = __pmHandleToPtr(n);

	if ((n = request_namebypmid (ctxp, pmid)) >= 0) {
	    n = receive_namesbyid (ctxp, namelist);
	}
	return n;
    }
}


/*
 * generic depth-first recursive descent of the PMNS
 */
static int
TraversePMNS_local(const char *name, void(*func)(const char *name))
{
    int		sts;
    char	**enfants;

    if ((sts = pmGetChildren(name, &enfants)) < 0) {
	return sts;
    }
    else if (sts == 0) {
	/* leaf node, name is full name of a metric */
	(*func)(name);
    }
    else if (sts > 0) {
	int	j;
	char	*newname;
	int	n;

	for (j = 0; j < sts; j++) {
	    newname = (char *)malloc(strlen(name) + 1 + strlen(enfants[j]) + 1);
	    if (newname == NULL) {
		printf("pmTraversePMNS: malloc: %s\n", strerror(errno));
		exit(1);
	    }
	    if (*name == '\0')
		strcpy(newname, enfants[j]);
	    else {
		strcpy(newname, name);
		strcat(newname, ".");
		strcat(newname, enfants[j]);
	    }
	    n = TraversePMNS_local(newname, func);
	    free(newname);
	    if (sts == 0)
		sts = n;
	}
	free(enfants);
    }

    return sts;
}

static int
request_traverse_pmns (__pmContext *ctxp, const char *name)
{
    int n;

    if (ctxp->c_pmcd->pc_curpdu != 0)
	return PM_ERR_CTXBUSY;
    n = __pmSendTraversePMNSReq(ctxp->c_pmcd->pc_fd, PDU_BINARY, name);
    if (n < 0)
	n = __pmMapErrno(n);
    return n;
}

int
pmRequestTraversePMNS (int ctx, const char *name)
{
    int n;
    __pmContext *ctxp;

    if ((n = __pmGetHostContextByID(ctx, &ctxp)) >= 0) {
	if ((n = request_traverse_pmns(ctxp, name)) >= 0) {
	    ctxp->c_pmcd->pc_curpdu = PDU_PMNS_TRAVERSE;
	    ctxp->c_pmcd->pc_tout_sec = TIMEOUT_DEFAULT;
	}
    }
    return n;
}

int
pmReceiveTraversePMNS (int ctxid, void(*func)(const char *name))
{
    int n;
    __pmContext *ctxp;
    __pmPDU *pb;

    if ((n = __pmGetBusyHostContextByID(ctxid, &ctxp, PDU_PMNS_TRAVERSE)) < 0)
	return n;

    n = __pmGetPDU(ctxp->c_pmcd->pc_fd, PDU_BINARY, 
		   ctxp->c_pmcd->pc_tout_sec, &pb);
    if (n == PDU_PMNS_NAMES) {
	int numnames;
	int i;
	char **namelist;

	n = __pmDecodeNameList(pb, PDU_BINARY, &numnames, &namelist, NULL);
	if (n >= 0) {
	    for (i=0; i<numnames; i++) {
		func(namelist[i]);
	    }
	
	    free(namelist);
	}
    }
    else if (n == PDU_ERROR) {
	__pmDecodeError(pb, PDU_BINARY, &n);
    }
    else if (n != PM_ERR_TIMEOUT) {
	n = PM_ERR_IPC;
    }

    ctxp->c_pmcd->pc_curpdu = 0;
    ctxp->c_pmcd->pc_tout_sec = 0;

    return n;
}

int
pmTraversePMNS(const char *name, void(*func)(const char *name))
{
    int pmns_location = GetLocation();

    if (pmns_location < 0)
	return pmns_location;

    if (name == NULL) 
	return PM_ERR_NAME;

    if (pmns_location == PMNS_LOCAL)
	return TraversePMNS_local(name, func);
    else { 
	int         n;
	__pmPDU      *pb;
	__pmContext  *ctxp;

        /* As we have PMNS_REMOTE there must be
         * a current host context.
         */
	n = pmWhichContext();
	assert(n >= 0);
	ctxp = __pmHandleToPtr(n);
	if ((n = request_traverse_pmns (ctxp, name)) < 0) {
	    return (n);
	} else {
	    n = __pmGetPDU(ctxp->c_pmcd->pc_fd, PDU_BINARY, 
                          TIMEOUT_DEFAULT, &pb);
	    if (n == PDU_PMNS_NAMES) {
		int numnames;
		int i;
                char **namelist;

		n = __pmDecodeNameList(pb, PDU_BINARY, &numnames, 
		                      &namelist, NULL);
		if (n < 0)
		  return n;

		for (i=0; i<numnames; i++) {
                    func(namelist[i]);
                }
		free(namelist);
                return n;
	    }
	    else if (n == PDU_ERROR) {
		__pmDecodeError(pb, PDU_BINARY, &n);
		return n;
            }
	    else if (n != PM_ERR_TIMEOUT)
		return PM_ERR_IPC;
            return n;
	}
    }
}

int
pmTrimNameSpace(void)
{
    int		i;
    __pmContext	*ctxp;
    __pmHashCtl	*hcp;
    __pmHashNode	*hp;
    int 	version;
    int pmns_location = GetLocation();

    if (pmns_location < 0)
	return pmns_location;

    else if (pmns_location == PMNS_REMOTE)
        return 0;

    /* for PMNS_LOCAL ... */

    if ((ctxp = __pmHandleToPtr(pmWhichContext())) == NULL)
	return PM_ERR_NOCONTEXT;

    if (ctxp->c_type != PM_CONTEXT_ARCHIVE) {
	/* unset all of the marks */
	mark_all(curr_pmns, 0);
	return 0;
    }

    version = ctxp->c_archctl->ac_log->l_label.ill_magic & 0xff;

    /* Don't do any trimming for the new archives -
     * they have their own built-in PMNS.
     * Exception: if an explicit load PMNS call was made.
     */
    if (version == PM_LOG_VERS01 || havePmLoadCall) {
	/*
	 * (1) set all of the marks, and
	 * (2) clear the marks for those metrics defined in the archive
	 */
	mark_all(curr_pmns, 1);
	hcp = &ctxp->c_archctl->ac_log->l_hashpmid;

	for (i = 0; i < hcp->hsize; i++) {
	    for (hp = hcp->hash[i]; hp != NULL; hp = hp->next) {
		mark_one(curr_pmns, (pmID)hp->key, 0);
	    }
	}
    }

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

    dumptree(f, 0, curr_pmns->root, verbosity);
}
