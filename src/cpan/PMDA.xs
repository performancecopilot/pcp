/*
 * Copyright (c) 2008 Aconex.  All Rights Reserved.
 * Copyright (c) 2004 Silicon Graphics, Inc.  All Rights Reserved.
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

/* XXX - TODO: need to install a SIGCHLD signal handler when pipes in use */
/* XXX - TODO: reconnect -- socket(host/port) and logrotate(inode/device) */

#ifdef __cplusplus
extern "C" {
#endif
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include <syslog.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "local.h"
#ifdef __cplusplus
}
#endif

static pmdaInterface dispatch;
static pmdaMetric *metrictab;
static int mtab_size;
static pmdaIndom *indomtab;
static int itab_size;

static SV *fetch_func;
static SV *instance_func;
static SV *store_cb_func;
static SV *fetch_cb_func;

static timers_t *timers;
static int ntimers;
static files_t *files;
static int nfiles;

static char local_buffer[4096];

int
local_timer(double timeout, SV *callback, int cookie)
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

SV *
local_timer_get_callback(int id)
{
    int i;

    for (i = 0; i < ntimers; i++)
	if (timers[i].id == id)
	    return timers[i].callback;
    return NULL;
}

int
local_fetch(int numpmid, pmID *pmidlist, pmResult **rp, pmdaExt *pmda)
{
    dSP;
    PUSHMARK(sp);

    perl_call_sv(fetch_func, G_DISCARD|G_NOARGS);
    return pmdaFetch(numpmid, pmidlist, rp, pmda);
}

int
local_instance(pmInDom indom, int a, char *b, __pmInResult **rp, pmdaExt *pmda)
{
    dSP;
    PUSHMARK(sp);
    XPUSHs(sv_2mortal(newSViv(indom)));
    PUTBACK;

    perl_call_sv(instance_func, G_VOID|G_DISCARD);
    return pmdaInstance(indom, a, b, rp, pmda);
}

void
local_timer_callback(int afid, void *data)
{
    dSP;
    PUSHMARK(sp);
    XPUSHs(sv_2mortal(newSViv(local_timer_get_cookie(afid))));
    PUTBACK;

    perl_call_sv(local_timer_get_callback(afid), G_VOID|G_DISCARD);
}

void
local_input_callback(SV *input_cb_func, char *string)
{
    dSP;
    PUSHMARK(sp);
    XPUSHs(sv_2mortal(newSVpv(string,0)));
    PUTBACK;

    perl_call_sv(input_cb_func, G_VOID|G_DISCARD);
}

int
local_fetch_callback(pmdaMetric *metric, unsigned int inst, pmAtomValue *atom)
{
    dSP;
    __pmID_int	*pmid;
    int		sts;

    ENTER;
    SAVETMPS;	/* allows us to tidy our perl stack changes later */

    pmid = (__pmID_int *) &metric->m_desc.pmid;

    PUSHMARK(sp);
    XPUSHs(sv_2mortal(newSViv(pmid->cluster)));
    XPUSHs(sv_2mortal(newSViv(pmid->item)));
    XPUSHs(sv_2mortal(newSViv(inst)));
    PUTBACK;

    sts = perl_call_sv(fetch_cb_func, G_ARRAY);
    SPAGAIN;	/* refresh local perl stack pointer after call */
    if (sts != 2) {
	croak("fetch CB error (returned %d values, expected 2)", sts); 
	sts = -EINVAL;
	goto fetch_end;
    }
    sts = POPi;		/* pop function return status */
    if (sts < 0) {
	goto fetch_end;
    } else if (sts == 0) {
	sts = POPi;
	goto fetch_end;
    }

    switch (metric->m_desc.type) {	/* pop result value */
	case PM_TYPE_32:	atom->l = POPi; break;
	case PM_TYPE_U32:	atom->ul = POPi; break;
	case PM_TYPE_64:	atom->ll = POPl; break;
	case PM_TYPE_U64:	atom->ull = POPl; break;
	case PM_TYPE_FLOAT:	atom->f = POPn; break;
	case PM_TYPE_DOUBLE:	atom->d = POPn; break;
	case PM_TYPE_STRING:	atom->cp = strdup(POPpx); break;
    }

fetch_end:
    PUTBACK;
    FREETMPS;
    LEAVE;	/* fix up the perl stack, freeing anything we created */
    return sts;
}

int
local_store(pmResult *result, pmdaExt *pmda)
{
    dSP;
    int		i, j;
    int		type;
    int		sts = 0;
    pmAtomValue	av;
    pmValueSet	*vsp;
    __pmID_int	*pmid;

    ENTER;
    SAVETMPS;	/* allows us to tidy our perl stack changes later */

    for (i = 0; i < result->numpmid; i++) {
	vsp = result->vset[i];
	pmid = (__pmID_int *)&vsp->pmid;

	/* need to find the type associated with this PMID */
	for (j = 0; j < mtab_size; j++)
	    if (metrictab[j].m_desc.pmid == *(pmID *)pmid)
		break;
	if (j == mtab_size) {
	    sts = PM_ERR_PMID;
	    goto store_end;
	}
	type = metrictab[j].m_desc.type;

	for (j = 0; j < vsp->numval; j++) {
	    PUSHMARK(sp);
	    XPUSHs(sv_2mortal(newSViv(pmid->cluster)));
	    XPUSHs(sv_2mortal(newSViv(pmid->item)));
	    XPUSHs(sv_2mortal(newSViv(vsp->vlist[j].inst)));
	    sts = pmExtractValue(vsp->valfmt, &vsp->vlist[j],type, &av,type);
	    if (sts < 0)
		goto store_end;
	    switch (type) {
		case PM_TYPE_32:     XPUSHs(sv_2mortal(newSViv(av.l))); break;
		case PM_TYPE_U32:    XPUSHs(sv_2mortal(newSViv(av.ul))); break;
		case PM_TYPE_64:     XPUSHs(sv_2mortal(newSViv(av.ul))); break;
		case PM_TYPE_U64:    XPUSHs(sv_2mortal(newSViv(av.ull))); break;
		case PM_TYPE_FLOAT:  XPUSHs(sv_2mortal(newSVnv(av.f))); break;
		case PM_TYPE_DOUBLE: XPUSHs(sv_2mortal(newSVnv(av.d))); break;
		case PM_TYPE_STRING: XPUSHs(sv_2mortal(newSVpv(av.cp,0))); break;
	    }
	    PUTBACK;

	    sts = perl_call_sv(store_cb_func, G_SCALAR);
	    SPAGAIN;	/* refresh local perl stack pointer after call */
	    if (sts != 1) {
		croak("store CB error (returned %d values, expected 1)", sts); 
		sts = -EINVAL;
		goto store_end;
	    }
	    sts = POPi;				/* pop function return status */
	    if (sts < 0)
		goto store_end;
	}
    }

store_end:
    PUTBACK;
    FREETMPS;
    LEAVE;	/* fix up the perl stack, freeing anything we created */
    return sts;
}


/*
 * converts Perl list ref like [a => 'foo', b => 'boo'] into an indom
 */
static int
local_list_to_indom(SV *list, pmdaInstid **set)
{
    int	i, len;
    SV	**id;
    SV	**name;
    AV	*ilist = (AV *) SvRV(list);
    pmdaInstid *instances;

    if (SvTYPE((SV *)ilist) != SVt_PVAV) {
	warn("final argument is not an array reference");
	return -1;
    }
    if ((len = av_len(ilist)) == -1) {	/* empty */
	*set = NULL;
	return 0;
    }
    if (len++ % 2 == 0) {
	warn("invalid instance list (length must be a multiple of 2)");
	return -1;
    }

    len /= 2;
    instances = (pmdaInstid *) calloc(len, sizeof(pmdaInstid));
    if (instances == NULL) {
	warn("insufficient memory for instance array");
	return -1;
    }
    for (i = 0; i < len; i++) {
	id = av_fetch(ilist,i*2,0);
	name = av_fetch(ilist,i*2+1,0);
	instances[i].i_inst = SvIV(*id);
	instances[i].i_name = strdup(SvPV(*name, PL_na));
	if (instances[i].i_name == NULL) {
	    warn("insufficient memory for instance array names");
	    return -1;
	}
    }
    *set = instances;
    return len;
}

static int
local_file(int type, int fd, SV *callback, int cookie)
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

static int
local_pipe(char *pipe, SV *callback, int cookie)
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

static int
local_tail(char *file, SV *callback, int cookie)
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

static int
local_sock(char *host, int port, SV *callback, int cookie)
{
    struct sockaddr_in myaddr;
    struct hostent *servinfo;
    struct linger nolinger = { 1, 0 };
    int me, fd, nodelay = 1;

    if ((servinfo = gethostbyname(host)) == NULL) {
	__pmNotifyErr(LOG_ERR, "gethostbyname (%s): %s", host, strerror(errno));
	exit(1);
    }
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	__pmNotifyErr(LOG_ERR, "socket (%s): %s", host, strerror(errno));
	exit(1);
    }
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, /* avoid 200 ms delay */
		    (char *)&nodelay, (socklen_t)sizeof(nodelay)) < 0) {
	__pmNotifyErr(LOG_ERR, "setsockopt1 (%s): %s", host, strerror(errno));
	exit(1);
    }
    if (setsockopt(fd, SOL_SOCKET, SO_LINGER, /* don't linger on close */
		    (char *)&nolinger, (socklen_t)sizeof(nolinger)) < 0) {
	__pmNotifyErr(LOG_ERR, "setsockopt2 (%s): %s", host, strerror(errno));
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

static int
local_files_get_descriptor(int id)
{
    if (id < 0 || id >= nfiles)
	return -1;
    return files[id].fd;
}

static void
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
	    close(files[nfiles].fd);
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

static void
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
					local_timer_callback);
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
	    bytes = read(fd, local_buffer, sizeof(local_buffer));
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
	    local_buffer[bytes] = '\0';
	    for (s = p = local_buffer; *s != '\0'; s++) {
		if (*s != '\n')
		    continue;
		*s = '\0';
		local_input_callback(files[i].callback, p);
		p = s + 1;
	    }
	}

	__pmAFunblock();
    }
}

static char *
local_strdup_suffix(const char *string, const char *suffix)
{
    size_t length = strlen(string) + strlen(suffix) + 1;
    char *result = malloc(length);

    if (!result)
	return result;
    sprintf(result, "%s%s", string, suffix);
    return result;
}

static char *
local_strdup_prefix(const char *prefix, const char *string)
{
    size_t length = strlen(prefix) + strlen(string) + 1;
    char *result = malloc(length);

    if (!result)
	return result;
    sprintf(result, "%s%s", prefix, string);
    return result;
}


MODULE = PCP::PMDA		PACKAGE = PCP::PMDA


pmdaInterface *
new(CLASS,name,domain)
	char *	CLASS
	char *	name
	int	domain
    PREINIT:
	char *	logfile;
	char *	pmdaname;
    CODE:
	pmProgname = name;
	RETVAL = &dispatch;
	logfile = local_strdup_suffix(name, ".log");
	pmdaname = local_strdup_prefix("pmda", name);
	pmProgname = pmdaname;
	atexit(&local_atexit);
	snprintf(local_buffer, sizeof(local_buffer), "%s/%s/help",
			pmGetConfig("PCP_PMDAS_DIR"), name);
	pmdaDaemon(RETVAL, PMDA_INTERFACE_LATEST, pmdaname, domain,
			logfile, local_buffer);
	pmdaOpenLog(RETVAL);
    OUTPUT:
	RETVAL

int
pmda_pmid(cluster,item)
	unsigned int	cluster
	unsigned int	item
    CODE:
	RETVAL = PMDA_PMID(cluster, item);
    OUTPUT:
	RETVAL

int
pmda_units(dim_space,dim_time,dim_count,scale_space,scale_time,scale_count)
	unsigned int	dim_space
	unsigned int	dim_time
	unsigned int	dim_count
	unsigned int	scale_space
	unsigned int	scale_time
	unsigned int	scale_count
    PREINIT:
	pmUnits	units;
    CODE:
	units.pad = 0;
	units.dimSpace = dim_space;	units.scaleSpace = scale_space;
	units.dimTime = dim_time;	units.scaleTime = scale_time;
	units.dimCount = dim_count;	units.scaleCount = scale_count;
	RETVAL = *(int *)(&units);
    OUTPUT:
	RETVAL

void
error(self,message)
	pmdaInterface *self
	char *	message
    CODE:
	__pmNotifyErr(LOG_ERR, message);

void
set_fetch(self,fetch)
	pmdaInterface *self
	SV *	fetch
    CODE:
	if (fetch != (SV *)NULL) {
	    fetch_func = newSVsv(fetch);
	    self->version.two.fetch = local_fetch;
	}

void
set_instance(self,instance)
	pmdaInterface *self
	SV *	instance
    CODE:
	if (instance != (SV *)NULL) {
	    instance_func = newSVsv(instance);
	    self->version.two.instance = local_instance;
	}

void
set_store_callback(self,store)
	pmdaInterface *self
	SV *	store
    CODE:
	if (store != (SV *)NULL) {
	    store_cb_func = newSVsv(store);
	    self->version.two.store = local_store;
	}

void
set_fetch_callback(self,fetch_callback)
	pmdaInterface *self
	SV *	fetch_callback
    CODE:
	if (fetch_callback != (SV *)NULL) {
	    fetch_cb_func = newSVsv(fetch_callback);
	    pmdaSetFetchCallBack(self, local_fetch_callback);
	}

void
set_inet_socket(self,port)
	pmdaInterface *self
	int	port
    CODE:
	self->version.two.ext->e_io = pmdaInet;
	self->version.two.ext->e_port = port;

void
set_unix_socket(self,socket_name)
	pmdaInterface *self
	char *	socket_name
    CODE:
	self->version.two.ext->e_io = pmdaUnix;
	self->version.two.ext->e_sockname = socket_name;

void
add_metric(self,pmid,type,indom,sem,units,name,help,longhelp)
	pmdaInterface *self
	int	pmid
	int	type
	int	indom
	int	sem
	int	units
	char *	name
	char *	help
	char *	longhelp
    PREINIT:
	pmdaMetric *p;
    CODE:
	metrictab = (pmdaMetric *)realloc(metrictab, sizeof(pmdaMetric)*(mtab_size+1));
	if (metrictab == NULL) {
	    warn("unable to allocate memory for metric table");
	    XSRETURN_UNDEF;
	}
	p = metrictab + mtab_size++;
	p->m_user = NULL;	p->m_desc.pmid = *(pmID *)&pmid;
	p->m_desc.type = type;	p->m_desc.indom = *(pmInDom *)&indom;
	p->m_desc.sem = sem;	p->m_desc.units = *(pmUnits *)&units;
	(void)self;	/*ARGSUSED*/
	(void)name;	/*ARGSUSED*/
	(void)help;	/*ARGSUSED*/
	(void)longhelp;	/*ARGSUSED*/

int
add_indom(self,indom,list,help,longhelp)
	pmdaInterface *	self
	int	indom
	SV *	list
	char *	help
	char *	longhelp
    PREINIT:
	pmdaIndom *	p;
    CODE:
	indomtab = (pmdaIndom *)realloc(indomtab, sizeof(pmdaIndom)*(itab_size+1));
	if (indomtab == NULL) {
	    warn("unable to allocate memory for indom table");
	    XSRETURN_UNDEF;
	}
	p = indomtab + itab_size;
	p->it_indom = *(pmInDom *)&indom;
	p->it_numinst = local_list_to_indom(list, &p->it_set);
	if (p->it_numinst == -1)
	    XSRETURN_UNDEF;
	else
	    RETVAL = itab_size++;	/* used in calls to replace_indom() */
	(void)self;	/*ARGSUSED*/
	(void)help;	/*ARGSUSED*/
	(void)longhelp;	/*ARGSUSED*/
    OUTPUT:
	RETVAL

int
replace_indom(self,index,list)
	pmdaInterface *	self
	int	index
	SV *	list
    PREINIT:
	pmdaIndom *	p;
	int		i;
    CODE:
	if (index >= itab_size || index < 0) {
	    warn("attempt to replace non-existent instance domain");
	    XSRETURN_UNDEF;
	}
	else {
	    p = indomtab + index;
	    if (p->it_set && p->it_numinst > 0) {
		for (i = 0; i < p->it_numinst; i++)
		    free(p->it_set[i].i_name);	/* local_list_to_indom strdup */
		free(p->it_set);	/* local_list_to_indom calloc */
	    }
	    p->it_numinst = local_list_to_indom(list, &p->it_set);
	    if (p->it_numinst == -1)
		XSRETURN_UNDEF;
	    else
		RETVAL = p->it_numinst;
	}
    OUTPUT:
	RETVAL

int
add_timer(self,timeout,callback,data)
	pmdaInterface *	self
	double	timeout
	SV *	callback
	int	data
    CODE:
	if (callback != (SV *)NULL)
	    RETVAL = local_timer(timeout, newSVsv(callback), data);
	else
	    XSRETURN_UNDEF;
    OUTPUT:
	RETVAL

int
add_pipe(self,command,callback,data)
	pmdaInterface *self
	char *	command
	SV *	callback
	int	data
    CODE:
	if (callback != (SV *)NULL)
	    RETVAL = local_pipe(command, newSVsv(callback), data);
	else
	    XSRETURN_UNDEF;
    OUTPUT:
	RETVAL

int
add_tail(self,filename,callback,data)
	pmdaInterface *self
	char *	filename
	SV *	callback
	int	data
    CODE:
	if (callback != (SV *)NULL)
	    RETVAL = local_tail(filename, newSVsv(callback), data);
	else
	    XSRETURN_UNDEF;
    OUTPUT:
	RETVAL

int
add_sock(self,hostname,port,callback,data)
	pmdaInterface *self
	char *	hostname
	int	port
	SV *	callback
	int	data
    CODE:
	if (callback != (SV *)NULL)
	    RETVAL = local_sock(hostname, port, newSVsv(callback), data);
	else
	    XSRETURN_UNDEF;
    OUTPUT:
	RETVAL

int
put_sock(self,id,output)
	pmdaInterface *self
	int	id
	char *	output
    CODE:
	RETVAL = write(local_files_get_descriptor(id), output, strlen(output));
    OUTPUT:
	RETVAL

void
log(self,message)
	pmdaInterface *self
	char *	message
    CODE:
	__pmNotifyErr(LOG_INFO, message);

void
err(self,message)
	pmdaInterface *self
	char *	message
    CODE:
	__pmNotifyErr(LOG_ERR, message);

void
run(self)
	pmdaInterface *self
    CODE:
	pmdaInit(self, indomtab, itab_size, metrictab, mtab_size);
	pmdaConnect(self);
	local_pmdaMain(self);

void
debug_metric(self)
	pmdaInterface *self
    PREINIT:
	int	i;
    CODE:
	/* NB: debugging only (used in test.pl to verify state) */
	fprintf(stderr, "metric table size = %d\n", mtab_size);
	for (i = 0; i < mtab_size; i++) {
	    fprintf(stderr, "metric idx = %d\n\tpmid = %s\n\ttype = %u\n"
			"\tindom= %d\n\tsem  = %u\n\tunits= %u\n",
		i, pmIDStr(metrictab[i].m_desc.pmid), metrictab[i].m_desc.type,
		(int)metrictab[i].m_desc.indom, metrictab[i].m_desc.sem,
		*(unsigned int *)&metrictab[i].m_desc.units);
	}
	(void)self;	/*ARGSUSED*/

void
debug_indom(self)
	pmdaInterface *self
    PREINIT:
	int	i,j;
    CODE:
	/* NB: debugging only (used in test.pl to verify state) */
	fprintf(stderr, "indom table size = %d\n", itab_size);
	for (i = 0; i < itab_size; i++) {
	    fprintf(stderr, "indom idx = %d\n\tindom = %d\n"
			    "\tninst = %u\n\tiptr = 0x%p\n",
		    i, *(int *)&indomtab[i].it_indom, indomtab[i].it_numinst,
		    indomtab[i].it_set);
	    for (j = 0; j < indomtab[i].it_numinst; j++) {
		fprintf(stderr, "\t\tid=%d name=%s\n",
		    indomtab[i].it_set[j].i_inst, indomtab[i].it_set[j].i_name);
	    }
	}
	(void)self;	/*ARGSUSED*/

void
debug_init(self)
	pmdaInterface *self
    CODE:
	/* NB: debugging only (used in test.pl to verify state) */
	pmdaInit(self, indomtab, itab_size, metrictab, mtab_size);

