/*
 * Copyright (c) 1995-2001,2004 Silicon Graphics, Inc.  All Rights Reserved.
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
 */
#include "pmapi.h"
#include "pmafm.h"
#include "libpcp.h"
#include <sys/stat.h>
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

/*
 * some extended state, make sure these values are different to
 * the PM_REC_* macros in <pmapi.h>
 */
#define PM_REC_BEGIN	81
#define PM_REC_HOST	82

static int	state = PM_REC_OFF;	/* where you are up to ... */
static char	*dir;		/* directory containing Archive Folio files */
static char	*base;			/* unique basename */
static FILE	*f_folio;		/* Archive Folio goes here */
static FILE	*f_replay;		/* current replay config file */
static int	_replay;		/* can replay? */
static char	*_folio;		/* remember the folio name */
static char	*_creator;		/* remember the creator name */
static int	_seendefault;		/* seen default host? */

typedef struct _record {
    struct _record	*next;
    int			state;
    char		*host;		/* host name */
    int			isdefault;	/* is this the default host? */
    pmRecordHost	public;		/* exposed to the caller */
    char		*base;		/* archive base */
    char		*logfile;	/* for -l ... not to be confused */
					/* with public.logfile which is the */
					/* full pathname */
    char		*config;	/* for -c */
    int			argc;
    char		**argv;
} record_t;

static record_t	*record;
static int	n_record;
#ifndef IS_MINGW	/* not yet ported */
static int	n_alive;
#endif
static char	tbuf[MAXPATHLEN];	/* used for mktemp(), messages, ... */

/*
 * initialize, and return stdio stream for writing replay config
 * (if any)
 */
FILE *
pmRecordSetup(const char *folio, const char *creator, int replay)
{
    char  	*p;
    char	c;
    time_t	now;
    int		sts;
    int		fd = -1;
    static char	host[MAXHOSTNAMELEN];
    char	foliopath[MAXPATHLEN];
    char	*temp = NULL;		/* for unlink() */
    record_t	*rp;
    mode_t	cur_umask;

    if (state != PM_REC_OFF) {
	/* already begun w/out end */
	setoserror(EINVAL);
	return NULL;
    }

    if (access(folio, F_OK) == 0) {
	setoserror(EEXIST);
	return NULL;
    }

    if (_folio != NULL)
	free(_folio);
    if ((_folio = strdup(folio)) == NULL)
	return NULL;

    if (_creator != NULL)
	free(_creator);
    if ((_creator = strdup(creator)) == NULL)
	return NULL;

    if ((f_folio = fopen(folio, "w")) == NULL)
	return NULL;

    dir = NULL;
    base = NULL;

    /*
     * have folio file created ... get unique base string
     */
    tbuf[0] = '\0';
    strcpy(foliopath, folio);
    if ((p = strrchr(foliopath, '/')) != NULL) {
	/* folio name contains a slash */
	p++;
	c = *p;
	*p = '\0';
	if (strcmp(foliopath, "./") != 0) {
	    if (dir != NULL)
		free(dir);
	    if ((dir = strdup(foliopath)) == NULL)
		goto failed;
	    strcpy(tbuf, dir);
	    strcat(tbuf, "/");
	}
	*p = c;
    }
    strcat(tbuf, "XXXXXX");
#if HAVE_MKSTEMP
    cur_umask = umask(S_IXUSR | S_IRWXG | S_IRWXO);
    if ((fd = mkstemp(tbuf)) < 0) {
	umask(cur_umask);
	goto failed;
    }
    umask(cur_umask);
#else
    if (mktemp(tbuf) == NULL)
	goto failed;

    cur_umask = umask(S_IXUSR | S_IRWXG | S_IRWXO);
    if ((fd = open(tbuf, O_CREAT | O_EXCL | O_RDWR, 0600)) < 0) {
	umask(cur_umask);
	goto failed;
    }
    umask(cur_umask);
#endif
    /*
     * file named tbuf is never used, it is the basename for the real
     * files we create.  remember it so we can cleanup.
     */
    temp = strdup(tbuf);

    if (dir == NULL)
	p = tbuf;
    else
	p =  strrchr(tbuf, '/') + 1;

    if ((base = strdup(p)) == NULL)
	goto failed;

    /*
     * folio preamble ...
     */
    fprintf(f_folio, "PCPFolio\nVersion: 1\n");
    fprintf(f_folio, "# use pmafm(1) to process this PCP Archive Folio\n#\n");
    time(&now);
    (void)gethostname(host, MAXHOSTNAMELEN);
    host[MAXHOSTNAMELEN-1] = '\0';
    fprintf(f_folio, "Created: on %s at %s", host, ctime(&now));
    fprintf(f_folio, "Creator: %s", creator);
    if (replay)
	fprintf(f_folio, " %s", base);
    fprintf(f_folio, "\n#               Host                    Basename\n#\n");

    _replay = replay;
    if (_replay) {
	f_replay = fdopen(fd, "w");
	if (f_replay == NULL)
	    goto failed;
    }
    else
	f_replay = fopen("/dev/null", "r");

    n_record = 0;
    for (rp = record; rp != NULL; rp = rp->next) {
	rp->state = PM_REC_BEGIN;
	rp->isdefault = 0;
	if (rp->host != NULL) {
	    free(rp->host);
	    rp->host = NULL;
	}
	if (rp->base != NULL) {
	    free(rp->base);
	    rp->base = NULL;
	}
	if (rp->logfile != NULL) {
	    free(rp->logfile);
	    rp->logfile = NULL;
	}
	if (rp->config != NULL) {
	    free(rp->config);
	    rp->config = NULL;
	}
	if (rp->argv != NULL) {
	    free(rp->argv);
	    rp->argv = NULL;
	}
	rp->argc = 0;
	if (rp->public.f_config != NULL) {
	    fclose(rp->public.f_config);
	    rp->public.f_config = NULL;
	}
	if (rp->public.fd_ipc != -1) {
	    close(rp->public.fd_ipc);
	    rp->public.fd_ipc = -1;
	}
	if (rp->public.logfile != NULL) {
	    free(rp->public.logfile);
	    rp->public.logfile = NULL;
	}
	rp->public.pid = (pid_t)0;
	rp->public.status = -1;
    }

    state = PM_REC_BEGIN;
    _seendefault = 0;

    if (temp) {
	unlink(temp);
	free(temp);
    }
    return f_replay;

failed:
    sts = oserror();
    if (dir != NULL)
	free(dir);
    if (base != NULL)
	free(base);
    unlink(folio);
    fclose(f_folio);
    f_folio = NULL;
    if (fd >= 0)
	close(fd);
    setoserror(sts);
    if (temp) {
	unlink(temp);
	free(temp);
    }
    return NULL;
}

/*
 * need to log another host in this folio ... must come here
 * at least once, but may be more than once before the recording
 * commences
 */
int
pmRecordAddHost(const char *host, int isdefault, pmRecordHost **rhp)
{
    char	*p;
    int		c;
    int		sts;
    record_t	*rp;

    *rhp = NULL;	/* in case of errors */

    if (state != PM_REC_BEGIN && state != PM_REC_HOST)
	/* botched order of calls ... */
	return -EINVAL;

    if (isdefault && _seendefault)
	/* only one default allowed per session! */
	return -EINVAL;

    for (rp = record; rp != NULL; rp = rp->next) {
	if (rp->state == PM_REC_BEGIN)
	    break;
    }
    if (rp == NULL) {
	/* need another one */
	if ((rp = (record_t *)malloc(sizeof(record_t))) == NULL)
	    return -oserror();
	rp->next = record;
	record = rp;
	rp->isdefault = 0;
	rp->host = NULL;
	rp->base = NULL;
	rp->logfile = NULL;
	rp->config = NULL;
	rp->argv = NULL;
	rp->argc = 0;
	rp->public.f_config = NULL;
	rp->public.fd_ipc = -1;
	rp->public.logfile = NULL;
	rp->public.pid = (pid_t)0;
	rp->public.status = -1;
    }

    rp->isdefault = isdefault;

    if (dir != NULL)
	strcpy(tbuf, dir);
    else
	tbuf[0] = '\0';
    strcat(tbuf, base);
    p = &tbuf[strlen(tbuf)];
    strcat(tbuf, ".");
    strcat(tbuf, host);
    strcat(tbuf, ".config");
    c = '\0';
    if (access(tbuf, F_OK) == 0) {
	p[0] = 'a';
	p[1] = '.';
	p[2] = '\0';
	strcat(p, host);
	strcat(p, ".config");
	while (p[0] <= 'z') {
	    if (access(tbuf, F_OK) != 0) {
		c = p[0];
		break;
	    }
	    p[0]++;
	}
	if (c == '\0') {
	    setoserror(EEXIST);
	    goto failed;
	}
    }

    if ((rp->host = strdup(host)) == NULL)
	goto failed;

    if ((rp->public.f_config = fopen(tbuf, "w")) == NULL)
	goto failed;

    if ((rp->base = malloc(strlen(base)+1+strlen(host)+3)) == NULL)
	goto failed;
    strcpy(rp->base, base);
    p = &rp->base[strlen(rp->base)];
    if (c != '\0') {
	*p++ = c;
    }
    *p++ = '.';
    *p = '\0';
    strcat(p, host);

    if ((rp->logfile = malloc(strlen(rp->base)+5)) == NULL)
	goto failed;
    strcpy(rp->logfile, rp->base);
    strcat(rp->logfile, ".log");

    if ((rp->config = malloc(strlen(rp->base)+8)) == NULL)
	goto failed;
    strcpy(rp->config, rp->base);
    strcat(rp->config, ".config");

    /* construct full pathname */
    rp->public.logfile = malloc(MAXPATHLEN);
    if (rp->public.logfile != NULL) {
	int sep = pmPathSeparator();
	if (dir != NULL && __pmAbsolutePath(dir))
	    strcpy(rp->public.logfile, dir);
	else {
	    if (getcwd(rp->public.logfile, MAXPATHLEN) == NULL)
		goto failed;
	}

	sts = strlen(rp->public.logfile);
	if (rp->public.logfile[sts - 1] != sep) {
	    rp->public.logfile[sts] = sep;
	    rp->public.logfile[sts+1] = '\0';
	}
	strcat(rp->public.logfile, rp->logfile);
    }
    else {
	/* malloc failure ... */
	goto failed;
    }

    n_record++;
    state = rp->state = PM_REC_HOST;
    *rhp = &rp->public;

    return 0;

failed:
    sts = -oserror();
    if (rp->public.f_config != NULL) {
	unlink(tbuf);
	fclose(rp->public.f_config);
	rp->public.f_config = NULL;
    }
    if (rp->host != NULL)
	free(rp->host);
    if (rp->base != NULL)
	free(rp->base);
    if (rp->logfile != NULL)
	free(rp->logfile);
    if (rp->config != NULL)
	free(rp->config);
    *rhp = NULL;
    return sts;
}

#ifndef IS_MINGW	/* not yet ported */
/*
 * simple control protocol between here and pmlogger
 *  - only write from app to pmlogger
 *  - no ack
 *  - commands are
 *	V<number>\n	- version
 *	F<folio>\n	- folio name
 *	P<name>\n	- launcher's name
 *	R[<msg>]\n	- launcher knows how to replay
 *	D[<msg>]\n	- detach from launcher
 *	Q[<msg>]\n	- quit pmlogger
 *	?[<msg>]\n	- display session status
 */
static int
xmit_to_logger(int fd, char tag, const char *msg)
{
    int		sts;
#ifdef HAVE_SIGPIPE
    SIG_PF	user_onpipe;
#endif

    if (fd < 0)
	return PM_ERR_IPC;

#ifdef HAVE_SIGPIPE
    user_onpipe = signal(SIGPIPE, SIG_IGN);
#endif
    sts = (int)write(fd, &tag, 1);
    if (sts != 1)
	goto fail;

    if (msg != NULL) {
	int	len = (int)strlen(msg);
	sts = (int)write(fd, msg, len);
	if (sts != len)
	    goto fail;
    }

    sts = (int)write(fd, "\n", 1);
    if (sts != 1)
	goto fail;

#ifdef HAVE_SIGPIPE
    signal(SIGPIPE, user_onpipe);
#endif
    return 0;

fail:
    if (oserror() == EPIPE)
	sts = PM_ERR_IPC;
    else
	sts = -oserror();
#ifdef HAVE_SIGPIPE
    signal(SIGPIPE, user_onpipe);
#endif
    return sts;
}
#endif

int
pmRecordControl(pmRecordHost *rhp, int request, const char *msg)
{
#ifndef IS_MINGW	/* not yet ported */
    pid_t	pid;
    record_t	*rp;
    int		sts;
    int		ok;
    int		cmd;
    int		mypipe[2];
    static int	maxseenfd = -1;

    /*
     * harvest old and smelly pmlogger instances
     */
    while ((pid = waitpid(-1, &sts, WNOHANG)) > 0) {
	for (rp = record; rp != NULL; rp = rp->next) {
	    if (pid == rp->public.pid) {
		rp->public.status = sts;
		if (rp->public.fd_ipc != -1) {
		    close(rp->public.fd_ipc);
		    rp->public.fd_ipc = -1;
		}
		break;
	    }
	}
    }

    sts = 0;

    switch (request) {

    case PM_REC_ON:
	    if (state != PM_REC_HOST || rhp != NULL) {
		sts = -EINVAL;
		break;
	    }

	    for (rp = record; rp != NULL; rp = rp->next) {
		if (rp->state != PM_REC_HOST)
		    continue;
		if (rp->isdefault) {
		    fprintf(f_folio, "%-15s %-23s %s\n", "Archive:", rp->host, rp->base);
		    break;
		}
	    }

	    for (rp = record; rp != NULL; rp = rp->next) {
		if (rp->state != PM_REC_HOST || rp->isdefault)
		    continue;
		fprintf(f_folio, "%-15s %-23s %s\n", "Archive:", rp->host, rp->base);
	    }

	    fflush(stderr);
	    fflush(stdout);
	    if (_replay)
		fflush(f_replay);
	    fflush(f_folio);

	    for (rp = record; rp != NULL; rp = rp->next) {
		if (rp->state != PM_REC_HOST)
		    continue;
		fclose(rp->public.f_config);
		rp->public.f_config = NULL;
		if (pipe1(mypipe) < 0) {
		    sts = -oserror();
		    break;
		}
		if (mypipe[1] > maxseenfd)
		    maxseenfd = mypipe[1];
		if (mypipe[0] > maxseenfd)
		    maxseenfd = mypipe[0];

		if ((rp->public.pid = fork()) == 0) {
		    /* pmlogger */
		    char	loggerpath[MAXPATHLEN];
		    char	fdnum[4];
		    int		fd;
		    int		i;

		    close(mypipe[1]);
		    pmsprintf(fdnum, sizeof(fdnum), "%d", mypipe[0]);
		    if (dir != NULL) {
			/* trim trailing separator */
			dir[strlen(dir)-1] = '\0';
			if (chdir(dir) < 0) {
			    /* not good! */
			    exit(1);
			}
		    }
		    /*
		     * leave stdin, tie stdout and stderr together, leave
		     * the ipc fd and close all other fds
		     */
		    dup2(2, 1);	/* stdout -> stderr */
		    for (fd = 3; fd <= maxseenfd; fd++) {
			if (fd != mypipe[0])
			    close(fd);
		    }
		    /*
		     * have:
		     *	argv[0] ... argv[argc-1] from PM_REC_SETARG
		     * want:
		     *	argv[0]				"pmlogger"
		     *  argv[1] ... argv[argc]		from PM_REC_SETARG
		     *  argv[argc+1], argv[argc+2]	"-x" fdnum
		     *  argv[argc+3], argv[argc+4]	"-h" host
		     *  argv[argc+5], argv[argc+6]	"-l" log
		     *  argv[argc+7], argv[argc+8]	"-c" config
		     *  argv[argc+9]			basename
		     *  argv[argc+10]			NULL
		     */
		    rp->argv = (char **)realloc(rp->argv, (rp->argc+11)*sizeof(rp->argv[0]));
		    if (rp->argv == NULL) {
			pmNoMem("pmRecordControl: argv[]", (rp->argc+11)*sizeof(rp->argv[0]), PM_FATAL_ERR);
			/*NOTREACHED*/
		    }
		    for (i = rp->argc; i > 0; i--)
			rp->argv[i] = rp->argv[i-1];
		    rp->argv[0] = "pmlogger";
		    i = rp->argc+1;
		    rp->argv[i++] = "-x";
		    rp->argv[i++] = fdnum;
		    rp->argv[i++] = "-h";
		    rp->argv[i++] = rp->host;
		    rp->argv[i++] = "-l";
		    rp->argv[i++] = rp->logfile;
		    rp->argv[i++] = "-c";
		    rp->argv[i++] = rp->config;
		    rp->argv[i++] = rp->base;
		    rp->argv[i++] = NULL;
#if DESPERATE
fprintf(stderr, "Launching pmlogger:");
for (i = 0; i < rp->argc+11; i++) fprintf(stderr, " %s", rp->argv[i]);
fputc('\n', stderr);
#endif
		    pmsprintf(loggerpath, sizeof(loggerpath), "%s%cpmlogger",
			pmGetConfig("PCP_BINADM_DIR"), pmPathSeparator());
		    execv(loggerpath, rp->argv);

		    /* this is really bad! */
		    exit(1);
		}
		else if (rp->public.pid < (pid_t)0) {
		    sts = -oserror();
		    break;
		}
		else {
		    /* the application launching pmlogger */
		    close(mypipe[0]);
		    rp->public.fd_ipc = mypipe[1];
		    /* send the protocol version */
		    ok = xmit_to_logger(rp->public.fd_ipc, 'V', "0");
		    if (ok < 0) {
			/* remember last failure */
			sts = ok;
			rp->public.fd_ipc = -1;
			continue;
		    }
		    /* send the folio name */
		    ok = xmit_to_logger(rp->public.fd_ipc, 'F', _folio);
		    if (ok < 0) {
			/* remember last failure */
			sts = ok;
			rp->public.fd_ipc = -1;
			continue;
		    }
		    /* send the my name */
		    ok = xmit_to_logger(rp->public.fd_ipc, 'P', _creator);
		    if (ok < 0) {
			/* remember last failure */
			sts = ok;
			rp->public.fd_ipc = -1;
			continue;
		    }
		    /* do we know how to replay? */
		    if (_replay) {
			ok = xmit_to_logger(rp->public.fd_ipc, 'R', NULL);
			if (ok < 0) {
			    /* remember last failure */
			    sts = ok;
			    rp->public.fd_ipc = -1;
			    continue;
			}
		    }
		}
		rp->state = PM_REC_ON;
	    }

	    if (sts < 0) {
		for (rp = record; rp != NULL; rp = rp->next) {
		    if (rp->public.fd_ipc >= 0) {
			close(rp->public.fd_ipc);
			rp->public.fd_ipc = -1;
		    }
		}
	    }
	    else {
		if (_replay) {
		    fclose(f_replay);
		    f_replay = NULL;
		}
		fclose(f_folio);
		f_folio = NULL;
		state = PM_REC_ON;
		n_alive = n_record;
	    }
	    break;

    case PM_REC_STATUS:
	    cmd = '?';
	    goto broadcast;

    case PM_REC_OFF:
	    cmd = 'Q';
	    goto broadcast;

    case PM_REC_DETACH:
	    cmd = 'D';

broadcast:
	    if (state != PM_REC_ON) {
		sts = -EINVAL;
		break;
	    }

	    for (rp = record; rp != NULL; rp = rp->next) {
		if (rhp != NULL && rhp != &rp->public)
		    continue;
		if (rp->state != PM_REC_ON) {
		    if (rhp != NULL)
			/* explicit pmlogger, should be "on" */
			sts = -EINVAL;
		    continue;
		}
		if (rp->public.fd_ipc >= 0) {
		    ok = xmit_to_logger(rp->public.fd_ipc, cmd, msg);
		    if (ok < 0) {
			/* remember last failure */
			sts = ok;
			rp->public.fd_ipc = -1;
		    }
		}
		else
		    sts = PM_ERR_IPC;

		if (cmd != '?') {
		    n_alive--;
		    rp->state = PM_REC_OFF;
		    if (rp->public.fd_ipc != -1) {
			close(rp->public.fd_ipc);
			rp->public.fd_ipc = -1;
		    }
		}
	    }

	    if (n_alive <= 0)
		state = PM_REC_OFF;

	    break;

    case PM_REC_SETARG:
	    if (state != PM_REC_HOST) {
		sts = -EINVAL;
		break;
	    }

	    for (rp = record; rp != NULL; rp = rp->next) {
		if (rhp != NULL && rhp != &rp->public)
		    continue;
		rp->argv = (char **)realloc(rp->argv, (rp->argc+1)*sizeof(rp->argv[0]));
		if (rp->argv == NULL) {
		    sts = -oserror();
		    rp->argc = 0;
		}
		else {
		    rp->argv[rp->argc] = strdup(msg);
		    if (rp->argv[rp->argc] == NULL)
			sts = -oserror();
		    else
			rp->argc++;
		}
	    }
	    break;

    default:
	    sts = -EINVAL;
	    break;
    }

    return sts;
#else
    return -EINVAL;
#endif
}
