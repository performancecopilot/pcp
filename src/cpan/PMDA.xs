/*
 * Copyright (c) 2004 Silicon Graphics, Inc.  All Rights Reserved.
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
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#ifdef __cplusplus
extern "C" {
#endif
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/pmda.h>
#include <syslog.h>
#ifdef __cplusplus
}
#endif

extern int	pmDebug;
extern char *	pmProgname;

static pmdaInterface	dispatch;
static pmdaMetric *	metrictab;
static pmdaIndom *	indomtab;
static int mtab_size;
static int itab_size;

static SV *fetch_func = (SV*)NULL;
static SV *instance_func = (SV*)NULL;
static SV *store_cb_func = (SV*)NULL;
static SV *fetch_cb_func = (SV*)NULL;

static SV *input_cb_func = (SV*)NULL;
static char input_buffer[4096];

static SV *timer_cb_func = (SV*)NULL;
static struct timeval timer_delta;
static int timer_afid;

typedef enum {
    MODE_NONE,
    MODE_PIPE,
    MODE_TAIL,
} pmda_mode_t;

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

    perl_call_sv(timer_cb_func, G_DISCARD|G_NOARGS);
}

void
local_input_callback(char *string)
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
    if (sts < 0)
	goto fetch_end;
    switch (metric->m_desc.type) {	/* pop result value */
	case PM_TYPE_32:	atom->l = POPi; break;
	case PM_TYPE_U32:	atom->ul = POPi; break;
	case PM_TYPE_64:	atom->ll = POPl; break;
	case PM_TYPE_U64:	atom->ull = POPl; break;
	case PM_TYPE_FLOAT:	atom->f = POPn; break;
	case PM_TYPE_DOUBLE:	atom->d = POPn; break;
	case PM_TYPE_STRING:	atom->cp = SvPV((POPs), PL_na); break;
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
list_to_indom(SV *list, pmdaInstid **set)
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

static void
local_pmdaMain(pmdaInterface *self, char *filename, pmda_mode_t mode)
{
    int pmcdfd, fd = -1;
    int nready, numfds;
    fd_set readyfds;
    fd_set fds;
    FILE *fp = NULL;

    FD_ZERO(&fds);

    if ((pmcdfd = __pmdaInFd(self)) < 0)
	exit(1);
    FD_SET(pmcdfd, &fds);

    if (mode == MODE_PIPE) {
	/* TODO - need an atexit(3) handler to pclose this guy */
	if ((fp = popen(filename, "r")) == NULL) {
	    __pmNotifyErr(LOG_ERR, "popen failed (%s): %s",
				    filename, strerror(errno));
	    exit(1);
	}
	fd = fileno(fp);
	FD_SET(fd, &fds);
    }
    else if (mode == MODE_TAIL) {
	/* TODO - "tail -f <filename>" mode */
	__pmNotifyErr(LOG_ERR, "tail mode not yet implemented");
	exit(1);
    }

    numfds = ((pmcdfd > fd) ? pmcdfd : fd) + 1;

    if (timer_cb_func != NULL)	/* TODO - need an add_timer() interface? */
	timer_afid = __pmAFregister(&timer_delta, NULL, local_timer_callback);

    /* custom PMDA main loop */
    for (;;) {
	memcpy(&readyfds, &fds, sizeof(readyfds));
	nready = select(numfds, &readyfds, NULL, NULL, NULL);

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

	if (fd != -1 && FD_ISSET(fd, &readyfds)) {
	    char *s, *p;
	    size_t bytes = read(fd, input_buffer, sizeof(input_buffer));
	    if (!bytes) {
		__pmNotifyErr(LOG_ERR, "No data read - pipe closed\n");
		exit(1);
	    }
	    input_buffer[bytes] = '\0';
	    for (s = p = input_buffer; *s != '\0'; s++) {
		if (*s != '\n')
		    continue;
		*s = '\0';
		local_input_callback(p);
		p = s + 1;
	    }
	}
	__pmAFunblock();
    }
}


MODULE = PCP::PMDA		PACKAGE = PCP::PMDA


pmdaInterface *
new(CLASS,name,domain,logfile,helpfile)
	char *	CLASS
	char *	name
	int	domain
	char *	logfile
	char *	helpfile
    CODE:
	pmProgname = name;
	RETVAL = &dispatch;
	if (helpfile && helpfile[0] == '\0')
	    helpfile = NULL;
	if (logfile && logfile[0] == '\0')
	    logfile = NULL;
	pmdaDaemon(RETVAL, PMDA_INTERFACE_LATEST, name, domain, logfile, helpfile);
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
openlog(self)
	pmdaInterface *self
    CODE:
	pmdaOpenLog(self);

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
set_timer_callback(self,timer_callback)
	pmdaInterface *self
	SV *	timer_callback
    CODE:
	if (timer_callback != (SV *)NULL) {
	    timer_cb_func = newSVsv(timer_callback);
	}

void
set_input_callback(self,input_callback)
	pmdaInterface *self
	SV *	input_callback
    CODE:
	if (input_callback != (SV *)NULL) {
	    input_cb_func = newSVsv(input_callback);
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
add_metric(self,pmid,type,indom,sem,units)
	pmdaInterface *self
	int	pmid
	int	type
	int	indom
	int	sem
	int	units
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

int
add_indom(self,indom,list)
	pmdaInterface *	self
	int		indom
	SV *		list
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
	p->it_numinst = list_to_indom(list, &p->it_set);
	if (p->it_numinst == -1)
	    XSRETURN_UNDEF;
	else
	    RETVAL = itab_size++;	/* used in calls to replace_indom() */
	(void)self;	/*ARGSUSED*/
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
		    free(p->it_set[i].i_name);	/* from list_to_indom strdup */
		free(p->it_set);	/* from list_to_indom calloc */
	    }
	    p->it_numinst = list_to_indom(list, &p->it_set);
	    if (p->it_numinst == -1)
		XSRETURN_UNDEF;
	    else
		RETVAL = p->it_numinst;
	}
	(void)self;	/*ARGSUSED*/
    OUTPUT:
	RETVAL

void
run(self)
	pmdaInterface *self
    CODE:
	pmdaInit(self, indomtab, itab_size, metrictab, mtab_size);
	pmdaConnect(self);
	local_pmdaMain(self, NULL, MODE_NONE);

void
pipe(self,command)
	pmdaInterface *self
	char *	command
    CODE:
	pmdaInit(self, indomtab, itab_size, metrictab, mtab_size);
	pmdaConnect(self);
	local_pmdaMain(self, command, MODE_PIPE);

void
tail(self,filename)
	pmdaInterface *self
	char *	filename
    CODE:
	pmdaInit(self, indomtab, itab_size, metrictab, mtab_size);
	pmdaConnect(self);
	local_pmdaMain(self, filename, MODE_TAIL);

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

