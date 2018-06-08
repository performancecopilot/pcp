/*
 * Copyright (c) 2012-2018 Red Hat.
 * Copyright (c) 1995-2001,2003 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <ctype.h>
#include <limits.h>
#include <sys/stat.h>
#include "logger.h"
#include <errno.h>

char		*configfile;		/* current config filename, must be *alloc()d */
__pmLogCtl	logctl;
__pmArchCtl	archctl;
int		exit_samples = -1;       /* number of samples 'til exit */
__int64_t	exit_bytes = -1;         /* number of bytes 'til exit */
__int64_t	vol_bytes;		 /* total in earlier volumes */
struct timeval  exit_time;               /* time interval 'til exit */
int		vol_switch_samples = -1; /* number of samples 'til vol switch */
__int64_t	vol_switch_bytes = -1;   /* number of bytes 'til vol switch */
struct timeval	vol_switch_time;         /* time interval 'til vol switch */
int		vol_samples_counter;     /* Counts samples - reset for new vol*/
int		vol_switch_afid = -1;    /* afid of event for vol switch */
int		vol_switch_flag;         /* sighup received - switch vol now */
int		vol_switch_alarm;	 /* vol_switch_callback() called */
int		run_done_alarm;		 /* run_done_callback() called */
int		log_alarm;	 	 /* log_callback() called */
int		parse_done;
int		primary;		/* Non-zero for primary pmlogger */
char	    	*archBase;		/* base name for log files */
char		*pmcd_host;
char		*pmcd_host_conn;
char		*pmcd_host_label;
int		host_context = PM_CONTEXT_HOST;	 /* pmcd / local context mode */
int		archive_version = PM_LOG_VERS02; /* Type of archive to create */
int		linger = 0;		/* linger with no tasks/events */
int		rflag;			/* report sizes */
int		Cflag;			/* parse config and exit */
struct timeval	epoch;
struct timeval	delta = { 60, 0 };	/* default logging interval */
int		sig_code;		/* caught signal */
int		qa_case;		/* QA error injection state */
char		*note;			/* note for port map file */

static int 	    pmcdfd = -1;	/* comms to pmcd */
static __pmFdSet    fds;		/* file descriptors mask for select */
static int	    numfds;		/* number of file descriptors in mask */

static int	rsc_fd = -1;	/* recording session control see -x */
static int	rsc_replay;
static time_t	rsc_start;
static char	*rsc_prog = "<unknown>";
static char	*folio_name = "<unknown>";
static char	*dialog_title = "PCP Archive Recording Session";
static int	sep;

void
run_done(int sts, char *msg)
{
    int	lsts;

    if (pmDebugOptions.log && pmDebugOptions.desperate) {
	fprintf(stderr, "run_done(%d, %s) last_log_offset=%d last_stamp=",
		sts, msg, last_log_offset);
	pmPrintStamp(stderr, &last_stamp);
	fputc('\n', stderr);
    }

    if ((lsts = do_epilogue()) < 0)
	fprintf(stderr, "Warning: problem writing archive epilogue: %s\n",
	    pmErrStr(lsts));

    if (msg != NULL)
    	fprintf(stderr, "pmlogger: %s, exiting\n", msg);
    else
    	fprintf(stderr, "pmlogger: End of run time, exiting\n");

    /*
     * write the last last temporal index entry with the time stamp
     * of the last pmResult and the seek pointer set to the offset
     * _before_ the last log record
     */
    if (last_stamp.tv_sec != 0) {
	pmTimeval	tmp;
	tmp.tv_sec = (__int32_t)last_stamp.tv_sec;
	tmp.tv_usec = (__int32_t)last_stamp.tv_usec;
	if (last_log_offset < sizeof(__pmLogLabel)+2*sizeof(int)) {
	    fprintf(stderr, "run_done: Botch: last_log_offset = %ld\n", (long)last_log_offset);
	}
	__pmFseek(archctl.ac_mfp, last_log_offset, SEEK_SET);
	__pmLogPutIndex(&archctl, &tmp);
    }

    exit(sts);
}

/*
 * Warning: called in signal handler context ... be careful
 */
static void
run_done_callback(int i, void *j)
{
    run_done_alarm = 1;
}

/*
 * Warning: called in signal handler context ... be careful
 */
static void
vol_switch_callback(int i, void *j)
{
    vol_switch_alarm = 1;
}

static int
maxfd(void)
{
    int i;
    int	max = 0;

    for (i = 0; i < CFD_NUM; ++i) {
	if (ctlfds[i] > max)
	    max = ctlfds[i];
    }
    if (clientfd > max)
	max = clientfd;
    if (pmcdfd > max)
	max = pmcdfd;
    if (rsc_fd > max)
	max = rsc_fd;
    return max;
}

/*
 * tolower_str - convert a string to all lowercase
 */
static void 
tolower_str(char *str)
{
    char *s = str;

    while (*s) {
      *s = tolower((int)*s);
      s++;
    }
}

/*
 * ParseSize - parse a size argument given in a command option
 *
 * The size can be in one of the following forms:
 *   "40"    = sample counter of 40
 *   "40b"   = byte size of 40
 *   "40Kb"  = byte size of 40*1024 bytes = 40 kilobytes (kibibytes)
 *   "40Mb"  = byte size of 40*1024*1024 bytes = 40 megabytes (mebibytes)
 *   time-format = time delta in seconds
 *
 */
static int
ParseSize(char *size_arg, int *sample_counter, __int64_t *byte_size, 
          struct timeval *time_delta)
{
    long x = 0; /* the size number */
    char *ptr = NULL;
    char *interval_err;

    *sample_counter = -1;
    *byte_size = -1;
    time_delta->tv_sec = -1;
    time_delta->tv_usec = -1;
  
    x = strtol(size_arg, &ptr, 10);

    /* must be positive */
    if (x <= 0)
	return -1;

    if (*ptr == '\0') {
	/* we have consumed entire string as a long */
	/* => we have a sample counter */
	*sample_counter = x;
	return 1;
    }

    /* we have a number followed by something else */
    if (ptr != size_arg) {
	int len;

	tolower_str(ptr);

	/* chomp off plurals */
	len = strlen(ptr);
	if (ptr[len-1] == 's')
	    ptr[len-1] = '\0';

	/* if bytes */
	if (strcmp(ptr, "b") == 0 ||
	    strcmp(ptr, "byte") == 0) {
	    *byte_size = x;
	    return 1;
	}  

	/* if kilobytes */
	if (strcmp(ptr, "k") == 0 ||
	    strcmp(ptr, "kb") == 0 ||
	    strcmp(ptr, "kib") == 0 ||
	    strcmp(ptr, "kbyte") == 0 ||
	    strcmp(ptr, "kibibyte") == 0 ||
	    strcmp(ptr, "kilobyte") == 0) {
	    *byte_size = x*1024;
	    return 1;
	}

	/* if megabytes */
	if (strcmp(ptr, "m") == 0 ||
	    strcmp(ptr, "mb") == 0 ||
	    strcmp(ptr, "mib") == 0 ||
	    strcmp(ptr, "mbyte") == 0 ||
	    strcmp(ptr, "mebibyte") == 0 ||
	    strcmp(ptr, "megabyte") == 0) {
	    *byte_size = x*1024*1024;
	    return 1;
	}

	/* if gigabytes */
	if (strcmp(ptr, "g") == 0 ||
	    strcmp(ptr, "gb") == 0 ||
	    strcmp(ptr, "gib") == 0 ||
	    strcmp(ptr, "gbyte") == 0 ||
	    strcmp(ptr, "gibibyte") == 0 ||
	    strcmp(ptr, "gigabyte") == 0) {
	    *byte_size = ((__int64_t)x)*1024*1024*1024;
	    return 1;
	}
    }

    /* Doesn't fit pattern above, try a time interval */
    if (pmParseInterval(size_arg, time_delta, &interval_err) >= 0)
        return 1;
    /* error message not used here */
    free(interval_err);
  
    /* Doesn't match anything, return an error */
    return -1;
}

/* time manipulation */
static void
tsub(struct timeval *a, struct timeval *b)
{
    pmtimevalDec(a, b);
    if (a->tv_sec < 0) {
        /* clip negative values at zero */
        a->tv_sec = 0;
        a->tv_usec = 0;
    }
}

static char *
do_size(double d)
{
    static char nbuf[100];

    if (d < 10 * 1024)
	pmsprintf(nbuf, sizeof(nbuf), "%ld bytes", (long)d);
    else if (d < 10.0 * 1024 * 1024)
	pmsprintf(nbuf, sizeof(nbuf), "%.1f Kbytes", d/1024);
    else if (d < 10.0 * 1024 * 1024 * 1024)
	pmsprintf(nbuf, sizeof(nbuf), "%.1f Mbytes", d/(1024 * 1024));
    else
	pmsprintf(nbuf, sizeof(nbuf), "%ld Mbytes", (long)d/(1024 * 1024));
    
    return nbuf;
}

/*
 * add text identified by p to the malloc buffer at bp[0] ... bp[nchar -1]
 * return the length of the result or -1 for an error
 */
static int
add_msg(char **bp, int nchar, char *p)
{
    int		add_len;
    char	*tmp_bp;

    if (nchar < 0 || p == NULL)
	return nchar;

    add_len = (int)strlen(p);
    if (nchar == 0)
	add_len++;
    if ((tmp_bp = (char *)realloc(*bp, nchar+add_len)) == NULL) {
	free(*bp);
	*bp = NULL;
	return -1;
    }
    *bp = tmp_bp;
    if (nchar == 0)
	strcpy(*bp, p);
    else
	strcat(&(*bp)[nchar-1], p);

    return nchar+add_len;
}


/*
 * generate dialog/message when launching application wishes to break
 * its association with pmlogger
 *
 * cmd is one of the following:
 *	D	detach pmlogger and let it run forever
 *	Q	terminate pmlogger
 *	?	display status
 *	X	fatal error or application exit ... user must decide
 *		to detach or quit
 */
void
do_dialog(char cmd)
{
    FILE	*msgf = NULL;
    time_t	now;
    static char	lbuf[100+MAXPATHLEN];
    double	archsize;
    char	*q;
    char	*p = NULL;
    int		nchar;
    char	*msg;
#if HAVE_MKSTEMP
    char	tmp[MAXPATHLEN];
#endif
    int			sts;
    __pmExecCtl_t	*argp = NULL;

    time(&now);
    now -= rsc_start;
    if (now == 0)
	/* hack is close enough! */
	now = 1;

    archsize = vol_bytes + __pmFtell(archctl.ac_mfp);

    nchar = add_msg(&p, 0, "");
    p[0] = '\0';

    pmsprintf(lbuf, sizeof(lbuf), "PCP recording for the archive folio \"%s\" and the host", folio_name);
    nchar = add_msg(&p, nchar, lbuf);
    if (now < 240)
	pmsprintf(lbuf, sizeof(lbuf), " \"%s\" has been in progress for %ld seconds",
	pmcd_host, (long)now);
    else
	pmsprintf(lbuf, sizeof(lbuf), " \"%s\" has been in progress for %ld minutes",
	pmcd_host, (long)((now + 30)/60));
    nchar = add_msg(&p, nchar, lbuf);
    nchar = add_msg(&p, nchar, " and in that time the pmlogger process has created an");
    nchar = add_msg(&p, nchar, " archive of ");
    q = do_size(archsize);
    nchar = add_msg(&p, nchar, q);
    nchar = add_msg(&p, nchar, ".");
    if (rsc_replay) {
	nchar = add_msg(&p, nchar, "\n\nThis archive may be replayed with the following command:\n");
	pmsprintf(lbuf, sizeof(lbuf), "  $ pmafm %s replay", folio_name);
	nchar = add_msg(&p, nchar, lbuf);
    }

    if (cmd == 'D') {
	nchar = add_msg(&p, nchar, "\n\nThe application that launched pmlogger has asked pmlogger");
	nchar = add_msg(&p, nchar, " to continue independently and the PCP archive will grow at");
	nchar = add_msg(&p, nchar, " the rate of ");
	q = do_size((archsize * 3600) / now);
	nchar = add_msg(&p, nchar, q);
	nchar = add_msg(&p, nchar, " per hour or ");
	q = do_size((archsize * 3600 * 24) / now);
	nchar = add_msg(&p, nchar, q);
	nchar = add_msg(&p, nchar, " per day.");
    }

    if (cmd == 'X') {
	nchar = add_msg(&p, nchar, "\n\nThe application that launched pmlogger has exited and you");
	nchar = add_msg(&p, nchar, " must decide if the PCP recording session should be terminated");
	nchar = add_msg(&p, nchar, " or continued.  If recording is continued the PCP archive will");
	nchar = add_msg(&p, nchar, " grow at the rate of ");
	q = do_size((archsize * 3600) / now);
	nchar = add_msg(&p, nchar, q);
	nchar = add_msg(&p, nchar, " per hour or ");
	q = do_size((archsize * 3600 * 24) / now);
	nchar = add_msg(&p, nchar, q);
	nchar = add_msg(&p, nchar, " per day.");
    }

    if (cmd == 'Q') {
	nchar = add_msg(&p, nchar, "\n\nThe application that launched pmlogger has terminated");
	nchar = add_msg(&p, nchar, " this PCP recording session.\n");
    }

    if (cmd != 'Q') {
	nchar = add_msg(&p, nchar, "\n\nAt any time this pmlogger process may be terminated with the");
	nchar = add_msg(&p, nchar, " following command:\n");
	pmsprintf(lbuf, sizeof(lbuf), "  $ pmsignal -s TERM %" FMT_PID "\n", (pid_t)getpid());
	nchar = add_msg(&p, nchar, lbuf);
    }

    if (cmd == 'X')
	nchar = add_msg(&p, nchar, "\n\nTerminate this PCP recording session now?");

    if (nchar > 0) {
	char * xconfirm = __pmNativePath(pmGetConfig("PCP_XCONFIRM_PROG"));
	int fd = -1;

#if HAVE_MKSTEMP
	pmsprintf(tmp, sizeof(tmp), "%s%cmsgXXXXXX", pmGetConfig("PCP_TMPFILE_DIR"), pmPathSeparator());
	msg = tmp;
	fd = mkstemp(tmp);
#else
	if ((msg = tmpnam(NULL)) != NULL)
	    fd = open(msg, O_WRONLY|O_CREAT|O_EXCL, 0600);
#endif
	if (fd >= 0)
	    msgf = fdopen(fd, "w");
	if (msgf == NULL) {
	    fprintf(stderr, "\nError: failed create temporary message file for recording session dialog\n");
	    fprintf(stderr, "Reason? %s\n", osstrerror());
	    if (fd != -1)
		close(fd);
	    goto failed;
	}
	fputs(p, msgf);
	fclose(msgf);
	msgf = NULL;

	if (cmd == 'X')
	    pmsprintf(lbuf, sizeof(lbuf), "%s -c -header \"%s - %s\" -file %s -icon question "
			  "-B Yes -b No 2>/dev/null",
		    xconfirm, dialog_title, rsc_prog, msg);
	else
	    pmsprintf(lbuf, sizeof(lbuf), "%s -c -header \"%s - %s\" -file %s -icon info "
			  "-b Close 2>/dev/null",
		    xconfirm, dialog_title, rsc_prog, msg);


	if ((sts = __pmProcessUnpickArgs(&argp, lbuf)) < 0) {
	    fprintf(stderr, "\nError: __pmProcessUnpickArgs failed for recording session dialog\n");
	    fprintf(stderr, "Command: \"%s\"\n", lbuf);
	    fprintf(stderr, "Error: %s\n", pmErrStr(sts));
	    goto failed;
	}
	if ((sts = __pmProcessPipe(&argp, "r", PM_EXEC_TOSS_NONE, &msgf)) < 0) {
	    fprintf(stderr, "\nError: failed to start command for recording session dialog\n");
	    fprintf(stderr, "Command: \"%s\"\n", lbuf);
	    goto failed;
	}

	if (fgets(lbuf, sizeof(lbuf), msgf) == NULL) {
	    fprintf(stderr, "\n%s: pmconfirm(1) failed for recording session dialog\n",
		    cmd == '?' ? "Warning" : "Error");
failed:
	    fprintf(stderr, "Dialog:\n");
	    fputs(p, stderr);
	    strcpy(lbuf, "Yes");
	}
	else {
	    /* strip at first newline */
	    for (q = lbuf; *q && *q != '\n'; q++)
		;
	    *q = '\0';
	}

	if (msgf != NULL)
	    __pmProcessPipeClose(msgf);
	unlink(msg);
    }
    else {
	fprintf(stderr, "Error: failed to create recording session dialog message!\n");
	fprintf(stderr, "Reason? %s\n", osstrerror());
	strcpy(lbuf, "Yes");
    }

    free(p);

    if (cmd == 'Q' || (cmd == 'X' && strcmp(lbuf, "Yes") == 0)) {
	run_done(0, "Recording session terminated");
	/*NOTREACHED*/
    }

    if (cmd != '?') {
	/* detach, silently go off to the races ... */
	close(rsc_fd);
	__pmFD_CLR(rsc_fd, &fds);
	rsc_fd = -1;
    }
}

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("Options"),
    { "config", 1, 'c', "FILE", "file to load configuration from" },
    { "check", 0, 'C', 0, "parse configuration and exit" },
    PMOPT_DEBUG,
    PMOPT_HOST,
    { "labelhost", 1, 'H', "LABELHOST", "override the hostname written into the label" },
    { "log", 1, 'l', "FILE", "redirect diagnostics and trace output" },
    { "linger", 0, 'L', 0, "run even if not primary logger instance and nothing to log" },
    { "note", 1, 'm', "MSG", "descriptive note to be added to the port map file" },
    PMOPT_SPECLOCAL,
    { "local-PMDA", 0, 'o', 0, "metrics sourced without connecting to pmcd" },
    PMOPT_NAMESPACE,
    { "PID", 1, 'p', "PID", "Log specified metric for the lifetime of the pid" },
    { "primary", 0, 'P', 0, "execute as primary logger instance" },
    { "report", 0, 'r', 0, "report record sizes and archive growth rate" },
    { "size", 1, 's', "SIZE", "terminate after endsize has been accumulated" },
    { "interval", 1, 't', "DELTA", "default logging interval [default 60.0 seconds]" },
    PMOPT_FINISH,
    { "", 0, 'u', 0, "output is unbuffered [default now, so -u is a no-op]" },
    { "username", 1, 'U', "USER", "in daemon mode, run as named user [default pcp]" },
    { "volsize", 1, 'v', "SIZE", "switch log volumes after size has been accumulated" },
    { "version", 1, 'V', "NUM", "version for archive (default and only version is 2)" },
    { "", 1, 'x', "FD", "control file descriptor for running from pmRecordControl(3)" },
    { "", 0, 'y', 0, "set timezone for times to local time rather than from PMCD host" },
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "c:CD:h:H:l:K:Lm:n:op:Prs:T:t:uU:v:V:x:y?",
    .long_options = longopts,
    .short_usage = "[options] archive",
};

static FILE *
do_pmcpp(char *configfile)
{
    FILE	*f;
    char	cmd[3*MAXPATHLEN+80];
    char	*bin_dir = pmGetConfig("PCP_BINADM_DIR");
    char	*lib_dir = pmGetConfig("PCP_VAR_DIR");
    int		sts;
    __pmExecCtl_t	*argp = NULL;

    if (configfile != NULL) {
	if ((f = fopen(configfile, "r")) == NULL) {
	    fprintf(stderr, "%s: Cannot open config file \"%s\": %s\n",
		pmGetProgname(), configfile, osstrerror());
	    exit(1);
	}
	fclose(f);
    }

    if (bin_dir == NULL) {
	fprintf(stderr, "%s: pmGetConfig: cannot get $PCP_BINADM_DIR value\n",
		pmGetProgname());
	exit(1);
    }
    if (lib_dir == NULL) {
	fprintf(stderr, "%s: pmGetConfig: cannot get $PCP_VAR_DIR value\n",
		pmGetProgname());
	exit(1);
    }

    pmsprintf(cmd, sizeof(cmd), "%s%cpmcpp -rs %s -I %s%cconfig%cpmlogger",
	bin_dir, sep, configfile == NULL ? "" : configfile, lib_dir, sep, sep);
    fprintf(stderr, "preprocessor cmd: %s\n", cmd);

    if ((sts = __pmProcessUnpickArgs(&argp, cmd)) < 0) {
	fprintf(stderr, "%s: __pmProcessUnpickArgs(..., \"%s\") failed: %s\n",
		pmGetProgname(), cmd, pmErrStr(sts));
	exit(1);
    }

    if ((sts = __pmProcessPipe(&argp, "r", PM_EXEC_TOSS_NONE, &f)) < 0) {
	fprintf(stderr, "%s: __pmProcessPipe for \"%s\" failed: %s\n",
		pmGetProgname(), cmd, pmErrStr(sts));
	exit(1);
    }

    return f;
}

int
main(int argc, char **argv)
{
    int			c;
    int			sts;
    int			use_localtime = 0;
    int			isdaemon = 0;
    char		*pmnsfile = PM_NS_DEFAULT;
    char		*username;
    char		*logfile = "pmlogger.log";
				    /* default log (not archive) file name */
    char		*endnum;
    int			i;
    task_t		*tp;
    optcost_t		ocp;
    __pmFdSet		readyfds;
    char		*p;
    char		*runtime = NULL;
    int	    		ctx;		/* handle corresponding to ctxp below */
    __pmContext  	*ctxp;		/* pmlogger has just this one context */
    int			niter;
    pid_t               target_pid = 0;
    int			exit_code = 0;
    char		*exit_msg;

    pmGetUsername(&username);
    sep = pmPathSeparator();
    if ((endnum = getenv("PMLOGGER_INTERVAL")) != NULL)
	delta.tv_sec = atoi(endnum);

    /*
     * Warning:
     *		If any of the pmlogger options change, make sure the
     *		corresponding changes are made to pmnewlog when pmlogger
     *		options are passed through from the control file
     */
    while ((c = pmgetopt_r(argc, argv, &opts)) != EOF) {
	switch (c) {

	case 'c':		/* config file */
	    if (access(opts.optarg, F_OK) == 0)
		configfile = strdup(opts.optarg);
	    else {
		/* does not exist as given, try the standard place */
		char *sysconf = pmGetConfig("PCP_VAR_DIR");
		int sz = strlen(sysconf)+strlen("/config/pmlogger/")+strlen(opts.optarg)+1;
		if ((configfile = (char *)malloc(sz)) == NULL)
		    pmNoMem("config file name", sz, PM_FATAL_ERR);
		pmsprintf(configfile, sz,
			"%s%c" "config%c" "pmlogger%c" "%s",
			sysconf, sep, sep, sep, opts.optarg);
		if (access(configfile, F_OK) != 0) {
		    /* still no good, error handling happens below */
		    free(configfile);
		    configfile = strdup(opts.optarg);
		}
	    }
	    break;

	case 'C':		/* parse config and exit */
	    Cflag = 1;
	    break;

	case 'D':	/* debug flag */
	    sts = pmSetDebug(opts.optarg);
	    if (sts < 0) {
		pmprintf("%s: unrecognized debug flag specification (%s)\n",
			pmGetProgname(), opts.optarg);
		opts.errors++;
	    }
	    break;

	case 'h':		/* hostname for PMCD to contact */
	    pmcd_host_conn = opts.optarg;
	    break;

	case 'H':		/* hostname to put in label*/
	    pmcd_host_label = strndup(opts.optarg, PM_LOG_MAXHOSTLEN-1);
	    break;

	case 'l':		/* log file name */
	    logfile = opts.optarg;
	    break;

	case 'K':
	    if ((endnum = pmSpecLocalPMDA(opts.optarg)) != NULL) {
		pmprintf("%s: pmSpecLocalPMDA failed: %s\n",
			pmGetProgname(), endnum);
		opts.errors++;
	    }
	    break;

	case 'L':		/* linger if not primary logger */
	    linger = 1;
	    break;

	case 'm':		/* note for port map file */
	    note = opts.optarg;
	    isdaemon = ((strcmp(note, "pmlogger_check") == 0) ||
			(strcmp(note, "pmlogger_daily") == 0));
	    break;

	case 'n':		/* alternative name space file */
	    pmnsfile = opts.optarg;
	    break;

	case 'o':		/* local context mode, no pmcd */
	    /*
	     * Note, using Lflag here because this has the same
	     * semantics as -L for all the other PCP commands, but
	     * -L was already taken (for "linger") in pmlogger, so
	     * we're forced to use -o on the command line.
	     */
	    host_context = PM_CONTEXT_LOCAL;
	    opts.Lflag = 1;
	    break;

	case 'p':
	    target_pid = (int)strtol(opts.optarg, &endnum, 10);
	    if (*endnum != '\0') {
		pmprintf("%s: invalid process identifier (%s)\n",
			 pmGetProgname(), opts.optarg);
		opts.errors++;
	    } else if (!__pmProcessExists(target_pid)) {
		pmprintf("%s: PID error - no such process (%" FMT_PID ")\n",
			 pmGetProgname(), target_pid);
		opts.errors++;
	    }
	    break;

	case 'P':		/* this is the primary pmlogger */
	    primary = 1;
	    isdaemon = 1;
	    break;

	case 'r':		/* report sizes of pmResult records */
	    rflag = 1;
	    break;

	case 's':		/* exit size */
	    sts = ParseSize(opts.optarg, &exit_samples, &exit_bytes, &exit_time);
	    if (sts < 0) {
		pmprintf("%s: illegal size argument '%s' for exit size\n",
			pmGetProgname(), opts.optarg);
		opts.errors++;
	    }
	    else if (exit_time.tv_sec > 0) {
		__pmAFregister(&exit_time, NULL, run_done_callback);
	    }
	    break;

	case 'T':		/* end time */
	    runtime = opts.optarg;
            break;

	case 't':		/* change default logging interval */
	    if (pmParseInterval(opts.optarg, &delta, &p) < 0) {
		pmprintf("%s: illegal -t argument\n%s", pmGetProgname(), p);
		free(p);
		opts.errors++;
	    }
	    break;

	case 'U':		/* run as named user */
	    username = opts.optarg;
	    isdaemon = 1;
	    break;

	case 'u':		/* flush output buffers after each fetch */
	    /*
	     * all archive write I/O is unbuffered now, so maintain -u
	     * for backwards compatibility only
	     */
	    break;

	case 'v':		/* volume switch after given size */
	    sts = ParseSize(opts.optarg, &vol_switch_samples, &vol_switch_bytes,
			    &vol_switch_time);
	    if (sts < 0) {
		pmprintf("%s: illegal size argument '%s' for volume size\n", 
			pmGetProgname(), opts.optarg);
		opts.errors++;
	    }
	    else if (vol_switch_time.tv_sec > 0) {
		vol_switch_afid = __pmAFregister(&vol_switch_time, NULL, 
						 vol_switch_callback);
            }
	    break;

        case 'V': 
	    archive_version = (int)strtol(opts.optarg, &endnum, 10);
	    if (*endnum != '\0' || archive_version != PM_LOG_VERS02) {
		pmprintf("%s: -V requires a version number of %d\n",
			 pmGetProgname(), PM_LOG_VERS02); 
		opts.errors++;
	    }
	    break;

	case 'x':		/* recording session control fd */
	    rsc_fd = (int)strtol(opts.optarg, &endnum, 10);
	    if (*endnum != '\0' || rsc_fd < 0) {
		pmprintf("%s: -x requires a non-negative numeric argument\n", pmGetProgname());
		opts.errors++;
	    }
	    else {
		time(&rsc_start);
	    }
	    break;

	case 'y':
	    use_localtime = 1;
	    break;

	case '?':
	default:
	    opts.errors++;
	    break;
	}
    }

    if (pmcd_host_conn != NULL && primary) {
	pmprintf(
	    "%s: -P and -h are mutually exclusive; use -P only when running\n"
	    "%s on the same (local) host as the PMCD to which it connects.\n",
		pmGetProgname(), pmGetProgname());
	opts.errors++;
    }

    if (pmcd_host_conn != NULL && host_context == PM_CONTEXT_LOCAL) {
	pmprintf(
	    "%s: -o and -h are mutually exclusive; use -o only when running\n"
	    "%s on the same (local) host as the DSO PMDA(s) being used.\n",
		pmGetProgname(), pmGetProgname());
	opts.errors++;
    }

    if (!opts.errors && opts.optind != argc - 1) {
	pmprintf("%s: insufficient arguments\n", pmGetProgname());
	opts.errors++;
    }

    if (opts.errors) {
	pmUsageMessage(&opts);
	exit(1);
    }

    if (rsc_fd != -1 && note == NULL) {
	/* add default note to indicate running with -x */
	static char	xnote[10];
	pmsprintf(xnote, sizeof(xnote), "-x %d", rsc_fd);
	note = xnote;
    }

    /* if we are running as a daemon, change user early */
    if (isdaemon)
	pmSetProcessIdentity(username);

    if (Cflag == 0) {
	pmOpenLog("pmlogger", logfile, stderr, &sts);
	if (sts != 1) {
	    fprintf(stderr, "%s: Warning: log file (%s) creation failed\n", pmGetProgname(), logfile);
	    /* continue on ... writing to stderr */
	}
    }

    /* base name for archive is here ... */
    archBase = argv[opts.optind];

    /* initialise access control */
    if (__pmAccAddOp(PM_OP_LOG_ADV) < 0 ||
	__pmAccAddOp(PM_OP_LOG_MAND) < 0 ||
	__pmAccAddOp(PM_OP_LOG_ENQ) < 0) {
	fprintf(stderr, "%s: access control initialisation failed\n", pmGetProgname());
	exit(1);
    }

    if (pmnsfile != PM_NS_DEFAULT) {
	if ((sts = pmLoadASCIINameSpace(pmnsfile, 1)) < 0) {
	    fprintf(stderr, "%s: Cannot load namespace from \"%s\": %s\n", pmGetProgname(), pmnsfile, pmErrStr(sts));
	    exit(1);
	}
    }

    if (host_context == PM_CONTEXT_LOCAL)
	pmcd_host_conn = "local context";
    else if (pmcd_host_conn == NULL)
	pmcd_host_conn = "local:";

    if ((ctx = pmNewContext(host_context, pmcd_host_conn)) < 0) {
	fprintf(stderr, "%s: Cannot connect to PMCD on host \"%s\": %s\n", pmGetProgname(), pmcd_host_conn, pmErrStr(ctx));
	exit(1);
    }
    pmcd_host = (char *)pmGetContextHostName(ctx);
    if (strlen(pmcd_host) == 0) {
	fprintf(stderr, "%s: pmGetContextHostName(%d) failed\n",
	    pmGetProgname(), ctx);
	exit(1);
    }

    if (rsc_fd == -1 && host_context != PM_CONTEXT_LOCAL) {
	/* no -x, so register client id with pmcd */
	__pmSetClientIdArgv(argc, argv);
    }

    /*
     * discover fd for comms channel to PMCD ... 
     */
    if (host_context != PM_CONTEXT_LOCAL) {
	if ((ctxp = __pmHandleToPtr(ctx)) == NULL) {
	    fprintf(stderr, "%s: botch: __pmHandleToPtr(%d) returns NULL!\n", pmGetProgname(), ctx);
	    exit(1);
	}
	pmcdfd = ctxp->c_pmcd->pc_fd;
	PM_UNLOCK(ctxp->c_lock);
    }

    yyin = do_pmcpp(configfile);
    /* do not return unless yyin is valid */
    if (configfile == NULL)
	configfile = strdup("<stdin>");

    __pmOptFetchGetParams(&ocp);
    ocp.c_scope = 1;
    __pmOptFetchPutParams(&ocp);

    /* prevent early timer events ... */
    __pmAFblock();

    if (yyparse() != 0)
	exit(1);
    __pmProcessPipeClose(yyin);
    yyend();

    fprintf(stderr, "Config parsed\n");

    if (pmDebugOptions.log) {
	fprintf(stderr, "optFetch Cost Parameters: pmid=%d indom=%d fetch=%d scope=%d\n",
		ocp.c_pmid, ocp.c_indom, ocp.c_fetch, ocp.c_scope);

	fprintf(stderr, "\nAfter loading config ...\n");
	for (tp = tasklist; tp != NULL; tp = tp->t_next) {
	    if (tp->t_numvalid == 0)
		continue;
	    fprintf(stderr, " state: %sin log, %savail, %s, %s",
		PMLC_GET_INLOG(tp->t_state) ? "" : "not ",
		PMLC_GET_AVAIL(tp->t_state) ? "" : "un",
		PMLC_GET_MAND(tp->t_state) ? "mand" : "adv",
		PMLC_GET_ON(tp->t_state) ? "on" : "off");
	    fprintf(stderr, " delta: %ld usec", 
			(long)(1000 * tp->t_delta.tv_sec + tp->t_delta.tv_usec));
	    fprintf(stderr, " numpmid: %d\n", tp->t_numpmid);
	    for (i = 0; i < tp->t_numpmid; i++) {
		fprintf(stderr, "  %s (%s):\n", pmIDStr(tp->t_pmidlist[i]), tp->t_namelist[i]);
	    }
	    __pmOptFetchDump(stderr, tp->t_fetch);
	}
    }

    if (Cflag)
	exit(0);

    fprintf(stderr, "Starting %slogger for host \"%s\" via \"%s\"\n",
            primary ? "primary " : "", pmcd_host, pmcd_host_conn);

    if (!primary && tasklist == NULL && !linger) {
	fprintf(stderr, "Nothing to log, and not the primary logger instance ... good-bye\n");
	exit(1);
    }

    if (pmcd_host_label != NULL) {
	pmcd_host=pmcd_host_label;
    }

    archctl.ac_log = &logctl;
    if ((sts = __pmLogCreate(pmcd_host, archBase, archive_version, &archctl)) < 0) {
	fprintf(stderr, "__pmLogCreate: %s\n", pmErrStr(sts));
	exit(1);
    }
    else {
	/*
	 * try and establish $TZ from the remote PMCD ...
	 * Note the label record has been set up, but not written yet
	 */
	char		*name = "pmcd.timezone";
	pmID		pmid;
	pmResult	*resp;

	pmtimevalNow(&epoch);
	sts = pmUseContext(ctx);

	if (sts >= 0)
	    sts = pmLookupName(1, &name, &pmid);
	if (sts >= 0)
	    sts = pmFetch(1, &pmid, &resp);
	if (sts >= 0) {
	    if (resp->vset[0]->numval > 0) { /* pmcd.timezone present */
		strcpy(logctl.l_label.ill_tz, resp->vset[0]->vlist[0].value.pval->vbuf);
		/* prefer to use remote time to avoid clock drift problems */
		epoch = resp->timestamp;		/* struct assignment */
		if (! use_localtime)
		    pmNewZone(logctl.l_label.ill_tz);
	    }
	    else if (pmDebugOptions.log) {
		fprintf(stderr,
			"main: Could not get timezone from host %s\n",
			pmcd_host);
	    }
	    pmFreeResult(resp);
	}
    }

    /* do ParseTimeWindow stuff for -T */
    if (runtime) {
        struct timeval res_end;    /* time window end */
        struct timeval start;
        struct timeval end;
        struct timeval last_delta;
        char *err_msg;             /* parsing error message */
        time_t now;
        struct timeval now_tv;

        time(&now);
        now_tv.tv_sec = now;
        now_tv.tv_usec = 0; 

        start = now_tv;
        end.tv_sec = INT_MAX;
        end.tv_usec = INT_MAX;
        sts = __pmParseTime(runtime, &start, &end, &res_end, &err_msg);
        if (sts < 0) {
	    fprintf(stderr, "%s: illegal -T argument\n%s", pmGetProgname(), err_msg);
            exit(1);
        }

        last_delta = res_end;
        tsub(&last_delta, &now_tv);
	__pmAFregister(&last_delta, NULL, run_done_callback);

        last_stamp = res_end;
    }

    fprintf(stderr, "Archive basename: %s\n", archBase);

#ifndef IS_MINGW
    /* detach yourself from the launching process */
    if (isdaemon)
        setpgid(getpid(), 0);
#endif

    /* set up control port */
    init_ports();
    __pmFD_ZERO(&fds);
    for (i = 0; i < CFD_NUM; ++i) {
	if (ctlfds[i] >= 0)
	    __pmFD_SET(ctlfds[i], &fds);
    }
#ifndef IS_MINGW
    if (pmcdfd != -1)
	__pmFD_SET(pmcdfd, &fds);
#endif
    if (rsc_fd != -1)
	__pmFD_SET(rsc_fd, &fds);
    numfds = maxfd() + 1;

    if ((sts = do_prologue()) < 0)
	fprintf(stderr, "Warning: problem writing archive prologue: %s\n",
	    pmErrStr(sts));

    sts = 0;		/* default exit status */

    parse_done = 1;	/* enable callback processing */
    __pmAFunblock();

    for ( ; ; ) {
	int		nready;

	if (pmDebugOptions.appl2 && pmDebugOptions.desperate) {
	    fprintf(stderr, "before __pmSelectRead(%d,...): run_done_alarm=%d vol_switch_alarm=%d log_alarm=%d\n", numfds, run_done_alarm, vol_switch_alarm, log_alarm);
	}

	niter = 0;
	while (log_alarm && niter++ < 10) {
	    __pmAFblock();
	    log_alarm = 0;
	    if (pmDebugOptions.appl2)
		fprintf(stderr, "delayed callback: log_alarm\n");
	    for (tp = tasklist; tp != NULL; tp = tp->t_next) {
		if (tp->t_alarm) {
		    tp->t_alarm = 0;
		    do_work(tp);
		}
	    }
	    __pmAFunblock();
	}

	if (vol_switch_alarm) {
	    __pmAFblock();
	    vol_switch_alarm = 0;
	    if (pmDebugOptions.appl2)
		fprintf(stderr, "delayed callback: vol_switch_alarm\n");
	    newvolume(VOL_SW_TIME);
	    __pmAFunblock();
	}

	if (run_done_alarm) {
	    if (pmDebugOptions.appl2)
		fprintf(stderr, "delayed callback: run_done_alarm\n");
	    run_done(0, NULL);
	    /*NOTREACHED*/
	}

	__pmFD_COPY(&readyfds, &fds);
	nready = __pmSelectRead(numfds, &readyfds, NULL);

	if (pmDebugOptions.appl2 && pmDebugOptions.desperate) {
	    fprintf(stderr, "__pmSelectRead(%d,...) done: nready=%d run_done_alarm=%d vol_switch_alarm=%d log_alarm=%d\n", numfds, nready, run_done_alarm, vol_switch_alarm, log_alarm);
	}

	__pmAFblock();
	if (nready > 0) {

	    /* handle request on control port */
	    for (i = 0; i < CFD_NUM; ++i) {
		if (ctlfds[i] >= 0 && __pmFD_ISSET(ctlfds[i], &readyfds)) {
		    if (control_req(ctlfds[i])) {
			/* new client has connected */
			__pmFD_SET(clientfd, &fds);
			if (clientfd >= numfds)
			    numfds = clientfd + 1;
		    }
		}
	    }
	    if (clientfd >= 0 && __pmFD_ISSET(clientfd, &readyfds)) {
		/* process request from client, save clientfd in case client
		 * closes connection, resetting clientfd to -1
		 */
		int	fd = clientfd;

		if (client_req()) {
		    /* client closed connection */
		    __pmFD_CLR(fd, &fds);
		    __pmCloseSocket(clientfd);
		    clientfd = -1;
		    pmlc_host[0] = '\0';
		    numfds = maxfd() + 1;
		    qa_case = 0;
		}
	    }
#ifndef IS_MINGW
	    if (pmcdfd >= 0 && __pmFD_ISSET(pmcdfd, &readyfds)) {
		/*
		 * do not expect this, given synchronous commumication with the
		 * pmcd ... either pmcd has terminated, or bogus PDU ... or its
		 * Win32 and we are operating under the different conditions of
		 * our AF.c implementation there, which has to deal with a lack
		 * of signal support on Windows - race condition exists between
		 * this check and the async event timer callback.
		 */
		__pmPDU		*pb;
		__pmPDUHdr	*php;
		sts = __pmGetPDU(pmcdfd, ANY_SIZE, TIMEOUT_NEVER, &pb);
		if (sts <= 0) {
		    if (sts < 0)
			fprintf(stderr, "Error: __pmGetPDU: %s\n", pmErrStr(sts));
		    disconnect(sts);
		}
		else {
		    php = (__pmPDUHdr *)pb;
		    fprintf(stderr, "Error: Unsolicited %s PDU from PMCD\n",
			__pmPDUTypeStr(php->type));
		    disconnect(PM_ERR_IPC);
		}
		if (sts > 0)
		    __pmUnpinPDUBuf(pb);
	    }
#endif
	    if (rsc_fd >= 0 && __pmFD_ISSET(rsc_fd, &readyfds)) {
		/*
		 * some action on the recording session control fd
		 * end-of-file means launcher has quit, otherwise we
		 * expect one of these commands
		 *	V<number>\n	- version
		 *	F<folio>\n	- folio name
		 *	P<name>\n	- launcher's name
		 *	R\n		- launcher can replay
		 *	D\n		- detach from launcher
		 *	Q\n		- quit pmlogger
		 */
		char	rsc_buf[MAXPATHLEN];
		char	*rp = rsc_buf;
		char	myc;
		int	fake_x = 0;

		for (rp = rsc_buf; ; rp++) {
		    if (read(rsc_fd, &myc, 1) <= 0) {
			if (pmDebugOptions.appl2)
			    fprintf(stderr, "recording session control: eof\n");
			if (rp != rsc_buf) {
			    *rp = '\0';
			    fprintf(stderr, "Error: incomplete recording session control message: \"%s\"\n", rsc_buf);
			}
			fake_x = 1;
			break;
		    }
		    if (rp >= &rsc_buf[MAXPATHLEN]) {
			fprintf(stderr, "Error: absurd recording session control message: \"%100.100s ...\"\n", rsc_buf);
			fake_x = 1;
			break;
		    }
		    if (myc == '\n') {
			*rp = '\0';
			break;
		    }
		    *rp = myc;
		}

		if (pmDebugOptions.appl2) {
		    if (fake_x == 0)
			fprintf(stderr, "recording session control: \"%s\"\n", rsc_buf);
		}

		if (fake_x)
		    do_dialog('X');
		else if (strcmp(rsc_buf, "Q") == 0 ||
		         strcmp(rsc_buf, "D") == 0 ||
			 strcmp(rsc_buf, "?") == 0)
		    do_dialog(rsc_buf[0]);
		else if (rsc_buf[0] == 'F')
		    folio_name = strdup(&rsc_buf[1]);
		else if (rsc_buf[0] == 'P')
		    rsc_prog = strdup(&rsc_buf[1]);
		else if (strcmp(rsc_buf, "R") == 0)
		    rsc_replay = 1;
		else if (rsc_buf[0] == 'V' && rsc_buf[1] == '0') {
		    /*
		     * version 0 of the recording session control ...
		     * this is all we grok at the moment
		     */
		    ;
		}
		else {
		    fprintf(stderr, "Error: illegal recording session control message: \"%s\"\n", rsc_buf);
		    do_dialog('X');
		}
	    }
	}
	else if (vol_switch_flag) {
	    newvolume(VOL_SW_SIGHUP);
	    vol_switch_flag = 0;
	}
	else if (nready < 0 && neterror() != EINTR)
	    fprintf(stderr, "Error: select: %s\n", netstrerror());

	__pmAFunblock();

	if (target_pid && !__pmProcessExists(target_pid)) {
	    exit_msg = "process from -p has vanished";
	    break;
	}

	if (sig_code) {
	    static char sig_msg[100];
	    pmsprintf(sig_msg, sizeof(sig_msg), "Caught signal %d", sig_code);
	    exit_msg = sig_msg;
	    break;
	}
    }

    run_done(exit_code, exit_msg);
    /*NOTREACHED*/
    return(0);
}

int
newvolume(int vol_switch_type)
{
    __pmFILE	*newfp;
    int		nextvol = archctl.ac_curvol + 1;
    time_t	now;
    static char *vol_sw_strs[] = {
       "SIGHUP", "pmlc request", "sample counter",
       "sample byte size", "sample time", "max data volume size"
    };

    vol_samples_counter = 0;
    vol_bytes += __pmFtell(archctl.ac_mfp);
    if (exit_bytes != -1) {
        if (vol_bytes >= exit_bytes) 
	    run_done(0, "Byte limit reached");
    }

    /* 
     * If we are not part of a callback but instead a 
     * volume switch from "pmlc" or a "SIGHUP" then
     * get rid of pending volume switch in event queue
     * as it will now be wrong, and schedule a new
     * volume switch event.
     */
    if (vol_switch_afid >= 0 && vol_switch_type != VOL_SW_TIME) {
      __pmAFunregister(vol_switch_afid);
      vol_switch_afid = __pmAFregister(&vol_switch_time, NULL,
                                   vol_switch_callback);
    }

    if ((newfp = __pmLogNewFile(archBase, nextvol)) != NULL) {
	if (logctl.l_state == PM_LOG_STATE_NEW) {
	    /*
	     * nothing has been logged as yet, force out the label records
	     */
	    pmtimevalNow(&last_stamp);
	    logctl.l_label.ill_start.tv_sec = (__int32_t)last_stamp.tv_sec;
	    logctl.l_label.ill_start.tv_usec = (__int32_t)last_stamp.tv_usec;
	    logctl.l_label.ill_vol = PM_LOG_VOL_TI;
	    __pmLogWriteLabel(logctl.l_tifp, &logctl.l_label);
	    logctl.l_label.ill_vol = PM_LOG_VOL_META;
	    __pmLogWriteLabel(logctl.l_mdfp, &logctl.l_label);
	    logctl.l_label.ill_vol = 0;
	    __pmLogWriteLabel(archctl.ac_mfp, &logctl.l_label);
	    logctl.l_state = PM_LOG_STATE_INIT;
	}

	/*
	 * Note:
	 * 	No need to update the temporal index here.
	 *	The new volume is empty (other than the label record)
	 *	at this stage, but when the first data record is
	 *	added we update the temporal index at that time ...
	 *	this happens in do_work() over in callback.c
	 */

	__pmFclose(archctl.ac_mfp);
	archctl.ac_mfp = newfp;
	logctl.l_label.ill_vol = archctl.ac_curvol = nextvol;
	__pmLogWriteLabel(archctl.ac_mfp, &logctl.l_label);
	time(&now);
	fprintf(stderr, "New log volume %d, via %s at %s",
		nextvol, vol_sw_strs[vol_switch_type], ctime(&now));
	return nextvol;
    }
    else
	return -oserror();
}

void
disconnect(int sts)
{
    time_t  		now;
    int			ctx;
    __pmContext		*ctxp = NULL;	/* pander to cppcheck */

    if ((ctx = pmWhichContext()) >= 0)
	ctxp = __pmHandleToPtr(ctx);
    if (ctx < 0 || ctxp == NULL) {
	fprintf(stderr, "%s: disconnect botch: cannot get context: %s\n", pmGetProgname(), pmErrStr(ctx));
	exit(1);
    }
    /*
     * Note: This application is single threaded, and once we have ctxp
     *	     the associated __pmContext will not move and will only be
     *	     accessed or modified synchronously either here or in libpcp.
     *	     We unlock the context so that it can be locked as required
     *	     within libpcp.
     */
    PM_UNLOCK(ctxp->c_lock);

    if (pmcdfd != -1) {
	/*
	 * only do this once per disconnect event ... because there
	 * multiple callbacks and PDU interchange attempts after a
	 * disconnect and before a successful pmReconnectContext(),
	 * only need to shut down the channel and report once.
	 */
	if (sts != -EINTR) {
	    time(&now);
	    if (sts != 0)
		fprintf(stderr, "%s: Error: %s\n", pmGetProgname(), pmErrStr(sts));
	    fprintf(stderr, "%s: Lost connection to PMCD on \"%s\" at %s",
		pmGetProgname(), pmcd_host, ctime(&now));
	}
	if (pmcdfd != -1) {
	    close(pmcdfd);
	    __pmFD_CLR(pmcdfd, &fds);
	    pmcdfd = -1;
	}
	numfds = maxfd() + 1;
	ctxp->c_pmcd->pc_fd = -1;
    }
}

int
reconnect(void)
{
    int	    		sts;
    int			ctx;
    time_t		now;
    __pmContext		*ctxp = NULL;	/* pander to cppcheck */

    if ((ctx = pmWhichContext()) >= 0)
	ctxp = __pmHandleToPtr(ctx);
    if (ctx < 0 || ctxp == NULL) {
	fprintf(stderr, "%s: reconnect botch: cannot get context: %s\n",
		pmGetProgname(), pmErrStr(ctx));
	exit(1);
    }
    /*
     * Note: This application is single threaded, and once we have ctxp
     *	     the associated __pmContext will not move and will only be
     *	     accessed or modified synchronously either here or in libpcp.
     *	     We unlock the context so that it can be locked as required
     *	     within libpcp.
     */
    PM_UNLOCK(ctxp->c_lock);
    sts = pmReconnectContext(ctx);
    if (sts >= 0) {
	pmcdfd = ctxp->c_pmcd->pc_fd;
	__pmFD_SET(pmcdfd, &fds);
	numfds = maxfd() + 1;
    }
    if (sts < 0)
	return sts;

    time(&now);
    fprintf(stderr, "%s: re-established connection to PMCD on \"%s\" at %s",
	    pmGetProgname(), pmcd_host, ctime(&now));

    /*
     * Metrics may have changed while PMCD was unreachable, so we
     * need to recheck each metric to make sure that its PMID and
     * semantics have not changed.  We cannot recover if there is
     * an incompatible change - must defer to controlling scripts
     * or processes (a new-named archive will have to be created,
     * from a new pmlogger process, and pmlogrewrite/pmlogextract
     * will need to become involved if they need to be merged).
     */
    validate_metrics();

    /*
     * All metrics have been validated, however, this state change
     * represents a potential gap in the stream of metrics.  So we
     * must store a <mark> record at this point.
     */
     if ((sts = putmark()) < 0) {
	fprintf(stderr, "putmark: %s\n", pmErrStr(sts));
	exit(1);
    }

    return 0;
}
