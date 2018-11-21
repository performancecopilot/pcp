/*
 * Copyright (c) 2013-2018 Red Hat.
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
    if (current->ntext == 0)
	fprintf(f, "  No text.\n");
    else {
	int		t;
	unsigned int	id;
	unsigned int	type;
	for (t = 0; t < current->ntext; t++) {
	    fprintf(f, "  text[%d] ", t);
	    id = current->text[t].id;
	    type = current->text[t].type;
	    if ((type & PM_TEXT_PMID))
		fprintf(f, "pmid=%s ", pmIDStr((pmID)id));
	    else if ((type & PM_TEXT_INDOM))
		fprintf(f, "indom=%s ", pmInDomStr((pmInDom)id));
	    else
		fprintf(f, "type=unknown ");

	    if ((type & PM_TEXT_ONELINE))
		fprintf(f, "[%s]", current->text[t].content);
	    else if ((type & PM_TEXT_HELP))
		fprintf(f, "\n%s", current->text[t].content);
	    else
		fprintf(f, "content=unknown");

	    fputc('\n', f);
	}
    }
    if (current->nlabel == 0)
	fprintf(f, "  No labels.\n");
    else {
	int		t;
	unsigned int	id;
	unsigned int	type;
	for (t = 0; t < current->nlabel; t++) {
	    fprintf(f, "  label[%d] ", t);
	    id = current->label[t].id;
	    type = current->label[t].type;
	    if ((type & PM_LABEL_CONTEXT))
		fprintf(f, "context ");
	    else if ((type & PM_LABEL_DOMAIN))
		fprintf(f, " domain=%d ", pmID_domain(id));
	    else if ((type & PM_LABEL_CLUSTER))
		fprintf(f, " cluster=%d.%d ", pmID_domain(id),
			pmID_cluster (id));
	    else if ((type & PM_LABEL_ITEM))
		fprintf(f, " item=%s ", pmIDStr(id));
	    else if ((type & PM_LABEL_INDOM))
		fprintf(f, " indom=%s ", pmInDomStr(id));
	    else if ((type & PM_LABEL_INSTANCES))
		fprintf(f, " instance=%d of indom=%s ",
			current->label[t].labelset->inst,
			pmInDomStr(id));
	    else
		fprintf(f, "type=unknown ");

	    pmPrintLabelSets(f, id, type, current->label[t].labelset, 1);
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

pmID
pmiCluster(int domain, int cluster)
{
    return pmID_build(domain, cluster, 0);
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
	case PMI_ERR_BADTEXTTYPE:
	    msg = "Illegal text type";
	    break;
	case PMI_ERR_BADTEXTCLASS:
	    msg = "Illegal text class";
	    break;
	case PMI_ERR_BADTEXTID:
	    msg = "Illegal text identifier";
	    break;
	case PMI_ERR_EMPTYTEXTCONTENT:
	    msg = "Text is empty";
	    break;
	case PMI_ERR_DUPTEXT:
	    msg = "Help text already exists";
	    break;
	case PMI_ERR_BADLABELTYPE:
	    msg = "Illegal label type";
	    break;
	case PMI_ERR_BADLABELID:
	    msg = "Illegal label id";
	    break;
	case PMI_ERR_BADLABELINSTANCE:
	    msg = "Illegal label instance";
	    break;
	case PMI_ERR_EMPTYLABELNAME:
	    msg = "Label name is empty";
	    break;
	case PMI_ERR_EMPTYLABELVALUE:
	    msg = "Label value is empty";
	    break;
	case PMI_ERR_ADDLABELERROR:
	    msg = "Error adding label";
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
	current->ntext = old_current->ntext;
	if (old_current->text != NULL) {
	    int		t;
	    current->text = (pmi_text *)malloc(current->ntext*sizeof(pmi_text));
	    if (current->text == NULL) {
		pmNoMem("pmiStart: pmi_text", current->ntext*sizeof(pmi_text), PM_FATAL_ERR);
	    }
	    for (t = 0; t < current->ntext; t++) {
		current->text[t].id = old_current->text[t].id;
		current->text[t].type = old_current->text[t].type;
		current->text[t].content = strdup(old_current->text[t].content);
		if (current->text[t].content == NULL) {
		    pmNoMem("pmiStart: pmi_text content", strlen(old_current->text[t].content) + 1, PM_FATAL_ERR);
		}
		current->text[t].meta_done = 0;
	    }
	}
	else
	    current->text = NULL;
	current->nlabel = old_current->nlabel;
	if (old_current->label != NULL) {
	    int		t;
	    current->label = (pmi_label *)malloc(current->nlabel*sizeof(pmi_label));
	    if (current->label == NULL) {
		pmNoMem("pmiStart: pmi_label", current->nlabel*sizeof(pmi_label), PM_FATAL_ERR);
	    }
	    for (t = 0; t < current->nlabel; t++) {
		current->label[t].id = old_current->label[t].id;
		current->label[t].type = old_current->label[t].type;
		if (old_current->label[t].labelset != NULL) {
		    current->label[t].labelset =
			__pmDupLabelSets(old_current->label[t].labelset, 1);
		    if (current->label[t].labelset == NULL) {
			pmNoMem("pmiStart: pmi_label labelset", 1, PM_FATAL_ERR);
		    }
		}
		else
		    current->label[t].labelset = NULL;
	    }
	}
	else
	    current->label = NULL;
	current->last_stamp = old_current->last_stamp;
    }
    else {
	current->nmetric = 0;
	current->metric = NULL;
	current->nindom = 0;
	current->indom = NULL;
	current->nhandle = 0;
	current->handle = NULL;
	current->ntext = 0;
	current->text = NULL;
	current->nlabel = 0;
	current->label = NULL;
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

int
pmiPutText(unsigned int type, unsigned int class, unsigned int id, const char *content)
{
    size_t		size;
    int			t;
    pmi_text		*tp;

    if (current == NULL)
	return PM_ERR_NOCONTEXT;

    /* Check the type */
    if (type != PM_TEXT_PMID && type != PM_TEXT_INDOM)
	return current->last_sts = PMI_ERR_BADTEXTTYPE;

    /* Check the class */
    if (class != PM_TEXT_ONELINE && class != PM_TEXT_HELP)
	return current->last_sts = PMI_ERR_BADTEXTCLASS;

    /* Check the id. */
    if (id == PM_ID_NULL)
	return current->last_sts = PMI_ERR_BADTEXTID;

    /* Make sure the content is not empty or NULL */
    if (content == NULL || *content == '\0')
	return current->last_sts = PMI_ERR_EMPTYTEXTCONTENT;

    /* Make sure that the text is not duplicate */
    for (t = 0; t < current->ntext; t++) {
	if (current->text[t].type != (type | class))
	    continue;
	if (current->text[t].id != id)
	    continue;
	if (strcmp(current->text[t].content, content) != 0)
	    continue;
	/* duplicate text is not good */
	return current->last_sts = PMI_ERR_DUPTEXT;
    }

    /* Add the new text. */
    current->ntext++;
    size = current->ntext * sizeof(pmi_text);
    current->text = (pmi_text *)realloc(current->text, size);
    if (current->text == NULL) {
	pmNoMem("pmiPutText: pmi_text", size, PM_FATAL_ERR);
    }
    tp = &current->text[current->ntext-1];
    tp->type = type | class;
    tp->id = id;
    tp->content = strdup(content);
    if (tp->content == NULL) {
	pmNoMem("pmiPutText: content", strlen(content)+1, PM_FATAL_ERR);
    }
    tp->meta_done = 0;

    return current->last_sts = 0;
}

int
pmiPutLabel(unsigned int type, unsigned int id, unsigned int inst, const char *name, const char *value)
{
    size_t	size;
    int		l;
    int		new_labelset = 0;
    pmi_label	*lp = NULL;
    char	buf[PM_MAXLABELJSONLEN];

    if (current == NULL)
	return PM_ERR_NOCONTEXT;

    /* Check the type */
    switch (type) {
    case PM_LABEL_CONTEXT:
    case PM_LABEL_DOMAIN:
    case PM_LABEL_INDOM:
    case PM_LABEL_CLUSTER:
    case PM_LABEL_ITEM:
	break; /* ok */
    case PM_LABEL_INSTANCES:
	/* Check the instance number. */
	if (inst == PM_IN_NULL)
	    return current->last_sts = PMI_ERR_BADLABELINSTANCE;
	break; /* ok */
    default:
	return current->last_sts = PMI_ERR_BADLABELTYPE;
    }

    /* Check the id. */
    if (id == PM_ID_NULL)
	return current->last_sts = PMI_ERR_BADLABELID;

    /* Make sure the name is not empty or NULL */
    if (name == NULL || *name == '\0')
	return current->last_sts = PMI_ERR_EMPTYLABELNAME;

    if (value == NULL || *value == '\0')
	return current->last_sts = PMI_ERR_EMPTYLABELVALUE;

    /* Find the labelset for this type/id/inst combination. */
    for (l = 0; l < current->nlabel; l++) {
	if (current->label[l].type != type)
	    continue;
	if (current->label[l].id != id)
	    continue;
	if (type == PM_LABEL_INSTANCES &&
	    current->label[l].labelset->inst != inst)
	    continue;
	break; /* found it */
    }

    if (l >= current->nlabel) {
	/* We need a new labelset. */
	new_labelset = 1;
	current->nlabel++;
	size = current->nlabel * sizeof(pmi_label);
	current->label = (pmi_label *)realloc(current->label, size);
	if (current->label == NULL) {
	    pmNoMem("pmiPutLabel: pmi_label", size, PM_FATAL_ERR);
	}
	lp = &current->label[current->nlabel-1];
	lp->type = type;
	lp->id = id;
	lp->labelset = NULL;
    }
    else
	lp = &current->label[l];

    /*
     * Add the label to the labelset. The value must be quoted unless it is
     * one of the JSON key values: true, false or null.
     */
    if (strcasecmp(value, "true") == 0 ||
	strcasecmp(value, "false") == 0 ||
	strcasecmp(value, "null") == 0)
	pmsprintf(buf, sizeof(buf), "{\"%s\":%s}", name, value);
    else
	pmsprintf(buf, sizeof(buf), "{\"%s\":\"%s\"}", name, value);

    if (__pmAddLabels(&lp->labelset, buf, type) < 0) {
	/*
	 * There was an error adding this label to the labelset. If this
	 * was the first label of its kind to to be added then we must free the
	 * storage pointed to by lp.
	*/
	if (new_labelset) {
	    current->nlabel--;
	    if (current->nlabel == 0) {
		free(current->label);
		current->label = NULL;
	    }
	    else {
		size = current->nlabel * sizeof(pmi_label);
		current->label = (pmi_label *)realloc(current->label, size);
		if (current->label == NULL) {
		    pmNoMem("pmiPutLabel: pmi_label", size, PM_FATAL_ERR);
		}
	    }
	}
	return current->last_sts = PMI_ERR_ADDLABELERROR;
    }

    if (type == PM_LABEL_INSTANCES)
	lp->labelset->inst = inst;
    
    return current->last_sts = 0;
}

/*
 * Search the current context for pending text to write. If found, return
 * 1 (true) otherwise return 0 (false).
 */
static int
text_pending(void)
{
    int t;
    for (t = 0; t < current->ntext; ++t) {
	if (current->text[t].meta_done == 0)
	    return 1; /* found one */
    }

    /* Not found */
    return 0;
}

static int
check_timestamp(const struct timeval *timestamp)
{
    if (timestamp->tv_sec < current->last_stamp.tv_sec ||
        (timestamp->tv_sec == current->last_stamp.tv_sec &&
	 timestamp->tv_usec < current->last_stamp.tv_usec)) {
	fprintf(stderr, "Fatal Error: timestamp ");
	printstamp(stderr, timestamp);
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
    struct timeval	timestamp;
    int			sts;

    if (current == NULL)
	return PM_ERR_NOCONTEXT;
    if (current->result == NULL && !text_pending() && current->label == NULL)
	return current->last_sts = PMI_ERR_NODATA;

    if (sec < 0) {
	pmtimevalNow(&timestamp);
    }
    else {
	timestamp.tv_sec = sec;
	timestamp.tv_usec = usec;
    }

    if ((sts = check_timestamp(&timestamp)) == 0) {
	/* We are guaranteed to be writing some data. */
	current->last_stamp = timestamp;

	/* Pending results? */
	if (current->result != NULL) {
	    current->result->timestamp = timestamp;
	    sts = _pmi_put_result(current, current->result);
	    pmFreeResult(current->result);
	    current->result = NULL;
	}

	if (sts >= 0) {
	    /* Pending text? */
	    sts = _pmi_put_text(current);

	    if (sts >= 0) {
		/* Pending labels? */
		sts = _pmi_put_label(current);
	    }
	}
    }

    return current->last_sts = sts;
}

int
pmiPutResult(const pmResult *result)
{
    int		sts;

    if (current == NULL)
	return PM_ERR_NOCONTEXT;

    current->result = (pmResult *)result;
    if ((sts = check_timestamp(&current->result->timestamp)) == 0) {
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
