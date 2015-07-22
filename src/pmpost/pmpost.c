/*
 * Copyright (c) 1995-2003 Silicon Graphics, Inc.  All Rights Reserved.
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
#include "impl.h"
#include <sys/stat.h>
#include <sys/file.h>
#ifdef HAVE_GRP_H
#include <grp.h>
#endif

/*
 * Attempt to setup the notices file in a way that members of
 * the (unprivileged) "pcp" group account can write to it.
 */
int gid;
#ifdef HAVE_GETGRNAM
static void
setup_group(void)
{
    char *name = pmGetConfig("PCP_GROUP");
    struct group *group;

    if (!name || name[0] == '\0')
	name = "pcp";
    group = getgrnam(name);
    if (group)
	gid = group->gr_gid;
}
#else
#define setup_group()
#endif

static int
mkdir_r(char *path)
{
    struct stat	sbuf;
    int sts;

    if (stat(path, &sbuf) < 0) {
	if (mkdir_r(dirname(strdup(path))) < 0)
	    return -1;
	sts = mkdir2(path, 0775);
#ifndef IS_MINGW
	if (chown(path, 0, gid) < 0)
	    fprintf(stderr, "pmpost: cannot set dir gid[%d]: %s\n", gid, path);
#endif
	return sts;
    }
    else if ((sbuf.st_mode & S_IFDIR) == 0) {
	fprintf(stderr, "pmpost: \"%s\" is not a directory\n", path);
	exit(1);
    }
    return 0;
}

#define LAST_UNDEFINED	-1
#define LAST_NEWFILE	-2

int
main(int argc, char **argv)
{
    int		i;
    int		fd;
    FILE	*np;
    char	*dir;
    struct stat	sbuf;
    time_t	now;
    int		lastday = LAST_UNDEFINED;
    struct tm	*tmp;
    int		sts = 0;
    char	notices[MAXPATHLEN];
#ifndef IS_MINGW
    char	*ep;
    struct flock lock;
    extern char **environ;
    static char *newenviron[] =
	{ "HOME=/nowhere", "SHELL=/noshell", "PATH=/nowhere", NULL };
    static char *keepname[] =
	{ "TZ", "PCP_DIR", "PCP_CONF", NULL };
    char *keepval[] =
	{ NULL, NULL,      NULL,       NULL };

    /*
     * Fix for bug #827972, do not trust the environment.
     * See also below.
     */
    for (i = 0; keepname[i] != NULL; i++)
	keepval[i] = getenv(keepname[i]);
    environ = newenviron;
    for (i = 0; keepname[i] != NULL; i++) {
	if (keepval[i] != NULL) {
	    snprintf(notices, sizeof(notices), "%s=%s", keepname[i], keepval[i]);
	    if ((ep = strdup(notices)) != NULL)
		putenv(ep);
	}
    }
#endif
    umask(0002);

    if ((argc == 1) || (argc == 2 && strcmp(argv[1], "-?") == 0)) {
	fprintf(stderr, "Usage: pmpost message\n");
	exit(1);
    }

    snprintf(notices, sizeof(notices), "%s%c" "NOTICES",
		pmGetConfig("PCP_LOG_DIR"), __pmPathSeparator());

    setup_group();
    dir = dirname(strdup(notices));
    if (mkdir_r(dir) < 0) {
	fprintf(stderr, "pmpost: cannot create directory \"%s\": %s\n",
	    dir, osstrerror());
	exit(1);
    }

    if ((fd = open(notices, O_WRONLY|O_APPEND, 0)) < 0) {
	if ((fd = open(notices, O_WRONLY|O_CREAT|O_APPEND, 0664)) < 0) {
	    fprintf(stderr, "pmpost: cannot create file \"%s\": %s\n",
		notices, osstrerror());
	    exit(1);
#ifndef IS_MINGW
	} else if ((fchown(fd, 0, gid)) < 0) {
	    fprintf(stderr, "pmpost: cannot set file gid \"%s\": %s\n",
		notices, osstrerror());
#endif
	}
	lastday = LAST_NEWFILE;
    }

#ifndef IS_MINGW
    /*
     * drop root privileges for bug #827972
     */
    if (setuid(getuid()) < 0)
    	exit(1);

    lock.l_type = F_WRLCK;
    lock.l_whence = 0;
    lock.l_start = 0;
    lock.l_len = 0;

    /*
     * willing to try for 3 seconds to get the lock ... note fcntl()
     * does not block, unlike flock()
     */
    for (i = 0; i < 3; i++) {
	if ((sts = fcntl(fd, F_SETLK, &lock)) != -1)
	    break;
	sleep(1);
    }
    
    if (sts == -1) {
	fprintf(stderr, "pmpost: warning, cannot lock file \"%s\"", notices);
	if (oserror() != EINTR)
	    fprintf(stderr, ": %s", osstrerror());
	fputc('\n', stderr);
    }
    sts = 0;
#endif

    /*
     * have lock, get last modified day unless file just created
     */
    if (lastday != LAST_NEWFILE) {
	if (fstat(fd, &sbuf) < 0)
	    /* should never happen */
	    ;
	else {
	    tmp = localtime(&sbuf.st_mtime);
	    lastday = tmp->tm_yday;
	}
    }

    if ((np = fdopen(fd, "a")) == NULL) {
	fprintf(stderr, "pmpost: fdopen: %s\n", osstrerror());
	exit(1);
    }

    time(&now);
    tmp = localtime(&now);

    if (lastday != tmp->tm_yday) {
	if (fprintf(np, "\nDATE: %s", ctime(&now)) < 0)
	    sts = oserror();
    }

    if (fprintf(np, "%02d:%02d", tmp->tm_hour, tmp->tm_min) < 0)
	sts = oserror();

    for (i = 1; i < argc; i++) {
	if (fprintf(np, " %s", argv[i]) < 0)
	    sts = oserror();
    }

    if (fputc('\n', np) < 0)
	sts = oserror();

    if (fclose(np) < 0)
	sts = oserror();

    if (sts < 0) {
	fprintf(stderr, "pmpost: write failed: %s\n", osstrerror());
	fprintf(stderr, "Lost message ...");
	for (i = 1; i < argc; i++) {
	    fprintf(stderr, " %s", argv[i]);
	}
	fputc('\n', stderr);
    }

    exit(0);
}
