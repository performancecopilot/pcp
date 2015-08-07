/*
 * Copyright (c) 2012-2013 Red Hat.
 * Copyright (c) 1995-2005 Silicon Graphics, Inc.  All Rights Reserved.
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
 * PMCD routines for reading config file, creating children and
 * attaching to DSOs.
 */

#include "pmapi.h"
#include "impl.h"
#include "pmcd.h"
#include <ctype.h>
#include <sys/stat.h>
#if defined(HAVE_SYS_WAIT_H)
#include <sys/wait.h>
#endif
#if defined(HAVE_SYS_UN_H)
#include <sys/un.h>
#endif
#if defined(HAVE_DLFCN_H)
#include <dlfcn.h>
#elif defined(HAVE_DL_H)
#include <dl.h>
#endif

#define MIN_AGENTS_ALLOC	3	/* Number to allocate first time */
#define LINEBUF_SIZE 200

/* Config file modification time */
#if defined(HAVE_STAT_TIMESTRUC)
static struct timestruc configFileTime;
#elif defined(HAVE_STAT_TIMESPEC)
static struct timespec  configFileTime;
#elif defined(HAVE_STAT_TIMESPEC_T)
static timespec_t       configFileTime;
#elif defined(HAVE_STAT_TIME_T)
static time_t   	configFileTime;
#else
!bozo!
#endif

int		szAgents;		/* Number currently allocated */
int		mapdom[MAXDOMID+2];	/* The DomainId-to-AgentIndex map */
					/* Don't use it during parsing! */

static FILE	*inputStream;		/* Input stream for scanner */
static int	scanInit;
static int	scanError;		/* Problem in scanner */
static char	*linebuf;		/* Buffer for input stream */
static int	szLineBuf;		/* Allocated size of linebuf */
static char	*token;			/* Start of current token */
static char	*tokenend;		/* End of current token */
static int	nLines;			/* Current line of config file */
static int	linesize;		/* Length of line in linebuf */

/* Macro to compare a string with token.  The linebuf is always null terminated
 * so there are no nasty boundary conditions.
 */
#define TokenIs(str)	((tokenend - token) == strlen(str) && \
			 !strncasecmp(token, str, strlen(str)))

/* Return the numeric value of token (or zero if token is not numeric). */
static int
TokenNumVal(void)
{
    int		val = 0;
    char	*p = token;
    while (isdigit((int)*p)) {
	val = val * 10 + *p - '0';
	p++;
    }
    return val;
}

/* Return true if token is a numeric value */
static int
TokenIsNumber(void)
{
    char	*p;
    if (token == tokenend)		/* Nasty end of input case */
	return 0;
    for (p = token; isdigit((int)*p); p++)
	;
    return p == tokenend;
}

/* Return a strdup-ed copy of the current token. */
static char*
CopyToken(void)
{
    int		len = (int)(tokenend - token);
    char	*copy = (char *)malloc(len + 1);
    if (copy != NULL) {
	strncpy(copy, token, len);
	copy[len] = '\0';
    }
    return copy;
}

/* Get the next line from the input stream into linebuf. */

static void
GetNextLine(void)
{
    char	*end;
    int		more;			/* There is more line to read */
    int		still_to_read;
    int		atEOF = 0;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2)
	fprintf(stderr, "%d: GetNextLine()\n", nLines);
#endif

    if (szLineBuf == 0) {
	szLineBuf = LINEBUF_SIZE;
	linebuf = (char *)malloc(szLineBuf);
	if (linebuf == NULL)
	    __pmNoMem("pmcd config: GetNextLine init", szLineBuf, PM_FATAL_ERR);
    }

    linebuf[0] = '\0';
    token = linebuf;
    if (feof(inputStream))
	return;

    end = linebuf;
    more = 0;
    still_to_read = szLineBuf;
    do {
	/* Read into linebuf.  If more is set, the read is into a realloc()ed
	 * version of linebuf.  In this case, more is the number of characters
	 * at the end of the previous batch that should be overwritten
	 */
	if (fgets(end, still_to_read, inputStream) == NULL) {
	    if (!feof(inputStream)) {
		fprintf(stderr, "pmcd config[line %d]: Error: fgets failed: %s\n",
			     nLines, osstrerror());
		scanError = 1;
		return;
	    }
	    atEOF = 1;
	}

	linesize = (int)strlen(linebuf);
	more = 0;
	if (linesize == 0)
	    break;
	if (linebuf[linesize - 1] != '\n') {
	    if (feof(inputStream)) {
		/* Final input line has no '\n', so add one.  If a terminating
		 * null fits after it, that's the line, so break out of loop.
		 */
		linebuf[linesize] = '\n';
		/* Add terminating null if it fits */
		if (linesize + 1 < szLineBuf) {
		    linebuf[++linesize] = '\0';
		    break;
		}
		/* If no room for null, get more buffer space */
	    }
	    more = 1;			/* More buffer space required */
	}
	/* Check for continued lines */
	else if (linesize > 1 && linebuf[linesize - 2] == '\\') {
	    linebuf[linesize - 2] = ' ';
	    linesize--;			/* Pretend the \n isn't there */
	    more = 2;			/* Overwrite \n and \0 on next read */
	}
	    
	/* Make buffer larger to accomodate more of the line. */
	if (more) {
	    if (szLineBuf > 10 * LINEBUF_SIZE) {
		fprintf(stderr,
			     "pmcd config[line %d]: Error: ridiculously long line (%d characters)\n",
			     nLines+1, szLineBuf);
		linebuf[0] = '\0';
		scanError = 1;
		return;
	    }
	    szLineBuf += LINEBUF_SIZE;
	    if ((linebuf = realloc(linebuf, szLineBuf)) == NULL) {
		static char	fallback[2];

		__pmNoMem("pmcd config: GetNextLine", szLineBuf, PM_RECOV_ERR);
		linebuf = fallback;
		linebuf[0] = '\0';
		scanError = 1;
		return;
	    }
	    end = linebuf + linesize;
	    still_to_read = LINEBUF_SIZE + more;
	    /* *end is where the next fgets will start putting data.
	     * There is a special case if we are already at end of input:
	     * *end is the '\n' added to the line since it didn't have one.
	     * We are here because the terminating null wouldn't fit.
	     */
	    if (atEOF) {
		end[1] = '\0';
		linesize++;
		break;
	    }
	    token = linebuf;		/* We may have a new buffer */
	}
    } while (more);
    nLines++;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2) {
	fprintf(stderr, "\n===================NEWLINE=================\n\n");
	fprintf(stderr, "len = %d\nline = \"%s\"\n", (int)strlen(linebuf), linebuf);
    }
#endif
}

/* Advance through the input stream until either a non-whitespace character, a
 * newline, or end of input is reached.
 */
static void
SkipWhitespace(void)
{
    while (*token) {
	char	ch = *token;

	if (isspace((int)ch))
	    if (ch == '\n')			/* Stop at end of line */
		return;
	    else
		token++;
	else if (ch == '#') {
	    token = &linebuf[linesize-1];
	    return;
	}
	else
	    return;
    }
}

static int	scanReadOnly;	/* Set => don't modify input scanner */
static int	doingAccess;	/* Set => parsing [access] section */
static int	tokenQuoted;	/* True when token a quoted string */

/* Print the current token on a given stream. */

static void
PrintToken(FILE *stream)
{
    char	*p;
    if (tokenQuoted)
	fputc('"', stream);
    for (p = token; p < tokenend; p++) {
	if (*p == '\n')
	    fputs("<newline>", stream);
	else if (*p == '\0')
	    fputs("<null>", stream);
	else
	    fputc(*p, stream);
    }
    if (tokenQuoted)
	fputc('"', stream);
}

/* Move to the next token in the input stream.  This is done by skipping any
 * non-whitespace characters to get to the end of the current token then
 * skipping any whitespace and newline characters to get to the next token.
 */

static void
FindNextToken(void)
{
    static char	*rawToken;		/* Used in pathological quoting case */
    char	ch;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2) {
	fprintf(stderr, "FindNextToken() ");
	fprintf(stderr, "scanInit=%d scanError=%d scanReadOnly=%d doingAccess=%d tokenQuoted=%d token=%p tokenend=%p\n", scanInit, scanError, scanReadOnly, doingAccess, tokenQuoted, token, tokenend);
    }
#endif

    do {
	if (scanInit) {
	    if (*token == '\0')		/* EOF is EOF and that's final */
		return;
	    if (scanError)		/* Ditto for scanner errors */
		return;

	    if (*token == '\n')		/* If at end of line, get next line */
		GetNextLine();
	    else {			/* Otherwise move past "old" token */
		if (tokenQuoted) {	/* Move past last quote */ 
		    tokenend++;
		    tokenQuoted = 0;
		}
		token = tokenend;
	    }
	    SkipWhitespace();		/* Find null, newline or non-space  */
	}
	else {
	    scanInit = 1;
	    scanError = 0;
	    GetNextLine();
	    SkipWhitespace();		/* Don't return yet, find tokenend */
	}
    } while (doingAccess && *token == '\n');

    /* Now we have the start of a token.  Find the end. */

    ch = *token;
    if (ch == '\0' || ch == '\n') {
	tokenend = token;
	return;
    }

    if (doingAccess)
	if (ch == ',' || ch == ':' || ch == ';' || ch == '[' || ch == ']') {
	    tokenend = token + 1;
	    return;
	}

    rawToken = token;			/* Save real token start in case it moves */
    tokenend = token + 1;
    if (ch == '#')			/* For comments, token is newline */
	token = tokenend = &linebuf[linesize-1];
    else {
	int	inQuotes = *token == '"';
	int	fixToken = 0;

	do {
	    int gotEnd = isspace((int)*tokenend);

	    while (!gotEnd) {
		switch (*tokenend) {
		    case '#':		/* \# or # in quotes does not start a comment */
			if (*(tokenend - 1) == '\\' || inQuotes)
			    fixToken = 1;
			else		/* Comments don't need whitespace in front */
			    gotEnd = 1;
			break;

		    case ',':
		    case ':':
		    case ';':
		    case '[':
		    case ']':
			gotEnd = doingAccess && !inQuotes;
			break;

		    case '"':
			if (*(tokenend - 1) == '\\')
			    fixToken = 1;
			else {
			    if (inQuotes) {
				inQuotes = 0;
				gotEnd = 1;
			    }
			}
			break;

		    default:
			gotEnd = isspace((int)*tokenend);
		}
		if (gotEnd)
		    break;
		tokenend++;
	    }
	    /* Skip any whitespace if still in quotes, but stop at end of line */
	    if (inQuotes)
		while (isspace((int)*tokenend) && *tokenend != '\n')
		    tokenend++;
	} while (inQuotes && *tokenend != '\n');

	if (inQuotes) {
	    scanError = 1;
	    *token = 0;
	    tokenend = token;
	    fprintf(stderr,
			 "pmcd config[line %d]: Error: unterminated quoted string\n",
			 nLines);
	    return;
	}

	/* Replace any \# or \" in the token with # or " */
	if (fixToken && !scanReadOnly) {
	    char	*p, *q;

	    for (p = q = tokenend; p >= token; p--) {
		if (*p == '\\' && ( p[1] == '#' || p[1] == '"') )
		    continue;
		*q-- = *p;
	    }
	    token = q + 1;
	}
    }

    /* If token originally started with a quote, token is what's inside quotes.
     * Note that *rawToken is checked since *token will also be " if the
     * token originally started with a \" that has been changed to ".
     */
    if (*rawToken == '"') {
	token++;
	tokenQuoted = 1;
    }
    else
	tokenQuoted = 0;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2) {
	fputs("TOKEN = '", stderr);
	PrintToken(stderr);
	fputs("' ", stderr);
	fprintf(stderr, "scanInit=%d scanError=%d scanReadOnly=%d doingAccess=%d tokenQuoted=%d token=%p tokenend=%p\n", scanInit, scanError, scanReadOnly, doingAccess, tokenQuoted, token, tokenend);
    }
#endif
}

/* Move to the next line of the input stream. */

static void
SkipLine(void)
{
    while (*token && *token != '\n')
	FindNextToken();
    FindNextToken();			/* Move over the newline */
}

/* From an argv, build a command line suitable for display in logs etc. */

static char *
BuildCmdLine(char **argv)
{
    int		i, cmdLen = 0;
    char	*cmdLine;
    char	*p;

    if (argv == NULL)
	return NULL;
    for (i = 0; argv[i] != NULL; i++) {
	cmdLen += strlen(argv[i]) + 1;	/* +1 for space separator or null */
	/* any arg with whitespace appears in quotes */
	if (strpbrk(argv[i], " \t") != NULL)
	    cmdLen += 2;
	/* any quote gets a \ prepended */
	for (p = argv[i]; *p; p++)
	    if (*p == '"')
		cmdLen++;
    }

    if ((cmdLine = (char *)malloc(cmdLen)) == NULL) {
	fprintf(stderr, "pmcd config[line %d]: Error: failed to build command line\n",
		nLines);
	__pmNoMem("pmcd config: BuildCmdLine", cmdLen, PM_RECOV_ERR);
	return NULL;
    }
    for (i = 0, p = cmdLine; argv[i] != NULL; i++) {
	int	quote = strpbrk(argv[i], " \t") != NULL;
	char	*q;

	if (quote)
	    *p++ = '"';
	for (q = argv[i]; *q; q++) {
	    if (*q == '"')
		*p++ = '\\';
	    *p++ = *q;
	}
	if (quote)
	    *p++ = '"';
	if (argv[i+1] != NULL)
	    *p++ = ' ';
    }
    *p = '\0';
    return cmdLine;
}


/* Build an argument list suitable for an exec call from the rest of the tokens
 * on the current line.
 */
char **
BuildArgv(void)
{
    int		nArgs;
    char	**result;

    nArgs = 0;
    result = NULL;
    do {
	/* Make result big enough for new arg and terminating NULL pointer */
	result = (char **)realloc(result, (nArgs + 2) * sizeof(char *));
	if (result != NULL) {
	    if (*token != '/')
		result[nArgs] = CopyToken();
	    else if ((result[nArgs] = CopyToken()) != NULL)
		__pmNativePath(result[nArgs]);
	}
	if (result == NULL || result[nArgs] == NULL) {
	    fprintf(stderr, "pmcd config[line %d]: Error: failed to build argument list\n",
		    nLines);
	    __pmNoMem("pmcd config: build argv", nArgs * sizeof(char *),
		     PM_RECOV_ERR);
	    if (result != NULL) {
		while (nArgs >= 0) {
		    if (result[nArgs] != NULL)
			free(result[nArgs]);
		    nArgs--;
		}
		free(result);
	    }
	    return NULL;
	}
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL2)
	    fprintf(stderr, "argv[%d] = '%s'\n", nArgs, result[nArgs]);
#endif

	nArgs++;
	FindNextToken();
    } while (*token && *token != '\n');
    result[nArgs] = NULL;

    return result;
}

/* Return the next unused index into the agent array, extending the array
   as necessary. */
static AgentInfo *
GetNewAgent(void)
{
    AgentInfo *na;

    if (agent == NULL) {
	agent = (AgentInfo*)malloc(sizeof(AgentInfo) * MIN_AGENTS_ALLOC);
	if (agent == NULL) {
	    perror("GetNewAgentIndex: malloc");
	    exit(1);
	}
	szAgents = MIN_AGENTS_ALLOC;
    }
    else if (nAgents >= szAgents) {
	int	i;
	agent = (AgentInfo*)
	    realloc(agent, sizeof(AgentInfo) * 2 * szAgents);
	if (agent == NULL) {
	    perror("GetNewAgentIndex: realloc");
	    exit(1);
	}
	for (i = 0; i < nAgents; i++)
	    pmdaInterfaceMoved(&agent[i].ipc.dso.dispatch);
	szAgents *= 2;
    }

    na = agent+nAgents; nAgents++;
    memset (na, 0, sizeof(AgentInfo));

    return na;
}

/* Free any malloc()-ed memory associated with an agent */

static void
FreeAgent(AgentInfo *ap)
{
    int		i;
    char	**argv = NULL;

    free(ap->pmDomainLabel);
    if (ap->ipcType == AGENT_DSO) {
	free(ap->ipc.dso.pathName);
	free(ap->ipc.dso.entryPoint);
    }
    else if (ap->ipcType == AGENT_SOCKET) {
	if (ap->ipc.socket.commandLine != NULL) {
	    free(ap->ipc.socket.commandLine);
	    argv = ap->ipc.socket.argv;
	}
    }
    else
	if (ap->ipc.pipe.commandLine != NULL) {
	    free(ap->ipc.pipe.commandLine);
	    argv = ap->ipc.pipe.argv;
	}
    
    if (argv != NULL) {
	for (i = 0; argv[i] != NULL; i++)
	    free(argv[i]);
	free(argv);
    }
}

/* Parse a DSO specification, creating and initialising a new entry in the
 * agent table if the spec has no errors.
 */
static int
ParseDso(char *pmDomainLabel, int pmDomainId)
{
    char	*pathName;
    char	*entryPoint;
    AgentInfo	*newAgent;
    int		xlatePath = 0;

    FindNextToken();
    if (*token == '\n') {
	fprintf(stderr, "pmcd config[line %d]: Error: expected DSO entry point\n", nLines);
	return -1;
    }
    if ((entryPoint = CopyToken()) == NULL) {
	fprintf(stderr, "pmcd config[line %d]: Error: couldn't copy DSO entry point\n",
			 nLines);
	__pmNoMem("pmcd config", tokenend - token + 1, PM_FATAL_ERR);
    }

    FindNextToken();
    if (*token == '\n') {
	fprintf(stderr, "pmcd config[line %d]: Error: expected DSO pathname\n", nLines);
	free(entryPoint);
	return -1;
    }
    if (*token != '/') {
	if (token[strlen(token)-1] == '\n')
	    token[strlen(token)-1] = '\0';
	fprintf(stderr, "pmcd config[line %d]: Error: path \"%s\" to PMDA is not absolute\n", nLines, token);
	free(entryPoint);
	return -1;
    }

    if ((pathName = CopyToken()) == NULL) {
	fprintf(stderr, "pmcd config[line %d]: Error: couldn't copy DSO pathname\n",
			nLines);
	__pmNoMem("pmcd config", tokenend - token + 1, PM_FATAL_ERR);
    }
    __pmNativePath(pathName);

    FindNextToken();
    if (*token != '\n') {
	fprintf(stderr, "pmcd config[line %d]: Error: too many parameters for DSO\n",
		     nLines);
	free(entryPoint);
	free(pathName);
	return -1;
    }

    /* Now create and initialise a slot in the agents table for the new agent */

    newAgent = GetNewAgent();

    newAgent->ipcType = AGENT_DSO;
    newAgent->pmDomainId = pmDomainId;
    newAgent->inFd = -1;
    newAgent->outFd = -1;
    newAgent->pmDomainLabel = strdup(pmDomainLabel);
    newAgent->ipc.dso.pathName = pathName;
    newAgent->ipc.dso.xlatePath = xlatePath;
    newAgent->ipc.dso.entryPoint = entryPoint;

    return 0;
}

/* Parse a socket specification, creating and initialising a new entry in the
 * agent table if the spec has no errors.
 */
static int
ParseSocket(char *pmDomainLabel, int pmDomainId)
{
    int		addrDomain, port = -1;
    char	*socketName = NULL;
    AgentInfo	*newAgent;

    FindNextToken();
    if (TokenIs("inet"))
	addrDomain = AF_INET;
    else if (TokenIs("ipv6"))
	addrDomain = AF_INET6;
    else if (TokenIs("unix"))
	addrDomain = AF_UNIX;
    else {
	fprintf(stderr,
		     "pmcd config[line %d]: Error: expected socket address domain (`inet', `ipv6', or `unix')\n",
		     nLines);
	return -1;
    }

    FindNextToken();
    if (*token == '\n') {
	fprintf(stderr, "pmcd config[line %d]: Error: expected socket port name or number\n",
		     nLines);
	return -1;
    }
    else if (TokenIsNumber())
	port = TokenNumVal();
    else
	if ((socketName = CopyToken()) == NULL) {
	    fprintf(stderr, "pmcd config[line %d]: Error: couldn't copy port name\n",
			 nLines);
	    __pmNoMem("pmcd config", tokenend - token + 1, PM_FATAL_ERR);
	}
    FindNextToken();

    /* If an internet domain port name was specified, find the corresponding
     port number. */

    if ((addrDomain == AF_INET || addrDomain == AF_INET6) && socketName) {
	struct servent *service;

	service = getservbyname(socketName, NULL);
	if (service)
	    port = service->s_port;
	else {
	    fprintf(stderr,
		"pmcd config[line %d]: Error: failed to get port number for port name %s\n",
		nLines, socketName);
	    free(socketName);
	    return -1;
	}
    }

    /* Now create and initialise a slot in the agents table for the new agent */

    newAgent = GetNewAgent();

    newAgent->ipcType = AGENT_SOCKET;
    newAgent->pmDomainId = pmDomainId;
    newAgent->inFd = -1;
    newAgent->outFd = -1;
    newAgent->pmDomainLabel = strdup(pmDomainLabel);
    newAgent->ipc.socket.addrDomain = addrDomain;
    newAgent->ipc.socket.name = socketName;
    newAgent->ipc.socket.port = port;
    if (*token != '\n') {
	newAgent->ipc.socket.argv = BuildArgv();
	if (newAgent->ipc.socket.argv == NULL) {
	    fprintf(stderr, "pmcd config[line %d]: Error: building argv for \"%s\" agent.\n",
			 nLines, newAgent->pmDomainLabel);
	    FreeAgent(newAgent);
	    nAgents--;
	    return -1;
	}
	newAgent->ipc.socket.commandLine = BuildCmdLine(newAgent->ipc.socket.argv);
    }
    newAgent->ipc.socket.agentPid = (pid_t)-1;

    return 0;
}

/* Parse a pipe specification, creating and initialising a new entry in the
 * agent table if the spec has no errors.
 */
static int
ParsePipe(char *pmDomainLabel, int pmDomainId)
{
    int		i;
    AgentInfo	*newAgent;
    int notReady = 0;

    FindNextToken();
    if (!TokenIs("binary")) {
	fprintf(stderr,
		     "pmcd config[line %d]: Error: pipe PDU type expected (`binary')\n",
		     nLines);
	return -1;
    }

    do {
	i = 0;
	FindNextToken();
	if (*token == '\n') {
	    fprintf(stderr,
		     "pmcd config[line %d]: Error: command to create pipe agent expected.\n",
		     nLines);
	    return -1;
	} else if ((i = TokenIs ("notready"))) {
	    notReady = 1;
	}
    } while (i);

    /* Now create and initialise a slot in the agents table for the new agent */

    newAgent = GetNewAgent();
    newAgent->ipcType = AGENT_PIPE;
    newAgent->pmDomainId = pmDomainId;
    newAgent->inFd = -1;
    newAgent->outFd = -1;
    newAgent->pmDomainLabel = strdup(pmDomainLabel);
    newAgent->status.startNotReady = notReady;
    newAgent->ipc.pipe.argv = BuildArgv();

    if (newAgent->ipc.pipe.argv == NULL) {
	fprintf(stderr, "pmcd config[line %d]: Error: building argv for \"%s\" agent.\n",
		     nLines, newAgent->pmDomainLabel);
	FreeAgent(newAgent);
	nAgents--;
	return -1;
    }
    newAgent->ipc.pipe.commandLine = BuildCmdLine(newAgent->ipc.pipe.argv);

    return 0;
}

static int
ParseAccessSpec(int allow, int *specOps, int *denyOps, int *maxCons, int recursion)
{
    int		op;			/* >0 for specific ops, 0 otherwise */
    int		haveOps = 0, haveAll = 0;
    int		haveComma = 0;

    if (*token == ';') {
	fprintf(stderr, "pmcd config[line %d]: Error: empty or incomplete permissions list\n",
		     nLines);
	return -1;
    }

    if (!recursion)			/* Set maxCons to unspecified 1st time */
	*maxCons = 0;
    while (*token && *token != ';') {
	op = 0;
	if (TokenIs("fetch"))
	    op = PMCD_OP_FETCH;
	else if (TokenIs("store"))
	    op = PMCD_OP_STORE;
	else if (TokenIs("all")) {
	    if (haveOps) {
		fprintf(stderr,
			     "pmcd config[line %d]: Error: can't have \"all\" mixed with specific permissions\n",
			     nLines);
		return -1;
	    }
	    haveAll = 1;
	    if (recursion) {
		fprintf(stderr,
			     "pmcd config[line %d]: Error: can't have \"all\" within an \"all except\"\n",
			     nLines);
		return -1;
	    }
	    FindNextToken();

	    /* Any "all" statement specifies permissions for all operations
	    * Start off with all operations in allow/disallowed state
	    */
	    *denyOps = allow ? PMCD_OP_NONE : PMCD_OP_ALL;

	    if (TokenIs("except")) {
		/* Now deal with exceptions by reversing the "allow" sense */
		int sts;

		FindNextToken();
		sts = ParseAccessSpec(!allow, specOps, denyOps, maxCons, 1);
		if (sts < 0) return -1;
	    }
	    *specOps = PMCD_OP_ALL;		/* Do this AFTER any recursive call */
	}
	else if (TokenIs("maximum") || TokenIs("unlimited")) {
	    int	unlimited = (*token == 'u' || *token == 'U');

	    if (*maxCons) {
		fprintf(stderr,
			     "pmcd config[line %d]: Error: connection limit already specified\n",
			     nLines);
		return -1;
	    }
	    if (recursion && !haveOps) {
		fprintf(stderr,
			     "pmcd config[line %d]: Error: connection limit may not immediately follow \"all except\"\n",
			     nLines);
		return -1;
	    }

	    /* "maximum N connections" or "unlimited connections" is not
	     * allowed in a disallow statement.  This is a bit tricky, because
	     * of the recursion in "all except", which flips "allow" into
	     * !"allow" and recursion from 0 to 1 for the recursive call to
	     * this function.  The required test is !XOR: "!recursion && allow"
	     * is an "allow" with no "except".  "recursion && !allow" is an
	     * "allow" with an "except" anything else is a "disallow" (i.e. an
	     * error)
	     */
	    if (!(recursion ^ allow)) { /* disallow statement */
		fprintf(stderr,
			     "pmcd config[line %d]: Error: can't specify connection limit in a disallow statement\n",
			     nLines);
		return -1;
	    }
	    if (unlimited)
		*maxCons = -1;
	    else {
		FindNextToken();
		if (!TokenIsNumber() || TokenNumVal() <= 0) {
		    fprintf(stderr,
				 "pmcd config[line %d]: Error: maximum connection limit must be a positive number\n",
				 nLines);
		    return -1;
		}
		*maxCons = TokenNumVal();
		FindNextToken();
	    }
	    if (!TokenIs("connections")) {
		fprintf(stderr,
			     "pmcd config[line %d]: Error: \"connections\" expected\n",
			     nLines);
		return -1;
	    }
	    FindNextToken();
	}
	else {
	    fprintf(stderr, "pmcd config[line %d]: Error: bad access specifier\n",
			 nLines);
	    return -1;
	}

	/* If there was a specific operation mentioned, (dis)allow it */
	if (op) {
	    if (haveAll) {
		fprintf(stderr,
			     "pmcd config[line %d]: Error: can't have \"all\" mixed with specific permissions\n",
			     nLines);
		return -1;
	    }
	    haveOps = 1;
	    *specOps |= op;
	    if (allow)
		*denyOps &= (~op);
	    else
		*denyOps |= op;
	    FindNextToken();
	}
	if (*token != ',' && *token != ';') {
	    fprintf(stderr,
			 "pmcd config[line %d]: Error: ',' or ';' expected in permission list\n",
			 nLines);
	    return -1;
	}
	if (*token == ',') {
	    haveComma = 1;
	    FindNextToken();
	}
	else
	    haveComma = 0;
    }
    if (haveComma) {
	fprintf(stderr,
		     "pmcd config[line %d]: Error: misplaced (trailing) ',' in permission list\n",
		     nLines);
	return -1;
    }
    return 0;
}

static int
ParseNames(char ***namesp, const char *nametype)
{
    static char	**names;
    static int	szNames;
    int		nnames = 0;
    int		another = 1;

    /* Beware of quoted tokens of length longer than 1. e.g. ":*" */
    while (*token && another &&
	   ((tokenend - token > 1) || (*token != ':' && *token != ';'))) {
	if (nnames == szNames) {
	    int		need;

	    szNames += 8;
	    need = szNames * (int)sizeof(char**);
	    if ((names = (char **)realloc(names, need)) == NULL)
		__pmNoMem("pmcd ParseNames name list", need, PM_FATAL_ERR);
	}
	if ((names[nnames++] = CopyToken()) == NULL)
	    __pmNoMem("pmcd ParseNames name", tokenend - token, PM_FATAL_ERR);
	FindNextToken();
	if (*token != ',' && *token != ':') {
	    fprintf(stderr,
			 "pmcd config[line %d]: Error: ',' or ':' expected after \"%s\"\n",
			 nLines, names[nnames-1]);
	    return -1;
	}
	if (*token == ',') {
	    FindNextToken();
	    another = 1;
	}
	else
	    another = 0;
    }
    if (nnames == 0) {
	fprintf(stderr,
		     "pmcd config[line %d]: Error: no %ss in allow/disallow statement\n",
		     nLines, nametype);
	return -1;
    }
    if (another) {
	fprintf(stderr, "pmcd config[line %d]: Error: %s expected after ','\n",
		     nLines, nametype);
	return -1;
    }
    if (*token != ':') {
	fprintf(stderr, "pmcd config[line %d]: Error: ':' expected after \"%s\"\n",
		nLines, names[nnames-1]);
	return -1;
    }
    *namesp = names;
    return nnames;
}

static int
ParseHosts(int allow)
{
    int		sts;
    int		nhosts;
    int		i;
    int		ok = 0;
    int		specOps = 0;
    int		denyOps = 0;
    int		maxCons = 0;		/* Zero=>unspecified, -1=>unlimited */
    char	**hostnames;

    if ((nhosts = ParseNames(&hostnames, "host")) < 0)
	goto error;

    FindNextToken();
    if (ParseAccessSpec(allow, &specOps, &denyOps, &maxCons, 0) < 0)
	goto error;

    if (pmDebug & DBG_TRACE_APPL1) {
	for (i = 0; i < nhosts; i++)
	    fprintf(stderr, "HOST ACCESS: %s specOps=%02x denyOps=%02x maxCons=%d\n",
		    hostnames[i], specOps, denyOps, maxCons);
    }

    /* Make new entries for hosts in host access list */
    for (i = 0; i < nhosts; i++) {
	if ((sts = __pmAccAddHost(hostnames[i], specOps, denyOps, maxCons)) < 0) {
	    if (sts == -EHOSTUNREACH || sts == -EHOSTDOWN)
		fprintf(stderr, "Warning: the following access control specification will be ignored\n");
	    fprintf(stderr,
			 "pmcd config[line %d]: Warning: access control error for host '%s': %s\n",
			 nLines, hostnames[i], pmErrStr(sts));
	    if (sts == -EHOSTUNREACH || sts == -EHOSTDOWN)
		;
	    else
		goto error;
	}
	else
	    ok = 1;
    }
    return ok;

error:
    for (i = 0; i < nhosts; i++)
	free(hostnames[i]);
    return -1;
}

static int
ParseUsers(int allow)
{
    int		sts;
    int		nusers;
    int		i;
    int		ok = 0;
    int		specOps = 0;
    int		denyOps = 0;
    int		maxCons = 0;		/* Zero=>unspecified, -1=>unlimited */
    char	**usernames;

    if ((nusers = ParseNames(&usernames, "user")) < 0)
	goto error;

    FindNextToken();
    if (ParseAccessSpec(allow, &specOps, &denyOps, &maxCons, 0) < 0)
	goto error;

    if (pmDebug & DBG_TRACE_APPL1) {
	for (i = 0; i < nusers; i++)
	    fprintf(stderr, "USER ACCESS: %s specOps=%02x denyOps=%02x maxCons=%d\n",
		    usernames[i], specOps, denyOps, maxCons);
    }

    /* Make new entries for users in user access list */
    for (i = 0; i < nusers; i++) {
	if ((sts = __pmAccAddUser(usernames[i], specOps, denyOps, maxCons)) < 0) {
	    fprintf(stderr,
			 "pmcd config[line %d]: Warning: access control error for user '%s': %s\n",
			 nLines, usernames[i], pmErrStr(sts));
	    goto error;
	}
	ok = 1;
    }
    return ok;

error:
    for (i = 0; i < nusers; i++)
	free(usernames[i]);
    return -1;
}

static int
ParseGroups(int allow)
{
    int		sts;
    int		ngroups;
    int		i;
    int		ok = 0;
    int		specOps = 0;
    int		denyOps = 0;
    int		maxCons = 0;		/* Zero=>unspecified, -1=>unlimited */
    char	**groupnames;

    if ((ngroups = ParseNames(&groupnames, "group")) < 0)
	goto error;

    FindNextToken();
    if (ParseAccessSpec(allow, &specOps, &denyOps, &maxCons, 0) < 0)
	goto error;

    if (pmDebug & DBG_TRACE_APPL1) {
	for (i = 0; i < ngroups; i++)
	    fprintf(stderr, "GROUP ACCESS: %s specOps=%02x denyOps=%02x maxCons=%d\n",
		    groupnames[i], specOps, denyOps, maxCons);
    }

    /* Make new entries for groups in group access list */
    for (i = 0; i < ngroups; i++) {
	if ((sts = __pmAccAddGroup(groupnames[i], specOps, denyOps, maxCons)) < 0) {
	    fprintf(stderr,
			 "pmcd config[line %d]: Warning: access control error for group '%s': %s\n",
			 nLines, groupnames[i], pmErrStr(sts));
	    goto error;
	}
	ok = 1;
    }
    return ok;

error:
    for (i = 0; i < ngroups; i++)
	free(groupnames[i]);
    return -1;
}

static int
ParseAccessControls(void)
{
    int		sts = 0;
    int		tmp;
    int		allow;
    int		naccess = 0;
    int		need_creds = 0;

    doingAccess = 1;
    /* This gets a little tricky, because the token may be "[access]", or
     * "[access" or "[".  "[" and "]" can't be made special characters until
     * the scanner knows it is in the access control section because the arg
     * lists for agents may contain them.
     */
    if (TokenIs("[access]"))
	FindNextToken();
    else {
	if (TokenIs("[")) {
	    FindNextToken();
	    if (!TokenIs("access")) {
		fprintf(stderr,
			     "pmcd config[line %d]: Error: \"access\" keyword expected\n",
			     nLines);
		return -1;
	    }
	}
	else if (!TokenIs("[access")) {
	    fprintf(stderr,
			 "pmcd config[line %d]: Error: \"access\" keyword expected\n",
			 nLines);
	    return -1;
	}
	FindNextToken();
	if (*token != ']') {
	    fprintf(stderr, "pmcd config[line %d]: Error: ']' expected\n", nLines);
	    return -1;
	}
	FindNextToken();
    }
    while (*token && !scanError) {
	if (TokenIs("allow"))
	    allow = 1;
	else if (TokenIs("disallow"))
	    allow = 0;
	else {
	    fprintf(stderr,
			 "pmcd config[line %d]: Error: allow or disallow statement expected\n",
			 nLines);
	    sts = -1;
	    while (*token && !scanError && *token != ';')
		FindNextToken();
	    if (*token && !scanError && *token == ';') {
		FindNextToken();
		continue;
	    }
	    return -1;
	}
	FindNextToken();
	if (TokenIs("user") || TokenIs("users")) {
	    FindNextToken();
	    if ((tmp = ParseUsers(allow)) < 0)
		sts = -1;
	    else
		need_creds = 1;
	} else if (TokenIs("group") || TokenIs("groups")) {
	    FindNextToken();
	    if ((tmp = ParseGroups(allow)) < 0)
		sts = -1;
	    else
		need_creds = 1;
	} else if (TokenIs("host") || TokenIs("hosts")) {
	    FindNextToken();
	    if ((tmp = ParseHosts(allow)) < 0)
		sts = -1;
	} else {
	    if ((tmp = ParseHosts(allow)) < 0)
		sts = -1;
	}
	if (tmp > 0)
	    naccess++;
	while (*token && !scanError && *token != ';')
	    FindNextToken();
	if (!*token || scanError)
	    return -1;
	FindNextToken();
    }
    if (sts != 0)
	return sts;

    if (naccess == 0) {
	fprintf(stderr,
		     "pmcd config[line %d]: Error: no valid statements in [access] section\n",
		     nLines);
	return -1;
    }

    if (need_creds)
	__pmServerSetFeature(PM_SERVER_FEATURE_CREDS_REQD);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1)
	__pmAccDumpLists(stderr);
#endif

    return 0;
}

/* Parse the configuration file, creating the agent list. */
static int
ReadConfigFile(FILE *configFile)
{
    char	*pmDomainLabel = NULL;
    int		i, pmDomainId;
    int		sts = 0;

    inputStream = configFile;
    scanInit = 0;
    scanError = 0;
    doingAccess = 0;
    nLines = 0;
    FindNextToken();
    while (*token && !scanError) {
	if (*token == '\n')		/* It's a comment or blank line */
	    goto doneLine;

	if (*token == '[')		/* Start of access control specs */
	    break;

	if ((pmDomainLabel = CopyToken()) == NULL)
	    __pmNoMem("pmcd config: domain label", tokenend - token + 1, PM_FATAL_ERR);

	FindNextToken();
	if (TokenIsNumber()) {
	    pmDomainId = TokenNumVal();
	    FindNextToken();
	}
	else {
	    fprintf(stderr,
			 "pmcd config[line %d]: Error: expected domain number for \"%s\" agent\n",
			 nLines, pmDomainLabel);
	    sts = -1;
	    goto doneLine;
	}
	if (pmDomainId < 0 || pmDomainId > MAXDOMID) {
	    fprintf(stderr,
			 "pmcd config[line %d]: Error: Illegal domain number (%d) for \"%s\" agent\n",
			 nLines, pmDomainId, pmDomainLabel);
	    sts = -1;
	    goto doneLine;
	}
	/* Can't use mapdom because it isn't built yet.  Can't build it during
	 * parsing because this might be a restart parse that fails, requiring
	 * a revert to the old mapdom.
	 */
	for (i = 0; i < nAgents; i++)
	    if (pmDomainId == agent[i].pmDomainId) {
		fprintf(stderr,
			     "pmcd config[line %d]: Error: domain number for \"%s\" agent clashes with \"%s\" agent\n",
			     nLines, pmDomainLabel, agent[i].pmDomainLabel);
		sts = -1;
		goto doneLine;
	    }

	/*
	 * ParseXXX routines must return
	 * -1 for failure and ensure a NewAgent structure has NOT been
	 *    allocated
	 *  0 for success with a NewAgent structure allocated
	 */
	if (TokenIs("dso"))
	    sts = ParseDso(pmDomainLabel, pmDomainId);
	else if (TokenIs("socket"))
	    sts = ParseSocket(pmDomainLabel, pmDomainId);
	else if (TokenIs("pipe"))
	    sts = ParsePipe(pmDomainLabel, pmDomainId);
	else {
	    fprintf(stderr,
			 "pmcd config[line %d]: Error: expected `dso', `socket' or `pipe'\n",
			 nLines);
	    sts = -1;
	}
doneLine:
	if (pmDomainLabel != NULL) {
	    free(pmDomainLabel);
	    pmDomainLabel = NULL;
	}
	SkipLine();
    }
    if (scanError) {
	fprintf(stderr, "pmcd config: Can't continue, giving up\n");
	sts = -1;
    }
    if (*token == '[' && sts != -1)
	if (ParseAccessControls() < 0)
	    sts = -1;
    return sts;
}

static int
DoAttributes(AgentInfo *ap, int clientID)
{
    int sts = 0;
    __pmHashCtl *attrs = &client[clientID].attrs;
    __pmHashNode *node;

    if ((ap->status.flags & (PDU_FLAG_AUTH|PDU_FLAG_CONTAINER)) == 0)
	return 0;

    if (ap->ipcType == AGENT_DSO) {
	if (ap->ipc.dso.dispatch.comm.pmda_interface < PMDA_INTERFACE_6 ||
	    ap->ipc.dso.dispatch.version.six.attribute == NULL)
	    return 0;
	for (node = __pmHashWalk(attrs, PM_HASH_WALK_START);
	     node != NULL;
	     node = __pmHashWalk(attrs, PM_HASH_WALK_NEXT)) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_ATTR) {
		char buffer[64];
		__pmAttrStr_r(node->key, node->data, buffer, sizeof(buffer));
		fprintf(stderr, "pmcd: send client[%d] attr %s to dso agent[%d]",
			clientID, buffer, (int)(ap - agent));
	    }
#endif
	    if ((sts = ap->ipc.dso.dispatch.version.six.attribute(
				clientID, node->key, node->data,
				node->data ? strlen(node->data)+1 : 0,
				ap->ipc.dso.dispatch.version.six.ext)) < 0)
		break;
	}
    } else {
	/* daemon PMDA ... ship attributes */
	if (ap->status.notReady)
	    return PM_ERR_AGAIN;
	for (node = __pmHashWalk(attrs, PM_HASH_WALK_START);
	     node != NULL;
	     node = __pmHashWalk(attrs, PM_HASH_WALK_NEXT)) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_ATTR) {
		char buffer[64];
		__pmAttrStr_r(node->key, node->data, buffer, sizeof(buffer));
		fprintf(stderr, "pmcd: send client[%d] attr %s to daemon agent[%d]",
			clientID, buffer, (int)(ap - agent));
	    }
#endif
	    if ((sts = __pmSendAttr(ap->inFd,
				clientID, node->key, node->data,
				node->data ? strlen(node->data)+1 : 0)) < 0)
		break;
	}
    }
    if (sts < 0) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0)
	    fprintf(stderr, "ATTR error: \"%s\" agent : %s\n",
		    ap->pmDomainLabel, pmErrStr(sts));
#endif
	CleanupAgent(ap, AT_COMM, ap->inFd);
	return PM_ERR_AGAIN;
    }
    return sts;
}

/*
 * Once a new client arrives, we'll need to inform any interested PMDAs -
 * iterate over all of the active agents and send connection attributes.
 */
int
AgentsAttributes(int clientID)
{
    int agentID, sts = 0;

    for (agentID = 0; agentID < nAgents; agentID++) {
	if (agent[agentID].status.connected &&
	   (sts = DoAttributes(&agent[agentID], clientID)) < 0)
	    break;
    }
    return sts;
}

/*
 * Once a PMDA has started, we may need to inform it about the clients -
 * iterate over the authenticated clients and send connection attributes.
 */
int
ClientsAttributes(AgentInfo *ap)
{
    int clientID, sts = 0;

    for (clientID = 0; clientID < nClients; clientID++) {
	if (client[clientID].status.connected &&
	   (sts = DoAttributes(ap, clientID)) < 0)
	    break;
    }
    return sts;
}

static int
DoAgentCreds(AgentInfo* aPtr, __pmPDU *pb)
{
    int			i;
    int			sts = 0;
    int			flags = 0;
    int			sender = 0;
    int			credcount = 0;
    int			version = UNKNOWN_VERSION;
    __pmCred		*credlist = NULL;
    __pmVersionCred	*vcp;

    if ((sts = __pmDecodeCreds(pb, &sender, &credcount, &credlist)) < 0)
	return sts;
    pmcd_trace(TR_RECV_PDU, aPtr->outFd, sts, (int)((__psint_t)pb & 0xffffffff));

    for (i = 0; i < credcount; i++) {
	switch (credlist[i].c_type) {
	case CVERSION:
	    vcp = (__pmVersionCred *)&credlist[i];
	    aPtr->pduVersion = version = vcp->c_version;
	    aPtr->status.flags = flags = vcp->c_flags;
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_CONTEXT)
		fprintf(stderr, "pmcd: version creds (version=%u,flags=%x)\n",
			aPtr->pduVersion, aPtr->status.flags);
#endif
	    break;
	}
    }

    if (credlist != NULL)
	free(credlist);

    if (((sts = __pmSetVersionIPC(aPtr->inFd, version)) < 0) ||
	((sts = __pmSetVersionIPC(aPtr->outFd, version)) < 0))
	return sts;

    if (version != UNKNOWN_VERSION) {	/* finish the version exchange */
	__pmVersionCred	handshake;
	__pmCred *cp = (__pmCred *)&handshake;

	/* return pmcd PDU version and all flags pmcd knows about */
	handshake.c_type = CVERSION;
	handshake.c_version = PDU_VERSION;
	handshake.c_flags = (flags & PDU_FLAG_AUTH);
	if ((sts = __pmSendCreds(aPtr->inFd, (int)getpid(), 1, cp)) < 0)
	    return sts;
	pmcd_trace(TR_XMIT_PDU, aPtr->inFd, PDU_CREDS, credcount);

	/* send connection attributes for existing connected clients */
	if ((flags & (PDU_FLAG_AUTH|PDU_FLAG_CONTAINER)) != 0 &&
	    (sts = ClientsAttributes(aPtr)) < 0)
	    return sts;
    }

    return 0;
}

/* version exchange - get a credentials PDU from 2.0 agents */
static int
AgentNegotiate(AgentInfo *aPtr)
{
    int		sts;
    __pmPDU	*ack;

    sts = __pmGetPDU(aPtr->outFd, ANY_SIZE, _creds_timeout, &ack);
    if (sts == PDU_CREDS) {
	if ((sts = DoAgentCreds(aPtr, ack)) < 0) {
	    fprintf(stderr, "pmcd: version exchange failed "
		"for \"%s\" agent: %s\n", aPtr->pmDomainLabel, pmErrStr(sts));
	}
	__pmUnpinPDUBuf(ack);
	return sts;
    }

    if (sts > 0) {
	fprintf(stderr, "pmcd: unexpected PDU type (0x%x) at initial "
		"exchange with %s PMDA\n", sts, aPtr->pmDomainLabel);
	__pmUnpinPDUBuf(ack);
    }
    else if (sts == 0)
	fprintf(stderr, "pmcd: unexpected end-of-file at initial "
		"exchange with %s PMDA\n", aPtr->pmDomainLabel);
    else
	fprintf(stderr, "pmcd: error at initial PDU exchange with "
		"%s PMDA: %s\n", aPtr->pmDomainLabel, pmErrStr(sts));
    return PM_ERR_IPC;
}

/* Connect to an agent's socket. */
static int
ConnectSocketAgent(AgentInfo *aPtr)
{
    int		sts = 0;
    int		fd = -1;	/* pander to gcc */

    if (aPtr->ipc.socket.addrDomain == AF_INET || aPtr->ipc.socket.addrDomain == AF_INET6) {
	__pmSockAddr	*addr;
	__pmHostEnt	*host;
	void		*enumIx;

	if ((host = __pmGetAddrInfo("localhost")) == NULL) {
	    fputs("pmcd: Error getting inet address for localhost\n", stderr);
	    goto error;
	}
	enumIx = NULL;
	for (addr = __pmHostEntGetSockAddr(host, &enumIx);
	     addr != NULL;
	     addr = __pmHostEntGetSockAddr(host, &enumIx)) {
	    if (__pmSockAddrIsInet(addr)) {
		/* Only consider addresses of the chosen family. */
		if (aPtr->ipc.socket.addrDomain != AF_INET)
		    continue;
	        fd = __pmCreateSocket();
	    }
	    else if (__pmSockAddrIsIPv6(addr)) {
		/* Only consider addresses of the chosen family. */
		if (aPtr->ipc.socket.addrDomain != AF_INET6)
		    continue;
	        fd = __pmCreateIPv6Socket();
	    }
	    else {
	        fprintf(stderr,
			"pmcd: Error creating socket for \"%s\" agent : invalid address family %d\n",
			aPtr->pmDomainLabel, __pmSockAddrGetFamily(addr));
		fd = -1;
	    }
	    if (fd < 0) {
	        __pmSockAddrFree(addr);
		continue; /* Try the next address */
	    }

	    __pmSockAddrSetPort(addr, aPtr->ipc.socket.port);
	    sts = __pmConnect(fd, (void *)addr, __pmSockAddrSize());
	    __pmSockAddrFree(addr);

	    if (sts == 0)
	        break; /* good connection */

	    /* Unsuccessful connection. */
	    __pmCloseSocket(fd);
	    fd = -1;
	}
	__pmHostEntFree(host);
    }
    else {
#if defined(HAVE_STRUCT_SOCKADDR_UN)
	struct sockaddr_un	addr;
	int			len;

	fd = socket(aPtr->ipc.socket.addrDomain, SOCK_STREAM, 0);
	if (fd < 0) {
	    fprintf(stderr,
		     "pmcd: Error creating socket for \"%s\" agent : %s\n",
		     aPtr->pmDomainLabel, netstrerror());
	    return -1;
	}
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, aPtr->ipc.socket.name);
	len = (int)offsetof(struct sockaddr_un, sun_path) + (int)strlen(addr.sun_path);
	sts = connect(fd, (struct sockaddr *) &addr, len);
#else
	fprintf(stderr, "pmcd: UNIX sockets are not supported : \"%s\" agent\n",
		     aPtr->pmDomainLabel);
	goto error;
#endif
    }
    if (sts < 0) {
	fprintf(stderr, "pmcd: Error connecting to \"%s\" agent : %s\n",
		     aPtr->pmDomainLabel, netstrerror());
	goto error;
    }
    aPtr->outFd = aPtr->inFd = fd;	/* Sockets are bi-directional */
    pmcd_openfds_sethi(fd);

    if ((sts = AgentNegotiate(aPtr)) < 0)
	goto error;

    return 0;

error:
    if (fd != -1) {
        if (aPtr->ipc.socket.addrDomain == AF_INET || aPtr->ipc.socket.addrDomain == AF_INET6)
	    __pmCloseSocket(fd);
	else
	    close(fd);
    }
    return -1;
}

#ifndef IS_MINGW
static pid_t
CreateAgentPOSIX(AgentInfo *aPtr)
{
    int		i;
    int		inPipe[2];	/* Pipe for input to child */
    int		outPipe[2];	/* For output to child */
    pid_t	childPid = (pid_t)-1;
    char	**argv = NULL;

    if (aPtr->ipcType == AGENT_PIPE) {
	argv = aPtr->ipc.pipe.argv;
	if (pipe1(inPipe) < 0) {
	    fprintf(stderr,
		    "pmcd: input pipe create failed for \"%s\" agent: %s\n",
		    aPtr->pmDomainLabel, osstrerror());
	    return (pid_t)-1;
	}

	if (pipe1(outPipe) < 0) {
	    fprintf(stderr,
		    "pmcd: output pipe create failed for \"%s\" agent: %s\n",
		    aPtr->pmDomainLabel, osstrerror());
	    close(inPipe[0]);
	    close(inPipe[1]);
	    return (pid_t)-1;
	}
	pmcd_openfds_sethi(outPipe[1]);
    }
    else if (aPtr->ipcType == AGENT_SOCKET)
	argv = aPtr->ipc.socket.argv;

    if (argv != NULL) {			/* Start a new agent if required */
	childPid = fork();
	if (childPid == (pid_t)-1) {
	    fprintf(stderr, "pmcd: creating child for \"%s\" agent: %s\n",
			 aPtr->pmDomainLabel, osstrerror());
	    if (aPtr->ipcType == AGENT_PIPE) {
		close(inPipe[0]);
		close(inPipe[1]);
		close(outPipe[0]);
		close(outPipe[1]);
	    }
	    return (pid_t)-1;
	}

	if (childPid) {
	    /* This is the parent (PMCD) */
	    if (aPtr->ipcType == AGENT_PIPE) {
		close(inPipe[0]);
		close(outPipe[1]);
		aPtr->inFd = inPipe[1];
		aPtr->outFd = outPipe[0];
	    }
	}
	else {
	    /*
	     * This is the child (new agent)
	     * make sure stderr is fd 2
	     */
	    dup2(fileno(stderr), STDERR_FILENO); 
	    if (aPtr->ipcType == AGENT_PIPE) {
		/* make pipe stdin for PMDA */
		dup2(inPipe[0], STDIN_FILENO);
		/* make pipe stdout for PMDA */
		dup2(outPipe[1], STDOUT_FILENO);
	    }
	    else {
		/*
		 * not a pipe, close stdin and attach stdout to stderr
		 */
		close(STDIN_FILENO);
		dup2(STDERR_FILENO, STDOUT_FILENO);
	    }

	    for (i = 0; i <= pmcd_hi_openfds; i++) {
		/* Close all except std{in,out,err} */
		if (i == STDIN_FILENO ||
		    i == STDOUT_FILENO ||
		    i == STDERR_FILENO)
		    continue;
		close(i);
	    }

	    execvp(argv[0], argv);
	    /* botch if reach here */
	    fprintf(stderr, "pmcd: error starting %s: %s\n",
			 argv[0], osstrerror());
	    /* avoid atexit() processing, so _exit not exit */
	    _exit(1);
	}
    }
    return childPid;
}

#else

static pid_t
CreateAgentWin32(AgentInfo *aPtr)
{
    SECURITY_ATTRIBUTES saAttr;
    PROCESS_INFORMATION piProcInfo;
    STARTUPINFO siStartInfo;
    HANDLE hChildStdinRd, hChildStdinWr, hChildStdoutRd, hChildStdoutWr;
    BOOL bSuccess = FALSE;
    LPTSTR command = NULL;

    if (aPtr->ipcType == AGENT_PIPE)
	command = (LPTSTR)aPtr->ipc.pipe.commandLine;
    else if (aPtr->ipcType == AGENT_SOCKET)
	command = (LPTSTR)aPtr->ipc.socket.commandLine;

    // Set the bInheritHandle flag so pipe handles are inherited
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    // Create a pipe for the child process's STDOUT.
    if (!CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &saAttr, 0)) {
	fprintf(stderr, "pmcd: stdout CreatePipe failed, \"%s\" agent: %s\n",
			aPtr->pmDomainLabel, osstrerror());
	return (pid_t)-1;
    }
    // Ensure the read handle to the pipe for STDOUT is not inherited.
    if (!SetHandleInformation(hChildStdoutRd, HANDLE_FLAG_INHERIT, 0)) {
	fprintf(stderr, "pmcd: stdout SetHandleInformation, \"%s\" agent: %s\n",
			aPtr->pmDomainLabel, osstrerror());
	return (pid_t)-1;
    }

    // Create a pipe for the child process's STDIN.
    if (!CreatePipe(&hChildStdinRd, &hChildStdinWr, &saAttr, 0)) {
	fprintf(stderr, "pmcd: stdin CreatePipe failed, \"%s\" agent: %s\n",
			aPtr->pmDomainLabel, osstrerror());
	return (pid_t)-1;
    }
    // Ensure the write handle to the pipe for STDIN is not inherited.
    if (!SetHandleInformation(hChildStdinWr, HANDLE_FLAG_INHERIT, 0)) {
	fprintf(stderr, "pmcd: stdin SetHandleInformation, \"%s\" agent: %s\n",
			aPtr->pmDomainLabel, osstrerror());
	return (pid_t)-1;
    }

    // Create the child process.

    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
    siStartInfo.cb = sizeof(STARTUPINFO);
    siStartInfo.hStdOutput = hChildStdoutWr;
    siStartInfo.hStdError = hChildStdoutWr;
    siStartInfo.hStdInput = hChildStdinRd;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    bSuccess = CreateProcess(NULL, command,
			NULL,          // process security attributes
			NULL,          // primary thread security attributes
			TRUE,          // handles are inherited
			0,             // creation flags
			NULL,          // use parent's environment
			NULL,          // use parent's current directory
			&siStartInfo,  // STARTUPINFO pointer
			&piProcInfo);  // receives PROCESS_INFORMATION
    if (!bSuccess) {
	fprintf(stderr, "pmcd: CreateProcess for \"%s\" agent: %s: %s\n",
			aPtr->pmDomainLabel, command, osstrerror());
	return (pid_t)-1;
    }

    aPtr->inFd = _open_osfhandle((intptr_t)hChildStdinRd, _O_WRONLY);
    aPtr->outFd = _open_osfhandle((intptr_t)hChildStdoutWr, _O_RDONLY);
    pmcd_openfds_sethi(aPtr->outFd);

    CloseHandle(piProcInfo.hProcess);
    CloseHandle(piProcInfo.hThread);
    CloseHandle(hChildStdoutRd);
    CloseHandle(hChildStdinWr);
    return piProcInfo.dwProcessId;
}
#endif

/* Create the specified agent running at the end of a pair of pipes. */
static int
CreateAgent(AgentInfo *aPtr)
{
    pid_t	childPid;
    int		sts;

    fflush(stderr);
    fflush(stdout);

#ifdef IS_MINGW
    childPid = CreateAgentWin32(aPtr);
#else
    childPid = CreateAgentPOSIX(aPtr);
#endif
    if (childPid < 0)
	return (int)childPid;

    aPtr->status.isChild = 1;
    if (aPtr->ipcType == AGENT_PIPE) {
	aPtr->ipc.pipe.agentPid = childPid;
	/* ready for version negotiation */
	if ((sts = AgentNegotiate(aPtr)) < 0) {
	    close(aPtr->inFd);
	    close(aPtr->outFd);
	    return sts;
	}
    }
    else if (aPtr->ipcType == AGENT_SOCKET)
	aPtr->ipc.socket.agentPid = childPid;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	fprintf(stderr, "pmcd: started PMDA %s (%d), pid=%" FMT_PID "\n",
	        aPtr->pmDomainLabel, aPtr->pmDomainId, childPid);
#endif
    return 0;
}

/* Print a table of all of the agent configuration info on a given stream. */
void
PrintAgentInfo(FILE *stream)
{
    int		i, version;
    AgentInfo	*aPtr;

    fputs("\nactive agent dom   pid  in out ver protocol parameters\n", stream);
    fputs(  "============ === ===== === === === ======== ==========\n", stream);
    for (i = 0; i < nAgents; i++) {
	aPtr = &agent[i];
	if (aPtr->status.connected == 0)
	    continue;
	fprintf(stream, "%-12s", aPtr->pmDomainLabel);

	switch (aPtr->ipcType) {
	case AGENT_DSO:
	    fprintf(stream, " %3d               %3d dso i:%d",
		    aPtr->pmDomainId,
		    aPtr->ipc.dso.dispatch.comm.pmapi_version,
		    aPtr->ipc.dso.dispatch.comm.pmda_interface);
	    fprintf(stream, "  lib=%s entry=%s [" PRINTF_P_PFX "%p]\n",
	        aPtr->ipc.dso.pathName, aPtr->ipc.dso.entryPoint,
	        aPtr->ipc.dso.initFn);
	    break;

	case AGENT_SOCKET:
	    version = __pmVersionIPC(aPtr->inFd);
	    fprintf(stream, " %3d %5" FMT_PID " %3d %3d %3d ",
		aPtr->pmDomainId, aPtr->ipc.socket.agentPid, aPtr->inFd, aPtr->outFd, version);
	    fputs("bin ", stream);
	    fputs("sock ", stream);
	    if (aPtr->ipc.socket.addrDomain == AF_UNIX)
		fprintf(stream, "dom=unix port=%s", aPtr->ipc.socket.name);
	    else if (aPtr->ipc.socket.addrDomain == AF_INET) {
		if (aPtr->ipc.socket.name)
		    fprintf(stream, "dom=inet port=%s (%d)",
		        aPtr->ipc.socket.name, aPtr->ipc.socket.port);
		else
		    fprintf(stream, "dom=inet port=%d", aPtr->ipc.socket.port);
	    }
	    else if (aPtr->ipc.socket.addrDomain == AF_INET6) {
		if (aPtr->ipc.socket.name)
		    fprintf(stream, "dom=ipv6 port=%s (%d)",
		        aPtr->ipc.socket.name, aPtr->ipc.socket.port);
		else
		    fprintf(stream, "dom=ipv6 port=%d", aPtr->ipc.socket.port);
	    }
	    else {
		fputs("dom=???", stream);
	    }
	    if (aPtr->ipc.socket.commandLine) {
		fputs(" cmd=", stream);
		fputs(aPtr->ipc.socket.commandLine, stream);
	    }
	    putc('\n', stream);
	    break;

	case AGENT_PIPE:
	    version = __pmVersionIPC(aPtr->inFd);
	    fprintf(stream, " %3d %5" FMT_PID " %3d %3d %3d ",
		aPtr->pmDomainId, aPtr->ipc.pipe.agentPid, aPtr->inFd, aPtr->outFd, version);
	    fputs("bin ", stream);
	    if (aPtr->ipc.pipe.commandLine) {
		fputs("pipe cmd=", stream);
		fputs(aPtr->ipc.pipe.commandLine, stream);
		putc('\n', stream);
	    }
	    break;

	default:
	    fputs("????\n", stream);
	    break;
	}
    }
    fflush(stream);			/* Ensure that it appears now */
}

/* Load the DSO for a specified agent and initialise it. */
static int
GetAgentDso(AgentInfo *aPtr)
{
    DsoInfo		*dso = &aPtr->ipc.dso;
    const char		*name;
    unsigned int	challenge;
#if defined(HAVE_DLOPEN)
    char		*dlerrstr;
#endif

    aPtr->status.connected = 0;
    aPtr->reason = REASON_NOSTART;

    name = __pmFindPMDA(dso->pathName);
    if (name == NULL) {
	fprintf(stderr, "Cannot find %s DSO at \"%s\"\n", 
		     aPtr->pmDomainLabel, dso->pathName);
	fputc('\n', stderr);
	return -1;
    }

    if (name != dso->pathName) {
	/* some searching was done */
	free(dso->pathName);
	dso->pathName = strdup(name);
	if (dso->pathName == NULL) {
	    __pmNoMem("pmcd config: pathName", strlen(name), PM_FATAL_ERR);
	}
	dso->xlatePath = 1;
    }

#if defined(HAVE_DLOPEN)
    /*
     * RTLD_NOW would be better in terms of detecting unresolved symbols
     * now, rather than taking a SEGV later ... but various combinations
     * of dynamic and static libraries used to create the DSO PMDA,
     * combined with hiding symbols in the DSO PMDA may result in benign
     * unresolved symbols remaining and the dlopen() would fail under
     * these circumstances.
     */
    dso->dlHandle = dlopen(dso->pathName, RTLD_LAZY);
#else
    fprintf(stderr, "Error attaching %s DSO at \"%s\"\n",
		     aPtr->pmDomainLabel, dso->pathName);
    fprintf(stderr, "No dynamic shared library support on this platform\n");
    return -1;
#endif

    if (dso->dlHandle == NULL) {
	fprintf(stderr, "Error attaching %s DSO at \"%s\"\n",
		     aPtr->pmDomainLabel, dso->pathName);
#if defined(HAVE_DLOPEN)
	fprintf(stderr, "%s\n\n", dlerror());
#else
	fprintf(stderr, "%s\n\n", osstrerror());
#endif
	return -1;
    }

    /* Get a pointer to the DSO's init function and call it to get the agent's
     dispatch table for the DSO. */

#if defined(HAVE_DLOPEN)
    dlerror();
    dso->initFn = (void (*)(pmdaInterface*))dlsym(dso->dlHandle, dso->entryPoint);
    if (dso->initFn == NULL) {
        dlerrstr = dlerror();
	fprintf(stderr, "Couldn't find init function `%s' in %s DSO: %s\n",
		     dso->entryPoint, aPtr->pmDomainLabel, dlerrstr);
	dlclose(dso->dlHandle);
	return -1;
    }
#endif

    /*
     * Pass in the expected domain id.
     * The PMDA initialization routine can (a) ignore it, (b) check it
     * is the expected value, or (c) self-adapt.
     */
    dso->dispatch.domain = aPtr->pmDomainId;

    /*
     * the PMDA interface / PMAPI version discovery as a "challenge" ...
     * for pmda_interface it is all the bits being set,
     * for pmapi_version it is the complement of the one you are using now
     */
    challenge = 0xff;
    dso->dispatch.comm.pmda_interface = challenge;
    dso->dispatch.comm.pmapi_version = ~PMAPI_VERSION;

    dso->dispatch.comm.flags = 0;
    dso->dispatch.status = 0;

    (*dso->initFn)(&dso->dispatch);

    if (dso->dispatch.status != 0) {
	/* initialization failed for some reason */
	fprintf(stderr,
		     "Initialization routine %s in %s DSO failed: %s\n", 
		     dso->entryPoint, aPtr->pmDomainLabel,
		     pmErrStr(dso->dispatch.status));
#if defined(HAVE_DLOPEN)
	    dlclose(dso->dlHandle);
#endif
	    return -1;
    }

    if (dso->dispatch.comm.pmda_interface < PMDA_INTERFACE_2 ||
	dso->dispatch.comm.pmda_interface > PMDA_INTERFACE_LATEST) {
	__pmNotifyErr(LOG_ERR,
		 "Unknown PMDA interface version (%d) used by DSO %s\n",
		 dso->dispatch.comm.pmda_interface, aPtr->pmDomainLabel);
#if defined(HAVE_DLOPEN)
	dlclose(dso->dlHandle);
#endif
	return -1;
    }

    if (dso->dispatch.comm.pmapi_version == PMAPI_VERSION_2)
	aPtr->pduVersion = PDU_VERSION2;
    else {
	__pmNotifyErr(LOG_ERR,
		 "Unsupported PMAPI version (%d) used by DSO %s\n",
		 dso->dispatch.comm.pmapi_version, aPtr->pmDomainLabel);
#if defined(HAVE_DLOPEN)
	dlclose(dso->dlHandle);
#endif
	return -1;
    }

    aPtr->reason = 0;
    aPtr->status.connected = 1;
    aPtr->status.flags = dso->dispatch.comm.flags;
    if (dso->dispatch.comm.flags & PDU_FLAG_AUTH)
	ClientsAttributes(aPtr);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	fprintf(stderr, "pmcd: started DSO PMDA %s (%d) using pmPMDA version=%d, "
		"PDU version=%d\n", aPtr->pmDomainLabel, aPtr->pmDomainId,
		dso->dispatch.comm.pmda_interface, aPtr->pduVersion);
#endif

    return 0;
}


/* For creating and establishing contact with agents of the PMCD. */
static void
ContactAgents(void)
{
    int		i;
    int		sts = 0;
    int		createdSocketAgents = 0;
    AgentInfo	*aPtr;

    for (i = 0; i < nAgents; i++) {
	aPtr = &agent[i];
	if (aPtr->status.connected)
	    continue;
	switch (aPtr->ipcType) {
	case AGENT_DSO:
	    sts = GetAgentDso(aPtr);
	    break;

	case AGENT_SOCKET:
	    if (aPtr->ipc.socket.argv) { /* Create agent if required */
		sts = CreateAgent(aPtr);
		if (sts >= 0)
		    createdSocketAgents = 1;

		/* Don't attempt to connect yet, if the agent has just been
		       created, it will need time to initialise socket. */
	    }
	    else
		sts = ConnectSocketAgent(aPtr);
	    break;			/* Connect to existing agent */

	case AGENT_PIPE:
	    sts = CreateAgent(aPtr);
	    break;
	}
	aPtr->status.connected = sts == 0;
	if (aPtr->status.connected) {
	    if (aPtr->ipcType == AGENT_DSO)
		pmcd_trace(TR_ADD_AGENT, aPtr->pmDomainId, -1, -1);
	    else
		pmcd_trace(TR_ADD_AGENT, aPtr->pmDomainId, aPtr->inFd, aPtr->outFd);
	    MarkStateChanges(PMCD_ADD_AGENT);
	    aPtr->status.notReady = aPtr->status.startNotReady;
	}
	else
	    aPtr->reason = REASON_NOSTART;
    }

    /* Allow newly created socket agents time to initialise before attempting
       to connect to them. */

    if (createdSocketAgents) {
	sleep(2);			/* Allow 2 second for startup */
	for (i = 0; i < nAgents; i++) {
	    aPtr = &agent[i];
	    if (aPtr->ipcType == AGENT_SOCKET &&
	        aPtr->ipc.socket.agentPid != (pid_t)-1) {
		sts = ConnectSocketAgent(aPtr);
		aPtr->status.connected = sts == 0;
		if (!aPtr->status.connected)
		    aPtr->reason = REASON_NOSTART;
	    }
	}
    }
}

int
ParseInitAgents(char *fileName)
{
    int		sts;
    int		i;
    FILE	*configFile;
    struct stat	statBuf;
    static int	firstTime = 1;

    memset(&configFileTime, 0, sizeof(configFileTime));
    configFile = fopen(fileName, "r");
    if (configFile == NULL)
	fprintf(stderr, "ParseInitAgents: %s: %s\n", fileName, osstrerror());
    else if (stat(fileName, &statBuf) == -1)
	fprintf(stderr, "ParseInitAgents: stat(%s): %s\n",
		     fileName, osstrerror());
    else {
#if defined(HAVE_ST_MTIME_WITH_E) && defined(HAVE_STAT_TIME_T)
	configFileTime = statBuf.st_mtime;
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0)
	    fprintf(stderr, "ParseInitAgents: configFileTime=%ld sec\n",
	        (long)configFileTime);
#endif
#elif defined(HAVE_ST_MTIME_WITH_SPEC)
	configFileTime = statBuf.st_mtimespec; /* struct assignment */
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0)
	    fprintf(stderr, "ParseInitAgents: configFileTime=%ld.%09ld sec\n",
	        (long)configFileTime.tv_sec, (long)configFileTime.tv_nsec);
#endif
#elif defined(HAVE_STAT_TIMESTRUC) || defined(HAVE_STAT_TIMESPEC) || defined(HAVE_STAT_TIMESPEC_T)
	configFileTime = statBuf.st_mtim; /* struct assignment */
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0)
	    fprintf(stderr, "ParseInitAgents: configFileTime=%ld.%09ld sec\n",
	        (long)configFileTime.tv_sec, (long)configFileTime.tv_nsec);
#endif
#else
!bozo!
#endif
    }
    if (configFile == NULL)
	return -1;

    if (firstTime)
	if (__pmAccAddOp(PMCD_OP_FETCH) < 0 || __pmAccAddOp(PMCD_OP_STORE) < 0) {
	    fprintf(stderr,
			 "ParseInitAgents: __pmAccAddOp: can't create access ops\n");
	    exit(1);
	}

    sts = ReadConfigFile(configFile);
    fclose(configFile);

    /* If pmcd is restarting, don't create/contact the agents until the results
     * of the parse can be compared with the previous setup to determine
     * whether anything has changed.
     */
    if (!firstTime)
	return sts;

    firstTime = 0;
    if (sts == 0) {
	ContactAgents();
	for (i = 0; i < MAXDOMID + 2; i++)
	    mapdom[i] = nAgents;
	for (i = 0; i < nAgents; i++)
	    if (agent[i].status.connected)
		mapdom[agent[i].pmDomainId] = i;
    }
    return sts;
}

static int
AgentsDiffer(AgentInfo *a1, AgentInfo *a2)
{
    int	i;

    if (a1->pmDomainId != a2->pmDomainId)
	return 1;
    if (a1->ipcType != a2->ipcType)
	return 1;
    if (a1->ipcType == AGENT_DSO) {
	DsoInfo	*dso1 = &a1->ipc.dso;
	DsoInfo	*dso2 = &a2->ipc.dso;
	if (strcmp(dso1->pathName, dso2->pathName) != 0)
	    return 1;
	if (dso1->entryPoint == NULL || dso2->entryPoint == NULL)
	    return 1;	/* should never happen */
	if (strcmp(dso1->entryPoint, dso2->entryPoint))
	    return 1;
    }
    else if (a1->ipcType == AGENT_SOCKET) {
	SocketInfo	*sock1 = &a1->ipc.socket;
	SocketInfo	*sock2 = &a2->ipc.socket;

	if (sock1 == NULL || sock2 == NULL)
		return 1;	/* should never happen */
	if (sock1->addrDomain != sock2->addrDomain)
	    return 1;
	/* The names don't really matter, it's the port that counts */
	if (sock1->port != sock2->port)
	    return 1;
	if ((sock1->commandLine == NULL && sock2->commandLine != NULL) ||
	    (sock1->commandLine != NULL && sock2->commandLine == NULL))
	    return 1;
	if (sock1->argv != NULL && sock2->argv != NULL) {
	    /* Don't just compare commandLines, changes may be cosmetic */
	    for (i = 0; sock1->argv[i] != NULL && sock2->argv[i] != NULL; i++)
		if (strcmp(sock1->argv[i], sock2->argv[i]))
		return 1;
	    if (sock1->argv[i] != NULL || sock2->argv[i] != NULL)
		return 1;
	}
	else if ((sock1->argv == NULL && sock2->argv != NULL) ||
		 (sock1->argv != NULL && sock2->argv == NULL))
		    return 1;
    }

    else {
	PipeInfo	*pipe1 = &a1->ipc.pipe;
	PipeInfo	*pipe2 = &a2->ipc.pipe;

	if (pipe1 == NULL || pipe2 == NULL)
		return 1;	/* should never happen */
	if ((pipe1->commandLine == NULL && pipe2->commandLine != NULL) ||
	    (pipe1->commandLine != NULL && pipe2->commandLine == NULL))
	    return 1;
	if (pipe1->argv != NULL && pipe2->argv != NULL) {
	    /* Don't just compare commandLines, changes may be cosmetic */
	    for (i = 0; pipe1->argv[i] != NULL && pipe2->argv[i] != NULL; i++)
		if (strcmp(pipe1->argv[i], pipe2->argv[i]))
		    return 1;
	    if (pipe1->argv[i] != NULL || pipe2->argv[i] != NULL)
		return 1;
	}
	else if ((pipe1->argv == NULL && pipe2->argv != NULL) ||
		 (pipe1->argv != NULL && pipe2->argv == NULL))
		    return 1;
    }
    return 0;
}

/* Make the "dest" agent the equivalent of an existing "src" agent.
 * This assumes that the agents are identical according to AgentsDiffer(), and
 * that they have distinct copies of the fields compared therein.
 * Note that only the the low level PDU I/O information is copied here.
 */
static void
DupAgent(AgentInfo *dest, AgentInfo *src)
{
    dest->inFd = src->inFd;
    dest->outFd = src->outFd;
    dest->profClient = src->profClient;
    dest->profIndex = src->profIndex;
    /* IMPORTANT: copy the status, connections stay connected */
    memcpy(&dest->status, &src->status, sizeof(dest->status));
    if (src->ipcType == AGENT_DSO) {
	dest->ipc.dso.dlHandle = src->ipc.dso.dlHandle;
	/*
	 * initFn is never needed again (DSO PMDA initialization has
	 * already been done), but copy it across so that PrintAgentInfo()
	 * reports the init routine's address
	 */
	dest->ipc.dso.initFn = src->ipc.dso.initFn;
	memcpy(&dest->ipc.dso.dispatch, &src->ipc.dso.dispatch,
	       sizeof(dest->ipc.dso.dispatch));
    }
    else if (src->ipcType == AGENT_SOCKET)
	dest->ipc.socket.agentPid = src->ipc.socket.agentPid;
    else
	dest->ipc.pipe.agentPid = src->ipc.pipe.agentPid;
}

void
ParseRestartAgents(char *fileName)
{
    int		sts;
    int		i, j;
    struct stat	statBuf;
    AgentInfo	*oldAgent;
    int		oldNAgents;
    AgentInfo	*ap;
    __pmFdSet	fds;

    /* Clean up any deceased agents.  We haven't seen an agent's death unless
     * a PDU transfer involving the agent has occurred.  This cleans up others
     * as well.
     */
    __pmFD_ZERO(&fds);
    j = -1;
    for (i = 0; i < nAgents; i++) {
	ap = &agent[i];
	if (ap->status.connected &&
	    (ap->ipcType == AGENT_SOCKET || ap->ipcType == AGENT_PIPE)) {

	    __pmFD_SET(ap->outFd, &fds);
	    if (ap->outFd > j)
		j = ap->outFd;
	}
    }
    if (++j) {
	/* any agent with output ready has either closed the file descriptor or
	 * sent an unsolicited PDU.  Clean up the agent in either case.
	 */
	struct timeval	timeout = {0, 0};

	sts = __pmSelectRead(j, &fds, &timeout);
	if (sts > 0) {
	    for (i = 0; i < nAgents; i++) {
		ap = &agent[i];
		if (ap->status.connected &&
		    (ap->ipcType == AGENT_SOCKET || ap->ipcType == AGENT_PIPE) &&
		    __pmFD_ISSET(ap->outFd, &fds)) {

		    /* try to discover more ... */
		    __pmPDU	*pb;
		    sts = __pmGetPDU(ap->outFd, ANY_SIZE, TIMEOUT_NEVER, &pb);
		    if (sts > 0)
			pmcd_trace(TR_RECV_PDU, ap->outFd, sts, (int)((__psint_t)pb & 0xffffffff));
		    if (sts == 0)
			pmcd_trace(TR_EOF, ap->outFd, -1, -1);
		    else {
			pmcd_trace(TR_WRONG_PDU, ap->outFd, -1, sts);
			if (sts > 0)
			    __pmUnpinPDUBuf(pb);
		    }

		    CleanupAgent(ap, AT_COMM, ap->outFd);
		}
	    }
	}
	else if (sts < 0)
	    fprintf(stderr, "pmcd: deceased agents select: %s\n",
			 netstrerror());
    }

    /* gather any deceased children */
    HarvestAgents(0);

    if (stat(fileName, &statBuf) == -1) {
	fprintf(stderr, "ParseRestartAgents: stat(%s): %s\n",
		     fileName, osstrerror());
	fprintf(stderr, "Configuration left unchanged\n");
	return;
    }

    /* If the config file's modification time hasn't changed, just try to
     * restart any deceased agents
     */
#if defined(HAVE_ST_MTIME_WITH_SPEC)
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	fprintf(stderr, "ParseRestartAgents: new configFileTime=%ld.%09ld sec\n",
	    (long)statBuf.st_mtimespec.tv_sec, (long)statBuf.st_mtimespec.tv_nsec);
#endif
    if (statBuf.st_mtimespec.tv_sec == configFileTime.tv_sec &&
        statBuf.st_mtimespec.tv_nsec == configFileTime.tv_nsec) {
#elif defined(HAVE_STAT_TIMESPEC_T) || defined(HAVE_STAT_TIMESTRUC) || defined(HAVE_STAT_TIMESPEC)
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	fprintf(stderr, "ParseRestartAgents: new configFileTime=%ld.%09ld sec\n",
	    (long)statBuf.st_mtim.tv_sec, (long)statBuf.st_mtim.tv_nsec);
#endif
    if (statBuf.st_mtim.tv_sec == configFileTime.tv_sec &&
        statBuf.st_mtim.tv_nsec == configFileTime.tv_nsec) {
#elif defined(HAVE_STAT_TIME_T)
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	fprintf(stderr, "ParseRestartAgents: new configFileTime=%ld sec\n",
	    (long)configFileTime);
#endif
    if (statBuf.st_mtime == configFileTime) {
#else
!bozo!
#endif
	fprintf(stderr, "Configuration file '%s' unchanged\n", fileName);
	fprintf(stderr, "Restarting any deceased agents:\n");
	j = 0;
	for (i = 0; i < nAgents; i++)
	    if (!agent[i].status.connected) {
		fprintf(stderr, "    \"%s\" agent\n",
			     agent[i].pmDomainLabel);
		j++;
	    }
	if (j == 0)
	    fprintf(stderr, "    (no agents required restarting)\n");
	else {
	    putc('\n', stderr);
	    ContactAgents();
	    for (i = 0; i < nAgents; i++) {
		mapdom[agent[i].pmDomainId] =
		    agent[i].status.connected ? i : nAgents;
	    }

	    MarkStateChanges(PMCD_RESTART_AGENT);
	}
	PrintAgentInfo(stderr);
	__pmAccDumpLists(stderr);
	return;
    }

    /* Save the current agent[] and host access tables, Reset the internal
     * state of the config file parser and re-parse the config file.
     */
    oldAgent = agent;
    oldNAgents = nAgents;
    agent = NULL;
    nAgents = 0;
    szAgents = 0;
    scanInit = 0;
    scanError = 0;
    if (__pmAccSaveLists() < 0) {
	fprintf(stderr, "Error saving access controls\n");
	sts = -2;
    }
    else
	sts = ParseInitAgents(fileName);

    /* If the config file had errors or there were no valid agents in the new
     * config file, ignore it and stick with the old setup.
     */
    if (sts < 0 || nAgents == 0) {
	if (sts == -1)
	    fprintf(stderr,
			 "Configuration file '%s' has errors\n", fileName);
	else
	    fprintf(stderr,
			 "Configuration file '%s' has no valid agents\n",
			 fileName);
	fprintf(stderr, "Configuration left unchanged\n");
	agent = oldAgent;
	nAgents = oldNAgents;
	if (sts != -2 && __pmAccRestoreLists() < 0) {
	    fprintf(stderr, "Error restoring access controls!\n");
	    exit(1);
	}
	PrintAgentInfo(stderr);
	__pmAccDumpLists(stderr);
	return;
    }

    /* Reconcile the old and new agent tables, creating or destroying agents
     * as reqired.
     */
    for (j = 0; j < oldNAgents; j++)
	oldAgent[j].status.restartKeep = 0;

    for (i = 0; i < nAgents; i++)
	for (j = 0; j < oldNAgents; j++)
	    if (!AgentsDiffer(&agent[i], &oldAgent[j]) &&
		oldAgent[j].status.connected) {
		DupAgent(&agent[i], &oldAgent[j]);
		pmdaInterfaceMoved(&agent[i].ipc.dso.dispatch);
		oldAgent[j].status.restartKeep = 1;
	    }

    for (j = 0; j < oldNAgents; j++) {
	if (oldAgent[j].status.connected && !oldAgent[j].status.restartKeep)
	    CleanupAgent(&oldAgent[j], AT_CONFIG, 0);
	FreeAgent(&oldAgent[j]);
    }
    free(oldAgent);
    __pmAccFreeSavedLists();

    /* Start the new agents */
    ContactAgents();
    for (i = 0; i < MAXDOMID + 2; i++)
	mapdom[i] = nAgents;
    for (i = 0; i < nAgents; i++)
	if (agent[i].status.connected)
	    mapdom[agent[i].pmDomainId] = i;

    /* Now recalculate the access controls for each client and update the
     * connection count in the ACL entries matching the client (and account).
     * If the client is no longer permitted the connection because of a change
     * in permissions or connection limit, the client's connection is closed.
     */
    for (i = 0; i < nClients; i++) {
	ClientInfo	*cp = &client[i];

	if (!cp->status.connected)
	    continue;
	if ((sts = CheckClientAccess(cp)) >= 0)
	    sts = CheckAccountAccess(cp);
	if (sts < 0) {
	    /* ignore errors, the client is being terminated in any case */
	    pmcd_trace(TR_XMIT_PDU, cp->fd, PDU_ERROR, sts);
	    __pmSendError(cp->fd, FROM_ANON, sts);
	    CleanupClient(cp, sts);
	}
    }

    PrintAgentInfo(stderr);
    __pmAccDumpLists(stderr);

    /* Gather any deceased children, some may be PMDAs that were
     * terminated by CleanupAgent or killed and had not exited
     * when the previous harvest() was done
     */
    HarvestAgents(0);
}
