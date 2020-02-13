/*
 * Utiility routines for pmlogrewrite
 *
 * Copyright (c) 2013-2018 Red Hat.
 * Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
 * Copyright (c) 1997-2000 Silicon Graphics, Inc.  All Rights Reserved.
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

#include "pmapi.h"
#include "libpcp.h"
#include "logger.h"
#include <assert.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>

void
yywarn(char *s)
{
    fprintf(stderr, "Warning [%s, line %d]\n%s\n", configfile, lineno, s);
}

void
yyerror(char *s)
{
    fprintf(stderr, "Specification error in configuration file (%s)\n",
	    configfile);
    fprintf(stderr, "[line %d] %s\n", lineno, s);
    exit(1);
}

void
yysemantic(char *s)
{
    fprintf(stderr, "Semantic error in configuration file (%s)\n",
	    configfile);
    fprintf(stderr, "%s\n", s);
    exit(1);
}

/*
 * instance name matching ... return
 *  0 for no match
 *  1 for match to first space
 *  2 for complete match
 * -1 if either name is empty or NULL
 */
int
inst_name_eq(const char *p, const char *q)
{
    if (p == NULL || *p == '\0')
	return -1;
    if (q == NULL || *q == '\0')
	return -1;

    for ( ; ; p++, q++) {
	if (*p == '\0' && *q == '\0')
	    return 2;
	if (*p == '\0' || *p == ' ') {
	    if (*q == '\0' || *q == ' ')
		return 1;
	    break;
	}
	if (*q == '\0' || *q == ' ') {
	    if (*p == '\0' || *p == ' ')
		return 1;
	    break;
	}
	if (*p != *q)
	    break;
    }
    return 0;
}

/*
 * Rename all the physical archive files with basename of old to
 * a basename of new.
 *
 * If _any_ error occurs, don't make any changes.
 *
 * Note: also handles compressed versions of files.
 */
int
_pmLogRename(const char *old, const char *new)
{
    int			sts;
    int			nfound = 0;
    char		**found = NULL;
    char		*dname;
    char		*obase;
    char		path[MAXPATHLEN+1];
    char		opath[MAXPATHLEN+1];
    char		npath[MAXPATHLEN+1];
    char		logbase[MAXPATHLEN+1];
    DIR			*dirp;
    const char		*p;
    struct dirent	*dp;
    struct stat		stbuf;

    strncpy(path, old, sizeof(path));
    path[sizeof(path)-1] = '\0';
    dname = dirname(path);

    if ((dirp = opendir(dname)) == NULL)
	return -oserror();

    strncpy(path, old, sizeof(path));
    path[sizeof(path)-1] = '\0';
    obase = basename(path);

    for ( ; ; ) {
	setoserror(0);
	if ((dp = readdir(dirp)) == NULL)
	    break;

	/*
	 * __pmLogBaseName modifies the buffer which is passed to it
	 * so we need a copy.
	 */
	strncpy(logbase, dp->d_name, sizeof(logbase));
	logbase[sizeof(logbase)-1] = '0';
	if (__pmLogBaseName(logbase) == NULL)
	    continue; /* not an archive file */

	if (strcmp(obase, logbase) != 0)
	    continue; /* Not the same archive */

	/* We have found a file from the given archive */
	p = &dp->d_name[strlen(obase)];
	pmsprintf(opath, sizeof(opath), "%s%s", old, p);
	pmsprintf(npath, sizeof(npath), "%s%s", new, p);
	if (stat(npath, &stbuf) == 0) {
	    fprintf(stderr, "__pmLogRename: destination file %s already exists\n", npath);
	    goto revert;
	}
	if (rename(opath, npath) == -1) {
	    fprintf(stderr, "__pmLogRename: rename %s -> %s failed: %s\n", opath, npath, pmErrStr(-oserror()));
	    goto revert;
	}
	nfound++;
	found = (char **)realloc(found, nfound*sizeof(found[0]));
	if (found == NULL) {
	    pmNoMem("__pmLogRename: realloc", nfound*sizeof(found[0]), PM_RECOV_ERR);
	    abandon();
	    /*NOTREACHED*/
	}
	if ((found[nfound-1] = strdup(p)) == NULL) {
	    pmNoMem("__pmLogRename: strdup", strlen(p)+1, PM_RECOV_ERR);
	    abandon();
	    /*NOTREACHED*/
	}
	if (pmDebugOptions.log)
	    fprintf(stderr, "__pmLogRename: %s -> %s\n", opath, npath);
    }

    if ((sts = oserror()) != 0) {
	fprintf(stderr, "__pmLogRename: readdir for %s failed: %s\n", dname, pmErrStr(-sts));
	goto revert;
    }

    sts = 0;
    goto cleanup;

revert:
    while (nfound > 0) {
	pmsprintf(opath, sizeof(opath), "%s%s", old, found[nfound-1]);
	pmsprintf(npath, sizeof(npath), "%s%s", new, found[nfound-1]);
	if (rename(npath, opath) == -1) {
	    fprintf(stderr, "__pmLogRename: arrgh trying to revert rename %s -> %s failed: %s\n", npath, opath, pmErrStr(-oserror()));
	}
	if (pmDebugOptions.log)
	    fprintf(stderr, "__pmLogRename: revert %s <- %s\n", opath, npath);
	nfound--;
    }
    sts = PM_ERR_GENERIC;

cleanup:
    closedir(dirp);
    while (nfound > 0) {
	free(found[nfound-1]);
	nfound--;
    }
    if (found != NULL)
	free(found);

    return sts;
}

/*
 * Remove the physical archive files with basename of base.
 * If upto is -1, remove them all (this is the normal abandon()
 * case).
 * Otherwise, remove .index, .meta and volume 0, 1, ... upto
 * and skip removal for any volumes upto+1, upto+2, ...
 *
 * Note: also handles compressed versions of files.
 */
int
_pmLogRemove(const char *name, int upto)
{
    int			sts;
    int			nfound = 0;
    char		*dname;
    char		*base;
    char		path[MAXPATHLEN+1];
    char		logbase[MAXPATHLEN+1];
    DIR			*dirp;
    const char		*p;
    struct dirent	*dp;

    strncpy(path, name, sizeof(path));
    path[sizeof(path)-1] = '\0';
    dname = strdup(dirname(path));
    if (dname == NULL) {
	pmNoMem("__pmLogRemove: dirname strdup", strlen(dirname(path))+1, PM_RECOV_ERR);
	abandon();
	/*NOTREACHED*/
    }

    if ((dirp = opendir(dname)) == NULL) {
	free(dname);
	return -oserror();
    }

    strncpy(path, name, sizeof(path));
    path[sizeof(path)-1] = '\0';
    base = strdup(basename(path));
    if (base == NULL) {
	pmNoMem("__pmLogRemove: basename strdup", strlen(basename(path))+1, PM_RECOV_ERR);
	abandon();
	/*NOTREACHED*/
    }

    for ( ; ; ) {
	setoserror(0);
	if ((dp = readdir(dirp)) == NULL)
	    break;

	/*
	 * __pmLogBaseName modifies the buffer which is passed to it
	 * so we need a copy.
	 */
	strncpy(logbase, dp->d_name, sizeof(logbase));
	logbase[sizeof(logbase)-1] = '0';
	if (__pmLogBaseName(logbase) == NULL)
	    continue; /* not an archive file */

	if (strcmp(base, logbase) != 0)
	    continue; /* Not the same archive */

	if (upto != -1) {
	    char	*q;
	    char	*qend;
	    int		vol;
	    q = &dp->d_name[strlen(logbase)];
	    if (*q != '.')
		goto smackit;
	    q++;
	    if (*q == '\0')
		goto smackit;
	    if (strcmp(q, "index") == 0 || strcmp(q, "meta") == 0)
		goto smackit;
	    vol = (int)strtol(q, &qend, 10);
	    if (*qend != '\0')
		goto smackit;
	    if (vol > upto) {
		if (pmDebugOptions.log)
		    fprintf(stderr, "__pmLogRemove: do not remove %s\n", path);
		continue;
	    }
	}

smackit:
	p = &dp->d_name[strlen(base)];
	pmsprintf(path, sizeof(path), "%s%s", name, p);
	unlink(path);
	nfound++;
	if (pmDebugOptions.log)
	    fprintf(stderr, "__pmLogRemove: %s\n", path);
    }

    if ((sts = oserror()) != 0) {
	fprintf(stderr, "__pmLogRemove: readdir for %s failed: %s\n", dname, pmErrStr(-sts));
	sts = -sts;
    }
    else
	sts = nfound;

    closedir(dirp);
    free(dname);
    free(base);

    return sts;
}

char *
dupcat(const char* s1, const char* s2)
{
    size_t	len;
    char	*s;

    len = strlen(s1) + strlen(s2) + 1;
    s = malloc(len);
    if (s == NULL) {
	fprintf(stderr, "dupcat malloc(%d) failed: %s\n",
		(int)len, strerror(errno));
	abandon();
	/*NOTREACHED*/
    }

    pmsprintf(s, len, "%s%s", s1, s2);
    return s;
}
 
char *
add_quotes(const char *s)
{
    size_t	len;
    char	*s1;

    len = strlen(s) + 2 + 1;
    s1 = malloc(len);
    if (s1 == NULL) {
	fprintf(stderr, "add_quotes malloc(%d) failed: %s\n",
		(int)len, strerror(errno));
	abandon();
	/*NOTREACHED*/
    }

    pmsprintf(s1, len, "\"%s\"", s);
    return s1;
}
