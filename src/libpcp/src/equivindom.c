/*
 * Copyright (c) 2023 Ken McDonell.  All Rights Reserved.
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
 * one_trip is protected by the __pmLock_libpcp mutex.
 *
 * map is only changed in the one trip code in __pmEquivInDom()
 * where it is protected by the __pmLock_libpcp mutex.
 */

#include "pmapi.h"
#include "libpcp.h"
#include "internal.h"
#include <ctype.h>

typedef struct map_t {
    struct map_t	*next;
    int			nelt;
    pmInDom		*elt;
} map_t;

map_t	*map;

// TODO
//	- config files for packaging

/*
 * <domain> can be no more than 2^9-1 = 511
 * <serial> can be no more than 2^22-1 = 4194303
 */
#define MAX_DOMAIN 511
#define MAX_SERIAL 4194303

/* parser (FSA) states */
#define S_EOL		1
#define S_COMMENT	2
#define S_DOMAIN	3
#define S_SERIAL	4
#define S_SKIP		5

/*
 * parse an indom config file that specifies sets of equivalent
 * indoms
 */
static int
parse(FILE *f, char *fname)
{
    int		sts = 0;
    int		state = S_EOL;
    int		domain;
    int		serial;
    int		c;
    map_t	*mp = NULL;
    int		lineno = 1;
    char	nbuf[7];	/* enough for <serial> */
    char	*np;

    while ((c = fgetc(f)) != EOF) {
	if (state == S_EOL) {
	    if (c == ' ' || c == '\t') {
		;
	    }
	    else if (c == '\n') {
		lineno++;
	    }
	    else if (c == '#') {
		state = S_COMMENT;
	    }
	    else if (isdigit(c)) {
		pmInDom	*tmp_elt;
		state = S_DOMAIN;
		np = nbuf;
		*np++ = c;
		if (mp == NULL) {
		    /* start a new set ... */
		    if ((mp = (map_t *)malloc(sizeof(map_t))) == NULL) {
			pmNoMem("__pmEquivInDom: map_t malloc", sizeof(map_t), PM_RECOV_ERR);
			sts = -1;
			break;
		    }
		    mp->nelt = 0;
		    mp->elt = NULL;
		}
		mp->nelt++;
		if ((tmp_elt = (pmInDom *)realloc(mp->elt, mp->nelt*sizeof(pmInDom))) == NULL) {
		    pmNoMem("__pmEquivInDom: map realloc", mp->nelt*sizeof(pmInDom), PM_RECOV_ERR);
		    sts = -1;
		    break;
		}
		else {
		    mp->elt = tmp_elt;
		}
	    }
	    else {
		pmNotifyErr(LOG_ERR, "__pmEquivInDom: [%s:%d] <domain> expected, found '%c'",
			    fname, lineno, c);
		goto skip;
	    }
	}
	else if (state == S_COMMENT) {
	    if (c == '\n') {
		lineno++;
		state = S_EOL;
	    }
	}
	else if (state == S_DOMAIN) {
	    if (isdigit(c)) {
		if (np > &nbuf[2]) {
		    *np = '\0';
		    pmNotifyErr(LOG_ERR, "__pmEquivInDom: [%s:%d] <domain> %s%c... too long",
				fname, lineno, nbuf, c);
		    goto skip;
		}
		*np++ = c;
	    }
	    else if (c == '.') {
		*np = '\0';
		domain = atoi(nbuf);
		if (domain > MAX_DOMAIN) {
		    pmNotifyErr(LOG_ERR, "__pmEquivInDom: [%s:%d] <domain> %d too big",
				fname, lineno, domain);
		    goto skip;
		}
		state = S_SERIAL;
		np = nbuf;
	    }
	    else {
		*np = '\0';
		pmNotifyErr(LOG_ERR, "__pmEquivInDom: [%s:%d] '.' expected after <domain> %s, found '%c'",
			    fname, lineno, nbuf, c);
		goto skip;
	    }
	}
	else if (state == S_SERIAL) {
	    if (np == nbuf) {
		/* must start with digit or * */
		if (isdigit(c) || c == '*') {
		    *np++ = c;
		}
		else {
		    pmNotifyErr(LOG_ERR, "__pmEquivInDom: [%s:%d] digit or '*' expected after <domain> %s, found '%c'",
				fname, lineno, nbuf, c);
		    goto skip;
		}
	    }
	    else {
		/* 2nd or later character ... */
		if (isdigit(c) && nbuf[0] != '*') {
		    if (np > &nbuf[6]) {
			*np = '\0';
			pmNotifyErr(LOG_ERR, "__pmEquivInDom: [%s:%d] <serial> %s%c... too long",
				    fname, lineno, nbuf, c);
			goto skip;
		    }
		    *np++ = c;
		}
		else if (c == '\n' || c == ' ' || c == '\t') {
		    *np = '\0';
		    if (nbuf[0] != '*') {
			serial = atoi(nbuf);
			if (serial > MAX_SERIAL) {
			    pmNotifyErr(LOG_ERR, "__pmEquivInDom: [%s:%d] <serial> %d too big",
					fname, lineno, serial);
			    if (c == '\n')
				lineno++;
			    goto skip;
			}
			mp->elt[mp->nelt-1] = pmInDom_build(domain, serial);
		    }
		    else {
			/*
			 * special encoding for serial '*' (match all)
			 */
			__pmInDom_int	*ip = (__pmInDom_int *)&mp->elt[mp->nelt-1];
			ip->flag = 1;
			ip->domain = domain;
			ip->serial = MAX_SERIAL;
		    }
		    if (c == '\n') {
			lineno++;
			mp->next = map;
			map = mp;
			mp = NULL;
		    }
		    state = S_EOL;
		    np = nbuf;
		}
		else {
		    *np = '\0';
		    pmNotifyErr(LOG_ERR, "__pmEquivInDom: [%s:%d] newline or whitespace expected after <serial> %s, found '%c'",
				fname, lineno, nbuf, c);
    skip:
		    if (mp != NULL) {
			if (mp->elt != NULL)
			    free(mp->elt);
			free(mp);
			mp = NULL;
		    }
		    sts = -1;
		    state = S_SKIP;
		}
	    }
	}
	else if (state == S_SKIP) {
	    if (c == '\n') {
		lineno++;
		state = S_EOL;
	    }
	}
	else {
	    fprintf(stderr, "__pmEquivInDom: [%s:%d] parse botch: state=%d\n", fname, lineno, state);
	    sts = -1;
	    break;
	}
    }

    if (sts < 0) {
	if (mp != NULL) {
	    if (mp->elt != NULL)
		free(mp->elt);
	    free(mp);
	}
	map = NULL;
    }

    return sts;
}

/*
 * Check if 2 indoms are considered equivalent.
 */
int
__pmEquivInDom(pmInDom a, pmInDom b)
{
    static int	one_trip = 1;
    int		sts;
    int		match_a;
    int		match_b;
    map_t	*mp;

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    if (one_trip) {
	/*
	 * If PCP_INDOM_CONFIG is NOT set, then by default we load the
	 * global indom mapping data from $PCP_ETC_DIR/indom.config.
	 *
	 * If PCP_INDOM_CONFIG is set to a zero length string, then don't
	 * load any indom mapping data.
	 *
	 * Else if PCP_INDOM_CONFIG is set then load user-defined indom
	 * mapping data from that file.
	 *
	 */
	FILE	*f;
	char	*p;
	char 	path[MAXPATHLEN];

	one_trip = 0;
	if ((p = getenv("PCP_INDOM_CONFIG")) == NULL) {
	    pmsprintf(path, sizeof(path), "%s%cpcp%c" "indom.conf",
		     pmGetConfig("PCP_ETC_DIR"), pmPathSeparator(), pmPathSeparator());
	    p = path;
	}
	else if (p[0] == '\0') {
	    /* don't load anything */
	    return 0;
	}
	    
	if ((f = fopen(p, "r")) == NULL) {
	    if (pmDebugOptions.indom)
		fprintf(stderr, "__pmEquivInDom: failed to open \"%s\"\n", p);
	    return 0;
	}
	sts = parse(f, p);
	if (sts < 0) {
	    map_t	*next;
	    /* parse error in config, cleanup ... */
	    for (mp = map; mp; ) {
		if (mp->elt != NULL)
		    free(mp->elt);
		next = mp->next;
		free(mp);
		mp = next;
	    }
	}
	fclose(f);
	if (pmDebugOptions.indom) {
	    int			i;
	    __pmInDom_int	*ip;
	    fprintf(stderr, "__pmEquivInDom: loaded ...\n");
	    for (mp = map; mp; mp = mp->next) {
		fprintf(stderr, "[map]");
		for (i = 0; i < mp->nelt; i++) {
		    ip = (__pmInDom_int *)&mp->elt[i];
		    if (ip->flag)
			fprintf(stderr, " %d.*",pmInDom_domain((mp->elt[i])));
		    else
			fprintf(stderr, " %s", pmInDomStr(mp->elt[i]));

		}
		fputc('\n', stderr);
	    }
	}
    }
    PM_UNLOCK(__pmLock_libpcp);
    if (map == NULL)
	return 0;

    /*
     * if _both_ indom a and indom b appear in the same map, we have
     * an equivalence
     */
    for (mp = map; mp; mp = mp->next) {
	int		i;
	int		match;
	__pmInDom_int	*ip;
	match_a = match_b = 0;
	for (i = 0; i < mp->nelt; i++) {
	    match = 0;
	    if (mp->elt[i] == a)
		match = 1;
	    else {
		ip = (__pmInDom_int *)&mp->elt[i];
		if (ip->flag &&
		    pmInDom_domain(mp->elt[i]) == pmInDom_domain(a))
		    match = 1;
	    }
	    if (match) {
		if (match_b)
		    return 1;
		match_a = 1;
	    }
	    match = 0;
	    if (mp->elt[i] == b)
		match = 1;
	    else {
		ip = (__pmInDom_int *)&mp->elt[i];
		if (ip->flag &&
		    pmInDom_domain(mp->elt[i]) == pmInDom_domain(b))
		    match = 1;
	    }
	    if (match) {
		if (match_a)
		    return 1;
		match_b = 1;
	    }
	}
    }

    return 0;
}
