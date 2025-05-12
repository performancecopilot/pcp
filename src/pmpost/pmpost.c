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
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/file.h>
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#ifdef HAVE_GRP_H
#include <grp.h>
#endif

/*
 * Attempt to setup the notices file in a way that the user "pcp"
 * can write to it.
 */
#ifndef IS_MINGW
uid_t	uid;
gid_t	gid;
#endif

#if defined(HAVE_GETPWENT) && defined(HAVE_GETGRNAM)
static void
setup_ids(void)
{
    char *name;
    struct passwd	*passwd;
    struct group	*group;

    name = pmGetConfig("PCP_USER");
    if (!name || name[0] == '\0')
	name = "pcp";
    passwd = getpwnam(name);
    if (passwd)
	uid = passwd->pw_uid;

    name = pmGetConfig("PCP_GROUP");
    if (!name || name[0] == '\0')
	name = "pcp";
    group = getgrnam(name);
    if (group)
	gid = group->gr_gid;
}
#else
#define setup_ids()
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
	if (sts >= 0) {
	    if ((sts = chown(path, 0, gid)) < 0)
		fprintf(stderr, "pmpost: cannot set dir gid[%d]: %s\n", gid, path);
	}
#endif
	return sts;
    }
    else if ((sbuf.st_mode & S_IFDIR) == 0) {
	fprintf(stderr, "pmpost: \"%s\" is not a directory\n", path);
	return -1;
    }
    return 0;
}

#define LAST_UNDEFINED	-1
#define LAST_NEWFILE	-2

static pmLongOptions longopts[] = {
    PMOPT_DEBUG,
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "D:?",
    .long_options = longopts,
    .short_usage = "message",
};

int
main(int argc, char **argv)
{
    int		i;
    int		c;
    int		fd;
    FILE	*np;
    char	*dir;
    struct stat	sbuf;
    struct timeval	now;
    int		lastday = LAST_UNDEFINED;
    struct tm	*tmp;
    int		sts = 0;
    char	notices[MAXPATHLEN];
    char	*p;
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
     * only possible command line option is -D
     */
    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	;
    }
    if (opts.errors || (opts.flags & PM_OPTFLAG_EXIT) || opts.optind == argc) {
	pmUsageMessage(&opts);
	exit(1);
    }

    /*
     * Fix for bug #827972, do not trust the environment.
     * See also below.
     */
    for (i = 0; keepname[i] != NULL; i++)
	keepval[i] = getenv(keepname[i]);
    environ = newenviron;
    for (i = 0; keepname[i] != NULL; i++) {
	if (keepval[i] != NULL) {
	    pmsprintf(notices, sizeof(notices), "%s=%s", keepname[i], keepval[i]);
	    if ((ep = strdup(notices)) != NULL)
		putenv(ep);
	}
    }
#endif
    umask(0002);

    pmsprintf(notices, sizeof(notices), "%s%c" "NOTICES",
		pmGetConfig("PCP_LOG_DIR"), pmPathSeparator());

    setup_ids();
    dir = dirname(strdup(notices));
    if (mkdir_r(dir) < 0) {
	fprintf(stderr, "pmpost: cannot create directory \"%s\": %s\n",
	    dir, osstrerror());
	goto oops;
    }

    if ((fd = open(notices, O_WRONLY|O_APPEND|O_NOFOLLOW, 0)) < 0) {
	if (oserror() == ELOOP) {
	    /* last component is symlink => attack? ... bail! */
	    goto oops;
	}
	if ((fd = open(notices, O_WRONLY|O_CREAT|O_APPEND|O_NOFOLLOW, 0664)) < 0) {
	    fprintf(stderr, "pmpost: cannot open or create file \"%s\": %s\n",
		notices, osstrerror());
	    if (pmDebugOptions.dev2) {
		char	shellcmd[2*MAXPATHLEN];
		int	lsts;
		/*
		 * -Ddev2 (set via $PCP_DEBUG)
		 * we need some more info to triage this ...
		 */
		fprintf(stderr, "pmpost: PCP id %d:%d, my id %d:%d\n", uid, gid, geteuid(), getegid());
		pmsprintf(shellcmd, sizeof(shellcmd), "[ -x /bin/ls ] && /bin/ls -ld %s >&2", dir);
		if ((lsts = system(shellcmd)) != 0)
		    fprintf(stderr, "command exit=%d?\n", lsts);
		pmsprintf(shellcmd, sizeof(shellcmd), "[ -x /bin/ls ] && /bin/ls -l %s >&2", notices);
		if ((lsts = system(shellcmd)) != 0)
		    fprintf(stderr, "command exit=%d?\n", lsts);
	    }
	    goto oops;
	}
#ifndef IS_MINGW
	/* if root, try to fix ownership */
	if (getuid() == 0) {
	    if ((fchown(fd, uid, gid)) < 0) {
		fprintf(stderr, "pmpost: cannot set file gid \"%s\": %s\n",
		    notices, osstrerror());
	    }
	}
#endif
	lastday = LAST_NEWFILE;
    }

#ifndef IS_MINGW
    /*
     * drop root privileges for bug #827972
     */
    if (setuid(getuid()) < 0)
    	goto oops;

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
	goto oops;
    }

    gettimeofday(&now, NULL);
    tmp = localtime(&now.tv_sec);

    if (lastday != tmp->tm_yday) {
	if (fprintf(np, "\nDATE: %s", ctime(&now.tv_sec)) < 0)
	    sts = oserror();
    }

    if (fprintf(np, "%02d:%02d:%02d.%03d", tmp->tm_hour, tmp->tm_min, tmp->tm_sec, (int)(now.tv_usec/1000)) < 0)
	sts = oserror();

    for (i = opts.optind; i < argc; i++) {
	if (fprintf(np, " %s", argv[i]) < 0)
	    sts = oserror();
    }

    if (fputc('\n', np) < 0)
	sts = oserror();

    if (fclose(np) < 0)
	sts = oserror();

    if (sts < 0) {
	fprintf(stderr, "pmpost: write failed: %s\n", osstrerror());
	goto oops;
    }

    exit(0);

oops:
    fprintf(stderr, "pmpost: unposted message: [");
    gettimeofday(&now, NULL);
    for (p = ctime(&now.tv_sec); *p != '\n'; p++)
	fputc(*p, stderr);
    fputc(']', stderr);
    for (i = opts.optind; i < argc; i++) {
	fprintf(stderr, " %s", argv[i]);
    }
    fputc('\n', stderr);

    exit(1);
}
