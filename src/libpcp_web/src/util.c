/*
 * Copyright (c) 2017-2022,2024 Red Hat.
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
#include <assert.h>
#include <ctype.h>
#include "pmapi.h"
#include "libpcp.h"
#include "pmwebapi.h"
#include "private.h"
#include "sdsalloc.h"
#include "zmalloc.h"
#include "maps.h"
#include "util.h"
#include "sha1.h"

const char *SDS_NOINIT = "SDS_NOINIT";	/* back-compat, exported global */

static void
default_oom(size_t size)
{
    fprintf(stderr, "Out of memory allocating %" FMT_UINT64 " bytes\n",
		(__uint64_t)size);
    fflush(stderr);
    abort();
}
static void (*oom_handler)(size_t) = default_oom;

void *
zmalloc(size_t size)
{
    void	*p;

    p = malloc(size);
    if (UNLIKELY(p == NULL))
	oom_handler(size);
    return p;
}

void *
zcalloc(size_t elem, size_t size)
{
    void	*p;

    p = calloc(elem, size);
    if (UNLIKELY(p == NULL))
	oom_handler(elem * size);
    return p;
}

void
zfree(void *ptr)
{
    if (LIKELY(ptr != NULL))
	free(ptr);
}

/* convert into <milliseconds>-<nanoseconds> format for series streaming */
const char *
timespec_stream_str(struct timespec *stamp, char *buffer, int buflen)
{
    __uint64_t	millipart, fractions, crossover;
    __uint32_t	nanoseconds = (__uint32_t)stamp->tv_nsec;

    crossover = (__uint64_t)nanoseconds / 1000000;
    millipart = ((__uint64_t)stamp->tv_sec) * 1000;
    millipart += crossover;
    fractions = stamp->tv_nsec % 1000000 / 1000;
    pmsprintf(buffer, buflen, "%" FMT_UINT64 "-%"FMT_UINT64,
		(__uint64_t)millipart, (__uint64_t)fractions);
    return buffer;
}

/* convert timespec into human readable date/time format for logging */
const char *
timespec_str(struct timespec *tvp, char *buffer, int buflen)
{
    struct tm	tmp;
    time_t	now = (time_t)tvp->tv_sec;

    pmLocaltime(&now, &tmp);
    pmsprintf(buffer, sizeof(buflen), "%02u:%02u:%02u.%09u",
	      tmp.tm_hour, tmp.tm_min, tmp.tm_sec, (unsigned int)tvp->tv_nsec);
    return buffer;
}

/* convert __pmTimestamp into human readable date/time format for logging */
const char *
timestamp_str(__pmTimestamp *tsp, char *buffer, int buflen)
{
    struct tm	tmp;
    time_t	now = (time_t)tsp->sec;

    pmLocaltime(&now, &tmp);
    pmsprintf(buffer, buflen, "%02u:%02u:%02u.%09u",
	      tmp.tm_hour, tmp.tm_min, tmp.tm_sec, (unsigned int)tsp->nsec);
    return buffer;
}

const char *
pmwebapi_indom_str(metric_t *metric, char *buffer, int buflen)
{
    if (metric->desc.indom != PM_INDOM_NULL)
	return pmInDomStr_r(metric->desc.indom, buffer, buflen);
    return "none";
}

const char *
pmwebapi_pmid_str(metric_t *metric, char *buffer, int buflen)
{
    if (metric->desc.pmid != PM_ID_NULL)
	return pmIDStr_r(metric->desc.pmid, buffer, buflen);
    return "none";
}

const char *
pmwebapi_semantics_str(metric_t *metric, char *buffer, int buflen)
{
    return pmSemStr_r(metric->desc.sem, buffer, buflen);
}

const char *
pmwebapi_units_str(metric_t *metric, char *buffer, int buflen)
{
    const char	*units;

    units = pmUnitsStr_r(&metric->desc.units, buffer, buflen);
    if (units && units[0] != '\0')
	return units;
    pmsprintf(buffer, buflen, "none");
    return (const char *)buffer;
}

const char *
pmwebapi_type_str(metric_t *metric, char *buffer, int buflen)
{
    static const char * const typename[] = {
	"32", "u32", "64", "u64", "float", "double", "string",
	"aggregate", "aggregate_static", "event", "highres_event"
    };
    int		type = metric->desc.type;

    if (type >= 0 && type < sizeof(typename) / sizeof(typename[0]))
	pmsprintf(buffer, buflen, "%s", typename[type]);
    else if (type == PM_TYPE_NOSUPPORT)
	pmsprintf(buffer, buflen, "unsupported");
    else
	pmsprintf(buffer, buflen, "unknown");
    return buffer;
}

#if 0	/* TODO */
/* drop any trailing zeros after a decimal point */
static void
value_precision(char *buf, int maxlen, int usedlen)
{
    char	*p, *mantissa = NULL;

    for (p = buf; *p; p++) {
	if (*p != '.')
	    continue;
	mantissa = p;
	break;
    }
    if (!mantissa)
	return;
    for (p = buf + usedlen; p > mantissa; p--) {
	if (*p && *p != '0')
	    return;
	*p = '\0';
    }
}
#endif

static int
default_labelset(context_t *c, pmLabelSet **sets)
{
    pmLabelSet	*lp = NULL;
    char	buf[PM_MAXLABELJSONLEN];
    int		sts;

    pmsprintf(buf, sizeof(buf), "{\"hostname\":\"%s\"}", c->host);
    if ((sts = __pmAddLabels(&lp, buf, PM_LABEL_CONTEXT)) > 0) {
	*sets = lp;
	return 0;
    }
    if (lp)
	free(lp); /* Coverity CID340558 */
    return sts;
}

int
metric_labelsets(metric_t *metric, char *buffer, int length,
	int (*filter)(const pmLabel *, const char *, void *), void *arg)
{
    pmLabelSet	*sets[5];
    cluster_t	*cluster = metric->cluster;
    domain_t	*domain = cluster->domain;
    context_t	*context = domain->context;
    indom_t	*indom = metric->indom;
    int		nsets = 0;

    if (context && context->labelset)
	sets[nsets++] = context->labelset;
    if (domain && domain->labelset)
	sets[nsets++] = domain->labelset;
    if (indom && indom->labelset)
	sets[nsets++] = indom->labelset;
    if (cluster && cluster->labelset)
	sets[nsets++] = cluster->labelset;
    if (metric && metric->labelset)
	sets[nsets++] = metric->labelset;

    return pmMergeLabelSets(sets, nsets, buffer, length, filter, arg);
}

int
instance_labelsets(indom_t *indom, instance_t *inst, char *buffer, int length,
	int (*filter)(const pmLabel *, const char *, void *), void *arg)
{
    pmLabelSet	*sets[4];
    domain_t	*domain = indom->domain;
    context_t	*context = domain->context;
    int		nsets = 0;

    if (context->labelset)
	sets[nsets++] = context->labelset;
    if (domain->labelset)
	sets[nsets++] = domain->labelset;
    if (indom->labelset)
	sets[nsets++] = indom->labelset;
    if (inst && inst->labelset)
	sets[nsets++] = inst->labelset;

    return pmMergeLabelSets(sets, nsets, buffer, length, filter, arg);
}

/* extract only the identifying labels (not optional) */
static int
labels(const pmLabel *label, const char *json, void *arg)
{
    if ((label->flags & PM_LABEL_OPTIONAL) != 0)
	return 0;
    return 1;
}

int
pmwebapi_source_meta(context_t *c, char *buffer, int length)
{
    pmLabelSet	**set = &c->labelset;
    char	host[MAXHOSTNAMELEN];
    int		sts;

    if ((sts = pmGetHostName(c->context, host, sizeof(host))) < 0) {
	c->setup = 0;
	return sts;
    }
    c->host = sdsnew(host);
    sts = pmGetContextLabels(set);
    if (sts == PM_ERR_IPC)
	c->setup = 0;
    if (sts <= 0 && default_labelset(c, set) < 0)
	return sts;
    return pmMergeLabelSets(set, 1, buffer, length, labels, NULL);
}

static int
context_labels_str(context_t *c, char *buffer, int length)
{
    return pmMergeLabelSets(&c->labelset, 1, buffer, length, NULL, NULL);
}

int
pmLogLevelIsTTY(void)
{
    if (getenv("FAKETTY"))
	return 0;
    return isatty(fileno(stdout));
}

const char *
pmLogLevelStr(pmLogLevel level)
{
    switch (level) {
    case PMLOG_TRACE:
	return "Trace";
    case PMLOG_DEBUG:
	return "Debug";
    case PMLOG_INFO:
	return "Info";
    case PMLOG_WARNING:
	return "Warning";
    case PMLOG_ERROR:
	return "Error";
    case PMLOG_REQUEST:
	return "Bad request";
    case PMLOG_RESPONSE:
	return "Bad response";
    case PMLOG_CORRUPT:
	return "Corrupt TSDB";
    default:
	break;
    }
    return "???";
}

#define ANSI_RESET	"\x1b[0m"
#define ANSI_FG_BLACK	"\x1b[30m"
#define ANSI_FG_RED	"\x1b[31m"
#define ANSI_FG_GREEN	"\x1b[32m"
#define ANSI_FG_YELLOW	"\x1b[33m"
#define ANSI_FG_BLUE	"\x1b[34m"
#define ANSI_FG_MAGENTA	"\x1b[35m"
#define ANSI_FG_CYAN	"\x1b[36m"
#define ANSI_BG_RED	"\x1b[41m"
#define ANSI_BG_WHITE	"\x1b[47m"
void
pmLogLevelPrint(FILE *stream, pmLogLevel level, sds message, int istty)
{
    const char		*colour, *levels = pmLogLevelStr(level);

    switch (level) {
    case PMLOG_TRACE:
	colour = ANSI_FG_MAGENTA;
	break;
    case PMLOG_DEBUG:
	colour = ANSI_FG_BLUE;
	break;
    case PMLOG_INFO:
	colour = ANSI_FG_GREEN;
	break;
    case PMLOG_WARNING:
	colour = ANSI_FG_YELLOW;
	break;
    case PMLOG_ERROR:
	colour = ANSI_FG_RED;
	break;
    case PMLOG_REQUEST:
	colour = ANSI_BG_WHITE ANSI_FG_RED;
	break;
    case PMLOG_RESPONSE:
	colour = ANSI_BG_WHITE ANSI_FG_RED;
	break;
    case PMLOG_CORRUPT:
	colour = ANSI_BG_RED ANSI_FG_BLACK;
	break;
    default:
	colour = ANSI_FG_CYAN;
	break;
    }
    if (istty)
	fprintf(stream, "%s: [%s%s%s] %s\n",
		pmGetProgname(), colour, levels, ANSI_RESET, message);
    else
	fprintf(stream, "%s: [%s] %s\n", pmGetProgname(), levels, message);
}

static char *
hash_identity(const unsigned char *hash, char *buffer, int buflen)
{
    int		nbytes, off;

    /* Input 20-byte SHA1 hash, output 40-byte representation */
    for (nbytes = off = 0; nbytes < 20; nbytes++)
	off += pmsprintf(buffer + off, buflen - off, "%02x", hash[nbytes]);
    buffer[buflen-1] = '\0';
    return buffer;
}

char *
pmwebapi_hash_str(const unsigned char *p, char *buffer, int buflen)
{
    assert(buflen > 40);
    return hash_identity(p, buffer, buflen);
}

sds
pmwebapi_hash_sds(sds s, const unsigned char *p)
{
    char	namebuf[42];

    hash_identity(p, namebuf, sizeof(namebuf));
    if (s == NULL)
	s = sdsnewlen(NULL, 40);
    sdsclear(s);
    return sdscatlen(s, namebuf, 40);
}

int
pmwebapi_string_hash(unsigned char *hash, const char *string, int length)
{
    SHA1_CTX		shactx;
    const char		prefix[] = "{\"series\":\"string\",\"value\":\"";
    const char		suffix[] = "\"}";

    /* Calculate unique string identifier 20-byte SHA1 hash */
    SHA1Init(&shactx);
    SHA1Update(&shactx, (unsigned char *)prefix, sizeof(prefix)-1);
    SHA1Update(&shactx, (unsigned char *)string, length);
    SHA1Update(&shactx, (unsigned char *)suffix, sizeof(suffix)-1);
    SHA1Final(hash, &shactx);
    return 0;
}

int
pmwebapi_source_hash(unsigned char *hash, const char *labels, int length)
{
    SHA1_CTX		shactx;
    const char		prefix[] = "{\"series\":\"source\",\"labels\":";
    const char		suffix[] = "}";

    /* Calculate unique source identifier 20-byte SHA1 hash */
    SHA1Init(&shactx);
    SHA1Update(&shactx, (unsigned char *)prefix, sizeof(prefix)-1);
    SHA1Update(&shactx, (unsigned char *)labels, length);
    SHA1Update(&shactx, (unsigned char *)suffix, sizeof(suffix)-1);
    SHA1Final(hash, &shactx);
    return 0;
}

int
pmwebapi_search_hash(unsigned char *hash, const char *string, int length)
{
    SHA1_CTX		shactx;
    const char		prefix[] = "{\"series\":\"search\",";
    const char		suffix[] = "}";

    /* Calculate unique source identifier 20-byte SHA1 hash */
    SHA1Init(&shactx);
    SHA1Update(&shactx, (unsigned char *)prefix, sizeof(prefix)-1);
    SHA1Update(&shactx, (unsigned char *)string, length);
    SHA1Update(&shactx, (unsigned char *)suffix, sizeof(suffix)-1);
    SHA1Final(hash, &shactx);
    return 0;
}

int
pmwebapi_context_hash(context_t *context)
{
    char		labels[PM_MAXLABELJSONLEN];
    int			sts;

    if (context->labels == NULL) {
	if ((sts = context_labels_str(context, labels, sizeof(labels))) < 0)
	    return sts;
	context->labels = sdsnewlen(labels, sts);
    }
    return pmwebapi_source_hash(context->name.hash, context->labels, sdslen(context->labels));
}

void
pmwebapi_metric_hash(metric_t *metric)
{
    SHA1_CTX		shactx;
    pmDesc		*desc = &metric->desc;
    sds			identifier;
    char		buf[PM_MAXLABELJSONLEN];
    char		sem[32], type[32], units[64];
    int			len, i;

    if (metric->labels == NULL) {
	len = metric_labelsets(metric, buf, sizeof(buf), labels, NULL);
	if (len <= 0)
	    len = pmsprintf(buf, sizeof(buf), "null");
	metric->labels = sdsnewlen(buf, len);
    }

    identifier = sdsempty();
    for (i = 0; i < metric->numnames; i++) {
	identifier = sdscatfmt(identifier,
		"{\"series\":\"metric\",\"name\":\"%S\",\"labels\":%S,"
		 "\"semantics\":\"%s\",\"type\":\"%s\",\"units\":\"%s\"}",
		metric->names[i].sds, metric->labels,
		pmSemStr_r(desc->sem, sem, sizeof(sem)),
		pmTypeStr_r(desc->type, type, sizeof(type)),
		pmUnitsStr_r(&desc->units, units, sizeof(units)));
	/* Calculate unique metric identifier 20-byte SHA1 hash */
	SHA1Init(&shactx);
	SHA1Update(&shactx, (unsigned char *)identifier, sdslen(identifier));
	SHA1Final(metric->names[i].hash, &shactx);
	sdsclear(identifier);
    }
    sdsfree(identifier);

    metric->cached = 0;
}

void
pmwebapi_add_indom_labels(indom_t *indom)
{
    char		buf[PM_MAXLABELJSONLEN];
    int			len;

    if (indom->labels == NULL) {
	len = instance_labelsets(indom, NULL, buf, sizeof(buf), labels, NULL);
	if (len <= 0)
	    len = pmsprintf(buf, sizeof(buf), "null");
	indom->labels = sdsnewlen(buf, len);
    }
}

void
pmwebapi_instance_hash(indom_t *ip, instance_t *instance)
{
    SHA1_CTX		shactx;
    sds			identifier;
    char		buf[PM_MAXLABELJSONLEN];
    int			len;

    if (instance->labels == NULL) {
	len = instance_labelsets(ip, instance, buf, sizeof(buf), labels, NULL);
	if (len <= 0)
	    len = pmsprintf(buf, sizeof(buf), "null");
	instance->labels = sdsnewlen(buf, len);
    }

    identifier = sdscatfmt(sdsempty(),
		"{\"series\":\"instance\",\"name\":\"%S\",\"labels\":%S}",
		instance->name.sds, instance->labels);
    /* Calculate unique instance identifier 20-byte SHA1 hash */
    SHA1Init(&shactx);
    SHA1Update(&shactx, (unsigned char *)identifier, sdslen(identifier));
    SHA1Final(instance->name.hash, &shactx);
    sdsfree(identifier);

    instance->cached = 0;
}

sds
pmwebapi_new_context(context_t *cp)
{
    char		labels[PM_MAXLABELJSONLEN];
    char		pmmsg[PM_MAXERRMSGLEN];
    sds			msg = NULL;
    int			sts;

    /* establish PMAPI context */
    if ((sts = cp->context = pmNewContext(cp->type, cp->name.sds)) < 0) {
	if (cp->type == PM_CONTEXT_HOST)
	    infofmt(msg, "cannot connect to PMCD: %s",
		    pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	else if (cp->type == PM_CONTEXT_LOCAL)
	    infofmt(msg, "cannot make standalone connection on localhost: %s",
		    pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	else
	    infofmt(msg, "cannot open archive \"%s\": %s",
		    cp->name.sds, pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
    } else if ((sts = pmwebapi_source_meta(cp, labels, sizeof(labels))) < 0) {
	infofmt(msg, "failed to get context labels: %s",
		    pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
    } else if ((sts = pmwebapi_source_hash(cp->name.hash, labels, sts)) < 0) {
	infofmt(msg, "failed to set context hash: %s",
		    pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
    } else {
	pmwebapi_setup_context(cp);
    }
    return msg;
}

void
pmwebapi_setup_context(context_t *cp)
{
    if (pmDebugOptions.series) {
	char		hashbuf[42];

	pmwebapi_hash_str(cp->name.hash, hashbuf, sizeof(hashbuf));
	fprintf(stderr, "pmwebapi_setup_context: SHA1=%s [%s]\n",
			hashbuf, cp->name.sds);
    }
    cp->pmids = dictCreate(&intKeyDictCallBacks, cp);	/* pmid: metric */
    cp->metrics = dictCreate(&sdsKeyDictCallBacks, cp);	/* name: metric */
    cp->indoms = dictCreate(&intKeyDictCallBacks, cp);	/* id: indom */
    cp->domains = dictCreate(&intKeyDictCallBacks, cp);	/* id: domain */
    cp->clusters = dictCreate(&intKeyDictCallBacks, cp);/* id: cluster */

    pmwebapi_locate_context(cp);
}

#define LATITUDE	"latitude"
#define LATITUDE_LEN	(sizeof(LATITUDE)-1)
#define LONGITUDE	"longitude"
#define LONGITUDE_LEN	(sizeof(LONGITUDE)-1)

void
pmwebapi_locate_context(context_t *cp)
{
    pmLabel		*label;
    double		location;
    char		*name, *value;
    int			i, count = cp->labelset->nlabels;

    /* update the location of a context using its labels */
    for (i = 0; i < count; i++) {
	label = &cp->labelset->labels[i];
	name = cp->labelset->json + label->name;
	value = cp->labelset->json + label->value;
	if (label->namelen == LONGITUDE_LEN &&
	    strncmp(name, LONGITUDE, LONGITUDE_LEN) == 0) {
	    if ((location = strtod(value, NULL)) != cp->location[1]) {
		cp->location[1] = location;
		cp->updated = 1;
	    }
	}
	else if (label->namelen == LATITUDE_LEN &&
	    strncmp(name, LATITUDE, LATITUDE_LEN) == 0) {
	    if ((location = strtod(value, NULL)) != cp->location[0]) {
		cp->location[0] = location;
		cp->updated = 1;
	    }
	}
    }

    if (pmDebugOptions.series) {
	char		hashbuf[42];

	pmwebapi_hash_str(cp->name.hash, hashbuf, sizeof(hashbuf));
	fprintf(stderr, "%s: source SHA1=%s latitude=%.5f longitude=%.5f\n",
			"pmwebapi_locate_context", hashbuf,
			cp->location[0], cp->location[1]);
    }
}

void
pmwebapi_free_context(context_t *cp)
{
    pmwebapi_release_context(cp);
    memset(cp, 0, sizeof(*cp));
    free(cp);
}

void
pmwebapi_release_context(context_t *cp)
{
    dictIterator	*iterator;
    dictEntry		*entry;

    if (cp->context >= 0) {
	pmDestroyContext(cp->context);
	cp->context = -1;
    }

    sdsfree(cp->name.sds);
    sdsfree(cp->origin);
    sdsfree(cp->host);

    sdsfree(cp->username);
    sdsfree(cp->password);
    sdsfree(cp->realm);

    sdsfree(cp->labels);
    if (cp->labelset)
	pmFreeLabelSets(cp->labelset, 1);

    if (cp->metrics)	/* use the same value pointers as cp->pmids */
	dictRelease(cp->metrics);	/* but, one entry per name */

    if (cp->pmids) {
	iterator = dictGetIterator(cp->pmids);
	while ((entry = dictNext(iterator)) != NULL)
	    pmwebapi_free_metric((metric_t *)dictGetVal(entry));
	dictReleaseIterator(iterator);
	dictRelease(cp->pmids);
    }
    if (cp->clusters) {
	iterator = dictGetIterator(cp->clusters);
	while ((entry = dictNext(iterator)) != NULL)
	    pmwebapi_free_cluster((cluster_t *)dictGetVal(entry));
	dictReleaseIterator(iterator);
	dictRelease(cp->clusters);
    }
    if (cp->indoms) {
	iterator = dictGetIterator(cp->indoms);
	while ((entry = dictNext(iterator)) != NULL)
	    pmwebapi_free_indom((indom_t *)dictGetVal(entry));
	dictReleaseIterator(iterator);
	dictRelease(cp->indoms);
    }
    if (cp->domains) {
	iterator = dictGetIterator(cp->domains);
	while ((entry = dictNext(iterator)) != NULL)
	    pmwebapi_free_domain((domain_t *)dictGetVal(entry));
	dictReleaseIterator(iterator);
	dictRelease(cp->domains);
    }
}

void
pmwebapi_free_domain(struct domain *domain)
{
    if (domain->labelset)
	pmFreeLabelSets(domain->labelset, 1);
    memset(domain, 0, sizeof(*domain));
    free(domain);
}

struct domain *
pmwebapi_new_domain(context_t *context, unsigned int key)
{
    struct domain	*domain;

    if ((domain = calloc(1, sizeof(domain_t))) == NULL)
	return NULL;
    domain->context = context;
    domain->domain = key;
    dictAdd(context->domains, &key, (void *)domain);
    return domain;
}

struct domain *
pmwebapi_add_domain(context_t *context, unsigned int key)
{
    dictEntry		*entry;

    if ((entry = dictFind(context->domains, &key)) != NULL)
	return (domain_t *)dictGetVal(entry);
    return pmwebapi_new_domain(context, key);
}

void
pmwebapi_add_domain_labels(struct context *context, struct domain *domain)
{
    char		errmsg[PM_MAXERRMSGLEN];
    int			sts;

    if (domain->labelset == NULL) {
	sts = pmGetDomainLabels(domain->domain, &domain->labelset);
	if (sts == PM_ERR_IPC)
	    context->setup = 0;
	if (sts < 0) {
	    if (pmDebugOptions.series)
		fprintf(stderr, "failed to get domain (%d) labels: %s\n",
			domain->domain,
			pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	    domain->labelset = NULL;
	}
    }
}

void
pmwebapi_free_cluster(struct cluster *cluster)
{
    if (cluster->labelset)
	pmFreeLabelSets(cluster->labelset, 1);
    memset(cluster, 0, sizeof(*cluster));
    free(cluster);
}

struct cluster *
pmwebapi_new_cluster(context_t *context, domain_t *domain, pmID pmid)
{
    struct cluster	*cluster;
    unsigned int	key;

    key = pmID_build(domain->domain, pmID_cluster(pmid), 0);
    if ((cluster = calloc(1, sizeof(cluster_t))) == NULL)
	return NULL;
    cluster->domain = domain;
    cluster->cluster = key;
    dictAdd(context->clusters, &key, (void *)cluster);
    return cluster;
}

struct cluster *
pmwebapi_add_cluster(context_t *context, domain_t *domain, pmID pmid)
{
    dictEntry		*entry;
    unsigned int	key;

    key = pmID_build(domain->domain, pmID_cluster(pmid), 0);
    if ((entry = dictFind(context->clusters, &key)) != NULL)
	return (cluster_t *)dictGetVal(entry);
    return pmwebapi_new_cluster(context, domain, pmid);
}

void
pmwebapi_add_cluster_labels(struct context *context, struct cluster *cluster)
{
    char		errmsg[PM_MAXERRMSGLEN];
    int			sts;

    if (cluster->labelset == NULL) {
	sts = pmGetClusterLabels(cluster->cluster, &cluster->labelset);
	if (sts == PM_ERR_IPC)
	    context->setup = 0;
	if (sts < 0) {
	    if (pmDebugOptions.series)
		fprintf(stderr, "failed to get cluster (%u) labels: %s\n",
			cluster->cluster,
			pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	    cluster->labelset = NULL;
	}
    }
}

void
pmwebapi_free_indom(indom_t *indom)
{
    dictIterator	*iterator;
    dictEntry		*entry;

    sdsfree(indom->helptext);
    sdsfree(indom->oneline);
    sdsfree(indom->labels);

    if (indom->labelset)
	pmFreeLabelSets(indom->labelset, 1);

    if (indom->insts) {
	iterator = dictGetIterator(indom->insts);
	while ((entry = dictNext(iterator)) != NULL)
	    pmwebapi_free_instance((instance_t *)dictGetVal(entry));
	dictReleaseIterator(iterator);
	dictRelease(indom->insts);
    }

    memset(indom, 0, sizeof(*indom));
    free(indom);
}

struct indom *
pmwebapi_new_indom(context_t *context, domain_t *domain, pmInDom key)
{
    struct indom	*indom;

    if ((indom = calloc(1, sizeof(indom_t))) == NULL)
	return NULL;
    indom->indom = key;
    indom->domain = domain;
    indom->insts = dictCreate(&intKeyDictCallBacks, indom);
    dictAdd(context->indoms, &key, (void *)indom);
    return indom;
}

struct indom *
pmwebapi_add_indom(context_t *context, domain_t *domain, pmInDom indom)
{
    dictEntry		*entry;

    if (indom == PM_INDOM_NULL)
	return NULL;
    if ((entry = dictFind(context->indoms, &indom)) != NULL)
        return (indom_t *)dictGetVal(entry);
    return pmwebapi_new_indom(context, domain, indom);
}

static int
labelsetlen(pmLabelSet *lp)
{
    if (lp->nlabels <= 0)
	return 0;
    return sizeof(pmLabelSet) + lp->jsonlen + (lp->nlabels * sizeof(pmLabel));
}

pmLabelSet *
pmwebapi_labelsetdup(pmLabelSet *lp)
{
    pmLabelSet		*dup;
    char		*json;

    if ((dup = calloc(1, sizeof(pmLabelSet))) == NULL)
	return NULL;
    *dup = *lp;
    if (lp->nlabels <= 0)
	return dup;
    if ((json = strdup(lp->json)) == NULL) {
	free(dup);
	return NULL;
    }
    if ((dup->labels = calloc(lp->nlabels, sizeof(pmLabel))) == NULL) {
	free(dup);
	free(json);
	return NULL;
    }
    memcpy(dup->labels, lp->labels, sizeof(pmLabel) * lp->nlabels);
    dup->json = json;
    return dup;
}

void
pmwebapi_add_instances_labels(struct context *context, struct indom *indom)
{
    struct instance	*instance;
    pmLabelSet		*labels, *labelsets = NULL;
    size_t		length;
    char		errmsg[PM_MAXERRMSGLEN], buffer[64];
    int			i, inst, sts = 0, nsets = 0;

    if (indom->labelset == NULL) {
	sts = pmGetInDomLabels(indom->indom, &indom->labelset);
	if (sts == PM_ERR_IPC)
	    context->setup = 0;
	if (sts < 0) {
	    if (pmDebugOptions.series)
		fprintf(stderr, "failed to get indom (%s) labels: %s\n",
			pmInDomStr_r(indom->indom, buffer, sizeof(buffer)),
			pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	    indom->labelset = NULL;
	    indom->updated = sts = 0;
	}
    }

    if (indom->updated == 0) {
	sts = nsets = pmGetInstancesLabels(indom->indom, &labelsets);
	if (sts == PM_ERR_IPC)
	    context->setup = 0;
	for (i = 0; i < nsets; i++) {
	    labels = &labelsets[i];
	    if ((length = labelsetlen(labels)) == 0)
		continue;
	    inst = labelsets[i].inst;
	    if ((instance = dictFetchValue(indom->insts, &inst)) == NULL)
		continue;
	    if ((labels = pmwebapi_labelsetdup(labels)) == NULL) {
		if (pmDebugOptions.series)
		    fprintf(stderr, "failed to dup %s instance labels: %s\n",
			    pmInDomStr_r(indom->indom, buffer, sizeof(buffer)),
			    pmErrStr_r(sts, errmsg, sizeof(errmsg)));
		continue;
	    }
	    if (instance->labelset)
		pmFreeLabelSets(instance->labelset, 1);
	    instance->labelset = labels;

	    pmwebapi_instance_hash(indom, instance);

	    if (pmDebugOptions.series) {
		pmwebapi_hash_str(instance->name.hash, buffer, sizeof(buffer));
		fprintf(stderr, "pmwebapi instance %s", instance->name.sds);
		fprintf(stderr, "\nSHA1=%s\n", buffer);
	    }
	}
	if (sts >= 0)
	    indom->updated = 1;
	else if (pmDebugOptions.series)
	    fprintf(stderr, "failed to get indom (%s) instance labels: %s\n",
		    pmInDomStr_r(indom->indom, buffer, sizeof(buffer)),
		    pmErrStr_r(sts, errmsg, sizeof(errmsg)));
    } else {
	assert(sts >= 0);
	indom->updated = 1;
    }

    if (labelsets)
	pmFreeLabelSets(labelsets, nsets);
}

void
pmwebapi_free_instance(instance_t *instance)
{
    labellist_t		*list = instance->labellist;

    sdsfree(instance->name.sds);
    sdsfree(instance->labels);

    if (instance->labelset)
	pmFreeLabelSets(instance->labelset, 1);

    while (list) {
	sdsfree(list->name);
	sdsfree(list->value);
	if (list->valuemap)
	    keyMapRelease(list->valuemap);
	list = list->next;
    }

    memset(instance, 0, sizeof(*instance));
    free(instance);
}

struct instance *
pmwebapi_new_instance(indom_t *indom, int inst, sds name)
{
    struct instance	*instance;

    if (name == NULL || sdslen(name) == 0)
	return NULL;

    if ((instance = calloc(1, sizeof(instance_t))) == NULL)
	return NULL;
    instance->inst = inst;
    instance->name.sds = name;
    pmwebapi_string_hash(instance->name.id, name, sdslen(name));
    pmwebapi_instance_hash(indom, instance);
    dictAdd(indom->insts, &inst, (void *)instance);
    return instance;
}

struct instance *
pmwebapi_add_instance(struct indom *indom, int inst, char *name)
{
    struct instance	*instance;
    size_t		length;

    if (name == NULL || (length = strlen(name)) == 0)
	return NULL;

    if ((instance = dictFetchValue(indom->insts, &inst)) != NULL) {
	/* has the external name changed for this internal identifier? */
	if ((sdslen(instance->name.sds) != length) ||
	    (strncmp(instance->name.sds, name, length) != 0)) {
	    sdsclear(instance->name.sds);
	    instance->name.sds = sdscatlen(instance->name.sds, name, length);
	    pmwebapi_string_hash(instance->name.id, name, length);
	    pmwebapi_instance_hash(indom, instance);
	}
	return instance;
    }
    return pmwebapi_new_instance(indom, inst, sdsnewlen(name, length));
}

struct instance *
pmwebapi_lookup_instance(struct indom *indom, int inst)
{
    struct instance	*instance;
    char		*name;

    if (pmNameInDom(indom->indom, inst, &name) < 0)
	return NULL;

    instance = pmwebapi_new_instance(indom, inst, sdsnew(name));
    free(name);

    return instance;
}

unsigned int
pmwebapi_add_indom_instances(struct context *context, struct indom *indom)
{
    struct instance	*instance;
    unsigned int	count = 0;
    dictIterator	*iterator;
    dictEntry		*entry;
    char		errmsg[PM_MAXERRMSGLEN], buffer[64], **namelist = NULL;
    int			*instlist = NULL, i, sts;

    /* refreshing instance domain entries so mark all out-of-date first */
    iterator = dictGetIterator(indom->insts);
    while ((entry = dictNext(iterator)) != NULL) {
	instance = dictGetVal(entry);
	instance->updated = 0;
    }
    dictReleaseIterator(iterator);

    if ((sts = pmGetInDom(indom->indom, &instlist, &namelist)) >= 0) {
	for (i = 0; i < sts; i++) {
	    if (namelist[i] == NULL || namelist[i][0] == '\0') {
		if (pmDebugOptions.dev0) {
		    if (namelist[i] == NULL)
			fprintf(stderr, "pmwebapi_add_indom_instances: Botch: indom %s numinst %d inst %d namelist[%d] NULL\n", 
				    pmInDomStr_r(indom->indom, buffer, sizeof(buffer)),
				    sts, instlist[i], i);
		    else
			fprintf(stderr, "pmwebapi_add_indom_instances: Botch: indom %s numinst %d inst %d namelist[%d] empty\n", 
				    pmInDomStr_r(indom->indom, buffer, sizeof(buffer)),
				    sts, instlist[i], i);
		}
		continue;
	    }
	    instance = pmwebapi_add_instance(indom, instlist[i], namelist[i]);
	    instance->updated = 1;
	    count++;
	}
    } else {
	if (sts == PM_ERR_IPC)
	    context->setup = 0;
	if (pmDebugOptions.series)
	    fprintf(stderr, "failed to get indom (%s) instances: %s\n",
		    pmInDomStr_r(indom->indom, buffer, sizeof(buffer)),
		    pmErrStr_r(sts, errmsg, sizeof(errmsg)));
    }
    if (instlist)
	free(instlist);
    if (namelist)
	free(namelist);
    return count;
}

void
pmwebapi_free_metric(metric_t *metric)
{
    labellist_t		*list = metric->labellist;
    int			i, type = metric->desc.type;

    sdsfree(metric->helptext);
    sdsfree(metric->oneline);
    sdsfree(metric->labels);

    if (metric->labelset)
	pmFreeLabelSets(metric->labelset, 1);

    while (list) {
	sdsfree(list->name);
	sdsfree(list->value);
	if (list->valuemap)
	    keyMapRelease(list->valuemap);
	list = list->next;
    }

    for (i = 0; i < metric->numnames; i++)
	sdsfree(metric->names[i].sds);
    if (metric->names)
	free(metric->names);

    if (metric->desc.indom == PM_INDOM_NULL) {
	pmwebapi_release_value(type, &metric->u.atom);
    } else if (metric->u.vlist) {
	for (i = 0; i < metric->u.vlist->listcount; i++)
	    pmwebapi_release_value(type, &metric->u.vlist->value[i].atom);
	free(metric->u.vlist);
    }

    memset(metric, 0, sizeof(*metric));
    free(metric);
}

struct metric *
pmwebapi_new_metric(context_t *cp, const sds name, pmDesc *desc,
		int numnames, char **names)
{
    struct seriesname	*series;
    struct metric	*metric;
    struct domain	*domain;
    int			i, len, numextra = 0;	/* is given name in names? */

    if (numnames <= 0)
	return NULL;

    for (i = 0; name && i < numnames; i++) {
	if (strcmp(name, names[i]) == 0)
	    break;
    }
    if (name && i == numnames)
	numextra = 1;

    if ((metric = dictFetchValue(cp->pmids, &desc->pmid)) != NULL)
	return pmwebapi_add_metric(cp, name, desc, numnames, names);

    if ((metric = calloc(1, sizeof(metric_t))) == NULL)
	return NULL;
    if ((series = calloc(numnames + numextra, sizeof(seriesname_t))) == NULL) {
	free(metric);
	return NULL;
    }
    for (i = 0; i < numnames; i++) {
	len = strlen(names[i]);
	series[i].sds = sdsnewlen(names[i], len);
	pmwebapi_string_hash(series[i].id, names[i], len);
    }
    if (numextra) {
	series[i].sds = sdsdup(name);
	pmwebapi_string_hash(series[i].id, name, sdslen(name));
    }

    domain = pmwebapi_add_domain(cp, pmID_domain(desc->pmid));
    metric->indom = pmwebapi_add_indom(cp, domain, desc->indom);
    metric->cluster = pmwebapi_add_cluster(cp, domain, desc->pmid);

    metric->desc = *desc;
    metric->names = series;
    metric->numnames = numnames + numextra;
    for (i = 0; i < numnames; i++)
	dictAdd(cp->metrics, series[i].sds, (void *)metric);
    if (numextra)
	dictAdd(cp->metrics, series[i].sds, (void *)metric);
    dictAdd(cp->pmids, &desc->pmid, (void *)metric);
    return metric;
}

struct metric *
pmwebapi_add_metric(context_t *cp, const sds base, pmDesc *desc, int numnames, char **names)
{
    struct metric	*metric;
    sds			name;
    int			i;

    /* search for a match on any of the given names */
    if (base && (metric = dictFetchValue(cp->metrics, base)) != NULL)
	return metric;

    name = sdsempty();
    for (i = 0; i < numnames; i++) {
	sdsclear(name);
	name = sdscat(name, names[i]);
	if ((metric = dictFetchValue(cp->metrics, name)) != NULL) {
	    sdsfree(name);
	    return metric;
	}
    }
    sdsfree(name);

    return pmwebapi_new_metric(cp, base, desc, numnames, names);
}

struct metric *
pmwebapi_new_pmid(context_t *cp, const sds name, pmID pmid,
		pmLogInfoCallBack info, void *arg)
{
    struct metric	*mp = NULL;
    pmDesc		desc;
    char		**names = NULL, errmsg[PM_MAXERRMSGLEN], buffer[64];
    int			sts, numnames = 0;

    if ((sts = pmUseContext(cp->context)) < 0) {
	fprintf(stderr, "%s: failed to use context for PMID %s: %s\n",
		"pmwebapi_new_pmid",
		pmIDStr_r(pmid, buffer, sizeof(buffer)),
		pmErrStr_r(sts, errmsg, sizeof(errmsg)));
    } else if ((sts = pmLookupDesc(pmid, &desc)) < 0) {
	if (sts == PM_ERR_IPC)
	    cp->setup = 0;
	if (pmDebugOptions.series)
	    fprintf(stderr, "%s: failed to lookup metric %s descriptor: %s\n",
		    "pmwebapi_new_pmid",
		    pmIDStr_r(pmid, buffer, sizeof(buffer)),
		    pmErrStr_r(sts, errmsg, sizeof(errmsg)));
    } else if ((numnames = sts = pmNameAll(pmid, &names)) < 0) {
	if (sts == PM_ERR_IPC)
	    cp->setup = 0;
	if (pmDebugOptions.series)
	    fprintf(stderr, "%s: failed to lookup metric %s names: %s\n",
		    "pmwebapi_new_pmid",
		    pmIDStr_r(pmid, buffer, sizeof(buffer)),
		    pmErrStr_r(sts, errmsg, sizeof(errmsg)));
    } else {
	mp = pmwebapi_new_metric(cp, name, &desc, numnames, names);
	if (numnames > 0 && names != NULL)
	    free(names);
    }
    return mp;
}


void
pmwebapi_add_item_labels(struct context *context, struct metric *metric)
{
    char		errmsg[PM_MAXERRMSGLEN];
    int			sts;

    if (metric->labelset == NULL) {
	sts = pmGetItemLabels(metric->desc.pmid, &metric->labelset);
	if (sts == PM_ERR_IPC)
	    context->setup = 0;
	if (sts < 0) {
	    if (pmDebugOptions.series)
		fprintf(stderr, "failed to get metric item (%u) labels: %s\n",
			pmID_item(metric->desc.pmid),
			pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	    metric->labelset = NULL;
	}
    }
}

void
pmwebapi_metric_help(struct context *context, struct metric *metric)
{
    pmID		pmid = metric->desc.pmid;
    char		*text;
    int			sts;

    if (metric->oneline == NULL) {
	sts = pmLookupText(pmid, PM_TEXT_ONELINE | PM_TEXT_DIRECT, &text);
	if (sts == 0) {
	    metric->oneline = sdsnew(text);
	    free(text);
	} else if (sts == PM_ERR_IPC) {
	    context->setup = 0;
	}
    }
    if (metric->helptext == NULL) {
	sts = pmLookupText(pmid, PM_TEXT_HELP | PM_TEXT_DIRECT, &text);
	if (sts == 0) {
	    metric->helptext = sdsnew(text);
	    free(text);
	} else if (sts == PM_ERR_IPC) {
	    context->setup = 0;
	}
    }
}

void
pmwebapi_indom_help(struct context *context, struct indom *indom)
{
    pmInDom		id = indom->indom;
    char		*text;
    int			sts;

    if (indom->oneline == NULL) {
	sts = pmLookupInDomText(id, PM_TEXT_ONELINE | PM_TEXT_DIRECT, &text);
	if (sts == 0) {
	    indom->oneline = sdsnew(text);
	    free(text);
	} else if (sts == PM_ERR_IPC) {
	    context->setup = 0;
	}
    }
    if (indom->helptext == NULL) {
	sts = pmLookupInDomText(id, PM_TEXT_HELP | PM_TEXT_DIRECT, &text);
	if (sts == 0) {
	    indom->helptext = sdsnew(text);
	    free(text);
	} else if (sts == PM_ERR_IPC) {
	    context->setup = 0;
	}
    }
}

void
pmwebapi_event_flags(void)
{
    /* TODO: not yet implemented */
}

void
pmwebapi_event_missed(void)
{
    /* TODO: not yet implemented */
}

sds
pmwebapi_event_parameter(sds s, pmValueSet *vsp, int inst, int *flags)
{
    (void)vsp;
    (void)inst;
    (void)flags;

    return s;	/* TODO: not yet implemented */
}

sds
pmwebapi_usectimestamp(sds s, struct timeval *timestamp)
{
    struct tm	tmp;
    time_t	now;
    size_t	length;
    char	buffer[32];

    now = (time_t)timestamp->tv_sec;
    pmLocaltime(&now, &tmp);
    length = pmsprintf(buffer, sizeof(buffer), "%02u:%02u:%02u.%06u",
			tmp.tm_hour, tmp.tm_min, tmp.tm_sec,
			(unsigned int)timestamp->tv_usec);
    return sdscatlen(s, buffer, length);
}

sds
pmwebapi_nsectimestamp(sds s, struct timespec *timestamp)
{
    struct tm	tmp;
    time_t	now;
    size_t	length;
    char	buffer[32];

    now = (time_t)timestamp->tv_sec;
    pmLocaltime(&now, &tmp);
    length = pmsprintf(buffer, sizeof(buffer), "%02u:%02u:%02u.%09u",
			tmp.tm_hour, tmp.tm_min, tmp.tm_sec,
			(unsigned int)timestamp->tv_nsec);
    return sdscatlen(s, buffer, length);
}
