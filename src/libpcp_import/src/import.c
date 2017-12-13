/*
 * Copyright (c) 2013-2017 Red Hat.
 * Copyright (c) 2010 Ken McDonell.  All Rights Reserved.
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
#include "libpcp.h"
#include "import.h"
#include "domain.h"
#include "private.h"
#include <ctype.h>

static pmi_context *context_tab;
static int ncontext;
static pmi_context *current;

static void
printstamp(FILE *f, const struct timeval *tp)
{
    struct tm	tmp;
    time_t	now;

    now = (time_t)tp->tv_sec;
    pmLocaltime(&now, &tmp);
    fprintf(f, "%4d-%02d-%02d %02d:%02d:%02d.%06d", 1900+tmp.tm_year, tmp.tm_mon, tmp.tm_mday, tmp.tm_hour, tmp.tm_min, tmp.tm_sec, (int)(tp->tv_usec));
}
 

void
pmiDump(void)
{
    FILE	*f = stderr;

    fprintf(f, "pmiDump: context %ld of %d",
	(long)(current - context_tab), ncontext);
    if (current == NULL) {
	fprintf(f, " Error: current context is not defined.\n");
	return;
    }
    else {
	fprintf(f, " archive: %s\n", 
	    current->archive == NULL ? "<undefined>" : current->archive);
    }
    fprintf(f, "  state: %d ", current->state);
    switch (current->state) {
	case CONTEXT_START:
	    fprintf(f, "(start)");
	    break;
	case CONTEXT_ACTIVE:
	    fprintf(f, "(active)");
	    break;
	case CONTEXT_END:
	    fprintf(f, "(end)");
	    break;
	default:
	    fprintf(f, "(BAD)");
	    break;
    }
    fprintf(f, " hostname: %s timezone: %s\n", 
	current->hostname == NULL ? "<undefined>" : current->hostname,
	current->timezone == NULL ? "<undefined>" : current->timezone);
    if (current->nmetric == 0)
	fprintf(f, "  No metrics.\n");
    else {
	int	m;
	char	strbuf[20];
	for (m = 0; m < current->nmetric; m++) {
	    fprintf(f, "  metric[%d] name=%s pmid=%s\n",
		m, current->metric[m].name,
		pmIDStr_r(current->metric[m].pmid, strbuf, sizeof(strbuf)));
	    pmPrintDesc(f, &current->metric[m].desc);
	}
    }
    if (current->nindom == 0)
	fprintf(f, "  No indoms.\n");
    else {
	int	i;
	char	strbuf[20];
	for (i = 0; i < current->nindom; i++) {
	    fprintf(f, "  indom[%d] indom=%s",
		i, pmInDomStr_r(current->indom[i].indom, strbuf, sizeof(strbuf)));
	    if (current->indom[i].ninstance == 0) {
		fprintf(f, "   No instances.\n");
	    }
	    else {
		int	j;
		fputc('\n', f);
		for (j = 0; j < current->indom[i].ninstance; j++) {
		    fprintf(f, "   instance[%d] %s (%d)\n",
			j, current->indom[i].name[j],
			current->indom[i].inst[j]);
		}
	    }
	}
    }
    if (current->nhandle == 0)
	fprintf(f, "  No handles.\n");
    else {
	int	h;
	char	strbuf[20];
	for (h = 0; h < current->nhandle; h++) {
	    fprintf(f, "  handle[%d] metric=%s (%s) instance=%d\n",
		h, current->metric[current->handle[h].midx].name,
		pmIDStr_r(current->metric[current->handle[h].midx].pmid, strbuf, sizeof(strbuf)),
		current->handle[h].inst);
	}
    }
    if (current->result == NULL)
	fprintf(f, "  No pmResult.\n");
    else
	__pmDumpResult(f, current->result);
}

pmUnits
pmiUnits(int dimSpace, int dimTime, int dimCount, int scaleSpace, int scaleTime, int scaleCount)
{
    static pmUnits units;
    units.dimSpace = dimSpace;
    units.dimTime = dimTime;
    units.dimCount = dimCount;
    units.scaleSpace = scaleSpace;
    units.scaleTime = scaleTime;
    units.scaleCount = scaleCount;

    return units;
}

pmID
pmiID(int domain, int cluster, int item)
{
    return pmID_build(domain, cluster, item);
}

pmInDom
pmiInDom(int domain, int serial)
{
    return pmInDom_build(domain, serial);
}

const char *
pmiErrStr(int sts)
{
    static char errmsg[PMI_MAXERRMSGLEN];
    pmiErrStr_r(sts, errmsg, sizeof(errmsg));
    return errmsg;
}

char *
pmiErrStr_r(int code, char *buf, int buflen)
{
    const char *msg;

    if (code == -1 && current != NULL) code = current->last_sts;
    switch (code) {
	case PMI_ERR_DUPMETRICNAME:
	    msg = "Metric name already defined";
	    break;
	case PMI_ERR_DUPMETRICID:
	    msg = "Metric pmID already defined";
	    break;
	case PMI_ERR_DUPINSTNAME:
	    msg = "External instance name already defined";
	    break;
	case PMI_ERR_DUPINSTID:
	    msg = "Internal instance identifer already defined";
	    break;
	case PMI_ERR_INSTNOTNULL:
	    msg = "Null instance expected for a singular metric";
	    break;
	case PMI_ERR_INSTNULL:
	    msg = "Null instance not allowed for a non-singular metric";
	    break;
	case PMI_ERR_BADHANDLE:
	    msg = "Illegal handle";
	    break;
	case PMI_ERR_DUPVALUE:
	    msg = "Value already assigned for this metric-instance";
	    break;
	case PMI_ERR_BADTYPE:
	    msg = "Illegal metric type";
	    break;
	case PMI_ERR_BADSEM:
	    msg = "Illegal metric semantics";
	    break;
	case PMI_ERR_NODATA:
	    msg = "No data to output";
	    break;
	case PMI_ERR_BADMETRICNAME:
	    msg = "Illegal metric name";
	    break;
	case PMI_ERR_BADTIMESTAMP:
	    msg = "Illegal result timestamp";
	    break;
	default:
	    return pmErrStr_r(code, buf, buflen);
    }
    strncpy(buf, msg, buflen);
    buf[buflen-1] = '\0';
    return buf;
}

int
pmiStart(const char *archive, int inherit)
{
    pmi_context	*old_current;
    char	*np;
    int		c = current - context_tab;

    ncontext++;
    context_tab = (pmi_context *)realloc(context_tab, ncontext*sizeof(context_tab[0]));
    if (context_tab == NULL) {
	pmNoMem("pmiStart: context_tab", ncontext*sizeof(context_tab[0]), PM_FATAL_ERR);
    }
    old_current = &context_tab[c];
    current = &context_tab[ncontext-1];

    current->state = CONTEXT_START;
    current->archive = strdup(archive);
    if (current->archive == NULL) {
	pmNoMem("pmiStart", strlen(archive)+1, PM_FATAL_ERR);
    }
    current->hostname = NULL;
    current->timezone = NULL;
    current->result = NULL;
    memset((void *)&current->logctl, 0, sizeof(current->logctl));
    memset((void *)&current->archctl, 0, sizeof(current->archctl));
    current->archctl.ac_log = &current->logctl;
    if (inherit && old_current != NULL) {
	current->nmetric = old_current->nmetric;
	if (old_current->metric != NULL) {
	    int		m;
	    current->metric = (pmi_metric *)malloc(current->nmetric*sizeof(pmi_metric));
	    if (current->metric == NULL) {
		pmNoMem("pmiStart: pmi_metric", current->nmetric*sizeof(pmi_metric), PM_FATAL_ERR);
	    }
	    for (m = 0; m < current->nmetric; m++) {
		current->metric[m].name = old_current->metric[m].name;
		current->metric[m].pmid = old_current->metric[m].pmid;
		current->metric[m].desc = old_current->metric[m].desc;
		current->metric[m].meta_done = 0;
	    }
	}
	else
	    current->metric = NULL;
	current->nindom = old_current->nindom;
	if (old_current->indom != NULL) {
	    int		i;
	    current->indom = (pmi_indom *)malloc(current->nindom*sizeof(pmi_indom));
	    if (current->indom == NULL) {
		pmNoMem("pmiStart: pmi_indom", current->nindom*sizeof(pmi_indom), PM_FATAL_ERR);
	    }
	    for (i = 0; i < current->nindom; i++) {
		int		j;
		current->indom[i].indom = old_current->indom[i].indom;
		current->indom[i].ninstance = old_current->indom[i].ninstance;
		current->indom[i].meta_done = 0;
		if (old_current->indom[i].ninstance > 0) {
		    current->indom[i].name = (char **)malloc(current->indom[i].ninstance*sizeof(char *));
		    if (current->indom[i].name == NULL) {
			pmNoMem("pmiStart: name", current->indom[i].ninstance*sizeof(char *), PM_FATAL_ERR);
		    }
		    current->indom[i].inst = (int *)malloc(current->indom[i].ninstance*sizeof(int));
		    if (current->indom[i].inst == NULL) {
			pmNoMem("pmiStart: inst", current->indom[i].ninstance*sizeof(int), PM_FATAL_ERR);
		    }
		    current->indom[i].namebuflen = old_current->indom[i].namebuflen;
		    current->indom[i].namebuf = (char *)malloc(old_current->indom[i].namebuflen);
		    if (current->indom[i].namebuf == NULL) {
			pmNoMem("pmiStart: namebuf", old_current->indom[i].namebuflen, PM_FATAL_ERR);
		    }
		    np = current->indom[i].namebuf;
		    for (j = 0; j < current->indom[i].ninstance; j++) {
			strcpy(np, old_current->indom[i].name[j]);
			current->indom[i].name[j] = np;
			np += strlen(np)+1;
			current->indom[i].inst[j] = old_current->indom[i].inst[j];
		    }
		}
		else {
		    current->indom[i].name = NULL;
		    current->indom[i].inst = NULL;
		    current->indom[i].namebuflen = 0;
		    current->indom[i].namebuf = NULL;
		}
	    }
	}
	else
	    current->indom = NULL;
	current->nhandle = old_current->nhandle;
	if (old_current->handle != NULL) {
	    int		h;
	    current->handle = (pmi_handle *)malloc(current->nhandle*sizeof(pmi_handle));
	    if (current->handle == NULL) {
		pmNoMem("pmiStart: pmi_handle", current->nhandle*sizeof(pmi_handle), PM_FATAL_ERR);
	    }
	    for (h = 0; h < current->nhandle; h++) {
		current->handle[h].midx = old_current->handle[h].midx;
		current->handle[h].inst = old_current->handle[h].inst;
	    }
	}
	else
	    current->handle = NULL;
	current->last_stamp = old_current->last_stamp;
    }
    else {
	current->nmetric = 0;
	current->metric = NULL;
	current->nindom = 0;
	current->indom = NULL;
	current->nhandle = 0;
	current->handle = NULL;
	current->last_stamp.tv_sec = current->last_stamp.tv_usec = 0;
    }
    return ncontext;
}

int
pmiUseContext(int context)
{
    if (context < 1 || context > ncontext) {
	if (current != NULL) current->last_sts = PM_ERR_NOCONTEXT;
	return PM_ERR_NOCONTEXT;
    }
    current = &context_tab[context-1];
    return current->last_sts = 0;
}

int
pmiEnd(void)
{
    if (current == NULL)
	return PM_ERR_NOCONTEXT;

    return current->last_sts = _pmi_end(current);
}

int
pmiSetHostname(const char *value)
{
    if (current == NULL)
	return PM_ERR_NOCONTEXT;
    current->hostname = strdup(value);
    if (current->hostname == NULL) {
	pmNoMem("pmiSetHostname", strlen(value)+1, PM_FATAL_ERR);
    }
    return current->last_sts = 0;
}

int
pmiSetTimezone(const char *value)
{
    if (current == NULL)
	return PM_ERR_NOCONTEXT;
    current->timezone = strdup(value);
    if (current->timezone == NULL) {
	pmNoMem("pmiSetTimezone", strlen(value)+1, PM_FATAL_ERR);
    }
    return current->last_sts = 0;
}

static int
valid_pmns_name(const char *name)
{
    const char *previous;

    /*
     * Ensure requested metric name conforms to the PMNS rules:
     * Should start with an alphabetic, then any combination of
     * alphanumerics or underscore.  Dot separators are OK, but
     * ensure only one (no repeats).
     */
    if (name == NULL)
	return 0;
    if (!isalpha((int)*name))
	return 0;
    for (previous = name++; *name != '\0'; name++) {
	if (!isalnum((int)*name) && *name != '_' && *name != '.')
	    return 0;
	if (*previous == '.') {
	    if (!isalpha((int)*name))	/* non-alphabetic first */
		return 0;
	    if (*name == *previous)	/* repeated . separator */
		return 0;
	}
	previous = name;
    }
    if (*previous == '.')	/* shouldn't end with separator */
	return 0;
    return 1;
}

int
pmiAddMetric(const char *name, pmID pmid, int type, pmInDom indom, int sem, pmUnits units)
{
    int		m;
    int		item;
    int		cluster;
    size_t	size;
    pmi_metric	*mp;

    if (current == NULL)
	return PM_ERR_NOCONTEXT;

    if (valid_pmns_name(name) == 0)
	return current->last_sts = PMI_ERR_BADMETRICNAME;

    for (m = 0; m < current->nmetric; m++) {
	if (strcmp(name, current->metric[m].name) == 0) {
	    /* duplicate metric name is not good */
	    return current->last_sts = PMI_ERR_DUPMETRICNAME;
	}
	if (pmid == current->metric[m].pmid) {
	    /* duplicate metric pmID is not good */
	    return current->last_sts = PMI_ERR_DUPMETRICID;
	}
    }

    /*
     * basic sanity check of metadata ... we do not check later so this
     * needs to be robust
     */
    switch (type) {
	case PM_TYPE_32:
	case PM_TYPE_U32:
	case PM_TYPE_64:
	case PM_TYPE_U64:
	case PM_TYPE_FLOAT:
	case PM_TYPE_DOUBLE:
	case PM_TYPE_STRING:
	    break;
	default:
	    return current->last_sts = PMI_ERR_BADTYPE;
    }
    switch (sem) {
	case PM_SEM_INSTANT:
	case PM_SEM_COUNTER:
	case PM_SEM_DISCRETE:
	    break;
	default:
	    return current->last_sts = PMI_ERR_BADSEM;
    }

    current->nmetric++;
    size = current->nmetric * sizeof(pmi_metric);
    current->metric = (pmi_metric *)realloc(current->metric, size);
    if (current->metric == NULL) {
	pmNoMem("pmiAddMetric: pmi_metric", size, PM_FATAL_ERR);
    }
    mp = &current->metric[current->nmetric-1];
    if (pmid != PM_ID_NULL) {
	mp->pmid = pmid;
    } else {
	/* choose a PMID on behalf of the caller - check boundaries first */
	item = cluster = current->nmetric;
	if (item >= (1<<22)) {	/* enough room for unique item:cluster? */
	    current->nmetric--;
	    return current->last_sts = PMI_ERR_DUPMETRICID;	/* wrap */
	}
	item %= (1<<10);
	cluster >>= 10;
	mp->pmid = pmID_build(PMI_DOMAIN, cluster, item);
    }
    mp->name = strdup(name);
    if (mp->name == NULL) {
	pmNoMem("pmiAddMetric: name", strlen(name)+1, PM_FATAL_ERR);
    }
    mp->desc.pmid = mp->pmid;
    mp->desc.type = type;
    mp->desc.indom = indom;
    mp->desc.sem = sem;
    mp->desc.units = units;
    mp->meta_done = 0;

    return current->last_sts = 0;
}

int
pmiAddInstance(pmInDom indom, const char *instance, int inst)
{
    pmi_indom	*idp;
    const char	*p;
    char	*np;
    int		spaced;
    int		i;
    int		j;

    if (current == NULL)
	return PM_ERR_NOCONTEXT;

    for (i = 0; i < current->nindom; i++) {
	if (current->indom[i].indom == indom)
	    break;
    }
    if (i == current->nindom) {
	/* extend indom table */
	current->nindom++;
	current->indom = (pmi_indom *)realloc(current->indom, current->nindom*sizeof(pmi_indom));
	if (current->indom == NULL) {
	    pmNoMem("pmiAddInstance: pmi_indom", current->nindom*sizeof(pmi_indom), PM_FATAL_ERR);
	}
	current->indom[i].indom = indom;
	current->indom[i].ninstance = 0;
	current->indom[i].name = NULL;
	current->indom[i].inst = NULL;
	current->indom[i].namebuflen = 0;
	current->indom[i].namebuf = NULL;
    }
    idp = &current->indom[i];
    /*
     * duplicate external instance identifier would be bad, but need
     * to honour unique to first space rule ...
     * duplicate instance internal identifier is also not allowed
     */
    for (p = instance; *p && *p != ' '; p++)
	;
    spaced = (*p == ' ') ? p - instance + 1: 0;	/* +1 => *must* compare the space too */
    for (j = 0; j < idp->ninstance; j++) {
	if (spaced) {
	    if (strncmp(instance, idp->name[j], spaced) == 0)
		return current->last_sts = PMI_ERR_DUPINSTNAME;
	} else {
	    if (strcmp(instance, idp->name[j]) == 0)
		return current->last_sts = PMI_ERR_DUPINSTNAME;
	}
	if (inst == idp->inst[j]) {
	    return current->last_sts = PMI_ERR_DUPINSTID;
	}
    }
    /* add instance marks whole indom as needing to be written */
    idp->meta_done = 0;
    idp->ninstance++;
    idp->name = (char **)realloc(idp->name, idp->ninstance*sizeof(char *));
    if (idp->name == NULL) {
	pmNoMem("pmiAddInstance: name", idp->ninstance*sizeof(char *), PM_FATAL_ERR);
    }
    idp->inst = (int *)realloc(idp->inst, idp->ninstance*sizeof(int));
    if (idp->inst == NULL) {
	pmNoMem("pmiAddInstance: inst", idp->ninstance*sizeof(int), PM_FATAL_ERR);
    }
    idp->namebuf = (char *)realloc(idp->namebuf, idp->namebuflen+strlen(instance)+1);
    if (idp->namebuf == NULL) {
	pmNoMem("pmiAddInstance: namebuf", idp->namebuflen+strlen(instance)+1, PM_FATAL_ERR);
    }
    strcpy(&idp->namebuf[idp->namebuflen], instance);
    idp->namebuflen += strlen(instance)+1;
    idp->inst[idp->ninstance-1] = inst;
    /* in case namebuf moves, need to redo name[] pointers */
    np = idp->namebuf;
    for (j = 0; j < current->indom[i].ninstance; j++) {
	idp->name[j] = np;
	np += strlen(np)+1;
    }

    return current->last_sts = 0;
}

static int
make_handle(const char *name, const char *instance, pmi_handle *hp)
{
    int		m;
    int		i;
    int		j;
    int		spaced;
    const char	*p;
    pmi_indom	*idp;

    if (instance != NULL && instance[0] == '\0')
	/* map "" to NULL to help Perl callers */
	instance = NULL;

    for (m = 0; m < current->nmetric; m++) {
	if (strcmp(name, current->metric[m].name) == 0)
	    break;
    }
    if (m == current->nmetric)
	return current->last_sts = PM_ERR_NAME;
    hp->midx = m;

    if (current->metric[hp->midx].desc.indom == PM_INDOM_NULL) {
	if (instance != NULL) {
	    /* expect "instance" to be NULL */
	    return current->last_sts = PMI_ERR_INSTNOTNULL;
	}
	hp->inst = PM_IN_NULL;
    }
    else {
	if (instance == NULL)
	    /* don't expect "instance" to be NULL */
	    return current->last_sts = PMI_ERR_INSTNULL;
	for (i = 0; i < current->nindom; i++) {
	    if (current->metric[hp->midx].desc.indom == current->indom[i].indom)
		break;
	}
	if (i == current->nindom)
	    return current->last_sts = PM_ERR_INDOM;
	idp = &current->indom[i];

	/* match to first space rule */

	for (p = instance; *p && *p != ' '; p++)
	    ;
	spaced = (*p == ' ') ? p - instance + 1: 0;	/* +1 => *must* compare the space too */
	for (j = 0; j < idp->ninstance; j++) {
	    if (spaced) {
		if (strncmp(instance, idp->name[j], spaced) == 0)
		    break;
	    } else {
		if (strcmp(instance, idp->name[j]) == 0)
		    break;
	    }
	}
	if (j == idp->ninstance)
	    return current->last_sts = PM_ERR_INST;
	hp->inst = idp->inst[j];
    }

    return current->last_sts = 0;
}

int
pmiPutValue(const char *name, const char *instance, const char *value)
{
    pmi_handle	tmp;
    int		sts;

    if (current == NULL)
	return PM_ERR_NOCONTEXT;

    sts = make_handle(name, instance, &tmp);
    if (sts != 0)
	return current->last_sts = sts;

    return current->last_sts = _pmi_stuff_value(current, &tmp, value);
}

int
pmiGetHandle(const char *name, const char *instance)
{
    int		sts;
    pmi_handle	tmp;
    pmi_handle	*hp;

    if (current == NULL)
	return PM_ERR_NOCONTEXT;

    sts = make_handle(name, instance, &tmp);
    if (sts != 0)
	return current->last_sts = sts;

    current->nhandle++;
    current->handle = (pmi_handle *)realloc(current->handle, current->nhandle*sizeof(pmi_handle));
    if (current->handle == NULL) {
	pmNoMem("pmiGetHandle: pmi_handle", current->nhandle*sizeof(pmi_handle), PM_FATAL_ERR);
    }
    hp = &current->handle[current->nhandle-1];
    hp->midx = tmp.midx;
    hp->inst = tmp.inst;

    return current->last_sts = current->nhandle;
}

int
pmiPutValueHandle(int handle, const char *value)
{
    if (current == NULL)
	return PM_ERR_NOCONTEXT;
    if (handle <= 0 || handle > current->nhandle)
	return current->last_sts = PMI_ERR_BADHANDLE;

    return current->last_sts = _pmi_stuff_value(current, &current->handle[handle-1], value);
}

static int
check_timestamp(void)
{
    if (current->result->timestamp.tv_sec < current->last_stamp.tv_sec ||
        (current->result->timestamp.tv_sec == current->last_stamp.tv_sec &&
	 current->result->timestamp.tv_usec < current->last_stamp.tv_usec)) {
	fprintf(stderr, "Fatal Error: timestamp ");
	printstamp(stderr, &current->result->timestamp);
	fprintf(stderr, " not greater than previous valid timestamp ");
	printstamp(stderr, &current->last_stamp);
	fputc('\n', stderr);
	return PMI_ERR_BADTIMESTAMP;
    }
    return 0;
}

int
pmiWrite(int sec, int usec)
{
    int		sts;

    if (current == NULL)
	return PM_ERR_NOCONTEXT;
    if (current->result == NULL)
	return current->last_sts = PMI_ERR_NODATA;

    if (sec < 0) {
	pmtimevalNow(&current->result->timestamp);
    }
    else {
	current->result->timestamp.tv_sec = sec;
	current->result->timestamp.tv_usec = usec;
    }
    if ((sts = check_timestamp()) == 0) {
	sts = _pmi_put_result(current, current->result);
	current->last_stamp = current->result->timestamp;
    }

    pmFreeResult(current->result);
    current->result = NULL;

    return current->last_sts = sts;
}

int
pmiPutResult(const pmResult *result)
{
    int		sts;

    if (current == NULL)
	return PM_ERR_NOCONTEXT;

    current->result = (pmResult *)result;
    if ((sts = check_timestamp()) == 0) {
	sts = _pmi_put_result(current, current->result);
	current->last_stamp = current->result->timestamp;
    }
    current->result = NULL;

    return current->last_sts = sts;
}

int
pmiPutMark(void)
{
    __pmArchCtl *acp;
    struct {
	__pmPDU		hdr;
	pmTimeval	timestamp;	/* when returned */
	int		numpmid;	/* zero PMIDs to follow */
	__pmPDU		tail;
    } mark;

    if (current == NULL)
	return PM_ERR_NOCONTEXT;

    if (current->last_stamp.tv_sec == 0 && current->last_stamp.tv_usec == 0)
	/* no earlier pmResult, no point adding a mark record */
	return 0;
    acp = &current->archctl;

    mark.hdr = htonl((int)sizeof(mark));
    mark.tail = mark.hdr;
    mark.timestamp.tv_sec = current->last_stamp.tv_sec;
    mark.timestamp.tv_usec = current->last_stamp.tv_usec + 1000;	/* + 1msec */
    if (mark.timestamp.tv_usec > 1000000) {
	mark.timestamp.tv_usec -= 1000000;
	mark.timestamp.tv_sec++;
    }
    mark.timestamp.tv_sec = htonl(mark.timestamp.tv_sec);
    mark.timestamp.tv_usec = htonl(mark.timestamp.tv_usec);
    mark.numpmid = htonl(0);

    if (__pmFwrite(&mark, 1, sizeof(mark), acp->ac_mfp) != sizeof(mark))
	return -oserror();

    return 0;
}
