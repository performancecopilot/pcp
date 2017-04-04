/*
 * Copyright (c) International Business Machines Corp., 2002
 * Copyright (c) 2003,2004 Silicon Graphics, Inc.  All Rights Reserved.
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
 * This code originally contributed by Mike Mason <mmlnx@us.ibm.com>
 * with hints from the procps and ksymoops projects.
 */

#include <ctype.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include "pmapi.h"
#include "impl.h"
#include "ksym.h"
#include "indom.h"

static struct ksym *ksym_a;
static size_t ksym_a_sz;

static int
find_index(__psint_t addr, int lo, int hi)
{
    int mid;

    if (lo > hi) {
	return -1;
    }

    mid = lo + ((hi - lo) / 2);
    if (addr == ksym_a[mid].addr || 
	(addr > ksym_a[mid].addr && addr < ksym_a[mid+1].addr)) {
	return mid;
    }

    if (addr > ksym_a[mid].addr)
	return find_index(addr, mid+1, hi);
    else
	return find_index(addr, lo, mid-1);
}

static char *
find_name_by_addr(__psint_t addr)
{
    int ix = -1;

    if (ksym_a)
	ix = find_index(addr, 0, ksym_a_sz - 1);
    if (ix < 0)
	return NULL;

    return ksym_a[ix].name;
}

static int
find_dup_name(int maxix, __psint_t addr, char *name)
{
    int i, res;

    for (i = 0; i < maxix; i++) {
	if (ksym_a[i].name) {
	    res = strcmp(ksym_a[i].name, name);
	    if (res > 0) 
		break;
	    if (res == 0) {
		if (addr == ksym_a[i].addr)
		    return KSYM_FOUND;
		else
		    return KSYM_FOUND_MISMATCH;
	    }
	}
    }

    return KSYM_NOT_FOUND;
}

/* Brute force linear search to determine if the kernel version
   in System.map matches the running kernel version and returns
   a tri-state result as follows:

   0 no match
   1 _end not found but version matched
   2 _end found and matched
 */
static int
validate_sysmap(FILE *fp, char *version, __psint_t end_addr)
{
    __psint_t addr;
    char type;
    int ret = 0;
    char kname[128];

    while (fscanf(fp, "%p %c %s", (void **)&addr, &type, kname) != EOF) {
	if (end_addr && strcmp(kname, "_end") == 0) {
	    ret = (end_addr == addr) ? 2 : 0;
	    break; /* no need to look any further */
	}
	if (strcmp(kname, version) == 0)
	    ret = 1;
    }

    return ret;
}

char *
wchan(__psint_t addr)
{
    static char zero;
    char *p = NULL;

    if (addr == 0) /* 0 address means not in kernel space */
	p = &zero;
    else if ((p = find_name_by_addr(addr))) {
	/* strip off "sys_" or leading "_"s if necessary */
	if (strncmp(p, "sys_", 4) == 0)
	    p += 4;
	while (*p == '_' && *p)
	    ++p;
    }

    return p;
}

static int
ksym_compare_addr(const void *e1, const void *e2)
{
    struct ksym *ks1 = (struct ksym *) e1;
    struct ksym *ks2 = (struct ksym *) e2;

    if (ks1->addr < ks2->addr)
	return -1;
    if (ks1->addr > ks2->addr)
	return 1;
    return 0;
}

static int
ksym_compare_name(const void *e1, const void *e2)
{
    struct ksym *ks1 = (struct ksym *) e1;
    struct ksym *ks2 = (struct ksym *) e2;

    return(strcmp(ks1->name, ks2->name));
}

static int
read_ksyms(__psint_t *end_addr)
{
    char	inbuf[256];
    char	*ip;
    char	*sp;
    char	*tp;
    char	*p;
    int		ix = 0;
    int		l = 0;
    int		len;
    int		err;
    FILE	*fp;
    struct ksym	*ksym_tmp;

    *end_addr = 0;
    if ((fp = proc_statsfile("/proc/ksyms", inbuf, sizeof(inbuf))) == NULL)
	return -oserror();

    while (fgets(inbuf, sizeof(inbuf), fp) != NULL) {
	l++;

	/*
	 * /proc/ksyms lines look like this on ia32 ...
	 *
	 * c8804060 __insmod_rtc_S.text_L4576	[rtc]
	 * c010a320 disable_irq_nosync
	 *
	 * else on ia64 ...
	 *
	 * a0000000003e0d28 debug	[arsess]
	 * e002100000891140 disable_irq_nosync
	 */

	if (strstr(inbuf, "\n") == NULL) {
	    fprintf(stderr, "read_ksyms: truncated /proc/ksyms line [%d]: %s\n", l-1, inbuf);
	    continue;
	}

	/* Increase array size, if necessary */
	if (ksym_a_sz < ix+1) {
	    if (ksym_a_sz > 0)
		ksym_a_sz += INCR_KSIZE;
	    else
		ksym_a_sz = INIT_KSIZE;
	    ksym_tmp = (struct ksym *)realloc(ksym_a, ksym_a_sz * sizeof(struct ksym));
	    if (ksym_tmp == NULL) {
		err = -oserror();
		free(ksym_a);
		fclose(fp);
		return err;
	    }
	    ksym_a = ksym_tmp;
	}

	ip = inbuf;
	/* parse over address */
	while (isxdigit((int)*ip)) ip++;

	if (!isspace((int)*ip) || ip-inbuf < 4) {
	    /* bad format line */
#if PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL2) {
		fprintf(stderr, "read_ksyms: bad addr? %c[%d] line=\"%s\"\n", *ip, (int)(ip-inbuf), inbuf);
	    }
#endif
	    continue;
	}

	sscanf(inbuf, "%p", (void **)&ksym_a[ix].addr);

	while (isblank((int)*ip)) ip++;

	/* next should be the symbol name */
	sp = ip++;
	while (!isblank((int)*ip) &&*ip != '\n') ip++;

	/* strip off GPLONLY_ prefix, if found */
	if (strncmp(sp, "GPLONLY_", 8) == 0)
	    sp += 8;

	/*
	 * strip off symbol version suffix, if found ... looking for
	 * trailing pattern of the form _R.*[0-9a-fA-F]{8,}
	 * - find rightmost _R, if any
	 */
	tp = sp;
    	while ((p = strstr(tp, "_R")) != NULL) tp = p+2;
	if (tp > sp) {
	    /*
	     * found _R, need the last 8 digits to be hex
	     */
	    if (ip - tp + 1 >= 8) {
		for (p = &ip[-8]; p < ip; p++) {
		    if (!isxdigit((int)*p)) {
			tp = sp;
			break;
		    }
		}
	    }
	    else {
		/* not enough characters for [0-9a-fA-f]{8,} at the end */
		tp = sp;
	    }
	}
	if (tp > sp)
	    /* need to strip the trailing _R.*[0-9a-fA-f]{8,} */
	    len = tp - sp - 2;
	else
	    len = ip - sp + 1;

	ksym_a[ix].name = strndup(sp, len);
	if (ksym_a[ix].name == NULL) {
	    err = -oserror();
	    fclose(fp);
	    return err;
	}
	ksym_a[ix].name[len-1] = '\0';

	if (*end_addr == 0 && strcmp(ksym_a[ix].name, "_end") == 0)
	    *end_addr = ksym_a[ix].addr;

	if (*ip == '\n')
	    /* nothing after the symbol name, so no module name */
	    goto next;

	while (isblank((int)*ip)) ip++;

	/* next expect module name */
	if (*ip != '[') {
#if PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL2) {
		fprintf(stderr, "read_ksyms: bad start module name %c[%d] != [ line=\"%s\"\n", *ip, (int)(ip-inbuf), inbuf);
	    }
#endif
	    free(ksym_a[ix].name);
	    continue;
	}

	sp = ++ip;
	while (!isblank((int)*ip) && *ip != ']') ip++;

	if (*ip != ']') {
#if PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL2) {
		fprintf(stderr, "read_ksyms: bad end module name %c[%d] != ] line=\"%s\"\n", *ip, (int)(ip-inbuf), inbuf);
	    }
#endif
	    free(ksym_a[ix].name);
	    continue;
	}

	ksym_a[ix].module = strndup(sp, ip - sp + 1);
	if (ksym_a[ix].module == NULL) {
	    err = -oserror();
	    fclose(fp);
	    free(ksym_a[ix].name);
	    return err;
	}
	ksym_a[ix].module[ip - sp] = '\0';

next:
	ix++;
    }

    /* release unused ksym array entries */
    if (ix) {
	ksym_tmp = (struct ksym *)realloc(ksym_a, ix * sizeof(struct ksym));
	if (ksym_tmp == NULL) {
	    free(ksym_a);
	    fclose(fp);
	    return -oserror();
	}
	ksym_a = ksym_tmp;
    }

    ksym_a_sz = ix;

    qsort(ksym_a, ksym_a_sz, sizeof(struct ksym), ksym_compare_name);

    fclose(fp);

#if PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2) {
	fprintf(stderr, "symbols from ksyms ...\n");
	for (ix = 0; ix < ksym_a_sz; ix++) {
	    fprintf(stderr, "ksym[%d] " PRINTF_P_PFX "%p %s", ix, (void *)ksym_a[ix].addr, ksym_a[ix].name);
	    if (ksym_a[ix].module != NULL) fprintf(stderr, " [%s]", ksym_a[ix].module);
	    fprintf(stderr, "\n");
	}
    }
#endif

    return ksym_a_sz;
}

static int
read_sysmap(const char *release, __psint_t end_addr)
{
    char	inbuf[256], path[MAXPATHLEN], **fmt;
    struct ksym	*ksym_tmp;
    __psint_t	addr;
    int		ix, res, e;
    int		l = 0;
    char	*ip;
    char	*sp;
    int		major, minor, patch;
    FILE	*fp;
    char	*bestpath = NULL;
    int		ksym_mismatch_count;
    char *sysmap_paths[] = {	/* Paths to check for System.map file */
	"%s/boot/System.map-%s",
	"%s/boot/System.map",
	"%s/lib/modules/%s/System.map",
	"%s/usr/src/linux/System.map",
	"%s/System.map",
	NULL
    };

    /* Create version symbol name to look for in System.map */
    if (sscanf(release, "%d.%d.%d", &major, &minor, &patch) < 3 )
	return -1;
    sprintf(inbuf, "Version_%u", KERNEL_VERSION(major, minor, patch));

    /*
     * Walk through System.map path list looking for one that matches
     * either _end from /proc/ksyms or the uts version.
     */
    for (fmt = sysmap_paths; *fmt; fmt++) {
	snprintf(path, MAXPATHLEN, *fmt, proc_statspath, release);
	if ((fp = fopen(path, "r"))) {
	    if ((e = validate_sysmap(fp, inbuf, end_addr)) != 0) {
		if (e == 2) {
		    /* matched _end, so this is the right System.map */
		    if (bestpath)
		    	free(bestpath);
		    bestpath = strdup(path);
		}
		else
		if (e == 1 && !bestpath)
		    bestpath = strdup(path);
	    }
	    fclose(fp);
	    if (e == 2) {
		/* _end matched => don't look any further */
	    	break;
	    }
	}
    }

    if (bestpath)
	fprintf(stderr, "NOTICE: using \"%s\" for kernel symbols map.\n", bestpath);
    else {
	/* Didn't find a valid System.map */
	fprintf(stderr, "Warning: Valid System.map file not found!\n");
	fprintf(stderr, "Warning: proc.psinfo.wchan_s symbol names cannot be derived!\n");
	fprintf(stderr, "Warning: Addresses will be returned for proc.psinfo.wchan_s instead!\n");
	/* Free symbol array */
	for (ix = 0; ix < ksym_a_sz; ix++) {
		if (ksym_a[ix].name)
			free(ksym_a[ix].name);
		if (ksym_a[ix].module)
			free(ksym_a[ix].module);
	}
	free(ksym_a);
	ksym_a = NULL;
	ksym_a_sz = 0;
	return -1;
    }

    /* scan the System map */
    if ((fp = proc_statsfile(bestpath, path, sizeof(path))) == NULL)
    	return -oserror();

    ix = ksym_a_sz;

    /* Read each line in System.map */
    ksym_mismatch_count = 0;
    while (fgets(inbuf, sizeof(inbuf), fp) != NULL) {
	/*
	 * System.map lines look like this on ia32 ...
	 *
	 * c010a320 T disable_irq_nosync
	 *
	 * else on ia64 ...
	 *
	 * e002000000014c80 T disable_irq_nosync
	 */

	if (strstr(inbuf, "\n") == NULL) {
	    fprintf(stderr, "read_sysmap: truncated System.map line [%d]: %s\n", l-1, inbuf);
	    continue;
	}

	/* Increase array size, if necessary */
	if (ksym_a_sz < ix+1) {
	    ksym_a_sz += INCR_KSIZE;
	    ksym_tmp = (struct ksym *)realloc(ksym_a, ksym_a_sz * sizeof(struct ksym));
	    if (ksym_tmp == NULL) {
		free(ksym_a);
		goto fail;
	    }
	    ksym_a = ksym_tmp;
	}

	ip = inbuf;
	/* parse over address */
	while (isxdigit((int)*ip)) ip++;

	if (!isspace((int)*ip) || ip-inbuf < 4) {
	    /* bad format line */
#if PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL2) {
		fprintf(stderr, "read_sysmap: bad addr? %c[%d] line=\"%s\"\n", *ip, (int)(ip-inbuf), inbuf);
	    }
#endif
	    continue;
	}

	sscanf(inbuf, "%p", (void **)&addr);

	while (isblank((int)*ip)) ip++;

	/* Only interested in symbol types that map to code addresses,
	 * so: t, T, W or A
	 */
	if (*ip != 't' && *ip != 'T' && *ip != 'W' && *ip != 'A')
	    continue;

	ip++;
	while (isblank((int)*ip)) ip++;

	/* next should be the symbol name */
	sp = ip++;
	while (!isblank((int)*ip) && *ip != '\n') ip++;
	*ip = '\0';

	/* Determine if symbol is already in ksym array.
	   If so, make sure the addresses match. */
	res = find_dup_name(ix - 1, addr, sp);
	if (res == KSYM_NOT_FOUND) { /* add it */
	    ksym_a[ix].name = strdup(sp);
	    if (ksym_a[ix].name == NULL)
		goto fail;
	    ksym_a[ix].addr = addr;
	    ix++;
	}
	else if (res == KSYM_FOUND_MISMATCH) {
	    if (ksym_mismatch_count++ < KSYM_MISMATCH_MAX_ALLOWED) {
		/*
		 * ia64 function pointer descriptors make this validation
		 * next to useless. So only report the first 
		 * KSYM_MISMATCH_MAX_ALLOWED mismatches found.
		 */
		fprintf(stderr, "Warning: mismatch for \"%s\" between System.map"
				" and /proc/ksyms.\n", sp);
	    }
	}
    }

    if (ksym_mismatch_count > KSYM_MISMATCH_MAX_ALLOWED) {
	fprintf(stderr, "Warning: only reported first %d out of %d mismatches "
			"between System.map and /proc/ksyms.\n",
	    KSYM_MISMATCH_MAX_ALLOWED, ksym_mismatch_count);
    }

    /* release unused ksym array entries */
    ksym_tmp = (struct ksym *)realloc(ksym_a, ix * sizeof(struct ksym));
    if (ksym_tmp == NULL) {
	free(ksym_a);
	goto fail;
    }
    ksym_a = ksym_tmp;
    ksym_a_sz = ix;

    qsort(ksym_a, ksym_a_sz, sizeof(struct ksym), ksym_compare_addr);

#if PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2) {
	fprintf(stderr, "symbols from ksyms + sysmap ...\n");
	for (ix = 0; ix < ksym_a_sz; ix++) {
	    fprintf(stderr, "ksym[%d] " PRINTF_P_PFX "%p %s", ix, (void *)ksym_a[ix].addr, ksym_a[ix].name);
	    if (ksym_a[ix].module != NULL) fprintf(stderr, " [%s]", ksym_a[ix].module);
	    fprintf(stderr, "\n");
	}
    }
#endif

    fclose(fp);

    return ksym_a_sz;

fail:
    e = -oserror();
    if (fp)
	fclose(fp);
    return e;
}

void
read_ksym_sources(const char *release) 
{
    __psint_t end_addr;

    if (read_ksyms(&end_addr) > 0)	/* read /proc/ksyms first */
	read_sysmap(release, end_addr);	/* then System.map  */
}
