/*
 * Copyright (c) 2021 Ken McDonell.  All Rights Reserved.
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
 *
 * Implement a simple name cache.  Quick scan of pmlogger config is used
 * to extract all possible PMNS names (leaf and non-leaf), then once the
 * scan is done, populate the cache with pmID and pmDesc metadata for all
 * the names that are leaf nodes in the PMNS.
 */

#include <ctype.h>
#include <sys/stat.h>
#include "logger.h"

/*
 * values for "state" of entry in name cache
 */
#define N_UNKNOWN	0
#define N_TRAVERSE	1
#define N_LEAF		2

typedef struct cache_entry {
    struct cache_entry	*next;		/* linked list of hash synonyms */
    char		*name;
    pmID		pmid;		
    pmDesc		desc;
    int			state;		/* as above */
} cache_entry_t;

static __pmHashCtl	name_cache;
static int		num_add;
static int		num_syn;
static int		num_dup;
static int		no_cache;	/* 1 => -Dappl7 or fatal problem in setup */

/*
 * free the name cache
 */
void
cache_free(void)
{
    __pmHashNode	*hp = __pmHashWalk(&name_cache, PM_HASH_WALK_START);
    cache_entry_t	*cep;
    cache_entry_t	*next;

    while (hp != NULL) {
	for (cep = (cache_entry_t *)hp->data; cep != NULL; ) {
	    free(cep->name);
	    next = cep->next;
	    free(cep);
	    cep = next;
	}
	hp->data = NULL;
	hp = __pmHashWalk(&name_cache, PM_HASH_WALK_NEXT);
    }
    __pmHashFree(&name_cache);
    no_cache = 1;
}

/*
 * Simple hash "string" -> unsigned int, using 4 byte at a time XOR
 * ... result suitable for use with __pmHash*() routines
 */
static unsigned int
fold_string(const char *str)
{
    union {
	unsigned int	key;
	char		p[4];
    } work = { 0 };
    int		i = 0;

    while (*str) {
	work.p[i] = work.p[i] ^ *str++;
	if (i < 3)
	    i++;
	else
	    i = 0;
    }

    return work.key;
}

static void
cache_add(const char *name)
{
    unsigned int	key = fold_string(name);
    __pmHashNode	*hp;
    cache_entry_t	*head;		/* of synonym chain */
    cache_entry_t	*cep;		/* new cache entry */
    int			sts;

    if ((hp = __pmHashSearch(key, &name_cache)) != NULL) {
	head = (cache_entry_t *)hp->data;
	for (cep = head; cep != NULL; cep = cep->next) {
	    if (strcmp(cep->name, name) == 0) {
		/*
		 * same name appears more than once in config, skip this
		 */
		if (pmDebugOptions.dev0) {
		    fprintf(stderr, "dup name: %s\n", name);
		}
		num_dup++;
		return;
	    }
	}
	num_syn++;
    }
    else
	head = NULL;
    if ((cep = (cache_entry_t *)malloc(sizeof(cache_entry_t))) == NULL) {
	fprintf(stderr, "cache_add(%s): cache_entry malloc failed\n", name);
	return;
    }
    if ((cep->name = strdup(name)) == NULL) {
	fprintf(stderr, "cache_add(%s): name strdup failed\n", name);
	free(cep);
	return;
    }
    /* mark entry as "needs metadata" for cache_bind() */
    cep->state = N_UNKNOWN;
    if (hp == NULL) {
	/*
	 * first with this key
	 */
	cep->next = NULL;
	sts = __pmHashAdd(key, (void *)cep, &name_cache);
	if (sts < 0) {
	    fprintf(stderr, "__pmHashAdd: failed for name=%s key=%u: %s\n", name, key, pmErrStr(sts));
	    free(cep->name);
	    free(cep);
	    return;
	}
	if (pmDebugOptions.dev0) {
	    fprintf(stderr, "name[%d] %s key=%d\n", num_add, name, key);
	}
    }
    else {
	/*
	 * synonym for key (different name), link to head of list
	 */
	cep->next = head;
	hp->data = (void *)cep;
	if (pmDebugOptions.dev0) {
	    fprintf(stderr, "name[%d] %s key=%d (synonym)\n", num_add, name, key);
	}
    }
    num_add++;
    return;
}

/*
 * Scan the name cache and try to fetch any missing metadata
 */
static void
cache_bind(void)
{
    __pmHashNode	*hp;
    cache_entry_t	*cep;
    char		**namelist = NULL;
    pmID		*pmidlist = NULL;
    pmDesc		*desclist = NULL;
    int			num_to_bind = 0;
    int			i;
    int			sts;

    hp = __pmHashWalk(&name_cache, PM_HASH_WALK_START);
    num_to_bind = 0;
    while (hp != NULL) {
	for (cep = (cache_entry_t *)hp->data; cep != NULL; cep = cep->next) {
	    if (cep->state == N_UNKNOWN)
		num_to_bind++;
	}
	hp = __pmHashWalk(&name_cache, PM_HASH_WALK_NEXT);
    }

    /* no metrics requiring metadata, nothing to be done */
    if (num_to_bind == 0)
	return;

    namelist = (char **)malloc(num_to_bind * sizeof(char *));
    if (namelist == NULL) {
	fprintf(stderr, "cache_bind(): namelist[%d] malloc failed\n", num_to_bind);
	goto cleanup;
    }

    pmidlist = (pmID *)malloc(num_to_bind * sizeof(pmID));
    if (pmidlist == NULL) {
	fprintf(stderr, "cache_bind(): pmidlist[%d] malloc failed\n", num_to_bind);
	goto cleanup;
    }

    desclist = (pmDesc *)malloc(num_to_bind * sizeof(pmDesc));
    if (desclist == NULL) {
	fprintf(stderr, "cache_bind(): desclist[%d] malloc failed\n", num_to_bind);
	goto cleanup;
    }

    hp = __pmHashWalk(&name_cache, PM_HASH_WALK_START);
    i = 0;
    while (hp != NULL) {
	for (cep = (cache_entry_t *)hp->data; cep != NULL; cep = cep->next) {
	    if (cep->state == N_UNKNOWN)
		namelist[i++] = cep->name;
	}
	hp = __pmHashWalk(&name_cache, PM_HASH_WALK_NEXT);
    }
    if (i != num_to_bind) {
	/* Snarfoo! */
	fprintf(stderr, "cache_bind(): setup %d metrics, expecting %d metrics\n", i, num_to_bind);
	goto cleanup;
    }

    if ((sts = pmLookupName(num_to_bind, (const char **)namelist, pmidlist)) < 0) {
	/*
	 * if there is a single metric in the config and it is a non-leaf
	 * (like in QA!), this can happen ...
	 */
	if (sts != PM_ERR_NONLEAF || num_to_bind > 1)
	    fprintf(stderr, "cache_bind(): pmLookupName(%d, ...): %s\n", num_to_bind, pmErrStr(sts));
	if (sts == PM_ERR_IPC) {
	    fprintf(stderr, "cache_bind(): Arrgh: lost connection to pmcd, giving up!\n");
	    exit(1);
	}
	goto cleanup;
    }
    if ((sts = pmLookupDescs(num_to_bind, pmidlist, desclist)) < 0) {
	fprintf(stderr, "cache_bind(): pmLookupDescs(%d, ...): %s\n", num_to_bind, pmErrStr(sts));
	if (sts == PM_ERR_IPC) {
	    fprintf(stderr, "cache_bind(): Arrgh: lost connection to pmcd, giving up!\n");
	    exit(1);
	}
	goto cleanup;
    }

    hp = __pmHashWalk(&name_cache, PM_HASH_WALK_START);
    i = 0;
    while (hp != NULL) {
	for (cep = (cache_entry_t *)hp->data; cep != NULL; cep = cep->next) {
	    if (cep->state == N_UNKNOWN) {
		cep->pmid = pmidlist[i]; 
		cep->desc = desclist[i];	/* struct assignment */
		if (cep->pmid != PM_ID_NULL)
		    cep->state = N_LEAF;
		i++;
	    }
	}
	hp = __pmHashWalk(&name_cache, PM_HASH_WALK_NEXT);
    }
    if (i != num_to_bind) {
	/* Snarfoo! */
	fprintf(stderr, "cache_bind(): metadata for %d metrics, expecting %d metrics\n", i, num_to_bind);
	goto cleanup;
    }

cleanup:
    if (namelist != NULL)
	free(namelist);
    if (pmidlist != NULL)
	free(pmidlist);
    if (desclist != NULL)
	free(desclist);
    return;
}

static void
cache_dump(FILE *f)
{
    __pmHashNode	*hp = __pmHashWalk(&name_cache, PM_HASH_WALK_START);
    cache_entry_t	*cep;
    int			i = 0;
    char		*sname[] = { "U", "T", "L" };

    while (hp != NULL) {
	fprintf(f, "cache list[%d]: ", i++);
	for (cep = (cache_entry_t *)hp->data; cep != NULL; cep = cep->next) {
	    if (cep != (cache_entry_t *)hp->data)
		fprintf(f, " -> ");
	    fprintf(f, "\"%s\"[%s] %s", cep->name, sname[cep->state], pmIDStr(cep->pmid));
	    if (cep->state == N_LEAF) {
		if (cep->desc.pmid == PM_ID_NULL)
		    fprintf(f, " [no pmDesc]");
		else
		    fprintf(f, " InDom:%s", pmInDomStr(cep->desc.indom));
	    }
	}
	fputc('\n', f);
	hp = __pmHashWalk(&name_cache, PM_HASH_WALK_NEXT);
    }
    fprintf(f, "%d unique names + %d duplicates (%d hash key synonyms)\n", num_add, num_dup, num_syn);
}

/*
 * Search name cache
 * return value of 1 => name is a valid leaf node in the PMNS
 * return pmid and/or desc if corresponding arg is != NULL
 */
int
cache_lookup(char *name, pmID *pmidp, pmDesc *dp)
{
    unsigned int	key = fold_string(name);
    __pmHashNode	*hp;
    cache_entry_t	*cep;

    if ((hp = __pmHashSearch(key, &name_cache)) != NULL) {
	for (cep = (cache_entry_t *)hp->data; cep != NULL; cep = cep->next) {
	    if (strcmp(cep->name, name) == 0) {
		if (cep->state != N_LEAF) {
		    /* in cache, but not a valid leaf node in the PMNS */
		    if (pmDebugOptions.appl6)
			fprintf(stderr, "cache_lookup(%s): in cache, but not a valid leaf node\n", name);
		    return 0;
		}
		if (cep->desc.pmid == PM_ID_NULL) {
		    /* in cache, a leaf node, but no pmDesc available */
		    if (pmDebugOptions.appl6)
			fprintf(stderr, "cache_lookup(%s): in cache, but no pmDesc\n", name);
		    return 0;
		}
		if (pmidp != NULL)
		    *pmidp = cep->pmid;
		if (dp != NULL)
		    *dp = cep->desc;	/* struct assignment */
		if (pmDebugOptions.appl6)
		    fprintf(stderr, "cache_lookup(%s): success\n", name);
		return 1;
	    }
	}
    }
    if (pmDebugOptions.appl6)
	fprintf(stderr, "cache_lookup(%s): not in cache\n", name);
    return 0;
}

/*
 * Walk the name cache and any metric "name" that still does not have
 * metadata (after pass0()) is a potential non-leaf name in the PMNS
 * ... so traverse and add anything you find to the name cache, then
 * try to fetch the metadata for these additional metrics
 */
static void
pass1(void)
{
    __pmHashNode	*hp;
    cache_entry_t	*cep;
    int			sts;

    /* first mark the ones we're "traversing" ... */
    hp = __pmHashWalk(&name_cache, PM_HASH_WALK_START);
    while (hp != NULL) {
	for (cep = (cache_entry_t *)hp->data; cep != NULL; cep = cep->next) {
	    if (cep->state == N_UNKNOWN)
		cep->state = N_TRAVERSE;
	}
	hp = __pmHashWalk(&name_cache, PM_HASH_WALK_NEXT);
    }

    hp = __pmHashWalk(&name_cache, PM_HASH_WALK_START);
    while (hp != NULL) {
	for (cep = (cache_entry_t *)hp->data; cep != NULL; cep = cep->next) {
	    if (cep->state == N_TRAVERSE) {
		/*
		 * failure is sort of expected here, especially if the
		 * name really is not in the PMNS ...
		 */
		sts = pmTraversePMNS(cep->name, cache_add);
		if (sts != PM_ERR_NAME && pmDebugOptions.appl6)
		    fprintf(stderr, "pass1: traverse(%s, ...): %s\n", cep->name, pmErrStr(sts));
		if (sts == PM_ERR_IPC) {
		    fprintf(stderr, "pass1(): Arrgh: lost connection to pmcd, giving up!\n");
		    exit(1);
		}
	    }
	}
	hp = __pmHashWalk(&name_cache, PM_HASH_WALK_NEXT);
	}

    cache_bind();

    /* and reset the state ... */
    hp = __pmHashWalk(&name_cache, PM_HASH_WALK_START);
    while (hp != NULL) {
	for (cep = (cache_entry_t *)hp->data; cep != NULL; cep = cep->next) {
	    if (cep->state == N_TRAVERSE)
		cep->state = N_UNKNOWN;
	}
	hp = __pmHashWalk(&name_cache, PM_HASH_WALK_NEXT);
    }
}

/*
 * Simple FSA lexical scanner for a pmlogger config file _after_ it has
 * been processed by pmcpp(1)
 *
 * State	Char		State
 * INIT		# 		INIT_EOL
 * INIT		{		METRICLIST
 * INIT		?		INIT
 * INIT_EOL	\n		INIT
 * INIT_EOL	?		INIT_EOL
 * METRICLIST	#		MLIST_EOL
 * METRICLIST	}		INIT
 * METRICLIST	[ \t]		METRICLIST
 * METRICLIST	alpha		NAME
 * METRICLIST	[		INST
 * MLIST_EOL	\n		METRICLIST
 * MLIST_EOL	?		MLIST_EOL
 * NAME		alpha|digit|_|.	NAME
 * NAME		[		INST
 * NAME		?		METRICLIST
 * INST		]		METRICLIST
 * INST		"		DQUOTE
 * DQUOTE	"		INST
 * DQUOTE	?		DQUOTE
 * INST		?		INST
 * INIT		EOF		DONE
 */

#define S_INIT	0
#define S_INIT_EOL	1
#define S_METRICLIST	2
#define S_MLIST_EOL	3
#define S_NAME		4
#define S_INST		5
#define S_DQUOTE	6

/*
 * If anything fatal happens, we do not return!
 */
FILE *
pass0(FILE *fpipe)
{
    int		state = S_INIT;
    int		c;
    int		linenum = 1;
    char	name[1024];
#if HAVE_MKSTEMP
    char	tmp[MAXPATHLEN];
    mode_t	mode;
#endif
    char	*tmpfname;
    char	*p;
    int		fd;
    FILE	*fp;			/* temp file, ready for next pass */

#if HAVE_MKSTEMP
    pmsprintf(tmp, sizeof(tmp), "%s%cpmlogger_configXXXXXX", pmGetConfig("PCP_TMPFILE_DIR"), pmPathSeparator());
    mode = umask(0177);
    fd = mkstemp(tmp);
    umask(mode);
    tmpfname = tmp;
#else
    if ((tmpfname = tmpnam(NULL)) != NULL)
	fd = open(tmpfname, O_RDWR|O_CREAT|O_EXCL, 0600);
#endif
    if (fd >= 0)
	fp = fdopen(fd, "w+");
    if (fd < 0 || fp == NULL) {
	fprintf(stderr, "\nError: failed create temporary config file (%s)\n", tmpfname);
	fprintf(stderr, "Reason? %s\n", osstrerror());
	(void)unlink(tmpfname);
	exit(1);
    }

    if (pmDebugOptions.appl7)
	no_cache = 1;

    while ((c = fgetc(fpipe)) != EOF) {
	fputc(c, fp);
	if (no_cache)
	    continue;		/* do nothing here */
	if (c == '\n')
	    linenum++;
	switch (state) {
	    case S_INIT:
	    	if (c == '#')
		    state = S_INIT_EOL;
		else if (c == '{')
		    state = S_METRICLIST;
		break;
	    case S_INIT_EOL:
	    	if (c == '\n')
		    state = S_INIT;
		break;
	    case S_METRICLIST:
	    	if (c == '#')
		    state = S_MLIST_EOL;
		else if (c == '}')
		    state = S_INIT;
		else if (isalpha(c)) {
		    state = S_NAME;
		    p = name;
		    *p++ = c;
		}
		else if (c == '[')
		    state = S_INST;
		break;
	    case S_MLIST_EOL:
	    	if (c == '\n')
		    state = S_METRICLIST;
		break;
	    case S_NAME:
		if (isalpha(c) || isdigit(c) || c == '_' || c == '.')
		    *p++ = c;
		else {
		    *p = '\0';
		    if (pmDebugOptions.appl6)
			fprintf(stderr, "pass0:%d name=\"%s\"\n", linenum, name);
		    cache_add(name);
		    if (c == '[')
			state = S_INST;
		    else
			state = S_METRICLIST;
		}
		break;
	    case S_INST:
		if (c == ']')
		    state = S_METRICLIST;
		else if (c == '"')
		    state = S_DQUOTE;
		break;
	    case S_DQUOTE:
		if (c == '"')
		    state = S_INST;
		break;
	    default:
		fprintf(stderr, "pass0: botch state=%d at line %d\n", state, linenum);
		exit(1);
	}
    }

    fflush(fp);
    rewind(fp);
    (void)unlink(tmpfname);

    /*
     * Now fetch the metadata ...
     */
    cache_bind();

    /*
     * Now deal with the non-leaf nodes in the PMNS
     */
    pass1();

    if (pmDebugOptions.appl6)
	cache_dump(stderr);

    return fp;
}
