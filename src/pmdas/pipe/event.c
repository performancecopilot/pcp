/*
 * Event support for the pipe performance metrics domain agent.
 *
 * Copyright (c) 2015 Red Hat.
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

#include "event.h"
#include "pmda.h"
#include "util.h"
#include <ctype.h>
#include <assert.h>

/* table of attached PMAPI client programs */
static struct pipe_client *ctxtab;
static int ctxtab_size;

/* table of configured pipe commands */
static struct pipe_command *cmdtab;
static int cmdtab_size;

/* table of instance access permissions */
static struct pipe_acl *acltab;
static int acltab_size;
#define MAX_ACL_OPS 32

static void
enlarge_ctxtab(int context)
{
    /* Grow the context table if needed */
    if (ctxtab_size <= context) {
	size_t		needs;
	int		i;

	needs = (context + 1) * sizeof(struct pipe_client);
	ctxtab = realloc(ctxtab, needs);
	if (ctxtab == NULL)
	    __pmNoMem("client ctx table", needs, PM_FATAL_ERR);

	/* Initialise new entries to zero, esp. "active" field */
	while (ctxtab_size <= context) {
	    pipe_client	*client = &ctxtab[ctxtab_size];
	    pipe_groot	*groots;

	    needs = cmdtab_size * sizeof(struct pipe_groot);
	    groots = calloc(cmdtab_size, sizeof(struct pipe_groot));
	    if (groots == NULL)
		__pmNoMem("client groots", needs, PM_FATAL_ERR);

	    memset(client, 0, sizeof(pipe_client));
	    client->pipes = groots;
	    for (i = 0; i < cmdtab_size; i++) {
		client->pipes[i].inst = cmdtab[i].inst;
		client->pipes[i].queueid = -1;
	    }
	    ctxtab_size++;
	}
    }
}

static pipe_command *
enlarge_cmdtab(void)
{
    /* Grow the command table */
    size_t	needed = (cmdtab_size + 1) * sizeof(pipe_command);
    pipe_command *pc;

    if ((cmdtab = realloc(cmdtab, needed)) == NULL)
	__pmNoMem("command table", needed, PM_FATAL_ERR);
    pc = &cmdtab[cmdtab_size++];
    memset(pc, 0, sizeof(*pc));
    return pc;
}

static pipe_acl *
enlarge_acltab(void)
{
    /* Grow the access control table */
    size_t	needed = (acltab_size + 1) * sizeof(pipe_acl);
    pipe_acl	*pa;

    if ((acltab = realloc(acltab, needed)) == NULL)
	__pmNoMem("access control table", needed, PM_FATAL_ERR);
    pa = &acltab[acltab_size++];
    memset(pa, 0, sizeof(*pa));
    return pa;
}

static int
add_parameter(const char *start, const char *end,
		int *nparams, char ***paramtab)
{
    int		size, count = (*nparams + 1);
    char	*param = strndup(start, end - start);
    char	**params = *paramtab;
    size_t	length = count * sizeof(char **);

    size = end - start + 1;
    if (param == NULL)
	__pmNoMem("param", size, PM_FATAL_ERR);
    if ((params = realloc(params, length)) == NULL)
	__pmNoMem("param table", length, PM_FATAL_ERR);
    params[count-1] = param;

    *paramtab = params;
    *nparams = count;
    return size - 1;
}

static void
free_parameters(int nparams, char **paramtab)
{
    while (--nparams >= 0)
	free(paramtab[nparams]);
    free(paramtab);
}

/* replace pipe command string wildcards with user-supplied parameters */
static char *
setup_cmdline(pipe_client *pc, pipe_command *cmd, char *params)
{
    static char		buffer[MAXPATHLEN];
    size_t		paramlen, total = 0;
    char		*start, *end, *p, *q;
    char		**paramtab = NULL;
    int			i, j, n, len, nparams = 0;

    memset(buffer, 0, sizeof(buffer));
    /* step 1: split params into the separate parameters */
    for (p = start = params; *p != '\0'; p++) {
	if (*p == ',')	/* accept comma as whitespace alternative */
	    *p = ' ';
	if (isspace(*p)) {
	    if (!isspace(*start) && start != p)
		total += add_parameter(start, p, &nparams, &paramtab);
	    start = p + 1;
	} else if (!isalnum(*p)) {
	    if (pmDebug & DBG_TRACE_APPL2)
		fprintf(stderr, "invalid parameter string at '%c'", *p);
	    goto fail;
	} else if (*(p + 1) == '\0') {
	    total += add_parameter(start, p + 1, &nparams, &paramtab);
	}
    }
    if (nparams)
	total += nparams - 1;	/* whitespace separators, in $0 expansion */

    /* step 2: build command buffer replacing each $1..N */
    q = &buffer[0];
    for (i = 0, p = cmd->command; i < sizeof(buffer)-2 && *p != '\0'; p++) {
	if (*p == '$') {
	    n = (int)strtol(++p, &end, 10);
	    if (p+1 != end && *end != '\0')	{	/* bad config? */
		if (pmDebug & DBG_TRACE_APPL2)
		    fprintf(stderr, "invalid configuration file parameter");
		goto fail;
	    }
	    p = end - 1;
	    if (n > nparams) {
		if (pmDebug & DBG_TRACE_APPL2)
		    fprintf(stderr, "too few parameters passed (%d >= %d)",
				n, nparams);
		goto fail;
	    }
	    /* check that the result will fit in the buffer */
	    if (n > 0)
		paramlen = strlen(paramtab[n-1]);
	    else
		paramlen = total;
	    if (paramlen + (q - buffer) >= sizeof(buffer) - 1) {
		if (pmDebug & DBG_TRACE_APPL2)
		    fprintf(stderr, "insufficient space for substituting "
				"parameter %d", n);
		goto fail;
	    }
	    /* copy into the buffer and adjust our position */
	    if (n)
		strncat(q, paramtab[n-1], paramlen);
	    else {	/* expand $0 */
		paramlen = 0;
		for (j = 0; j < nparams; j++) {
		    if (j != 0) {
			strncat(q, " ", 1);
			paramlen++;
		    }
		    len = strlen(paramtab[j]);
		    strncat(q, paramtab[j], len);
		    paramlen += len;
		}
		assert(paramlen == total);
	    }
	    q += paramlen;
	    p = end - 1;
	} else {
	    *q++ = *p;
	}
    }
    *q = '\0';
    free_parameters(nparams, paramtab);
    return buffer;

fail:
    free_parameters(nparams, paramtab);
    return NULL;
}

static int
check_access(pmInDom aclops, pipe_client *client, pipe_command *cmd)
{
    unsigned int denyops = 0;
    int		sts, operation = 0;

    if (acltab_size == 0)
	return 0;

    if ((sts = pmdaCacheLookupName(aclops, cmd->identifier, &operation, NULL)) < 0)
	return 0;	/* not a controlled operation, allow */

    if ((sts = __pmAccAddAccount(client->uid, client->gid, &denyops)) < 0)
	return sts;

    if (pmDebug & DBG_TRACE_AUTH) {
	__pmNotifyErr(LOG_DEBUG, "check_access: access %s for %s"
				 " (uid=%s,gid=%s operation=%d denyops=%u)",
		(denyops & operation) ? "denied":"granted", cmd->identifier,
		client->uid, client->gid, operation, denyops);
    }

    if (denyops & operation)
	return PM_ERR_PERMISSION;
    return 0;
}

int
event_init(int context, pmInDom aclops, pipe_command *cmd, char *params)
{
    struct pipe_client	*client;
    struct pipe_groot	*groot = NULL;
    char		*comm;
    int			i, sts;

    if (pmDebug & DBG_TRACE_APPL0)
        __pmNotifyErr(LOG_DEBUG, "event_init: %s[%d] starting: %s [%s] maxmem=%ld",
		cmd->identifier, cmd->inst, cmd->command, params, (long)maxmem);

    assert(ctxtab_size > context);	/* event_client_access ensures this */
    client = &ctxtab[context];

    for (i = 0; i < cmdtab_size; i++) {
	groot = &client->pipes[i];
	if (groot->inst == cmd->inst)
	    break;
    }
    assert(i != cmdtab_size);		/* pipe_indom ensures this */

    if ((sts = check_access(aclops, client, cmd)) < 0)
	return sts;

    if ((comm = setup_cmdline(client, cmd, params)) == NULL)
	return PM_ERR_BADSTORE;

    if ((sts = start_cmd(comm, cmd->user, &groot->pid)) < 0)
	return sts;
    groot->fd = pipe_setfd(sts);

    snprintf(groot->qname, sizeof(groot->qname), "%s#%d",
		cmd->identifier, context);
    groot->queueid = pmdaEventNewQueue(groot->qname, maxmem);
    pmdaEventSetAccess(context, groot->queueid, 1);
    groot->active = 1;

    if (pmDebug & DBG_TRACE_APPL0)
        __pmNotifyErr(LOG_DEBUG, "event_init: %s started: pid=%d fd=%d qid=%d",
		cmd->identifier, groot->pid, groot->fd, groot->queueid);

    return 0;
}

void
event_client_access(int context)
{
    enlarge_ctxtab(context);
    pmdaEventNewClient(context);
}

static int
event_create(struct pipe_groot *pipe)
{
    int			j;
    char		*s, *p;
    size_t		offset;
    ssize_t		bytes;
    struct timeval	timestamp;

    static char		*buffer;
    static int		bufsize;

    /*
     * Using a static (global) event buffer to hold initial read.
     * The aim is to reduce memory allocation until we know we'll
     * need to keep something.
     */
    if (!buffer) {
	int	sts = 0;

	bufsize = 16 * getpagesize();
#ifdef HAVE_POSIX_MEMALIGN
	sts = posix_memalign((void **)&buffer, getpagesize(), bufsize);
#else
#ifdef HAVE_MEMALIGN
	buffer = (char *)memalign(getpagesize(), bufsize);
	if (buffer == NULL) sts = -1;
#else
	buffer = (char *)malloc(bufsize);
	if (buffer == NULL) sts = -1;
#endif
#endif
	if (sts != 0) {
	    __pmNotifyErr(LOG_ERR, "event buffer allocation failure");
	    return -1;
	}
    }

    offset = 0;
multiread:
    if (pipe->fd < 0)
    	return 0;
    bytes = read(pipe->fd, buffer + offset, bufsize - 1 - offset);
    /*
     * Ignore the error if:
     * - we've got EOF (0 bytes read)
     * - EBADF (fd isn't valid - most likely a closed pipe)
     * - EAGAIN/EWOULDBLOCK (fd is marked nonblocking and read would block)
     * - EINVAL/EISDIR (fd is a directory - config file botch)
     */
    if (bytes == 0)
	return 0;
    if (bytes < 0 && (errno == EBADF || errno == EISDIR || errno == EINVAL))
	return 0;
    if (bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
	return 0;
    if (bytes > maxmem)
	return 0;
    if (bytes < 0) {
	__pmNotifyErr(LOG_ERR, "read failure on client fd %d: %s",
		      pipe->fd, strerror(errno));
	return -1;
    }

    /*
     * good read ... data up to buffer + offset + bytes is all OK
     * so mark end of data
     */
    buffer[offset+bytes] = '\0';

    gettimeofday(&timestamp, NULL);
    for (s = p = buffer, j = 0; *s != '\0' && j < bufsize-1; s++, j++) {
	if (*s != '\n')
	    continue;
	*s = '\0';
	bytes = (s+1) - p;
	pmdaEventQueueAppend(pipe->queueid, p, bytes, &timestamp);
	p = s + 1;
    }
    /* did we just do a full buffer read? */
    if (p == buffer) {
	char msg[64];
	__pmNotifyErr(LOG_ERR, "Ignoring long (%d bytes) line: \"%s\"", (int)
			bytes, __pmdaEventPrint(p, bytes, msg, sizeof(msg)));
    } else if (j == bufsize - 1) {
	offset = bufsize-1 - (p - buffer);
	memmove(buffer, p, offset);
	goto multiread;	/* read rest of line */
    }
    return 1;
}

/* check for input on active file descriptors, enqueue events */
void
event_capture(fd_set *readyfds)
{
    struct pipe_client	*client;
    struct pipe_groot	*groot;
    int			i, j;

    for (i = 0; i < ctxtab_size; i++) {
	client = &ctxtab[i];
	for (j = 0; j < cmdtab_size; j++) {
	    groot = &client->pipes[j];
	    if (groot->active &&
	        groot->fd > 0 &&
		FD_ISSET(groot->fd, readyfds))
		event_create(groot);
	}
    }
}

/* force pipe process(es) to end when client disconnects */
void
event_client_shutdown(int context)
{
    struct pipe_client	*client;
    struct pipe_groot	*groot;
    int			i;

    if (ctxtab_size <= context)
	return;

    client = &ctxtab[context];
    for (i = 0; i < cmdtab_size; i++) {
	groot = &client->pipes[i];
	if (groot->pid > 0) {
	    stop_cmd(groot->pid);
	    groot->pid = 0;
	}
	if (groot->fd > 0) {
	    pipe_clearfd(groot->fd);
	    close(groot->fd);
	    groot->fd = 0;
	}
	pmdaEventQueueShutdown(groot->queueid);
    }
    if (client->uid)
	free(client->uid);
    client->uid = NULL;
    if (client->gid)
	free(client->gid);
    client->gid = NULL;
    for (i = 0; i < cmdtab_size; i++) {
	memset(&client->pipes[i], 0, sizeof(pipe_groot));
	client->pipes[i].inst = cmdtab[i].inst;
	client->pipes[i].queueid = -1;
    }
}

/*
 * Some pipe process ended (SIGCHLD), reap process(es) and data.
 * Note that we may still have PMAPI clients waiting on the data
 * so care taken to not disrupt contexts.
 */
void
event_child_shutdown(void)
{
    struct pipe_client	*client;
    struct pipe_groot	*groot;
    int			i, j, sts;

    for (i = 0; i < ctxtab_size; i++) {
	client = &ctxtab[i];
	for (j = 0; j < cmdtab_size; j++) {
	    groot = &client->pipes[j];
	    if (!groot->pid)
		continue;
	    if ((sts = wait_cmd(groot->pid)) < 0)
		continue;
	    groot->status = sts;
	    groot->exited = 1;
	    groot->pid = 0;
	    if (groot->fd > 0) {
		pipe_clearfd(groot->fd);
		event_create(groot);
		close(groot->fd);
	    }
	    pmdaEventQueueShutdown(groot->queueid);
	    groot->active = 0;
	}
    }
}

int
event_qactive(int context, unsigned int inst)
{
    struct pipe_client	*client;
    struct pipe_groot	*groot;
    int			i;

    if (context < 0 || context >= ctxtab_size)
	return PM_ERR_INST;
    client = &ctxtab[context];
    for (i = 0; i < cmdtab_size; i++) {
	groot = &client->pipes[i];
	if (inst == groot->inst)
	    return groot->active;
    }
    return PM_ERR_INST;
}

int
event_userid(int context, const char *user)
{
    if (context < 0 || context >= ctxtab_size)
	return PM_ERR_INST;
    ctxtab[context].uid = strdup(user);
    return 0;
}

int
event_groupid(int context, const char *group)
{
    if (context < 0 || context >= ctxtab_size)
	return PM_ERR_INST;
    ctxtab[context].gid = strdup(group);
    return 0;
}

int
event_queueid(int context, unsigned int inst)
{
    struct pipe_client	*client;
    struct pipe_groot	*groot;
    int			i;

    if (context < 0 || context >= ctxtab_size)
	return PM_ERR_INST;
    client = &ctxtab[context];
    for (i = 0; i < cmdtab_size; i++) {
	groot = &client->pipes[i];
	if (inst != groot->inst)
	    continue;
	if (groot->queueid < 0)
	    return PM_ERR_NOTCONN;
	return groot->queueid;
    }
    return PM_ERR_INST;
}

void *
event_qdata(int context, unsigned int inst)
{
    struct pipe_client	*client;
    struct pipe_groot	*groot;
    int			i;

    client = &ctxtab[context];
    for (i = 0; i < cmdtab_size; i++) {
	groot = &client->pipes[i];
	if (inst == groot->inst)
	    return (void *)groot;
    }
    return NULL;
}

int
event_decoder(int eventarray, void *buffer, size_t size,
		struct timeval *timestamp, void *data)
{
    struct pipe_groot	*groot = (struct pipe_groot *)data;
    pmAtomValue		atom;
    int			count = 0;
    int			flag = 0;
    int			sts;

    if (pmDebug & DBG_TRACE_APPL0)
	__pmNotifyErr(LOG_DEBUG, "event_decoder on queue %s", groot->qname);

    if (groot->count++ == 0)
	flag |= PM_EVENT_FLAG_START;
    else if (!groot->exited)
	flag |= PM_EVENT_FLAG_POINT;
    if (groot->exited)
	flag |= PM_EVENT_FLAG_END;

    sts = pmdaEventAddRecord(eventarray, timestamp, flag);
    if (sts < 0)
	return sts;

    atom.cp = buffer;
    sts = pmdaEventAddParam(eventarray, *paramline, PM_TYPE_STRING, &atom);
    if (sts < 0)
	return sts;
    count++;

    return count;
}

static int 
event_parse_acl(const char *fname, char *p, int linenum)
{
    pipe_acl	*pa;
    char	*token;

    pa = enlarge_acltab();

    /* positioned at start of allow/disallow directive */
    token = p;
    while (!isspace(*p) && *p != '\0')
	p++;

    if (strncmp(token, "disallow", sizeof("disallow")-1) == 0)
	pa->disallow = 1;
    else if (strncmp(token, "allow", sizeof("allow")-1) == 0)
	pa->allow = 1;
    if (!pa->disallow && !pa->allow) {
	fprintf(stderr, "event_config: %s line %d, bad access directive '%s'\n",
			fname, linenum, token);
	return -1;
    }
    while (isspace(*p) && *p != '\0')
	p++;

    /* positioned at start of user/group directive */
    token = p;
    while (!isspace(*p) && *p != '\0')
	p++;
    if (strncmp(token, "group", sizeof("group")-1) == 0)
	pa->group = 1;
    else if (strncmp(token, "user", sizeof("user")-1) == 0)
	pa->user = 1;
    if (!pa->user && !pa->group) {
	fprintf(stderr, "event_config: %s line %d, bad access entity '%s'\n",
			fname, linenum, token);
	return -1;
    }
    while (isspace(*p) && *p != '\0')
	p++;

    /* positioned at start of the actual user/group name */
    pa->name = p;
    while (!isspace(*p) && *p != '\0' && *p != ':')
	p++;
    *p++ = '\0';
    if (!pa->name || !*pa->name) {
	fprintf(stderr, "event_config: %s line %d, bad %s name '%s'\n",
			fname, linenum, pa->user ? "user" : "group", pa->name);
	return -1;
    }
    while (isspace(*p) && *p != '\0')
	p++;
    if (*p == ':') {
	p++;
	while (isspace(*p) && *p != '\0')
	    p++;
    }
    pa->name = strdup(pa->name);

    /* positioned at start of the pipe instance name */
    pa->identifier = p;
    while (!isspace(*p) && *p != '\0')
	p++;
    *p++ = '\0';
    if (!pa->identifier || !*pa->identifier) {
	fprintf(stderr, "event_config: %s line %d, bad instance '%s'\n",
			fname, linenum, pa->identifier);
	return -1;
    }
    pa->identifier = strdup(pa->identifier);

    if (pmDebug & DBG_TRACE_APPL0)
	fprintf(stderr, "[%s %s=%s line=%d] instance: %s\n",
		pa->allow? "allow":"disallow", pa->user? "user":"group",
		pa->name, linenum, pa->identifier);
    return 0;
}

static int
event_parse_cmd(const char *fname, char *p, int linenum)
{
    pipe_command	*pc;

    /*
     * split out instance, username, and command fields
     */
    pc = enlarge_cmdtab();
    pc->identifier = p;
    while (!isspace(*p) && *p != '\0')
	p++;
    *p = '\0';
    pc->identifier = strdup(pc->identifier);
    if (*(p+1) == '\0')
	goto done;
    p++;

    while (isspace(*p))
	p++;
    pc->user = p;
    while (!isspace(*p) && *p != '\0')
	p++;
    *p = '\0';
    pc->user = strdup(pc->user);
    if (*(p+1) == '\0')
	goto done;
    p++;

    while (isspace(*p))
	p++;
    pc->command = p;
    while (*p != '\n' && *p != '\r' && *p != '\0')
	p++;
    *p = '\0';
    pc->command = strdup(pc->command);

done:
    /* verify contents from this line now that it's completely parsed */
    if (pc->identifier == NULL || *pc->identifier == '\0') {
	fprintf(stderr, "event_config: %s line %d missing identifier\n",
			fname, linenum);
	return -1;
    }
    if (pc->user == NULL || *pc->user == '\0') {
	fprintf(stderr, "event_config: %s line %d missing user name\n",
			fname, linenum);
	return -1;
    }
    if (pc->command == NULL || *pc->command == '\0') {
	fprintf(stderr, "event_config: %s line %d missing a command\n",
			fname, linenum);
	return -1;
    }

    if (pmDebug & DBG_TRACE_APPL0)
	fprintf(stderr, "[name=%s user=%s line=%d] command: %s\n",
		pc->identifier, pc->user, linenum, pc->command);
    return 0;
}

/*
 * Parse the configuration file and do initial data structure setup.
 */
int
event_config(const char *fname)
{
    FILE		*config;
    char		*p, line[MAXPATHLEN * 2];
    int			initial_size = cmdtab_size;
    int			access_control = 0;
    int			linenum = 0;
    int			sts = 0;

    if ((config = fopen(fname, "r")) == NULL) {
	__pmNotifyErr(LOG_ERR, "event_config: %s: %s", fname, strerror(errno));
	return -1;
    }

    while (!feof(config)) {
	if (fgets(line, sizeof(line), config) == NULL) {
	    if (feof(config))
		break;
	    __pmNotifyErr(LOG_ERR, "event_config: fgets: %s", strerror(errno));
	    sts = -1;
	    break;
	}

	linenum++;
	p = line;

	/* skip over any whitespace at start of line */
	while (isspace(*p))
	    p++;
	/* skip empty or comment lines (hash-prefix) */
	if (*p == '\n' || *p == '\0' || *p == '#')
	    continue;

	if (pmDebug & DBG_TRACE_APPL1)
	    fprintf(stderr, "[%d] %s", linenum, line);

	/* handle access control section separately */
	if (strncmp(p, "[access]", 8) == 0) {
	    access_control = 1;
	    continue;
	}

	sts = access_control?
		event_parse_acl(fname, p, linenum) :
		event_parse_cmd(fname, p, linenum);
	if (sts < 0)
	    break;
    }
    fclose(config);
    if (sts < 0) {
	cmdtab_size = 0;
	free(cmdtab);
	return sts;
    }
    if (cmdtab_size == initial_size) {
	__pmNotifyErr(LOG_ERR, "event_config: no commands found in %s", fname);
	cmdtab_size = 0;
	free(cmdtab);
	return -1;
    }
    return cmdtab_size;
}

int
event_config_dir(const char *dname)
{
    struct dirent	**list;
    char		path[MAXPATHLEN];
    int			sep = __pmPathSeparator();
    int			i, n, sts = 0;

    if ((n = scandir(dname, &list, NULL, NULL)) < 0) {
	__pmNotifyErr(LOG_ERR, "event_config_dir: %s - %s",
			dname, osstrerror());
	return n;
    }
    for (i = 0; i < n; i++) {
	if (list[i]->d_name[0] == '.')
	    continue;
	snprintf(path, sizeof(path), "%s%c%s", dname, sep, list[i]->d_name);
	path[sizeof(path)-1] = '\0';
	if ((sts = event_config(path)) < 0)
	    break;
    }
    for (i = 0; i < n; i++)
	free(list[i]);
    free(list);

    return sts;
}

/*
 * Setup the libpcp access control list operations cache;
 * we need one operation ID for each instance configured
 * to be under access control.
 */
void
event_acl(pmInDom aclops)
{
    pipe_acl		*pa;
    int			i, allops = 0, sts = 0;
    int			nusers = 0, ngroups = 0;

    pmdaCacheOp(aclops, PMDA_CACHE_CULL);
    pmdaCacheResize(aclops, MAX_ACL_OPS - 1);

    /*
     * Stash each identifier (instance name) into the ACL operation cache.
     * This assigns the operation identifiers we'll use for checks, later.
     */
    for (i = 0; i < acltab_size; i++) {
	pa = &acltab[i];

	if (strcmp(pa->identifier, "*") == 0)
	    continue;
	pmdaCacheStore(aclops, PMDA_CACHE_ADD, pa->identifier, NULL);
	pmdaCacheLookupName(aclops, pa->identifier, &pa->operation, NULL);

	if (pa->user)
	    nusers++;
	if (pa->group)
	    ngroups++;

	if (pmDebug & DBG_TRACE_APPL0)
	    __pmNotifyErr(LOG_DEBUG, "event_acl: added op %s[%u]",
			pa->identifier, pa->operation);
    }

    /* walk the indom (hash) and add each op via __pmAccAddOp */
    for (pmdaCacheOp(aclops, PMDA_CACHE_WALK_REWIND);;) {
	if ((i = pmdaCacheOp(aclops, PMDA_CACHE_WALK_NEXT)) < 0)
	    break;
	if ((sts = __pmAccAddOp(1 << i)) < 0) {
	    __pmNotifyErr(LOG_ERR, "event_acl: __pmAccAddOp[%d] - %s",
			i, pmErrStr(sts));
	    exit(1);
	}
	allops |= (1 << i);
    }

    /* walk ACLs once more to setup libpcp user/group deny/allow mappings */
    for (i = 0; i < acltab_size; i++) {
	int	denyOps = 0, specOps = 0;

	pa = &acltab[i];

	if (strcmp(pa->identifier, "*") != 0) {
	    specOps = (1 << pa->operation);
	} else {
	    denyOps = pa->allow ? 0 : allops;
	    specOps = allops;
	}
	if (pa->allow)
	    denyOps &= ~(1 << pa->operation);
	else
	    denyOps |= (1 << pa->operation);

	if (pa->user) {
	    if ((sts = __pmAccAddUser(pa->name, specOps, denyOps, 0)) < 0) {
		__pmNotifyErr(LOG_ERR, "event_acl: __pmAccAddUser[%s] - %s",
				pa->name, pmErrStr(sts));
		exit(1);
	    }
	}
	if (pa->group) {
	    if ((sts = __pmAccAddGroup(pa->name, specOps, denyOps, 0)) < 0) {
		__pmNotifyErr(LOG_ERR, "event_acl: __pmAccAddGroup[%s] - %s",
				pa->name, pmErrStr(sts));
		exit(1);
	    }
	}
    }

    if (pmDebug & DBG_TRACE_APPL1) {
	if (nusers)
	    __pmAccDumpUsers(stderr);
	if (ngroups)
	    __pmAccDumpGroups(stderr);
    }
}

/*
 * Setup the pipe indom table, after all realloc calls have been made
 */
void
event_indom(pmInDom pipe_indom)
{
    pipe_command	*pc;
    int			i;

    /* initialize the instance domain cache */
    pmdaCacheOp(pipe_indom, PMDA_CACHE_LOAD);
    pmdaCacheOp(pipe_indom, PMDA_CACHE_INACTIVE);

    for (i = 0; i < cmdtab_size; i++) {
	pc = &cmdtab[i];
	pmdaCacheStore(pipe_indom, PMDA_CACHE_ADD, pc->identifier, pc);
	pmdaCacheLookupName(pipe_indom, pc->identifier, &pc->inst, NULL);

	if (pmDebug & DBG_TRACE_APPL0)
            __pmNotifyErr(LOG_DEBUG, "event_indom: added %s[%d]", pc->identifier, pc->inst);
    }

    pmdaCacheOp(pipe_indom, PMDA_CACHE_SAVE);
}
