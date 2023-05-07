/*
 * Copyright (c) 2014 Red Hat.
 * Copyright (c) 1995 Silicon Graphics, Inc.  All Rights Reserved.
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

#include "logger.h"

void
buildinst(int *numinst, int **intlist, char ***extlist, int intid, char *extid)
{
    char	**el;
    char	**tmp_el;
    int		*il;
    int		*tmp_il;
    int		num = *numinst;

    if (num == 0) {
	il = NULL;
	el = NULL;
    }
    else {
	il = *intlist;
	el = *extlist;
    }

    tmp_el = (char **)realloc(el, (num+1)*sizeof(el[0]));
    if (tmp_el == NULL) {
	pmNoMem("buildinst extlist", (num+1)*sizeof(el[0]), PM_FATAL_ERR);
	/* NOTREACHED */
    }
    el = tmp_el;
    tmp_il = (int *)realloc(il, (num+1)*sizeof(il[0]));
    if (tmp_il == NULL) {
	pmNoMem("buildinst intlist", (num+1)*sizeof(il[0]), PM_FATAL_ERR);
	/* NOTREACHED */
    }
    il = tmp_il;

    il[num] = intid;

    if (extid == NULL)
	el[num] = NULL;
    else {
	if (*extid == '"') {
	    char	*p;
	    p = ++extid;
	    while (*p && *p != '"') p++;
	    *p = '\0';
	}
	el[num] = strdup(extid);
    }

    *numinst = ++num;
    *intlist = il;
    *extlist = el;
}

void
freeinst(int *numinst, int *intlist, char **extlist)
{
    int		i;

    if (*numinst) {
	free(intlist);
	for (i = 0; i < *numinst; i++)
	    free(extlist[i]);
	free(extlist);

	*numinst = 0;
    }
}

/*
 * Link the new fetch groups back to their task structure
 */
void
linkback(task_t *tp)
{
    fetchctl_t	*fcp;

    for (fcp = tp->t_fetch; fcp != NULL; fcp = fcp->f_next)
	fcp->f_aux = (void *)tp;
}

/*
 * Given a directory dname place the run-time directory name in
 * result (which is pre-allocated by the caller to be at least
 * MAXPATHLEN bytes long).
 *
 * If dname contains meta characters these are expanded by
 * echoing them in sh(1).
 *
 * If dname is NULL or the empty string, result[] is set to "",
 * otherwise result[] ends with the native filesystem directory
 * separator (/ for *nix).
 */
#define CMD_A "echo \""
#define CMD_B "\" | sed -e \"s/LOCALHOSTNAME/`hostname || echo localhost`/\""
int
do_dir(char *dname, char *result)
{
    char	*cmd;
    char	*p;
    int		c;
    FILE	*f;
    char	sep = pmPathSeparator();
    char	here[MAXPATHLEN];
    int		sts = 0;

    if (dname == NULL || dname[0] == '\0') {
	result[0] = '\0';
	return 0;
    }

    cmd = malloc(strlen(CMD_A) + strlen(dname) + strlen(CMD_B) + 1);
    if (cmd == NULL) {
	sts = -oserror();
	pmNoMem("do_dir", strlen(CMD_A) + strlen(dname) + strlen(CMD_B) + 1, PM_RECOV_ERR);
	result[0] = '\0';
	return sts;
    }
    sprintf(cmd, "%s%s%s", CMD_A, dname, CMD_B);
    f = popen(cmd, "r");
    if (f == NULL) {
	sts = -oserror();
	fprintf(stderr, "do_dir(%s, ...): popen()%s, ...) failed: %s\n", dname, cmd, pmErrStr(sts));
	result[0] = '\0';
	free(cmd);
	return sts;
    }
    for (p = result; ; ) {
	if ((c = fgetc(f)) == EOF)
	    break;
	/* strip newlines ... only expected one before EOF */
	if (c != '\n')
	    *p++ = c;
    }
    *p++ = sep;
    *p = '\0';
    free(cmd);
    pclose(f);

    if (result[0] != sep) {
	fprintf(stderr, "%s: expanded -d argument (%s) must be an absolute path\n",
		pmGetProgname(), result);
	sts = PM_ERR_GENERIC;
	goto done;
    }

    /*
     * result is an absolute path, so ensure "result" dir exists,
     * and cd there and restore cwd when done.
     */
    if (getcwd(here, MAXPATHLEN) == NULL) {
	sts = -oserror();
	fprintf(stderr, "do_dir(%s, ...): getcwd() failed: %s\n", dname, pmErrStr(sts));
	goto done;
    }
    if (chdir(result) < 0) {
	/*
	 * can't cd there, try component dirs from root to leaf
	 * creating any missing ones
	 */
	p = &result[1];
	while (*p) {
	    if (*p == sep) {
		*p = '\0';
		if (chdir(result) < 0) {
		    if (mkdir(result, 0777) < 0) {
			sts = -oserror();
			fprintf(stderr, "do_dir(%s, ...): mkdir(%s, ...) failed: %s\n", dname, result, pmErrStr(sts));
			*p = sep;
			goto restore;
		    }
		}
		*p = sep;
	    }
	    p++;
	}
	/* try again ... */
	if (chdir(result) < 0) {
	    if (mkdir(result, 0777) < 0) {
		sts = -oserror();
		fprintf(stderr, "do_dir(%s, ...): mkdir(%s, ...) failed: %s\n", dname, result, pmErrStr(sts));
		goto restore;
	    }
	}
    }

restore:
    /* restore cwd */
    if (chdir(here) < 0) {
	sts = -oserror();
	fprintf(stderr, "do_dir(%s, ...): chdir(%s) failed: %s\n", dname, here, pmErrStr(sts));
    }

done:
    return sts;
}
