/*
 * Copyright (c) 2023,2024 Ken McDonell.  All Rights Reserved.
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
 * This simple wrapper has to be run as root and the single argument is a
 * sh(1) command to be run under the uid and gid of the pcp "user".
 */
#include <pcp/pmapi.h>
#ifdef HAVE_GRP_H
#include <grp.h>
#endif
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    { "shell", 1, 's', "SHELL", "shell to use, defaults to /bin/sh" },
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static int
override(int opt, pmOptions *opts)
{
    return (opt == 's');
}

static pmOptions opts = {
    .short_options = "D:s:?",
    .long_options = longopts,
    .short_usage = "[options] shell-command",
    .override = override,
};

int main(int argc, char **argv)
{
    struct passwd	*pw;
    struct group	*gr;
    int			sts;
    char		*shell = "/bin/sh";
    char		*myname = argv[0];
    char		*user;
    char		*group;
    char		**nargv;
    int			i;
    int			c;

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {

	case 's':	/* shell */
	    shell = opts.optarg;
	    break;
	}
    }
    if (opts.errors || opts.optind != argc-1) {
	pmUsageMessage(&opts);
	return 1;
    }

    if (pmDebugOptions.appl0)
	fprintf(stderr, "%s called ...\n", myname);

    /*
     * get $PCP_USER ... and $PCP_GROUP
     */
    if ((user = pmGetConfig("PCP_USER")) == NULL) {
	fprintf(stderr, "%s: pmGetConfig(PCP_USER) failed: ", myname);
	return 1;
    }
    if ((group = pmGetConfig("PCP_GROUP")) == NULL) {
	fprintf(stderr, "%s: pmGetConfig(PCP_GROUP) failed: ", myname);
	return 1;
    }

    /*
     * get the passwd file entry for the PCP_USER "user"
     */
    pw = getpwnam(user);
    if (pw == NULL) {
	fprintf(stderr, "%s: getpwnam(%s) failed: ", myname, user);
	if (errno != 0)
	    fprintf(stderr, "%s\n", strerror(errno));
	else
	    fprintf(stderr, "entry not found in passwd file\n");
	return 1;
    }

    /*
     * get the group file entry for the PCP_GROUP "group"
     */
    gr = getgrnam(group);
    if (gr == NULL) {
	fprintf(stderr, "%s: getgrname(%s) failed: ", myname, group);
	if (errno != 0)
	    fprintf(stderr, "%s\n", strerror(errno));
	else
	    fprintf(stderr, "entry not found in group file\n");
	return 1;
    }

    /*
     * change group id and user id
     */
    if (setgroups(0, NULL) < 0) {
	fprintf(stderr, "%s: setgroups(0, NULL) failed: %s\n", myname, strerror(errno));
	return 1;
    }
    if (setgid(gr->gr_gid) < 0) {
	fprintf(stderr, "%s: setgid(%d) failed: %s\n", myname, (int)gr->gr_gid, strerror(errno));
	return 1;
    }
    if (setuid(pw->pw_uid) < 0) {
	fprintf(stderr, "%s: setuid(%d) failed: %s\n", myname, (int)pw->pw_uid, strerror(errno));
	return 1;
    }
    if (pmDebugOptions.appl0)
	fprintf(stderr, "yipee, I am uid %d and gid %d\n", (int)getuid(), (int)getgid());

    /*
     * build a new argv[]
     */
    nargv = (char **)malloc((4)*sizeof(nargv[0]));
    if (nargv == NULL) {
	fprintf(stderr, "%s: malloc(%d) failed for nargv[]\n", myname, (int)((4)*sizeof(nargv[0])));
	return 1;
    }
    nargv[0] = shell;
    nargv[1] = "-c";
    nargv[2] = argv[opts.optind];
    nargv[3] = NULL;
    if (pmDebugOptions.appl0) {
	fprintf(stderr, "sh\n");
	for (i = 0; i < 4; i++)
	    fprintf(stderr, "argv[%d] \"%s\"\n", i, nargv[i]);
    }

    /*
     * off to the races ...
     */
    sts = execv(shell, nargv);

    fprintf(stderr, "%s: surprise! execv() returns %d (errno %d: %s)\n", myname, sts, errno, strerror(errno));

    return 1;
}
