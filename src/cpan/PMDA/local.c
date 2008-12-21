/*
 * Copyright (c) 2008 Aconex.  All Rights Reserved.
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include "local.h"
#include <search.h>
#include <sys/stat.h>

static timers_t *timers;
static int ntimers;
static files_t *files;
static int nfiles;

static char buffer[4096];

extern void timer_callback(int, void *);
extern void input_callback(scalar_t *, int, char *);

char *
local_strdup_hashed(const char *string)
{
    static int local_hash;
    ENTRY e;

    if (!local_hash) {
	local_hash = 1;
	hcreate(500);
    }
    e.key = e.data = (char *)string;
    if (!hsearch(e, FIND)) {
	e.key = e.data = strdup(string);
	hsearch(e, ENTER);
    }
    return e.key;
}

char *
local_strdup_suffix(const char *string, const char *suffix)
{
    size_t length = strlen(string) + strlen(suffix) + 1;
    char *result = malloc(length);

    if (!result)
	return result;
    sprintf(result, "%s%s", string, suffix);
    return result;
}

char *
local_strdup_prefix(const char *prefix, const char *string)
{
    size_t length = strlen(prefix) + strlen(string) + 1;
    char *result = malloc(length);

    if (!result)
	return result;
    sprintf(result, "%s%s", prefix, string);
    return result;
}

int
local_timer(double timeout, scalar_t *callback, int cookie)
{
    int size = sizeof(*timers) * (ntimers + 1);
    delta_t delta;

    delta.tv_sec = (time_t)timeout;
    delta.tv_usec = (long)((timeout - (double)delta.tv_sec) * 1000000.0);

    if ((timers = realloc(timers, size)) == NULL)
	__pmNoMem("timers resize", size, PM_FATAL_ERR);
    timers[ntimers].id = -1;	/* not yet registered */
    timers[ntimers].delta = delta;
    timers[ntimers].cookie = cookie;
    timers[ntimers].callback = callback;
    return ntimers++;
}

int
local_timer_get_cookie(int id)
{
    int i;

    for (i = 0; i < ntimers; i++)
	if (timers[i].id == id)
	    return timers[i].cookie;
    return -1;
}

scalar_t *
local_timer_get_callback(int id)
{
    int i;

    for (i = 0; i < ntimers; i++)
	if (timers[i].id == id)
	    return timers[i].callback;
    return NULL;
}

static int
local_file(int type, int fd, scalar_t *callback, int cookie)
{
    int size = sizeof(*files) * (nfiles + 1);

    if ((files = realloc(files, size)) == NULL)
	__pmNoMem("files resize", size, PM_FATAL_ERR);
    files[nfiles].type = type;
    files[nfiles].fd = fd;
    files[nfiles].cookie = cookie;
    files[nfiles].callback = callback;
    return nfiles++;
}

int
local_pipe(char *pipe, scalar_t *callback, int cookie)
{
    FILE *fp = popen(pipe, "r");
    int me;

    if (!fp) {
	__pmNotifyErr(LOG_ERR, "popen failed (%s): %s", pipe, strerror(errno));
	exit(1);
    }
    me = local_file(FILE_PIPE, fileno(fp), callback, cookie);
    files[me].me.tail.file = fp;
    return fileno(fp);
}

int
local_tail(char *file, scalar_t *callback, int cookie)
{
    FILE *fp = fopen(file, "r");
    struct stat stats;
    int me;

    if (!fp) {
	__pmNotifyErr(LOG_ERR, "fopen failed (%s): %s", file, strerror(errno));
	exit(1);
    }
    if (stat(file, &stats) < 0) {
	__pmNotifyErr(LOG_ERR, "stat failed (%s): %s", file, strerror(errno));
	exit(1);
    }
    me = local_file(FILE_TAIL, fileno(fp), callback, cookie);
    files[me].me.tail.file = fp;
    files[me].me.tail.dev = stats.st_dev;
    files[me].me.tail.ino = stats.st_ino;
    return me;
}

int
local_sock(char *host, int port, scalar_t *callback, int cookie)
{
    struct sockaddr_in myaddr;
    struct hostent *servinfo;
    int me, fd;

    if ((servinfo = gethostbyname(host)) == NULL) {
	__pmNotifyErr(LOG_ERR, "gethostbyname (%s): %s", host, strerror(errno));
	exit(1);
    }
    if ((fd = __pmCreateSocket()) < 0) {
	__pmNotifyErr(LOG_ERR, "socket (%s): %s", host, strerror(errno));
	exit(1);
    }
    memset(&myaddr, 0, sizeof(myaddr));
    myaddr.sin_family = AF_INET;
    memcpy(&myaddr.sin_addr, servinfo->h_addr, servinfo->h_length);
    myaddr.sin_port = htons(port);
    if (connect(fd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
	__pmNotifyErr(LOG_ERR, "connect (%s): %s", host, strerror(errno));
	exit(1);
    }
    me = local_file(FILE_SOCK, fd, callback, cookie);
    files[me].me.sock.host = strdup(host);
    files[me].me.sock.port = port;
    return me;
}

static char *
local_filetype(int type)
{
    if (type == FILE_SOCK)
	return "socket connection";
    if (type == FILE_PIPE)
	return "command pipe";
    if (type == FILE_TAIL)
	return "tailed file";
    return NULL;
}

int
local_files_get_descriptor(int id)
{
    if (id < 0 || id >= nfiles)
	return -1;
    return files[id].fd;
}

void
local_atexit(void)
{
    while (ntimers > 0) {
	--ntimers;
	__pmAFunregister(timers[ntimers].id);
    }
    if (timers) {
	free(timers);
	timers = NULL;
    }
    while (nfiles > 0) {
	--nfiles;
	if (files[nfiles].type == FILE_PIPE)
	    pclose(files[nfiles].me.pipe.file);
	if (files[nfiles].type == FILE_TAIL)
	    fclose(files[nfiles].me.tail.file);
	if (files[nfiles].type == FILE_SOCK) {
	    __pmCloseSocket(files[nfiles].fd);
	    if (files[nfiles].me.sock.host)
		free(files[nfiles].me.sock.host);
	    files[nfiles].me.sock.host = NULL;
	}
    }
    if (files) {
	free(files);
	files = NULL;
    }
}

void
local_pmdaMain(pmdaInterface *self)
{
    int pmcdfd, nready, nfds, i, fd, maxfd = -1;
    fd_set fds, readyfds;
    size_t bytes;
    char *s, *p;

    if ((pmcdfd = __pmdaInFd(self)) < 0)
	exit(1);

    FD_ZERO(&fds);
    FD_SET(pmcdfd, &fds);
    for (i = 0; i < nfiles; i++) {
	fd = files[i].fd;
	FD_SET(fd, &fds);
	if (fd > maxfd)
	    maxfd = fd;
    }
    nfds = ((pmcdfd > maxfd) ? pmcdfd : maxfd) + 1;

    for (i = 0; i < ntimers; i++) {
	timers[i].id = __pmAFregister(&timers[i].delta, &timers[i].cookie,
					timer_callback);
    }

    /* custom PMDA main loop */
    for (;;) {
	memcpy(&readyfds, &fds, sizeof(readyfds));
	nready = select(nfds, &readyfds, NULL, NULL, NULL);
	if (nready == 0)
	    continue;
	if (nready < 0) {
	    if (errno != EINTR) {
		__pmNotifyErr(LOG_ERR, "select failed: %s\n", strerror(errno));
		exit(1);
	    }
	    continue;
	}

	__pmAFblock();

	if (FD_ISSET(pmcdfd, &readyfds)) {
	    if (__pmdaMainPDU(self) < 0) {
		__pmAFunblock();
		exit(1);
	    }
	}

	for (i = 0; i < nfiles; i++) {
	    fd = files[i].fd;
	    if (!(FD_ISSET(fd, &readyfds)))
		continue;
	    bytes = read(fd, buffer, sizeof(buffer));
	    if (bytes < 0) {
		__pmNotifyErr(LOG_ERR, "Data read error on %s: %s\n",
				local_filetype(files[i].type), strerror(errno));
		exit(1);
	    }
	    if (bytes == 0) {
		__pmNotifyErr(LOG_ERR, "No data to read - %s may be closed\n",
				local_filetype(files[i].type));
		exit(1);
	    }
	    buffer[bytes] = '\0';
	    for (s = p = buffer; *s != '\0'; s++) {
		if (*s != '\n')
		    continue;
		*s = '\0';
		input_callback(files[i].callback, files[i].cookie, p);
		p = s + 1;
	    }
	}

	__pmAFunblock();
    }
}

/* Create a root for a local filesystem namespace tree */
char *
local_pmns_root(void)
{
    static char buffer[256];

    snprintf(buffer, sizeof(buffer), "%s/pmns", pmGetConfig("PCP_TMP_DIR"));
    rmdir(buffer);
    if (mkdir2(buffer, 0755) == 0)
	return buffer;
    return NULL;
}

static void
lchdir(const char *path)
{
    (void)chdir(path);	/* debug hook, workaround rval compiler warning */
}

/*
 * Split "metric" up based on "." separators - for non-leaf nodes create a
 * directory, for each leaf node we create a regular file containing the PMID.
 */
int
local_pmns_split(const char *root, const char *metric, const char *pmid)
{
    char *p, *path, mypmid[32] = { 0 }, mymetric[256] = { 0 };
    int fd;

    /* Take copies, so we don't risk scribbling on Perl strings */
    strncpy(mymetric, metric, sizeof(mymetric)-1);
    strncpy(mypmid, pmid, sizeof(mypmid)-1);

    /* Replace '.' with ':' in our local pmid string */
    p = mypmid;
    while ((p = index(p, '.')))
	*p++ = ':';

    mkdir2(root, 0777);
    lchdir(root);
    p = strtok(mymetric, ".");
    do {
	path = p;
	p = strtok(NULL, ".");
	if (p) {
	    mkdir2(path, 0777);
	    lchdir(path);
	} else {
	    fd = open(path, O_WRONLY|O_CREAT|O_EXCL, 0644);
	    write(fd, mypmid, strlen(mypmid));
	    close(fd);
	}
    } while (p);
    return 0;
}

static char *
local_pmns_path(const char *root)
{
    static int offset;
    static char path[MAXPATHLEN];
    char *p, *s;

    p = getcwd(path, sizeof(path));
    if (!offset) {		/* first call, we're at the root */
	offset = strlen(root);
	return NULL;
    }
    p += offset + 1;		/* move past the tmpdir prefix */
    for (s = p; *s; s++)	/* replace path-pmns separator */
	if (*s == '/' || *s == '\\')
	    *s = '.';
    return p;
}

/*
 * Print out all entries at the current level, including leaf nodes
 * (with PMIDs) then recursively descend into subtrees.  Use chdir
 * and getcwd to avoid building up the path at each step of the way.
 */
int
local_pmns_write(const char *path)
{
    struct dirent **list;
    struct stat sbuf;
    char *p, *pmns, pmid[32];
    int i, fd, num;

    lchdir(path);
    p = pmns = local_pmns_path(path);
    if (p != NULL)
	printf("%s {\n", local_pmns_path(p));
    else if (strcmp(getenv("PCP_PERL_PMNS"), "root") == 0) {
	pmns = "root";
	printf("%s {\n", pmns);
    }

    num = scandir(".", &list, NULL, NULL);
    for (i = 0; i < num; i++) {
	p = list[i]->d_name;
	if (*p == '.')
	    goto clobber;
	if (stat(p, &sbuf) != 0)
	    return -1;
	if (S_ISDIR(sbuf.st_mode)) {
	    if (pmns)
		printf("\t%s\n", p);
	    continue;	/* directories are not clobbered, descend later */
	}
	else {
	    fd = open(p, O_RDONLY);
	    memset(pmid, 0, sizeof(pmid));
	    if (read(fd, pmid, sizeof(pmid)-1) < 0)
		return -1;
	    close(fd);
	    printf("\t%s\t\t%s\n", p, pmid);
	}
clobber:	/* overwrite entries we are done with */
	*p = '\0';
    }
    if (pmns)
	printf("}\n\n");

    for (i = 0; i < num; i++) {
	p = list[i]->d_name;
	if (*p) {
	    lchdir(path);
	    if (local_pmns_write(p) < 0)
		return -1;
	    lchdir("..");
	}
	free(list[i]);
    }
    free(list);
    return 0;
}

/*
 * Walk the tree depth-first, unlink files, rmdir directories (cleanup)
 */
int
local_pmns_clear(const char *root)
{
    struct dirent **list;
    struct stat sbuf;
    int i, num;
    char *p;

    lchdir(root);
    num = scandir(".", &list, NULL, NULL);
    for (i = 0; i < num; i++) {
	p = list[i]->d_name;
	if (*p == '.')
	    ;
	else if (stat(p, &sbuf) < 0)
	    ;
	else if (!S_ISDIR(sbuf.st_mode))
	    unlink(p);
	else
	    local_pmns_clear(p);
	free(list[i]);
    }
    free(list);
    lchdir("..");
    rmdir(root);
    return 0;
}
