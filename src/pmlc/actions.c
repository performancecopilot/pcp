/*
 * Copyright (c) 2014 Red Hat.
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <inttypes.h>
#include "pmapi.h"
#include "impl.h"
#include "pmlc.h"

/* for the pmlogger/PMCD we currently have a connection to */
static int	logger_fd = -1;		/* file desc pmlogger */
static char	*lasthost;		/* host that logger_ctx is for */
static int	src_ctx = -1;		/* context for logged host's PMCD*/
static char	*srchost;		/* host that logged_ctx is for */

static time_t	tmp;		/* for pmCtime */

/* PCP 1.x pmlogger returns one of these when a status request is made */
typedef struct {
    __pmTimeval  ls_start;	/* start time for log */
    __pmTimeval  ls_last;	/* last time log written */
    __pmTimeval  ls_timenow;	/* current time */
    int		ls_state;	/* state of log (from __pmLogCtl) */
    int		ls_vol;		/* current volume number of log */
    __int64_t	ls_size;	/* size of current volume */
    char	ls_hostname[PM_LOG_MAXHOSTLEN];
				/* name of pmcd host */
    char	ls_tz[40];      /* $TZ at collection host */
    char	ls_tzlogger[40]; /* $TZ at pmlogger */
} __pmLoggerStatus_v1;

static int
IsLocal(const char *hostspec)
{
    if (strcmp(hostspec, "localhost") == 0 ||
	strcmp(hostspec, "local:") == 0 ||
	strcmp(hostspec, "unix:") == 0)
	return 1;
    return 0;
}
int
ConnectPMCD(void)
{
    int			sts;
    __pmPDU		*pb = NULL;

    if (src_ctx >= 0)
	return src_ctx;

    if (__pmVersionIPC(logger_fd) >= LOG_PDU_VERSION2) {
	__pmLoggerStatus	*lsp;
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PDU)
	     fprintf(stderr, "pmlc: sending version 2 status request\n");
#endif
	if ((sts = __pmSendLogRequest(logger_fd, LOG_REQUEST_STATUS)) < 0) {
	    fprintf(stderr, "Error sending request to pmlogger: ");
	    if (still_connected(sts))
		fprintf(stderr, "%s\n", pmErrStr(sts));
	    return sts;
	}
	if ((sts = __pmGetPDU(logger_fd, ANY_SIZE, __pmLoggerTimeout(), &pb)) <= 0) {
	    if (sts == 0)
		/* end of file! */
		sts = PM_ERR_IPC;
	    fprintf(stderr, "Error receiving response from pmlogger: ");
	    if (still_connected(sts))
		fprintf(stderr, "%s\n", pmErrStr(sts));
	    return sts;
	}
	if (sts == PDU_ERROR) {
	    __pmDecodeError(pb, &sts);
	    fprintf(stderr, "Error: ");
	    if (still_connected(sts))
		fprintf(stderr, "%s\n", pmErrStr(sts));
	    sts = 0;
	    goto done;
	}
	if (sts != PDU_LOG_STATUS) {
	    fprintf(stderr, "Error PDU response from pmlogger %s", __pmPDUTypeStr(sts));
	    fprintf(stderr, " not %s as expected\n", __pmPDUTypeStr(PDU_LOG_STATUS));
	    sts = 0;
	    goto done;
	}
	sts = __pmDecodeLogStatus(pb, &lsp);
	__pmUnpinPDUBuf(pb);
	if (sts < 0) {
	    fprintf(stderr, "Error decoding response from pmlogger: ");
	    if (still_connected(sts))
		fprintf(stderr, "%s\n", pmErrStr(sts));
	    goto done;
	} 
	if (IsLocal(lsp->ls_fqdn)) {
	    /*
	     * if pmcd host is "localhost"-alike then use hostname that
	     * was used to contact pmlogger, as from here (where pmlc is
	     * running) "localhost" is likely to connect us to the wrong
	     * pmcd or no pmcd at all.
	     */
	    srchost = strdup(lasthost);
	    if (srchost == NULL)
		__pmNoMem("Error copying host name", strlen(lasthost), PM_FATAL_ERR);
	}
	else {
	    srchost = strdup(lsp->ls_fqdn);
	    if (srchost == NULL)
		__pmNoMem("Error copying host name", strlen(lsp->ls_fqdn), PM_FATAL_ERR);
	}
    }

    if ((sts = pmNewContext(PM_CONTEXT_HOST, srchost)) < 0) {
	/* no PMCD connection, we can't do anything, give up */
	fprintf(stderr, "Error trying to connect to PMCD on %s: %s\n",
		srchost, pmErrStr(sts));
    }
    else
        src_ctx = sts;

done:
    if (pb)
	__pmUnpinPDUBuf(pb);
    return sts;
}

int
ConnectLogger(char *hostname, int *pid, int *port)
{
    int		sts;

    if (lasthost != NULL) {
	free(lasthost);
	lasthost = NULL;
    }
    DisconnectLogger();

    if (src_ctx != -1) {
	if ((sts = pmDestroyContext(src_ctx)) < 0)
		fprintf(stderr, "Error deleting PMCD connection to %s: %s\n",
			srchost, pmErrStr(sts));
	src_ctx = -1;
    }
    if (srchost != NULL) {
	free(srchost);
	srchost = NULL;
    }

    if ((sts = __pmConnectLogger(hostname, pid, port)) < 0) {
	logger_fd = -1;
	return sts;
    }
    else {
	logger_fd = sts;
	if ((lasthost = strdup(hostname)) == NULL) {
	    __pmNoMem("Error copying host name", strlen(hostname), PM_FATAL_ERR);
	}
	return 0;
    }
}

void
DisconnectLogger(void)
{
    if (logger_fd != -1) {
	__pmCloseSocket(logger_fd);
	logger_fd = -1;
	sleep(1);
    }
}

void
ShowLoggers(char *hostname)
{
    int		i, n;
    int		ctx;
    int		primary = -1;		/* ports[] index for primary logger */
    int		pport = -1;		/* port for primary logger */
    __pmLogPort	*ports;

    if ((n = __pmIsLocalhost(hostname)) == 0) {
	/* remote, need PMCD's help for __pmLogFindPort */
	if ((ctx = pmNewContext(PM_CONTEXT_HOST, hostname)) < 0) {
	    fprintf(stderr, "Error trying to connect to PMCD on %s: %s\n",
		hostname, pmErrStr(ctx));
	    return;
	}
    }
    else
	ctx = -1;

    if ((n = __pmLogFindPort(hostname, PM_LOG_ALL_PIDS, &ports)) < 0) {
	fprintf(stderr, "Error finding pmloggers on %s: ",
		hostname);
	if (still_connected(n))
	    fprintf(stderr, "%s\n", pmErrStr(n));
    }
    else if (n == 0)
	    printf("No pmloggers running on %s\n", hostname);
    else {
	/* find the position of the primary logger */
	for (i = 0; i < n; i++) {
	    if (ports[i].pid == PM_LOG_PRIMARY_PID) {
		pport = ports[i].port;
		break;
	    }
	}
	for (i = 0; i < n; i++) {
	    if (ports[i].port == pport) {
		primary = i;
		break;
	    }
	}
	printf("The following pmloggers are running on %s:\n    ", hostname);
	/* print any primary logger first, with its pid alias in parentheses) */
	if (primary != -1) {
	    printf("primary");
	    printf(" (%d)", ports[primary].pid);
	}
	/* now print everything except the primary logger */
	for (i = 0; i < n; i++) {
	    if (i != primary &&
		ports[i].pid != PM_LOG_PRIMARY_PID) {
		    printf(" %d", ports[i].pid);
	    }
	}
	putchar('\n');

	/* Note: Don't free ports, it's storage is managed by __pmLogFindPort() */
    }

    if (ctx >= 0)
	pmDestroyContext(ctx);
}

static void
PrintState(int state)
{
    static char	*units[] = {"msec", "sec ", "min ", "hour"};
    static int	factor[] = {1000, 60, 60, 24};
    int		nfactors = sizeof(factor) / sizeof(factor[0]);
    int		i, j, is_on;
    int		delta = PMLC_GET_DELTA(state);
    float	t = delta;
    int		frac;

    fputs(PMLC_GET_MAND(state) ? "mand " : "adv  ", stdout);
    is_on = PMLC_GET_ON(state);
    fputs(is_on ? "on  " : "off ", stdout);
    if (PMLC_GET_INLOG(state))
	fputs(PMLC_GET_AVAIL(state) ? "   " : "na ", stdout);
    else
	fputs("nl ", stdout);

    /* don't display time unless logging on */
    if (!is_on) {
	fputs("            ", stdout);
	return;
    }

    if (delta == 0) {
	fputs("        once", stdout);
	return;
    }

    for (i = 0; i < nfactors; i++) {
	if (t < factor[i])
	    break;
	t /= factor[i];
    }
    if (i >= nfactors)
	i = nfactors - 1;
    
    frac = (int) ((t - (int)t) * 1000);	/* get 3 decimal places */
    if (frac % 10)
	j = 3;
    else
	if (frac % 100)
	    j = 2;
	else
	    if (frac % 1000)
		j = 1;
	    else
		j = 0;
    fprintf(stdout, "%*.*f %s", 7 - j, j, t, units[i]);
    return;
}


/* this pmResult is built during parsing of each pmlc statement.
 * the metrics and indoms likewise
 */
extern pmResult	*logreq;
extern metric_t	*metric;
extern int	n_metrics;
extern indom_t	*indom;
extern int	n_indoms;

void
Query(void)
{
    int		i, j, k, inst;
    metric_t	*mp;
    pmValueSet	*vsp;
    pmResult	*res;

    if (!connected())
	return;

    if ((i = __pmControlLog(logger_fd, logreq, PM_LOG_ENQUIRE, 0, 0, &res)) < 0) {
	fprintf(stderr, "Error receiving response from pmlogger: ");
	if (still_connected(i))
	    fprintf(stderr, "%s\n", pmErrStr(i));
	return;
    }
    if (res == NULL) {
	fprintf(stderr, "Error: NULL result from __pmControlLog\n");
	return;
    }

    for (i = 0, mp = metric; i < n_metrics; i++, mp++) {
	vsp = res->vset[i];
	if (mp->pmid != vsp->pmid) {
	    fprintf(stderr, "GAK! %s not found in returned result\n", mp->name);
	    return;
	}
	puts(mp->name);
	if (vsp->numval < 0) {
	    fputs("    ", stdout);
	    puts(pmErrStr(vsp->numval));
	}
	else if (mp->indom == -1) {
	    fputs("    ", stdout);
	    PrintState(vsp->vlist[0].value.lval);
	    putchar('\n');
	}
	else if (mp->status.has_insts)
	    for (j = 0; j < mp->n_insts; j++) {
		inst = mp->inst[j];
		for (k = 0; k < vsp->numval; k++)
		    if (inst == vsp->vlist[k].inst)
			break;
		if (k >= vsp->numval) {
		    printf("                  [%d] (not found)\n", inst);
		    continue;
		}
		fputs("    ", stdout);
		PrintState(vsp->vlist[k].value.lval);
		for (k = 0; k < indom[mp->indom].n_insts; k++)
		    if (inst == indom[mp->indom].inst[k])
			break;
		printf(" [%d or \"%s\"]\n", inst,
		       (k < indom[mp->indom].n_insts) ? indom[mp->indom].name[k] : "???");
	    }
	else {
	    if (vsp->numval <= 0)
		puts("    (no instances)");
	    else
		for (j = 0; j < vsp->numval; j++) {
		    fputs("    ", stdout);
		    PrintState(vsp->vlist[j].value.lval);
		    inst = vsp->vlist[j].inst;
		    for (k = 0; k < indom[mp->indom].n_insts; k++)
			if (inst == indom[mp->indom].inst[k])
			    break;
		    printf(" [%d or \"%s\"]\n",
			   inst,
			   (k < indom[mp->indom].n_insts) ? indom[mp->indom].name[k] : "???");
		}
	}
	putchar('\n');
    }
    pmFreeResult(res);
}

void LogCtl(int control, int state, int delta)
{
    int		i;
    metric_t	*mp;
    pmValueSet	*vsp;
    pmResult	*res;
    int		newstate, newdelta;	/* from pmlogger */
    int		expstate = 0;		/* expected from pmlogger */
    int		expdelta;
    int		firsterr = 1;
    unsigned	statemask;
    static char	*heading = "Warning: unable to change logging state for:";

    if (!connected())
	return;

    i = __pmControlLog(logger_fd, logreq, control, state, delta, &res);
    if (i < 0 && i != PM_ERR_GENERIC) {
	fprintf(stderr, "Error receiving response from pmlogger: ");
	if (still_connected(i))
	    fprintf(stderr, "%s\n", pmErrStr(i));
	return;
    }
    if (res == NULL) {
	fprintf(stderr, "Error: NULL result from __pmControlLog\n");
	return;
    }

    /* Set up the state that we expect pmlogger to return.  The encoding for
     * control and state passed to __pmControlLog differs from that returned
     * in the result.
     */
    statemask = 0;
    if (state != PM_LOG_MAYBE) {
	if (control == PM_LOG_MANDATORY)
	    PMLC_SET_MAND(expstate, 1);
	else
	    PMLC_SET_MAND(expstate, 0);
	if (state == PM_LOG_ON)
	    PMLC_SET_ON(expstate, 1);
	else
	    PMLC_SET_ON(expstate, 0);
	PMLC_SET_MAND(statemask, 1);
	PMLC_SET_ON(statemask, 1);
    }
    else {
	/* only mandatory+maybe is allowed by parser, which should return
	 * advisory+off OR advisory+on from pmlogger
	 */
	PMLC_SET_MAND(expstate, 0);
	PMLC_SET_MAND(statemask, 1);
	/* don't set ON bit in statemask; ignore returned on/off status */
    }

    expdelta = PMLC_GET_ON(expstate) ? delta : 0;

    for (i = 0, mp = metric; i < n_metrics; i++, mp++) {
	int	j, k, inst;
	int	hadinstmsg;
	char	*name;

	vsp = res->vset[i];
	if (mp->pmid != vsp->pmid) {
	    fprintf(stderr, "GAK! %s not found in returned result\n", mp->name);
	    return;
	}
	if (vsp->numval < 0) {
	    if (firsterr) {
		firsterr = 0;
		puts(heading);
	    }
	    printf("%s:%s\n", mp->name, pmErrStr(vsp->numval));
	    continue;
	}

	if (mp->indom == -1) {
	    newstate = PMLC_GET_STATE(vsp->vlist[0].value.lval) & statemask;
	    newdelta = PMLC_GET_DELTA(vsp->vlist[0].value.lval);
	    if (expstate != newstate || expdelta != newdelta) {
		if (firsterr) {
		    firsterr = 0;
		    puts(heading);
		}
		printf("%s\n\n", mp->name);
	    }
	}
	else if (mp->status.has_insts) {
	    long	val;

	    hadinstmsg = 0;
	    for (j = 0; j < mp->n_insts; j++) {
		inst = mp->inst[j];
		/* find inst name via mp->indom */
		for (k = 0; k < indom[mp->indom].n_insts; k++)
		    if (inst == indom[mp->indom].inst[k])
			break;
		if (k < indom[mp->indom].n_insts)
		    name = indom[mp->indom].name[k];
		else
		    name = "???";
		/* look for inst in pmValueSet from pmlogger */
		for (k = 0; k < vsp->numval; k++)
		    if (inst == vsp->vlist[k].inst)
			break;
		if (k >= vsp->numval) {
		    if (firsterr) {
			firsterr = 0;
			puts(heading);
		    }
		    if (!hadinstmsg) {
			hadinstmsg = 1;
			printf("%s\n", mp->name);
		    }
		    printf("    [%d or \"%s\"] instance not found\n", inst, name);
		    continue;
		}
		val = vsp->vlist[k].value.lval;
		newstate = (int)PMLC_GET_STATE(val) & statemask;
		newdelta = PMLC_GET_DELTA(val);
		if (expstate != newstate || expdelta != newdelta) {
		    if (firsterr) {
			firsterr = 0;
			puts(heading);
		    }
		    if (!hadinstmsg) {
			hadinstmsg = 1;
			printf("%s instance(s):\n", mp->name);
		    }
		    printf("    [%d or \"%s\"]\n", inst, name);
		}
	    }
	    if (hadinstmsg)
		putchar('\n');
	}
	else {
	    hadinstmsg = 0;
	    for (j = 0; j < vsp->numval; j++) {
		newstate = PMLC_GET_STATE(vsp->vlist[j].value.lval) & statemask;
		newdelta = PMLC_GET_DELTA(vsp->vlist[j].value.lval);
		if (expstate != newstate || expdelta != newdelta) {
		    if (firsterr) {
			firsterr = 0;
			puts(heading);
		    }
		    if (!hadinstmsg) {
			hadinstmsg = 1;
			printf("%s instance(s):\n", mp->name);
		    }
		    inst = vsp->vlist[j].inst;
		    for (k = 0; k < indom[mp->indom].n_insts; k++)
			if (inst == indom[mp->indom].inst[k])
			    break;
		    if (k < indom[mp->indom].n_insts)
			name = indom[mp->indom].name[k];
		    else
			name = "???";
		    printf("    [%d or \"%s\"]\n", inst, name);
		}
	    }
	    if (hadinstmsg)
		putchar('\n');
	}
    }
    pmFreeResult(res);
}

/*
 * Used to flag timezone changes.
 * These changes are only relevant to Status() so they are made here.
 */
int	tzchange = 0;

#define TZBUFSZ	30			/* for pmCtime buffers */

void Status(int pid, int primary)
{
    static int		localtz = -1;
    static char 	*ltzstr = "";	/* pmNewZone doesn't like null pointers */
    char		*str;
    __pmLoggerStatus	*lsp;
    static char		localzone[] = "local"; 
    static char		*zonename = localzone;
    char		*tzlogger;
    __pmTimeval		*start;
    __pmTimeval		*last;
    __pmTimeval		*timenow;
    char		*hostname;
    int			state;
    int			vol;
    __int64_t		size;
    char		startbuf[TZBUFSZ];
    char		lastbuf[TZBUFSZ];
    char		timenowbuf[TZBUFSZ];
    int			sts;
    __pmPDU		*pb;

    if (!connected())
	return;

    if (__pmVersionIPC(logger_fd) >= LOG_PDU_VERSION2) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PDU)
	    fprintf(stderr, "pmlc: sending version 2 status request\n");
#endif
	if ((sts = __pmSendLogRequest(logger_fd, LOG_REQUEST_STATUS)) < 0) {
	    fprintf(stderr, "Error sending status request to pmlogger: ");
	    if (still_connected(sts))
		fprintf(stderr, "%s\n", pmErrStr(sts));
	    return;
	}
	if ((sts = __pmGetPDU(logger_fd, ANY_SIZE, __pmLoggerTimeout(), &pb)) <= 0) {
	    if (sts == 0)
		/* end of file! */
		sts = PM_ERR_IPC;
	    fprintf(stderr, "Error receiving response from pmlogger: ");
	    if (still_connected(sts))
		fprintf(stderr, "%s\n", pmErrStr(sts));
	    return;
	}
	if (sts == PDU_ERROR) {
	    __pmDecodeError(pb, &sts);
	    fprintf(stderr, "Error: ");
	    if (still_connected(sts))
		fprintf(stderr, "%s\n", pmErrStr(sts));
	    sts = 0;
	    goto done;
	}
	if (sts != PDU_LOG_STATUS) {
	    fprintf(stderr, "Error PDU response from pmlogger %s", __pmPDUTypeStr(sts));
	    fprintf(stderr, " not %s as expected\n", __pmPDUTypeStr(PDU_LOG_STATUS));
	    __pmUnpinPDUBuf(pb);
	    return;
	}
	sts = __pmDecodeLogStatus(pb, &lsp);
	__pmUnpinPDUBuf(pb);
	if (sts < 0) {
	    fprintf(stderr, "Error decoding response from pmlogger: ");
	    if (still_connected(sts))
		fprintf(stderr, "%s\n", pmErrStr(sts));
	    return;
	}
	tzlogger = lsp->ls_tzlogger;
	start = &lsp->ls_start;
	last = &lsp->ls_last;
	timenow = &lsp->ls_timenow;
	hostname = lsp->ls_hostname;
	state = lsp->ls_state;
	vol = lsp->ls_vol;
	size = lsp->ls_size;
    }
    else {
	fprintf(stderr, "Error: logger IPC version < LOG_PDU_VERSION2, not supported\n");
	return;
    }

    if (tzchange) {
	switch (tztype) {
	    case TZ_LOCAL:
		if (localtz == -1) {
                    str = __pmTimezone();
		    if (str != NULL)
			ltzstr = str;
		    localtz = pmNewZone(ltzstr);
		    /* (exits if it fails) */
		}
		else
		    pmUseZone(localtz);
		zonename = localzone;
		break;

	    case TZ_LOGGER:
		if (tzlogger)
		    pmNewZone(tzlogger);	/* but keep me! */
		zonename = "pmlogger";
		break;

	    case TZ_OTHER:
		pmNewZone(tz);
		zonename = tz;
		break;
	}
    }
    tmp = start->tv_sec;
    pmCtime(&tmp, startbuf);
    startbuf[strlen(startbuf)-1] = '\0'; /* zap the '\n' at the end */
    tmp = last->tv_sec;
    pmCtime(&tmp, lastbuf);
    lastbuf[strlen(lastbuf)-1] = '\0';
    tmp = timenow->tv_sec;
    pmCtime(&tmp, timenowbuf);
    timenowbuf[strlen(timenowbuf)-1] = '\0';
    printf("pmlogger ");
    if (primary)
	printf("[primary]");
    else
	printf("[%d]", pid);
    printf(" on host %s is logging metrics from host %s\n",
	lasthost, hostname);
    /* NB: FQDN cleanup: note that this is not 'the fqdn' of the
       pmlogger host or that of its target.  */
    if (__pmVersionIPC(logger_fd) >= LOG_PDU_VERSION2)
	printf("PMCD host        %s\n",
		IsLocal(lsp->ls_fqdn) ? hostname : lsp->ls_fqdn);
    if (state == PM_LOG_STATE_NEW) {
	puts("logging hasn't started yet");
	goto done;
    }
    printf("log started      %s (times in %s time)\n"
	   "last log entry   %s\n"
	   "current time     %s\n"
	   "log volume       %d\n"
	   "log size         %" PRIi64 "\n",
	   startbuf, zonename, lastbuf, timenowbuf, vol, size);

done:
    return;

}

void
Sync(void)
{
    int			sts;
    __pmPDU		*pb;

    if (!connected())
	return;

    if (__pmVersionIPC(logger_fd) >= LOG_PDU_VERSION2) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PDU)
	    fprintf(stderr, "pmlc: sending version 2 sync request\n");
#endif
	if ((sts = __pmSendLogRequest(logger_fd, LOG_REQUEST_SYNC)) < 0) {
	    fprintf(stderr, "Error sending sync request to pmlogger: ");
	    if (still_connected(sts))
		fprintf(stderr, "%s\n", pmErrStr(sts));
	    return;
	}
    }

    if ((sts = __pmGetPDU(logger_fd, ANY_SIZE, __pmLoggerTimeout(), &pb)) != PDU_ERROR) {
	if (sts > 0)
	    __pmUnpinPDUBuf(pb);
	if (sts == 0)
	    /* end of file! */
	    sts = PM_ERR_IPC;
	fprintf(stderr, "Error receiving response from pmlogger: ");
	if (still_connected(sts))
	    fprintf(stderr, "%s\n", pmErrStr(sts));
	return;
    }
    __pmDecodeError(pb, &sts);
    __pmUnpinPDUBuf(pb);
    if (sts < 0) {
	fprintf(stderr, "Error: ");
	if (still_connected(sts))
	    fprintf(stderr, "%s\n", pmErrStr(sts));
	return;
    }

    return;
}

void
Qa(void)
{
    int			sts;
    __pmPDU		*pb;

    if (!connected())
	return;

    if (__pmVersionIPC(logger_fd) >= LOG_PDU_VERSION2) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PDU)
	    fprintf(stderr, "pmlc: sending version 2 qa request\n");
#endif
	if ((sts = __pmSendLogRequest(logger_fd, 100+qa_case)) < 0) {
	    fprintf(stderr, "Error sending qa request to pmlogger: ");
	    if (still_connected(sts))
		fprintf(stderr, "%s\n", pmErrStr(sts));
	    return;
	}
    }

    if ((sts = __pmGetPDU(logger_fd, ANY_SIZE, __pmLoggerTimeout(), &pb)) != PDU_ERROR) {
	if (sts > 0)
	    __pmUnpinPDUBuf(pb);
	if (sts == 0)
	    /* end of file! */
	    sts = PM_ERR_IPC;
	fprintf(stderr, "Error receiving response from pmlogger: ");
	if (still_connected(sts))
	    fprintf(stderr, "%s\n", pmErrStr(sts));
	return;
    }
    __pmDecodeError(pb, &sts);
    __pmUnpinPDUBuf(pb);
    if (sts < 0) {
	fprintf(stderr, "Error: ");
	if (still_connected(sts))
	    fprintf(stderr, "%s\n", pmErrStr(sts));
	return;
    }

    return;
}

void
NewVolume(void)
{
    int			sts;
    __pmPDU		*pb;

    if (!connected())
	return;

    if (__pmVersionIPC(logger_fd) >= LOG_PDU_VERSION2) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PDU)
	    fprintf(stderr, "pmlc: sending version 2 newvol request\n");
#endif
	if ((sts = __pmSendLogRequest(logger_fd, LOG_REQUEST_NEWVOLUME)) < 0) {
	    fprintf(stderr, "Error sending newvolume request to pmlogger: ");
	    if (still_connected(sts))
		fprintf(stderr, "%s\n", pmErrStr(sts));
	    return;
	}
    }

    if ((sts = __pmGetPDU(logger_fd, ANY_SIZE, __pmLoggerTimeout(), &pb)) != PDU_ERROR) {
	if (sts > 0)
	    __pmUnpinPDUBuf(pb);
	if (sts == 0)
	    /* end of file! */
	    sts = PM_ERR_IPC;
	fprintf(stderr, "Error receiving response from pmlogger: ");
	if (still_connected(sts))
	    fprintf(stderr, "%s\n", pmErrStr(sts));
	return;
    }
    __pmDecodeError(pb, &sts);
    __pmUnpinPDUBuf(pb);
    if (sts < 0) {
	fprintf(stderr, "Error: ");
	if (still_connected(sts))
	    fprintf(stderr, "%s\n", pmErrStr(sts));
	return;
    }
    else
	fprintf(stderr, "New log volume %d\n", sts);

    __pmUnpinPDUBuf(pb);
    return;
}

int
connected(void)
{
    if (logger_fd == -1) {
	yyerror("Not connected to any pmlogger instance");
	return 0;
    }
    else
	return 1;
}

int
still_connected(int sts)
{
    if (sts == PM_ERR_IPC || sts == -EPIPE) {
	fprintf(stderr, "Lost connection to the pmlogger instance\n");
	DisconnectLogger();
	return 0;
    }
    if (sts == PM_ERR_TIMEOUT) {
	fprintf(stderr, "Timeout, closed connection to the pmlogger instance\n");
	DisconnectLogger();
	return 0;
    }

    return 1;
}
