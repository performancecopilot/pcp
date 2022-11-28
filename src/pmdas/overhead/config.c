/*
 * Copyright (c) 2022 Ken McDonell.  All Rights Reserved.
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
 * Parse configuration files for the overhead PMDA.
 */

#include "overhead.h"
#include <sys/stat.h>
#include <ctype.h>

/*
 * Parser states
 */
#define S_INIT	0		/* initial state */
#define S_VERSION	1	/* seen "version" at start */
#define S_GROUP	2		/* looking for "group" */
#define S_NAME	3		/* seen "group" looking for <name> */
#define S_LBR	4		/* seen <name> looking for "{" */
#define S_PARAM	5		/* looking for parameter clause */
#define S_ID	6		/* seen "id" at start of parameter clause */
#define S_PATTERN	7	/* seen "pattern" at start of parameter clause */

// typical example config ...
//
// version 1
// group pcp {
//     id: 0
//     pattern: (^pm(cd|proxy|logger|chart|time|rep|ie|val|info|pause)$)|pmda
// }

static char		*fname;
static FILE		*fp;
static char		token[256];
static int		lineno = 1;

static char *
statestr(int state)
{
    static char		buf[128];
    if (state == S_INIT) return "INIT";
    if (state == S_VERSION) return "VERSION";
    if (state == S_GROUP) return "GROUP";
    if (state == S_NAME) return "NAME";
    if (state == S_LBR) return "LBR";
    if (state == S_PARAM) return "PARAM";
    if (state == S_ID) return "ID";
    if (state == S_PATTERN) return "PATTERN";
    snprintf(buf, sizeof(buf), "%d - UNKNOWN", state);
    return buf;
}

/*
 * get next token ... start already read (1st character of token)
 * and stop at first delim[] character
 * result returned in token[]
 */
static int
gettok(int start)
{
    static char	*delim = " \t\n";
    char	*p = token;
    int		c;
    char	*q;

    *p++ = start;

    while ((c = fgetc(fp)) != EOF) {
	for (q = delim; *q; q++) {
	    if (c == *q) {
		ungetc(c, fp);
		goto done;
	    }
	}
	if (p - token >= sizeof(token)) {
	    *--p = '\0';
	    fprintf(stderr, "parse: %s[%d]: Error: token \"%s\" too long\n", fname, lineno, token);
	    return -1;
	}
	*p++ = c;
    }
done:
    *p = '\0';

    if (pmDebugOptions.appl0 && pmDebugOptions.desperate)
	fprintf(stderr, "gettok() -> \"%s\" @ line %d\n", token, lineno);

    return 0;
}

/*
 * config file name in configfile, file is unopened on entry.
 * returun value -1 for errors (reported before returning)
 * return value 0 => OK
 */
static int
parse(char *configfile)
{
    int		c;
    char	*p;
    int		state;
    char	*name;
    int		id;
    char	*pattern;
    int		version = 1;
    grouptab_t	*gp;

    if (pmDebugOptions.appl0)
	fprintf(stderr, "parse(%s) ...\n", configfile);

    if ((fp = fopen(configfile, "r")) == NULL) {
	fprintf(stderr, "parse: cannot open \"%s\": %s\n", configfile, pmErrStr(-oserror()));
	return -1;
    }

    fname = configfile;
    name = NULL;
    pattern = NULL;

    state = S_INIT;

    while ((c = fgetc(fp)) != EOF) {
	if (c == ' ' || c == '\t')
	    continue;
	if (c == '\n') {
	    lineno++;
	    continue;
	}
	if (c == '#') {
	    /* comment ... input to end of line */
	    while ((c = fgetc(fp)) != EOF && c != '\n')
		;
	    if (c == '\n')
		lineno++;
	    continue;
	}
	if (state == S_INIT) {
	    if (c == 'v') {
		if (gettok(c) < 0)
		    goto fail;
		if (strcmp(token, "version") == 0) {
		    state = S_VERSION;
		    continue;
		}
		else {
		    fprintf(stderr, "parse: %s[%d]: Error: expecting \"version\" not \"%s\"\n", fname, lineno, token);
		    goto fail;
		}
	    }
	}
	else if (state == S_VERSION) {
	    if (gettok(c) < 0)
		goto fail;
	    version = atoi(token);
	    /* only version 1 at this stage */
	    if (version != 1) {
		fprintf(stderr, "parse: %s[%d]: Error: version \"%s\" not supported\n", fname, lineno, token);
		goto fail;
	    }
	    state = S_GROUP;
	    continue;
	}
	if (state == S_INIT) {
	    fprintf(stderr, "parse: %s[%d]: Warning: version 1 assumed\n", fname, lineno);
	    state = S_GROUP;
	}
	if (state == S_GROUP) {
	    if (c == 'g') {
		if (gettok(c) < 0)
		    goto fail;
		if (strcmp(token, "group") == 0) {
		    state = S_NAME;
		}
		else {
		    fprintf(stderr, "parse: %s[%d]: Error: expecting \"group\" not \"%s\"\n", fname, lineno, token);
		    goto fail;
		}
	    }
	    else {
		gettok(c);
		fprintf(stderr, "parse: %s[%d]: Error: expecting \"group\" not \"%s\"\n", fname, lineno, token);
		goto fail;
	    }
	}
	else if (state == S_NAME) {
	    if (gettok(c) < 0)
		goto fail;
	    if (!isalpha(token[0])) {
		fprintf(stderr, "parse: %s[%d]: Error: \"%s\" group name must start with an alphabetic\n", fname, lineno, token);
		goto fail;
	    }
	    for (p = &token[1]; *p; p++) {
		if (!isalpha(*p) && !isdigit(*p) && *p != '_') {
		    fprintf(stderr, "parse: %s[%d]: Error: \"%s\" group name contains illegal characters, not [a-zA-Z0-9_]\n", fname, lineno, token);
		    goto fail;
		}
	    }
	    /*
	     * semantic check - name must be unique
	     */
	    for (gp = grouptab; gp < &grouptab[ngroup]; gp++) {
		if (strcmp(gp->name, token) == 0) {
		    fprintf(stderr, "parse: %s[%d]: Error: group name: \"%s\" already assigned to group id %d\n", fname, lineno, token, gp->id);
		    goto fail;
		}
	    }
	    name = strdup(token);
	    if (name == NULL) {
		pmNoMem("parse: name", strlen(token), PM_FATAL_ERR);
		/* NOTREACHED */
	    }
	    state = S_LBR;
	}
	else if (state == S_LBR) {
	    if (c == '{') {
		state = S_PARAM;
		id = -1;
		pattern = NULL;
	    }
	    else {
		fprintf(stderr, "parse: %s[%d]: Error: expected \"{\" after group name\n", fname, lineno);
		goto fail;
	    }
	}
	else if (state == S_PARAM) {
	    if (c == '}') {
		int		lsts;
		/*
		 * _really_ add this one into grouptab[]
		 */
		ngroup++;
		gp = (grouptab_t *)realloc(grouptab, ngroup * sizeof(grouptab_t));
		if (gp == NULL) {
		    pmNoMem("parse: grouptab", ngroup * sizeof(grouptab_t), PM_FATAL_ERR);
		    /* NOTREACHED */
		}
		grouptab = gp;
		gp = &grouptab[ngroup-1];
		gp->id = id;
		gp->name = name;
		gp->nproctab = gp->nproc = gp->nproc_active = 0;
		gp->indom_cycle = -1;
		gp->proctab = NULL;
		gp->pattern = pattern;
		if ((lsts = regcomp(&gp->regex, gp->pattern, REG_EXTENDED|REG_NOSUB)) != 0) {
		    char errbuf[1024];
		    regerror(lsts, &gp->regex, errbuf, sizeof(errbuf));
		    fprintf(stderr, "parse: %s[%d]: Error: bad pattern=\"%s\": %s\n", fname, lineno, gp->pattern, errbuf);
		    goto fail;
		}
		if (pmDebugOptions.appl0)
		    fprintf(stderr, "add group \"%s\" id=%d pattern=\"%s\"\n", name, id, pattern);
		/* ready for next "group", if any */
		state = S_GROUP;
	    }
	    else {
		if (gettok(c) < 0)
		    goto fail;
		if (strcmp(token, "id:") == 0) {
		    if (id != -1) {
			fprintf(stderr, "parse: %s[%d]: Error: duplicate id: parameter\n", fname, lineno);
			goto fail;
		    }
		    state = S_ID;
		}
		else if (strcmp(token, "pattern:") == 0) {
		    if (pattern != NULL) {
			fprintf(stderr, "parse: %s[%d]: Error: duplicate pattern: parameter\n", fname, lineno);
			goto fail;
		    }
		    state = S_PATTERN;
		}
		else {
		    fprintf(stderr, "parse: %s[%d]: Error: \"%s\" is not a valid parameter name\n", fname, lineno, token);
		    goto fail;
		}
	    }
	}
	else if (state == S_ID) {
	    if (gettok(c) < 0)
		goto fail;
	    id = (int)strtol(token, &p, 10);
	    if (*p != '\0' || id < 0 || id >= 4095) {
		fprintf(stderr, "parse: %s[%d]: Error: id: \"%s\" (%d) is not legal\n", fname, lineno, token, id);
		goto fail;
	    }
	    /*
	     * semantic check - id must be unique
	     */
	    for (gp = grouptab; gp < &grouptab[ngroup]; gp++) {
		if (gp->id == id) {
		    fprintf(stderr, "parse: %s[%d]: Error: id: %d already assigned to group \"%s\"\n", fname, lineno, id, gp->name);
		    goto fail;
		}
	    }
	    state = S_PARAM;
	}
	else if (state == S_PATTERN) {
	    /*
	     * pattern is from c to \n ... can't use gettok() because
	     * pattern may contain whitespace (although unlikely)
	     */
	    p = token;
	    *p++ = c;
	    while ((c = fgetc(fp)) != EOF && c != '\n') {
		*p++ = c;
	    }
	    if (c == '\n')
		lineno++;
	    *p = '\0';
	    pattern = strdup(token);
	    if (pattern == NULL) {
		pmNoMem("parse: pattern", strlen(token), PM_FATAL_ERR);
		/* NOTREACHED */
	    }
	    state = S_PARAM;
	}
	else {
	    fprintf(stderr, "parse: %s[%d]: Error: \"%s\" is not expected\n", fname, lineno, token);
	    if (pmDebugOptions.appl0) {
		fprintf(stderr, "parse: state=%s input to follow: \"", statestr(state));
		c = fgetc(fp);
		if (c != EOF) {
		    fputc(c, stderr);
		    c = fgetc(fp);
		    if (c != EOF) {
			fputc(c, stderr);
			c = fgetc(fp);
			if (c != EOF)
			    fputc(c, stderr);
			else
			    fprintf(stderr, "<EOF>");
		    }
		    else
			fprintf(stderr, "<EOF>");
		}
		else
		    fprintf(stderr, "<EOF>");
		fprintf(stderr, "...\"\n");
	    }
	    goto fail;
	}
    }

    fclose(fp);
    return 0;

fail:
    if (name != NULL)
	free(name);
    if (pattern != NULL)
	free(pattern);
    fclose(fp);
    return -1;
}

int
do_config(char *configfile)
{
    int			sts;
    struct stat		sbuf;
    char		*p;
    char		*pathname;
    static char		path[MAXPATHLEN];
    DIR			*dirp;
    struct dirent	*dp;

    if (configfile == NULL) {
	p = pmGetConfig("PCP_SYSCONF_DIR");
	if (p == NULL) {
	    fprintf(stderr, "do_config: Arrgh: cannot get \"$PCP_SYSCONF_DIR\"\n");
	    exit(1);
	}
	strncpy(path, p, MAXPATHLEN - 1);
	strncat(path, "/overhead/conf.d", MAXPATHLEN - strlen(path) - 1);
	pathname = path;
    }
    else
	pathname = configfile;

    if ((sts = stat(pathname, &sbuf)) < 0) {
	fprintf(stderr, "do_config: cannot stat \"%s\": %s\n", pathname, pmErrStr(-oserror()));
	return -1;
    }
    if (S_ISREG(sbuf.st_mode))
	return parse(pathname);
    if (!S_ISDIR(sbuf.st_mode)) {
	fprintf(stderr, "do_config: \"%s\" is neither a directory nor a file\n", pathname);
	return -1;
    }

    if (pathname != path) {
	strcpy(path, pathname);
	pathname = path;
    }
    strncat(pathname, "/", MAXPATHLEN - strlen(path) - 1);
    p = &pathname[strlen(pathname)];

    if ((dirp = opendir(pathname)) == NULL) {
	fprintf(stderr, "do_config: opendir(\"%s\") failed: %s\n",
	    pathname, pmErrStr(-oserror()));
	return -1;
    }

    sts = 0;
    while ((dp = readdir(dirp)) != NULL) {
	if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
	    continue;
	*p = '\0';
	strncat(p, dp->d_name, MAXPATHLEN - strlen(p) - 1);
	if ((sts = stat(pathname, &sbuf)) < 0) {
	    fprintf(stderr, "do_config: Warning: cannot stat \"%s\": %s\n", pathname, pmErrStr(-oserror()));
	    continue;
	}
	if (S_ISREG(sbuf.st_mode)) {
	    if (parse(pathname) < 0)
		sts = -1;
	}
    }
    closedir(dirp);

    return sts;
}
