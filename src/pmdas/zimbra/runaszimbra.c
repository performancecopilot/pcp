/*
 * Copyright (c) 2023 Ken McDonell.  All Rights Reserved.
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
 * On Selinux (and in particular RHEL 7 on vm29), no combination of su(1),
 * sudo(1) or runuser(1) seems to be able to run zmcontrol from zimbraprobe
 * (which is run from pmdazimbra, which is run from pmcd).
 *
 * This simple wrapper has to be run as root (zimbraprobe runs as root) and
 * the single argument is a sh(1) command to be run under the uid of the
 * zimbra "user".
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <pwd.h>


int main(int argc, char **argv)
{
    struct passwd	*pw;
    int			sts;
    char		*path;
    char		*tmp;
    size_t		nch;
    char		*myname = argv[0];
    char		**nargv;
    int			i;
    int			debug = 0;	/* change to 1 for chatter on stderr */

    if (debug)
	fprintf(stderr, "%s called ...\n", myname);

    if (argc != 2) {
	fprintf(stderr, "Usage: %s shell-command\n", myname);
	return 1;
    }

    /*
     * get the passwd file entry for the zimbra "user" ... this gives us
     * the user id, $HOME and $SHELL
     */
    pw = getpwnam("zimbra");
    if (pw == NULL) {
	fprintf(stderr, "%s: getpwnam(zimbra) failed: ", myname);
	if (errno != 0)
	    fprintf(stderr, "%s\n", strerror(errno));
	else
	    fprintf(stderr, "entry not found in passwd file\n");
	return 1;
    }

    /*
     * change user id
     */
    if (setuid(pw->pw_uid) < 0) {
	fprintf(stderr, "%s: setuid(%d) failed: %s\n", myname, (int)pw->pw_uid, strerror(errno));
	return 1;
    }
    if (debug)
	fprintf(stderr, "yipee, I am uid %d\n", (int)getuid());

    /*
     * ensure zimbra's $HOME/bin is on $PATH ... this is where zmcontrol is hiding
     */
    path = getenv("PATH");
    if (path == NULL) {
	fprintf(stderr, "%s: getenv(PATH) failed: punting on /bin:/usr/bin\n", myname);
	path="/bin:/usr/bin";
    }
    nch = strlen(pw->pw_dir) + strlen("/bin:") + strlen(path) + 1;
    tmp = (char *)malloc(nch);
    if (tmp == NULL) {
	fprintf(stderr, "%s: malloc(%d) failed for $PATH\n", myname, (int)nch);
	return 1;
    }
    snprintf(tmp, nch, "%s/bin:%s", pw->pw_dir, path);
    if ((sts = setenv("PATH", tmp, 1)) < 0) {
	fprintf(stderr, "%s: setenv(PATH, %s, 1) failed: %s\n", myname, tmp, strerror(errno));
	return 1;
    }

    /*
     * build a new argv[]
     */
    nargv = (char **)malloc((4)*sizeof(nargv[0]));
    if (nargv == NULL) {
	fprintf(stderr, "%s: malloc(%d) failed for nargv[]\n", myname, (int)((4)*sizeof(nargv[0])));
	return 1;
    }
    nargv[0] = "sh";
    nargv[1] = "-c";
    nargv[2] = argv[1];
    nargv[3] = NULL;
    if (debug) {
	fprintf(stderr, "%s\n", pw->pw_shell);
	for (i = 0; i < argc+1; i++)
	    fprintf(stderr, "argv[%d] \"%s\"\n", i, nargv[i]);
    }

    /*
     * off to the races ...
     */
    sts = execv(pw->pw_shell, nargv);

    fprintf(stderr, "%s: surprise! execv() returns %d (errno %d: %s)\n", myname, sts, errno, strerror(errno));

    return 1;
}
