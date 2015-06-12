/*
 * Copyright (c) 2013,2015 Red Hat.
 * Copyright (c) 2005 Silicon Graphics, Inc.  All Rights Reserved.
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
 */

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>

static __uint32_t hash(const signed char *, int, __uint32_t);

/*
 * simple linked list for each cache at this stage
 */
typedef struct entry {
    struct entry	*next;		/* in inst identifier order */
    struct entry	*h_inst;	/* inst hash chain */
    struct entry	*h_name;	/* name hash chain */
    int			inst;
    char		*name;
    int			hashlen;	/* smaller of strlen(name) and chars to first space */
    int			keylen;		/* > 0 if have key from pmdaCacheStoreKey() */
    void		*key;		/* != NULL if have key from pmdaCacheStoreKey() */
    int			state;
    void		*private;
    time_t		stamp;
} entry_t;

#define CACHE_VERSION1	1
#define CACHE_VERSION2	2
#define CACHE_VERSION	CACHE_VERSION2	/* version of external file format */
#define MAX_HASH_TRY	10

/*
 * linked list of cache headers
 */
typedef struct hdr {
    struct hdr		*next;		/* linked list of indoms */
    entry_t		*first;		/* in inst order */
    entry_t		*last;		/* in inst order */
    entry_t		*save;		/* used in cache_walk() */
    entry_t		**ctl_inst;	/* hash by inst chains */
    entry_t		**ctl_name;	/* hash by name chains */
    pmInDom		indom;
    int			hsize;
    int			hbits;
    int			nentry;		/* number of entries */
    int			ins_mode;	/* see insert_cache() */
    int			hstate;		/* dirty/clean/string state */
    int			keyhash_cnt[MAX_HASH_TRY];
    int			maxinst;	/* maximum inst */
} hdr_t;

#define DEFAULT_MAXINST 0x7fffffff

/* bitfields for hstate */
#define DIRTY_INSTANCE	0x1
#define DIRTY_STAMP	0x2
#define CACHE_STRINGS	0x4

static hdr_t	*base;		/* start of cache headers */
static char 	filename[MAXPATHLEN];
				/* for load/save ops */
static char	*vdp;		/* first trip mkdir for load/save */

/*
 * Count character to end of string or first space, whichever comes
 * first.  In the special case of string caches, spaces are allowed.
 */
static int
get_hashlen(hdr_t *h, const char *str)
{
    const char	*q = str;

    while (*q && (*q != ' ' || (h->hstate & CACHE_STRINGS) != 0))
	q++;
    return (int)(q-str);
}

static unsigned int
hash_str(const signed char *str, int len)
{
    return hash(str, len, 0);
}

static void
KeyStr(FILE *f, int keylen, const char *key)
{
    int		i;
    if (keylen > 0) {
	fprintf(f, "[key=0x");
	for (i = 0; i < keylen; i++, key++)
	    fprintf(f, "%02x", (*key & 0xff));
	fputc(']', f);
    }
    else
	fprintf(f, "[no key]");
}

/*
 * The magic "match up to a space" instance name matching ...
 *
 * e->name	e->hashlen	name		hashlen		result
 * foo...	>3		foo		3		0
 * foo		3		foo...		>3		0
 * foo		3		foo		3		1
 * foo bar	3		foo		3		1
 * foo bar	3		foo bar		3		1
 * foo		3		foo bar		3		-1 bad
 * foo blah	3		foo bar		3		-1 bad
 *
 */
static int
name_eq(entry_t *e, const char *name, int hashlen)
{
    if (e->hashlen != hashlen)
	return 0;
    if (strncmp(e->name, name, hashlen) != 0)
	return 0;
    if (name[hashlen] == '\0')
	return 1;
    if (e->name[hashlen] == '\0')
	return -1;
    if (strcmp(&e->name[hashlen+1], &name[hashlen+1]) == 0)
	return 1;
    return -1;
}

static int
key_eq(entry_t *e, int keylen, const char *key)
{
    const char	*ekp;
    const char	*kp;
    int		i;

    if (e->keylen != keylen)
	return 0;

    ekp = (const char *)e->key;
    kp = (const char *)key;
    for (i = 0; i < keylen; i++) {
	if (*ekp != *kp)
	    return 0;
	ekp++;
	kp++;
    }

    return 1;
}

static hdr_t *
find_cache(pmInDom indom, int *sts)
{
    hdr_t	*h;
    int		i;

    for (h = base; h != NULL; h = h->next) {
	if (h->indom == indom)
	    return h;
    }

    if ((h = (hdr_t *)malloc(sizeof(hdr_t))) == NULL) {
	char	strbuf[20];
	__pmNotifyErr(LOG_ERR, 
	     "find_cache: indom %s: unable to allocate memory for hdr_t",
	     pmInDomStr_r(indom, strbuf, sizeof(strbuf)));
	*sts = PM_ERR_GENERIC;
	return NULL;
    }
    h->next = base;
    base = h;
    h->first = NULL;
    h->last = NULL;
    h->hsize = 16;
    h->hbits = 0xf;
    h->ctl_inst = (entry_t **)calloc(h->hsize, sizeof(entry_t *));
    h->ctl_name = (entry_t **)calloc(h->hsize, sizeof(entry_t *));
    h->indom = indom;
    h->nentry = 0;
    h->ins_mode = 0;
    h->hstate = 0;
    for (i = 0; i < MAX_HASH_TRY; i++)
	h->keyhash_cnt[i] = 0;
    h->maxinst = DEFAULT_MAXINST;
    return h;
}

/*
 * Traverse the cache in ascending inst order
 */
static entry_t *
walk_cache(hdr_t *h, int op)
{
    entry_t	*e;

    if (op == PMDA_CACHE_WALK_REWIND) {
	h->save = h->first;
	return NULL;
    }
    e = h->save;
    if (e != NULL)
	h->save = e->next;
    return e;
}

/*
 * inst_or_name is 0 for inst hash list, 1 for name hash list
 */
static void
dump_hash_list(FILE *fp, hdr_t *h, int inst_or_name, int i)
{
    entry_t	*e;

    fprintf(fp, " [%03d]", i);
    if (inst_or_name == 1) {
	for (e = h->ctl_name[i]; e != NULL; e = e->h_name) {
	    fprintf(fp, " -> %d", e->inst);
	    if (e->state == PMDA_CACHE_EMPTY)
		fputc('E', fp);
	    else if (e->state == PMDA_CACHE_INACTIVE)
		fputc('I', fp);
	}
	fputc('\n', fp);
    }
    else {
	for (e = h->ctl_inst[i]; e != NULL; e = e->h_inst) {
	    fprintf(fp, " -> %d", e->inst);
	    if (e->state == PMDA_CACHE_EMPTY)
		fputc('E', fp);
	    else if (e->state == PMDA_CACHE_INACTIVE)
		fputc('I', fp);
	}
	fputc('\n', fp);
    }
}

static void
dump(FILE *fp, hdr_t *h, int do_hash)
{
    entry_t	*e;
    char	strbuf[20];
    int		i;

    fprintf(fp, "pmdaCacheDump: indom %s: nentry=%d ins_mode=%d hstate=%d hsize=%d\n",
	pmInDomStr_r(h->indom, strbuf, sizeof(strbuf)), h->nentry, h->ins_mode, h->hstate, h->hsize);
    for (e = h->first; e != NULL; e = e->next) {
	if (e->state == PMDA_CACHE_EMPTY) {
	    fprintf(fp, "(%10d) %8s\n", e->inst, "empty");
	}
	else {
	    fprintf(fp, " %10d  %8s " PRINTF_P_PFX "%p %s",
		e->inst, e->state == PMDA_CACHE_ACTIVE ? "active" : "inactive",
		e->private, e->name);
	    if (strlen(e->name) > e->hashlen)
		fprintf(fp, " [match len=%d]", e->hashlen);
	    if (e->keylen > 0) {
		fputc(' ', fp);
		KeyStr(fp, e->keylen, (const char *)e->key);
	    }
	    fputc('\n', fp);
	}
    }

    if (do_hash == 0)
	return;

    for (i = 0; i < MAX_HASH_TRY; i++) {
	if (h->keyhash_cnt[i])
	    break;
    }

    if (i < MAX_HASH_TRY) {
	fprintf(fp, "pmdaCacheStoreKey hash stats ...\n");
	for (i = 0; i < MAX_HASH_TRY; i++) {
	    if (h->keyhash_cnt[i] != 0) {
		if (i == 0)
		    fprintf(fp, "hash once: %d times\n", h->keyhash_cnt[i]);
		else
		    fprintf(fp, "%d hash attempts: %d times\n", i+1, h->keyhash_cnt[i]);
	    }
	}
    }

    if (h->ctl_inst != NULL) {
	int	i;
	fprintf(fp, "inst hash\n");
	for (i = 0; i < h->hsize; i++) {
	    dump_hash_list(fp, h, 0, i);
	}
    }
    if (h->ctl_name != NULL) {
	int	i;
	fprintf(fp, "name hash\n");
	for (i = 0; i < h->hsize; i++) {
	    dump_hash_list(fp, h, 1, i);
	}
    }
}

static entry_t *
find_name(hdr_t *h, const char *name, int *sts)
{
    entry_t	*e;
    int		hashlen = get_hashlen(h, name);

    *sts = 0;
    for (e = h->first; e != NULL; e = e->next) {
	if (e->state != PMDA_CACHE_EMPTY) {
	    if ((*sts = name_eq(e, name, hashlen)))
		break;
	}
    }
    return e;
}

static entry_t *
find_inst(hdr_t *h, int inst)
{
    entry_t	*e;

    for (e = h->first; e != NULL; e = e->next) {
	if (e->inst == inst && e->state != PMDA_CACHE_EMPTY)
	    break;
    }
    return e;
}

/*
 * supports find by instance identifier (name == NULL) else
 * find by instance name
 */
static entry_t *
find_entry(hdr_t *h, const char *name, int inst, int *sts)
{
    entry_t	*e;

    *sts = 0;
    if (name == NULL) {
	/*
	 * search by instance identifier (inst)
	 */
	if (h->ctl_inst == NULL)
	    /* no hash, use linear search */
	    return find_inst(h, inst);
	for (e = h->ctl_inst[inst & h->hbits]; e != NULL; e = e->h_inst) {
	    if (e->inst == inst && e->state != PMDA_CACHE_EMPTY)
		return e;
	}
    }
    else {
	/*
	 * search by instance name
	 */
	int	hashlen = get_hashlen(h, name);

	if (h->ctl_name == NULL)
	    /* no hash, use linear search */
	    return find_name(h, name, sts);
	for (e = h->ctl_name[hash_str((const signed char *)name, hashlen) & h->hbits]; e != NULL; e = e->h_name) {
	    if (e->state != PMDA_CACHE_EMPTY) {
		if ((*sts = name_eq(e, name, hashlen)))
		    return e;
	    }
	}
    }

    return NULL;
}

/*
 * optionally resize the hash table first (if resize == 1)
 *
 * then re-order each hash chain so that active entires are
 * before inactive entries, and culled entries dropped
 *
 * applies to _both_ the inst and name hashes
 */
static void
redo_hash(hdr_t *h, int resize)
{
    entry_t	*e;
    entry_t	*last_e = NULL;
    entry_t	*t;
    int		i;
    entry_t	*last_active;
    entry_t	*inactive;
    entry_t	*last_inactive;

    if (resize) {
	entry_t		**old_inst;
	entry_t		**old_name;
	int		oldsize;
	int		oldi;

	old_inst = h->ctl_inst;
	old_name = h->ctl_name;
	oldsize = h->hsize;
	h->hsize <<= 1;
	h->ctl_inst = (entry_t **)calloc(h->hsize, sizeof(entry_t *));
	if (h->ctl_inst == NULL) {
	    h->ctl_inst = old_inst;
	    h->hsize = oldsize;
	    goto reorder;
	}
	h->ctl_name = (entry_t **)calloc(h->hsize, sizeof(entry_t *));
	if (h->ctl_name == NULL) {
	    free(h->ctl_inst);
	    h->ctl_inst = old_inst;
	    h->ctl_name = old_name;
	    h->hsize = oldsize;
	    goto reorder;
	}
	h->hbits = (h->hbits << 1) | 1;
	for (oldi = 0; oldi < oldsize; oldi++) {
	    for (e = old_inst[oldi]; e != NULL; ) {
		t = e;
		e = e->h_inst;
		i = t->inst & h->hbits;
		t->h_inst = h->ctl_inst[i];
		h->ctl_inst[i] = t;
	    }
	}
	for (oldi = 0; oldi < oldsize; oldi++) {
	    for (e = old_name[oldi]; e != NULL; ) {
		t = e;
		e = e->h_name;
		i = hash_str((const signed char *)t->name, t->hashlen) & h->hbits;
		t->h_name = h->ctl_name[i];
		h->ctl_name[i] = t;
	    }
	}
	free(old_inst);
	free(old_name);
    }
reorder:

    /*
     * first the inst hash list, moving active entries before inactive ones,
     * and unlinking any empty ones
     */ 
    for (i = 0; i < h->hsize; i++) {
	last_active = NULL;
	inactive = NULL;
	last_inactive = NULL;
	e = h->ctl_inst[i];
	h->ctl_inst[i] = NULL;
	while (e != NULL) {
	    t = e;
	    e = e->h_inst;
	    t->h_inst = NULL;
	    if (t->state == PMDA_CACHE_ACTIVE) {
		if (last_active == NULL)
		    h->ctl_inst[i] = t;
		else
		    last_active->h_inst = t;
		last_active = t;
	    }
	    else if (t->state == PMDA_CACHE_INACTIVE) {
		if (last_inactive == NULL)
		    inactive = t;
		else
		    last_inactive->h_inst = t;
		last_inactive = t;
	    }
	}
	if (last_active == NULL)
	    h->ctl_inst[i] = inactive;
	else
	    last_active->h_inst = inactive;
    }

    /*
     * and now the name hash list, doing the same thing
     */
    for (i = 0; i < h->hsize; i++) {
	last_active = NULL;
	inactive = NULL;
	last_inactive = NULL;
	e = h->ctl_name[i];
	h->ctl_name[i] = NULL;
	while (e != NULL) {
	    t = e;
	    e = e->h_name;
	    t->h_name = NULL;
	    if (t->state == PMDA_CACHE_ACTIVE) {
		if (last_active == NULL)
		    h->ctl_name[i] = t;
		else
		    last_active->h_name = t;
		last_active = t;
	    }
	    else if (t->state == PMDA_CACHE_INACTIVE) {
		if (last_inactive == NULL)
		    inactive = t;
		else
		    last_inactive->h_name = t;
		last_inactive = t;
	    }
	}
	if (last_active == NULL)
	    h->ctl_name[i] = inactive;
	else
	    last_active->h_name = inactive;
    }

    /*
     * now walk the instance list, removing any culled entries and
     * rebuilding the linked list
     */
    e = h->first;
    while (e != NULL) {
	t = e;
	e = e->next;
	if (t->state == PMDA_CACHE_EMPTY) {
	    if (last_e == NULL)
		h->first = e;
	    else
		last_e->next = e;
	    if (t->name)
		free(t->name);
	    free(t);
	}
	else
	    last_e = t;
    }

}

/*
 * We need to keep the instances in ascending inst order.
 * If inst _is_ PM_IN_NULL, then we need to choose a value ...
 * The default mode is appending to use the last value+1 (this is
 * ins_mode == 0).  If we wrap the instance identifier range, or
 * PMDA_CACHE_REUSE has been used, then ins_mode == 1 and we walk
 * the list starting from the beginning, looking for the first 
 * unused inst value.
 *
 * If inst is _not_ PM_IN_NULL, we're being called from load_cache
 * or pmdaCacheStoreKey() and the inst is known ... so we need to
 * check for possible duplicate entries.
 */
static entry_t *
insert_cache(hdr_t *h, const char *name, int inst, int *sts)
{
    entry_t	*e;
    entry_t	*last_e = NULL;
    char	*dup;
    int		i;
    int		hashlen = get_hashlen(h, name);

    *sts = 0;

    if (inst != PM_IN_NULL) {
	/*
	 * Check if instance id or instance name already in cache
	 * ... if id and name are the the same, keep the existing one
	 * and ignore the new one (in particular state is not reset to
	 * inactive).
	 * If one matches but the other is different, keep the
	 * matching entry, but return an error as a warning.
	 * If both fail to match, we're OK to insert the new entry,
	 * although we need to run down the list quickly to find the
	 * correct place to insert the new one.
	 */
	e = find_entry(h, NULL, inst, sts);
	if (e != NULL) {
	    if (name_eq(e, name, hashlen) != 1) {
		/* instance id the same, different name */
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_INDOM) {
		    fprintf(stderr, "pmdaCache: store: indom %s: instance %d ", pmInDomStr(h->indom), e->inst);
		    fprintf(stderr, " in cache, name \"%s\" does not match new entry \"%s\"\n", e->name, name);
		}
#endif
		*sts = PM_ERR_INST;
	    }
	    return e;
	}
	e = find_entry(h, name, PM_IN_NULL, sts);
	if (e != NULL) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_INDOM) {
		fprintf(stderr, "pmdaCacheStoreKey: indom %s: instance \"%s\"", pmInDomStr(h->indom), e->name);
		fprintf(stderr, " in cache, id %d does not match new entry %d\n", e->inst, inst);
	    }
#endif
	    *sts = PM_ERR_INST;
	    return e;
	}
	for (e = h->first; e != NULL; e = e->next) {
	    if (e->inst < inst)
		last_e = e;
	    else if (e->inst > inst)
		break;
	}
    }

    if ((dup = strdup(name)) == NULL) {
	char	strbuf[20];
	__pmNotifyErr(LOG_ERR, 
	     "insert_cache: indom %s: unable to allocate %d bytes for name: %s\n",
	     pmInDomStr_r(h->indom, strbuf, sizeof(strbuf)), (int)strlen(name), name);
	*sts = PM_ERR_GENERIC;
	return NULL;
    }

    if (inst == PM_IN_NULL) {
	if (h->ins_mode == 0) {
	    last_e = h->last;
	    if (last_e == NULL)
		inst = 0;
	    else {
		if (last_e->inst == h->maxinst) {
		    /*
		     * overflowed inst identifier, need to shift to 
		     * ins_mode == 1
		     */
		    h->ins_mode = 1;
		    last_e = NULL;
		    goto retry;
		}
		inst = last_e->inst+1;
	    }
	}
	else {
retry:
	    inst = 0;
	    for (e = h->first; e != NULL; e = e->next) {
		if (inst < e->inst)
		    break;
		if (inst == h->maxinst) {
		    /*
		     * 2^32-1 is the maximum number of instances we can have
		     */
		    char	strbuf[20];
		    __pmNotifyErr(LOG_ERR, 
			 "insert_cache: indom %s: too many instances",
			 pmInDomStr_r(h->indom, strbuf, sizeof(strbuf)));
		    *sts = PM_ERR_GENERIC;
		    free(dup);
		    return NULL;
		}
		inst++;
		last_e = e;
	    }
	}
    }

    if ((e = (entry_t *)malloc(sizeof(entry_t))) == NULL) {
	char	strbuf[20];
	__pmNotifyErr(LOG_ERR, 
	     "insert_cache: indom %s: unable to allocate memory for entry_t",
	     pmInDomStr_r(h->indom, strbuf, sizeof(strbuf)));
	*sts = PM_ERR_GENERIC;
	free(dup);
	return NULL;
    }

    if (last_e == NULL) {
	/* head of list */
	e->next = h->first;
	h->first = e;
    }
    else {
	/* middle of list */
	e->next = last_e->next;
	last_e->next = e;
    }
    e->inst = inst;
    e->name = dup;
    e->hashlen = get_hashlen(h, dup);
    e->key = NULL;
    e->state = PMDA_CACHE_INACTIVE;
    e->private = NULL;
    e->stamp = 0;
    if (h->last == NULL || h->last->inst < inst)
	h->last = e;
    h->nentry++;

    if (h->hsize > 0 && h->hsize < 1024 && h->nentry > 4 * h->hsize)
	redo_hash(h, 1);

    /* link into the inst hash list, if any */
    if (h->ctl_inst != NULL) {
	i = inst & h->hbits;
	e->h_inst = h->ctl_inst[i];
	h->ctl_inst[i] = e;
    }
    else
	e->h_inst = NULL;

    /* link into the name hash list, if any */
    if (h->ctl_name != NULL) {
	i = hash_str((const signed char *)e->name, e->hashlen) & h->hbits;
	e->h_name = h->ctl_name[i];
	h->ctl_name[i] = e;
    }
    else
	e->h_name = NULL;

    return e;
}

static int
load_cache(hdr_t *h)
{
    FILE	*fp;
    entry_t	*e;
    int		cnt;
    int		x;
    int		inst;
    int		keylen = 0;
    void	*key = NULL;
    int		s;
    char	buf[1024];	/* input line buffer, is this big enough? */
    char	*p;
    int		sts;
    int		sep = __pmPathSeparator();
    char	strbuf[20];

    if (vdp == NULL) {
	if ((vdp = pmGetOptionalConfig("PCP_VAR_DIR")) == NULL)
	    return PM_ERR_GENERIC;
	snprintf(filename, sizeof(filename),
		"%s%c" "config" "%c" "pmda", vdp, sep, sep);
	mkdir2(filename, 0755);
    }

    snprintf(filename, sizeof(filename), "%s%cconfig%cpmda%c%s",
		vdp, sep, sep, sep, pmInDomStr_r(h->indom, strbuf, sizeof(strbuf)));
    if ((fp = fopen(filename, "r")) == NULL)
	return -oserror();
    if (fgets(buf, sizeof(buf), fp) == NULL) {
	__pmNotifyErr(LOG_ERR, 
	     "pmdaCacheOp: %s: empty file?", filename);
	fclose(fp);
	return PM_ERR_GENERIC;
    }
    /* First grab the file version. */
    s = sscanf(buf, "%d ", &x);
    if (s != 1 || x <= 0 || x > CACHE_VERSION) {
	__pmNotifyErr(LOG_ERR, 
	     "pmdaCacheOp: %s: illegal cache header record: %s",
	     filename, buf);
	fclose(fp);
	return PM_ERR_GENERIC;
    }	

    /* Based on the file version, grab the entire line. */
    switch (x) {
	case CACHE_VERSION1:
	    h->maxinst = DEFAULT_MAXINST;
	    s = sscanf(buf, "%d %d", &x, &h->ins_mode);
	    if (s != 2)
		s = 0;
	    break;
	default:
	    s = sscanf(buf, "%d %d %d", &x, &h->ins_mode, &h->maxinst);
	    if (s != 3)
		s = 0;
	    break;
    }
    if (s == 0 || h->ins_mode < 0 || h->ins_mode > 1 || h->maxinst < 0
	|| h->maxinst > DEFAULT_MAXINST) {
	__pmNotifyErr(LOG_ERR, 
	     "pmdaCacheOp: %s: illegal cache header record: %s",
	     filename, buf);
	fclose(fp);
	return PM_ERR_GENERIC;
    }

    for (cnt = 0; ; cnt++) {
	if (fgets(buf, sizeof(buf), fp) == NULL)
	    break;
	if ((p = strchr(buf, '\n')) != NULL)
	    *p = '\0';
	p = buf;
	while (*p && isascii((int)*p) && isspace((int)*p))
	    p++;
	if (*p == '\0') goto bad;
	inst = 0;
	while (*p && isascii((int)*p) && isdigit((int)*p)) {
	    inst = inst*10 + (*p-'0');
	    p++;
	}
	while (*p && isascii((int)*p) && isspace((int)*p))
	    p++;
	if (inst < 0 || *p == '\0') goto bad;
	x = 0;
	while (*p && isascii((int)*p) && isdigit((int)*p)) {
	    x = x*10 + (*p-'0');
	    p++;
	}
	while (*p && isascii((int)*p) && isspace((int)*p))
	    p++;
	if (*p == '[') {
	    char	*pend;
	    char	*q;
	    int		i;
	    int		tmp;
	    p++;
	    pend = p;
	    while (*pend && *pend != ']')
		pend++;
	    if (*pend != ']')
		goto bad;
	    /*
	     * convert key in place ...
	     */
	    keylen = (pend - p) / 2;
	    if ((key = malloc(keylen)) == NULL) {
		__pmNotifyErr(LOG_ERR, 
		     "load_cache: indom %s: unable to allocate memory for keylen=%d",
		     pmInDomStr(h->indom), keylen);
		fclose(fp);
		return PM_ERR_GENERIC;
	    }
	    q = key;
	    for (i = 0; i < keylen; i++) {
		sscanf(p, "%2x", &tmp);
		*q++ = (tmp & 0xff);
		p += 2;
	    }
	    p += 2;
	    while (*p && isascii((int)*p) && isspace((int)*p))
		p++;
	}
	else {
	    keylen = 0;
	    key = NULL;
	}
	if (*p == '\0') {
bad:
	    __pmNotifyErr(LOG_ERR, 
		 "pmdaCacheOp: %s: illegal record: %s",
		 filename, buf);
	    if (key) free(key);
	    fclose(fp);
	    return PM_ERR_GENERIC;
	}
	e = insert_cache(h, p, inst, &sts);
	if (e == NULL) {
	    if (key) free(key);
	    fclose(fp);
	    return sts;
	}
	if (sts != 0) {
	    __pmNotifyErr(LOG_WARNING,
		"pmdaCacheOp: %s: loading instance %d (\"%s\") ignored, already in cache as %d (\"%s\")",
		filename, inst, p, e->inst, e->name);
	}
	e->keylen = keylen;
	e->key = key;
	e->stamp = x;
    }
    fclose(fp);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_INDOM) {
	fprintf(stderr, "After PMDA_CACHE_LOAD\n");
	dump(stderr, h, 0);
    }
#endif

    return cnt;
}

static int
save_cache(hdr_t *h, int hstate)
{
    FILE	*fp;
    entry_t	*e;
    int		cnt;
    time_t	now;
    int		sep = __pmPathSeparator();
    char	strbuf[20];

    if ((h->hstate & hstate) == 0 || (h->hstate & CACHE_STRINGS) != 0) {
	/* nothing to be done */
	return 0;
    }

    if (vdp == NULL) {
	if ((vdp = pmGetOptionalConfig("PCP_VAR_DIR")) == NULL)
	    return PM_ERR_GENERIC;
	snprintf(filename, sizeof(filename),
		"%s%c" "config" "%c" "pmda", vdp, sep, sep);
	mkdir2(filename, 0755);
    }

    snprintf(filename, sizeof(filename), "%s%cconfig%cpmda%c%s",
		vdp, sep, sep, sep, pmInDomStr_r(h->indom, strbuf, sizeof(strbuf)));
    if ((fp = fopen(filename, "w")) == NULL)
	return -oserror();
    fprintf(fp, "%d %d %d\n", CACHE_VERSION, h->ins_mode, h->maxinst);

    now = time(NULL);
    cnt = 0;
    for (e = h->first; e != NULL; e = e->next) {
	if (e->state == PMDA_CACHE_EMPTY)
	    continue;
	if (e->stamp == 0)
	    e->stamp = now;
	fprintf(fp, "%d %d", e->inst, (int)e->stamp);
	if (e->keylen > 0) {
	    char	*p = (char *)e->key;
	    int		i;
	    fprintf(fp, " [");
	    for (i = 0; i < e->keylen; i++, p++)
		fprintf(fp, "%02x", (*p & 0xff));
	    fputc(']', fp);
	}
	fprintf(fp, " %s\n", e->name);
	cnt++;
    }
    fclose(fp);
    h->hstate = 0;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_INDOM) {
	fprintf(stderr, "After cache_save hstate={");
	if (hstate & DIRTY_INSTANCE) fprintf(stderr, "DIRTY_INSTANCE");
	if (hstate & DIRTY_STAMP) fprintf(stderr, "DIRTY_STAMP");
	fprintf(stderr, "}\n");
	dump(stderr, h, 0);
    }
#endif

    return cnt;
}

void
__pmdaCacheDumpAll(FILE *fp, int do_hash)
{
    hdr_t	*h;

    for (h = base; h != NULL; h = h->next) {
	dump(fp, h, do_hash);
    }
}

void
__pmdaCacheDump(FILE *fp, pmInDom indom, int do_hash)
{
    hdr_t	*h;

    for (h = base; h != NULL; h = h->next) {
	if (h->indom == indom) {
	    dump(fp, h, do_hash);
	}
    }
}

static int
store(pmInDom indom, int flags, const char *name, pmInDom inst, int keylen, const char *key, void *private)
{
    hdr_t	*h;
    entry_t	*e;
    int		sts;

    if ((h = find_cache(indom, &sts)) == NULL)
	return sts;

    if ((e = find_entry(h, name, inst, &sts)) == NULL) {

	if (flags != PMDA_CACHE_ADD) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_INDOM) {
		fprintf(stderr, "pmdaCache store: indom %s: instance \"%s\"", pmInDomStr(indom), name);
		if (inst != PM_IN_NULL)
		    fprintf(stderr, " (%d)", inst);
		fprintf(stderr, " not in cache: flags=%d not allowed\n", flags);
	    }
#endif
	    return PM_ERR_INST;
	}

	if ((e = insert_cache(h, name, inst, &sts)) == NULL)
	    return sts;
	h->hstate |= DIRTY_INSTANCE;	/* added a new entry */
    }
    else {
	if (sts == -1)
	    /*
	     * name contains space, match up to space (short name) but
	     * mismatch after that ... cannot store name in the cache,
	     * the PMDA would have to cull the entry that matches on
	     * the short name first
	     */
	    return -EINVAL;
    }

    switch (flags) {
	case PMDA_CACHE_ADD:
	    e->keylen = keylen;
	    if (keylen > 0) {
		if ((e->key = malloc(keylen)) == NULL) {
		    __pmNotifyErr(LOG_ERR, 
			 "store: indom %s: unable to allocate memory for keylen=%d",
			 pmInDomStr(indom), keylen);
		    return PM_ERR_GENERIC;
		}
		memcpy(e->key, key, keylen);
	    }
	    else
		e->key = NULL;
	    e->state = PMDA_CACHE_ACTIVE;
	    e->private = private;
	    e->stamp = 0;		/* flag, updated at next cache_save() */
	    h->hstate |= DIRTY_STAMP;	/* timestamp needs updating */
	    break;

	case PMDA_CACHE_HIDE:
	    e->state = PMDA_CACHE_INACTIVE;
	    break;

	case PMDA_CACHE_CULL:
	    e->state = PMDA_CACHE_EMPTY;
	    /*
	     * we don't clean anything up, which may be a problem in the
	     * presence of lots of culling ... see redo_hash() for how
	     * the culled entries can be reclaimed
	     */
	    h->hstate |= DIRTY_INSTANCE;	/* entry will not be saved */
	    break;

	default:
	    return -EINVAL;
    }

    return e->inst;
}

int
pmdaCacheStore(pmInDom indom, int flags, const char *name, void *private)
{
    if (indom == PM_INDOM_NULL)
	return PM_ERR_INDOM;

    return store(indom, flags, name, PM_IN_NULL, 0, NULL, private);
}

/*
 * Generate a new 31-bit (positive) instance number from a key provided
 * as a ``hint'' via key[] (first keylen bytes) or name[] if keylen < 1
 * or key == NULL ... useful for compressing natural 64-bit or larger
 * instance identifiers into the 31-bits required for the PCP APIs
 * and PDUs.
 */
int
pmdaCacheStoreKey(pmInDom indom, int flags, const char *name, int keylen, const void *key, void *private)
{
    int		inst;
    int		sts;
    int		i;
    __uint32_t	try = 0;
    hdr_t	*h;
    entry_t	*e;
    const char	*mykey;
    int		mykeylen;

    if (indom == PM_INDOM_NULL)
	return PM_ERR_INDOM;

    if (flags != PMDA_CACHE_ADD)
	return store(indom, flags, name, PM_IN_NULL, 0, NULL, private);

    /*
     * This is the PMDA_CACHE_ADD case, so need to find an instance id
     */
    if ((h = find_cache(indom, &sts)) == NULL)
	return sts;

    if (keylen < 1 || key == NULL) {
	/* use name[] instead of keybuf[] */
	mykey = (const char *)name;
	mykeylen = strlen(name);
    }
    else {
	mykey = key;
	mykeylen = keylen;
    }

    if ((e = find_entry(h, name, PM_IN_NULL, &sts)) != NULL) {
	/*
	 * cache entry already exists for this name ...
	 * if keys are not equal => failure
	 */
	if (key_eq(e, mykeylen, mykey) == 0) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_INDOM) {
		fprintf(stderr, "pmdaCacheStoreKey: indom %s: instance \"%s\" (%d) in cache ", pmInDomStr(indom), e->name, e->inst);
		KeyStr(stderr, e->keylen, (const char *)e->key);
		fprintf(stderr, " does not match new entry ");
		KeyStr(stderr, mykeylen, mykey);
		fputc('\n', stderr);
	    }
#endif
	    return PM_ERR_INST;
	}
	/* keys the same, use inst from existing entry */
	inst = e->inst;
    }
    else {
	/* we're in the inst guessing game ... */
	for (i = 0; i < MAX_HASH_TRY; i++) {
	    try = hash((const signed char *)mykey, mykeylen, try);
	    /* strip top bit ... instance id must be positive */
	    inst = try & ~(1 << (8*sizeof(__uint32_t)-1));
	    e = find_entry(h, NULL, inst, &sts);
	    if (e == NULL) {
		h->keyhash_cnt[i]++;
		break;
	    }
	    /*
	     * Found matching entry using the guessed inst ...
	     *
	     * If the key[]s are the same and the name[]s are the same
	     * then the matching entry is already in the cache, so use
	     * this instance identifier.
	     *
	     * If the key[]s match, but the name[]s are different, this
	     * is an error (duplicate instance name).
	     *
	     * If the names[] match, but the key[]s are different, this
	     * is an error (duplicate key).
	     *
	     * Otherwise instance id is in use for another instance, so
	     * keep trying by rehashing.
	     */
	    if (strcmp(e->name, name) == 0) {
		if (key_eq(e, mykeylen, mykey) == 1)
		    break;
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_INDOM) {
		    fprintf(stderr, "pmdaCacheStoreKey: indom %s: instance \"%s\" (%d) in cache, ", pmInDomStr(indom), e->name, e->inst);
		    KeyStr(stderr, e->keylen, (const char *)e->key);
		    fprintf(stderr, " does not match new entry ");
		    KeyStr(stderr, mykeylen, mykey);
		    fputc('\n', stderr);
		}
#endif
		return PM_ERR_INST;
	    }
	    else if (key_eq(e, mykeylen, mykey) == 1) {
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_INDOM) {
		    fprintf(stderr, "pmdaCacheStoreKey: indom %s: instance %d ", pmInDomStr(indom), e->inst);
		    KeyStr(stderr, e->keylen, (const char *)e->key);
		    fprintf(stderr, " in cache, name \"%s\" does not match new entry \"%s\"\n", e->name, name);
		}
#endif
		return PM_ERR_INST;
	    }
	}
	if (i == MAX_HASH_TRY) {
	    /* failed after MAX_HASH_TRY rehash attempts ... */
	    __pmNotifyErr(LOG_ERR, 
		 "pmdaCacheStoreKey: indom %s: unable allocate a new id for instance \"%s\" based on a key of %d bytes\n",
		 pmInDomStr(h->indom), name, keylen);
	    return PM_ERR_GENERIC;
	}

	/*
	 * when using key[] or name[] as a hint, we permanently change
	 * to PMDA_CACHE_REUSE mode
	 */
	h->ins_mode = 1;
    }

    return store(indom, flags, name, inst, mykeylen, mykey, private);
}

int pmdaCacheOp(pmInDom indom, int op)
{
    hdr_t	*h;
    entry_t	*e;
    int		sts;

    if (indom == PM_INDOM_NULL)
	return PM_ERR_INDOM;

    if (op == PMDA_CACHE_CHECK) {
	/* is there a cache for this one? */
	for (h = base; h != NULL; h = h->next) {
	    if (h->indom == indom)
		return 1;
	}
	return 0;
    }

    if ((h = find_cache(indom, &sts)) == NULL)
	return sts;

    switch (op) {
	case PMDA_CACHE_LOAD:
	    return load_cache(h);

	case PMDA_CACHE_SAVE:
	    return save_cache(h, DIRTY_INSTANCE);

	case PMDA_CACHE_SYNC:
	    return save_cache(h, DIRTY_INSTANCE|DIRTY_STAMP);

	case PMDA_CACHE_STRINGS:
	    /* must be set before any cache entries are added */
	    if (h->nentry > 0)
		return -E2BIG;
	    h->hstate |= CACHE_STRINGS;
	    return 0;

	case PMDA_CACHE_ACTIVE:
	    sts = 0;
	    for (e = h->first; e != NULL; e = e->next) {
		if (e->state == PMDA_CACHE_INACTIVE) {
		    e->state = PMDA_CACHE_ACTIVE;
		    sts++;
		}
	    }
	    /* no instances added or deleted, so no need to save */
	    return sts;

	case PMDA_CACHE_INACTIVE:
	    sts = 0;
	    for (e = h->first; e != NULL; e = e->next) {
		if (e->state == PMDA_CACHE_ACTIVE) {
		    e->state = PMDA_CACHE_INACTIVE;
		    sts++;
		}
	    }
	    /* no instances added or deleted, so no need to save */
	    return sts;

	case PMDA_CACHE_CULL:
	    sts = 0;
	    for (e = h->first; e != NULL; e = e->next) {
		if (e->state != PMDA_CACHE_EMPTY) {
		    e->state = PMDA_CACHE_EMPTY;
		    sts++;
		}
	    }
	    if (sts > 0)
		h->hstate |= DIRTY_INSTANCE;	/* entries culled */
	    return sts;

	case PMDA_CACHE_SIZE:
	    return h->nentry;

	case PMDA_CACHE_SIZE_ACTIVE:
	    sts = 0;
	    for (e = h->first; e != NULL; e = e->next) {
		if (e->state == PMDA_CACHE_ACTIVE)
		    sts++;
	    }
	    return sts;

	case PMDA_CACHE_SIZE_INACTIVE:
	    sts = 0;
	    for (e = h->first; e != NULL; e = e->next) {
		if (e->state == PMDA_CACHE_INACTIVE)
		    sts++;
	    }
	    return sts;

	case PMDA_CACHE_REUSE:
	    h->ins_mode = 1;
	    return 0;

	case PMDA_CACHE_REORG:
	    redo_hash(h, 0);
	    return 0;

	case PMDA_CACHE_WALK_REWIND:
	    walk_cache(h, PMDA_CACHE_WALK_REWIND);
	    return 0;

	case PMDA_CACHE_WALK_NEXT:
	    while ((e = walk_cache(h, PMDA_CACHE_WALK_NEXT)) != NULL) {
		if (e->state == PMDA_CACHE_ACTIVE)
		    return e->inst;
	    }
	    return -1;

	case PMDA_CACHE_DUMP:
	    dump(stderr, h, 0);
	    return 0;

	case PMDA_CACHE_DUMP_ALL:
	    dump(stderr, h, 1);
	    return 0;

	default:
	    return -EINVAL;
    }
}

int pmdaCacheLookupName(pmInDom indom, const char *name, int *inst, void **private)
{
    hdr_t	*h;
    entry_t	*e;
    int		sts;

    if (indom == PM_INDOM_NULL)
	return PM_ERR_INDOM;

    if ((h = find_cache(indom, &sts)) == NULL)
	return sts;

    if ((e = find_entry(h, name, PM_IN_NULL, &sts)) == NULL) {
	if (sts == 0) sts = PM_ERR_INST;
	return sts;
    }

    if (private != NULL)
	*private = e->private;

    if (inst != NULL)
	*inst = e->inst;

    return e->state;
}

int pmdaCacheLookup(pmInDom indom, int inst, char **name, void **private)
{
    hdr_t	*h;
    entry_t	*e;
    int		sts;

    if (indom == PM_INDOM_NULL)
	return PM_ERR_INDOM;

    if ((h = find_cache(indom, &sts)) == NULL)
	return sts;

    if ((e = find_entry(h, NULL, inst, &sts)) == NULL) {
	if (sts == 0) sts = PM_ERR_INST;
	return sts;
    }

    if (name != NULL)
	*name = e->name;
    if (private != NULL)
	*private = e->private;

    return e->state;
}

int pmdaCacheLookupKey(pmInDom indom, const char *name, int keylen, const void *key, char **oname, int *inst, void **private)
{
    hdr_t	*h;
    entry_t	*e;
    int		sts;
    const char	*mykey;
    int		mykeylen;

    if (indom == PM_INDOM_NULL)
	return PM_ERR_INDOM;

    if ((h = find_cache(indom, &sts)) == NULL)
	return sts;

    if (keylen < 1 || key == NULL) {
	/* use name[] instead of keybuf[] */
	mykey = (const char *)name;
	mykeylen = strlen(name);
    }
    else {
	mykey = key;
	mykeylen = keylen;
    }

    /*
     * No hash list for key[]s ... have to walk the cache.
     * pmdaCacheStoreKey() ensures the key[]s are unique, so first match
     * wins.
     */
    walk_cache(h, PMDA_CACHE_WALK_REWIND);
    while ((e = walk_cache(h, PMDA_CACHE_WALK_NEXT)) != NULL) {
	if (e->state == PMDA_CACHE_EMPTY)
	    continue;
	if (key_eq(e, mykeylen, mykey) == 1) {
	    if (oname != NULL)
		*oname = e->name;
	    if (inst != NULL)
		*inst = e->inst;
	    if (private != NULL)
		*private = e->private;
	    return e->state;
	}
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_INDOM) {
	    fprintf(stderr, "pmdaCacheLookupKey: indom %s: ", pmInDomStr(h->indom));
	    KeyStr(stderr, mykeylen, mykey);
	    fprintf(stderr, ": no matching key in cache\n");
	}
#endif
    return PM_ERR_INST;
}

int pmdaCachePurge(pmInDom indom, time_t recent)
{
    hdr_t	*h;
    entry_t	*e;
    time_t	epoch = time(NULL) - recent;
    int		cnt;
    int		sts;

    if (indom == PM_INDOM_NULL)
	return PM_ERR_INDOM;

    if ((h = find_cache(indom, &sts)) == NULL)
	return sts;

    cnt = 0;
    for (e = h->first; e != NULL; e = e->next) {
	/*
	 * e->stamp == 0 => recently ACTIVE and no subsequent SAVE ...
	 * keep these ones
	 */
	if (e->stamp != 0 && e->stamp < epoch) {
	    e->state = PMDA_CACHE_EMPTY;
	    cnt++;
	}
    }
    if (cnt > 0)
	h->hstate |= DIRTY_INSTANCE;	/* entries marked empty */

    return cnt;
}

int pmdaCacheResize(pmInDom indom, int maximum)
{
    hdr_t	*h;
    int		sts;
    entry_t	*e;

    if (indom == PM_INDOM_NULL)
	return PM_ERR_INDOM;

    if ((h = find_cache(indom, &sts)) == NULL)
	return sts;

    if (maximum < 0)
	return PM_ERR_SIGN;

    /* Find the largest inst in the queue. */
    for (e = h->first; e != NULL; e = e->next) {
	/* If the new maximum is smaller than an existing inst, error. */
	if (maximum < e->inst)
	    return PM_ERR_TOOBIG;
    }
    h->maxinst = maximum;
    /* The timestamp doesn't really need updating, but the cache
       header does now that we've modified the maximum value. */
    h->hstate |= DIRTY_STAMP;
    return 0;
}

/*
--------------------------------------------------------------------
lookup2.c, by Bob Jenkins, December 1996, Public Domain.
hash(), hash2(), hash3, and mix() are externally useful functions.
Routines to test the hash are included if SELF_TEST is defined.
You can use this free for any purpose.  It has no warranty.
--------------------------------------------------------------------
*/

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
typedef  unsigned long  int  ub4;   /* unsigned 4-byte quantities */
typedef  unsigned       char ub1;

#define hashsize(n) ((ub4)1<<(n))
#define hashmask(n) (hashsize(n)-1)

/*
--------------------------------------------------------------------
mix -- mix 3 32-bit values reversibly.
For every delta with one or two bit set, and the deltas of all three
  high bits or all three low bits, whether the original value of a,b,c
  is almost all zero or is uniformly distributed,
* If mix() is run forward or backward, at least 32 bits in a,b,c
  have at least 1/4 probability of changing.
* If mix() is run forward, every bit of c will change between 1/3 and
  2/3 of the time.  (Well, 22/100 and 78/100 for some 2-bit deltas.)
mix() was built out of 36 single-cycle latency instructions in a 
  structure that could supported 2x parallelism, like so:
      a -= b; 
      a -= c; x = (c>>13);
      b -= c; a ^= x;
      b -= a; x = (a<<8);
      c -= a; b ^= x;
      c -= b; x = (b>>13);
      ...
  Unfortunately, superscalar Pentiums and Sparcs can't take advantage 
  of that parallelism.  They've also turned some of those single-cycle
  latency instructions into multi-cycle latency instructions.  Still,
  this is the fastest good hash I could find.  There were about 2^^68
  to choose from.  I only looked at a billion or so.
--------------------------------------------------------------------
*/
#define mix(a,b,c) \
{ \
  a -= b; a -= c; a ^= (c>>13); \
  b -= c; b -= a; b ^= (a<<8); \
  c -= a; c -= b; c ^= (b>>13); \
  a -= b; a -= c; a ^= (c>>12);  \
  b -= c; b -= a; b ^= (a<<16); \
  c -= a; c -= b; c ^= (b>>5); \
  a -= b; a -= c; a ^= (c>>3);  \
  b -= c; b -= a; b ^= (a<<10); \
  c -= a; c -= b; c ^= (b>>15); \
}

/*
--------------------------------------------------------------------
hash() -- hash a variable-length key into a 32-bit value
  k     : the key (the unaligned variable-length array of bytes)
  len   : the length of the key, counting by bytes
  level : can be any 4-byte value
Returns a 32-bit value.  Every bit of the key affects every bit of
the return value.  Every 1-bit and 2-bit delta achieves avalanche.
About 36+6len instructions.

The best hash table sizes are powers of 2.  There is no need to do
mod a prime (mod is sooo slow!).  If you need less than 32 bits,
use a bitmask.  For example, if you need only 10 bits, do
  h = (h & hashmask(10));
In which case, the hash table should have hashsize(10) elements.

If you are hashing n strings (ub1 **)k, do it like this:
  for (i=0, h=0; i<n; ++i) h = hash( k[i], len[i], h);

By Bob Jenkins, 1996.  bob_jenkins@burtleburtle.net.  You may use this
code any way you wish, private, educational, or commercial.  It's free.

See http://burlteburtle.net/bob/hash/evahash.html
Use for hash table lookup, or anything where one collision in 2^32 is
acceptable.  Do NOT use for cryptographic purposes.
--------------------------------------------------------------------
*/

static __uint32_t
hash(const signed char *k, int length, __uint32_t initval)
{
   __uint32_t a,b,c,len;

   /* Set up the internal state */
   len = length;
   a = b = 0x9e3779b9;  /* the golden ratio; an arbitrary value */
   c = initval;           /* the previous hash value */

   /*---------------------------------------- handle most of the key */
   while (len >= 12)
   {
      a += (k[0] +((__uint32_t)k[1]<<8) +((__uint32_t)k[2]<<16)
      +((__uint32_t)k[3]<<24));
      b += (k[4] +((__uint32_t)k[5]<<8) +((__uint32_t)k[6]<<16)
      +((__uint32_t)k[7]<<24));
      c += (k[8] +((__uint32_t)k[9]<<8)
      +((__uint32_t)k[10]<<16)+((__uint32_t)k[11]<<24));
      mix(a,b,c);
      k += 12; len -= 12;
   }

   /*------------------------------------- handle the last 11 bytes */
   c += length;
   switch(len)              /* all the case statements fall through */
   {
   case 11: c+=((__uint32_t)k[10]<<24);
   case 10: c+=((__uint32_t)k[9]<<16);
   case 9 : c+=((__uint32_t)k[8]<<8);
      /* the first byte of c is reserved for the length */
   case 8 : b+=((__uint32_t)k[7]<<24);
   case 7 : b+=((__uint32_t)k[6]<<16);
   case 6 : b+=((__uint32_t)k[5]<<8);
   case 5 : b+=k[4];
   case 4 : a+=((__uint32_t)k[3]<<24);
   case 3 : a+=((__uint32_t)k[2]<<16);
   case 2 : a+=((__uint32_t)k[1]<<8);
   case 1 : a+=k[0];
     /* case 0: nothing left to add */
   }
   mix(a,b,c);
   /*-------------------------------------------- report the result */
   return c;
}
