/*
 * Copyright (c) 1995 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2009 Aconex.  All Rights Reserved.
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

#include "pmcd.h"

#ifdef IS_MINGW

void
StartDaemon(int argc, char **argv)
{
    PROCESS_INFORMATION piProcInfo;
    STARTUPINFO siStartInfo;
    LPTSTR cmdline = NULL;
    int i, sz = 3; /* -f\0 */

    for (i = 0; i < argc; i++)
	sz += strlen(argv[i]) + 1;
    if ((cmdline = malloc(sz)) == NULL) {
	__pmNotifyErr(LOG_ERR, "StartDaemon: no memory");
	exit(1);
    }
    for (sz = i = 0; i < argc; i++)
	sz += sprintf(cmdline, "%s ", argv[i]);
    sprintf(cmdline + sz, "-f");

    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
    siStartInfo.cb = sizeof(STARTUPINFO);

    if (0 == CreateProcess(
		NULL, cmdline,
		NULL, NULL,	/* process and thread attributes */
		FALSE,		/* inherit handles */
		CREATE_NEW_PROCESS_GROUP | CREATE_NO_WINDOW | DETACHED_PROCESS,
		NULL,		/* environment (from parent) */
		NULL,		/* current directory */
		&siStartInfo,	/* STARTUPINFO pointer */
		&piProcInfo)) {	/* receives PROCESS_INFORMATION */
	__pmNotifyErr(LOG_ERR, "StartDaemon: CreateProcess");
	/* but keep going */
    }
    else {
	/* parent, let her exit, but avoid ugly "Log finished" messages */
	fclose(stderr);
	exit(0);
    }
}

#else

/* Based on Stevens (Unix Network Programming, p.83) */
void
StartDaemon(int argc, char **argv)
{
    pid_t childpid;

    (void)argc; (void)argv;

#if defined(HAVE_TERMIO_SIGNALS)
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
#endif

    if ((childpid = fork()) < 0)
	__pmNotifyErr(LOG_ERR, "StartDaemon: fork");
	/* but keep going */
    else if (childpid > 0) {
	/* parent, let her exit, but avoid ugly "Log finished" messages */
	fclose(stderr);
	exit(0);
    }

    /* not a process group leader, lose controlling tty */
    if (setsid() == -1)
	__pmNotifyErr(LOG_WARNING, "StartDaemon: setsid");
	/* but keep going */

    close(0);
    /* don't close other fd's -- we know that only good ones are open! */

    /* don't chdir("/") -- we still need to open pmcd.log */
}
#endif
