/*
 * Copyright (c) 2017-2018 Red Hat.
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
#include <assert.h>
#include <ctype.h>
#include "pmapi.h"
#include "libpcp.h"
#include "private.h"
#include "sdsalloc.h"
#include "zmalloc.h"
#include "util.h"
#include "sha1.h"

/* dynamic memory manipulation */
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
s_malloc(size_t size)
{
    void	*p;

    p = malloc(size);
    if (UNLIKELY(p == NULL))
	oom_handler(size);
    return p;
}

void *
s_realloc(void *ptr, size_t size)
{
    void	*p;

    if (ptr == NULL)
	return s_malloc(size);
    p = realloc(ptr, size);
    if (UNLIKELY(p == NULL))
	oom_handler(size);
    return p;
}

void
s_free(void *ptr)
{
    if (LIKELY(ptr != NULL))
	free(ptr);
}

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
zcalloc(size_t size)
{
    void	*p;

    p = calloc(1, size);
    if (UNLIKELY(p == NULL))
	oom_handler(size);
    return p;
}

void
zfree(void *ptr)
{
    if (LIKELY(ptr != NULL))
	free(ptr);
}

/* time structure manipulation */
int
tsub(struct timeval *a, struct timeval *b)
{
    if ((a == NULL) || (b == NULL))
	return -1;
    pmtimevalDec(a, b);
    if (a->tv_sec < 0) {
	/* clip negative values at zero */
	a->tv_sec = 0;
	a->tv_usec = 0;
    }
    return 0;
}

int
tadd(struct timeval *a, struct timeval *b)
{
    if ((a == NULL) || (b == NULL))
	return -1;
    pmtimevalInc(a, b);
    return 0;
}

/* convert into <milliseconds>-<nanoseconds> format for series streaming */
const char *
timeval_stream_str(struct timeval *stamp, char *buffer, int buflen)
{
    __uint64_t	millipart;
    __uint64_t	nanopart;
    __uint64_t	crossover = stamp->tv_usec / 1000;

    millipart = stamp->tv_sec * 1000;
    millipart += crossover;
    nanopart = stamp->tv_usec * 1000;
    nanopart -= crossover;

    pmsprintf(buffer, buflen, "%" FMT_UINT64 "-%"FMT_UINT64,
		(__uint64_t)millipart, (__uint64_t)nanopart);
    return buffer;
}

/* convert timeval into human readable date/time format for logging */
const char *
timeval_str(struct timeval *tvp, char *buffer, int buflen)
{
    struct tm	tmp;
    time_t	now = (time_t)tvp->tv_sec;

    pmLocaltime(&now, &tmp);
    pmsprintf(buffer, sizeof(buflen), "%02u:%02u:%02u.%06u",
	      tmp.tm_hour, tmp.tm_min, tmp.tm_sec, (unsigned int)tvp->tv_usec);
    return buffer;
}

/* convert into <milliseconds>-<nanoseconds> format for series streaming */
const char *
timespec_stream_str(pmTimespec *stamp, char *buffer, int buflen)
{
    pmsprintf(buffer, buflen, "%" FMT_UINT64 "-%"FMT_UINT64,
		(__uint64_t)stamp->tv_sec, (__uint64_t)stamp->tv_nsec);
    return buffer;
}

/* convert timespec into human readable date/time format for logging */
const char *
timespec_str(pmTimespec *tsp, char *buffer, int buflen)
{
    struct tm	tmp;
    time_t	now = (time_t)tsp->tv_sec;

    pmLocaltime(&now, &tmp);
    pmsprintf(buffer, buflen, "%02u:%02u:%02u.%09u",
	      tmp.tm_hour, tmp.tm_min, tmp.tm_sec, (unsigned int)tsp->tv_nsec);
    return buffer;
}

const char *
indom_str(metric_t *metric)
{
    if (metric->desc.indom != PM_INDOM_NULL)
	return pmInDomStr(metric->desc.indom);
    return "none";
}

const char *
pmid_str(metric_t *metric)
{
    if (metric->desc.pmid != PM_ID_NULL)
	return pmIDStr(metric->desc.pmid);
    return "none";
}

const char *
semantics_str(metric_t *metric)
{
    return pmSemStr(metric->desc.sem);
}

const char *
units_str(metric_t *metric)
{
    const char	*units;

    units = pmUnitsStr(&metric->desc.units);
    if (units && units[0] != '\0')
	return units;
    return "none";
}

const char *
type_str(metric_t *metric)
{
    static const char * const typename[] = {
	"32", "u32", "64", "u64", "float", "double", "string",
	"aggregate", "aggregate_static", "event", "highres_event"
    };
    static char	typebuf[32];
    int		type = metric->desc.type;

    if (type >= 0 && type < sizeof(typename) / sizeof(typename[0]))
	pmsprintf(typebuf, sizeof(typebuf), "%s", typename[type]);
    else if (type == PM_TYPE_NOSUPPORT)
	pmsprintf(typebuf, sizeof(typebuf), "unsupported");
    else
	pmsprintf(typebuf, sizeof(typebuf), "unknown");
    return typebuf;
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

    if (context->labels)
	sets[nsets++] = context->labels;
    if (domain->labels)
	sets[nsets++] = domain->labels;
    if (indom && indom->labels)
	sets[nsets++] = indom->labels;
    if (cluster->labels)
	sets[nsets++] = cluster->labels;
    if (metric->labels)
	sets[nsets++] = metric->labels;

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

    if (context->labels)
	sets[nsets++] = context->labels;
    if (domain->labels)
	sets[nsets++] = domain->labels;
    if (indom->labels)
	sets[nsets++] = indom->labels;
    if (inst->labels)
	sets[nsets++] = inst->labels;

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
    pmLabelSet	**set = &c->labels;
    char	host[MAXHOSTNAMELEN];
    int		sts;

    if ((pmGetContextHostName_r(c->context, host, sizeof(host))) == NULL)
	return PM_ERR_GENERIC;
    c->host = sdsnew(host);
    if ((sts = pmGetContextLabels(set)) <= 0 && default_labelset(c, set) < 0)
	return sts;
    return pmMergeLabelSets(set, 1, buffer, length, labels, NULL);
}

static int
context_labels_str(context_t *c, char *buffer, int length)
{
    return pmMergeLabelSets(&c->labels, 1, buffer, length, labels, NULL);
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
pmwebapi_hash_str(const unsigned char *p)
{
    static char	namebuf[40+1];

    return hash_identity(p, namebuf, sizeof(namebuf));
}

sds
pmwebapi_hash_sds(const unsigned char *p)
{
    char	namebuf[40+1];

    hash_identity(p, namebuf, sizeof(namebuf));
    return sdsnewlen(namebuf, sizeof(namebuf));
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
source_hash(context_t *context)
{
    char		labels[PM_MAXLABELJSONLEN];
    int			sts;

    if ((sts = context_labels_str(context, labels, sizeof(labels))) < 0)
	return sts;
    return pmwebapi_source_hash(context->name.hash, labels, strlen(labels));
}

void
metric_hash(metric_t *metric)
{
    SHA1_CTX		shactx;
    pmDesc		*desc = &metric->desc;
    sds			identifier, labelset;
    char		buf[PM_MAXLABELJSONLEN];
    char		sem[32], type[32], units[64];
    int			len, i;

    len = metric_labelsets(metric, buf, sizeof(buf), labels, NULL);
    if (len <= 0)
	len = pmsprintf(buf, sizeof(buf), "null");

    identifier = sdsempty();
    labelset = sdsnewlen(buf, len);

    for (i = 0; i < metric->numnames; i++) {
	identifier = sdscatfmt(identifier,
		"{\"series\":\"metric\",\"name\":\"%S\",\"labels\":%S,"
		 "\"semantics\":\"%s\",\"type\":\"%s\",\"units\":\"%s\"}",
		metric->names[i].sds, labelset,
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
    sdsfree(labelset);
}

void
instance_hash(indom_t *ip, instance_t *instance)
{
    SHA1_CTX		shactx;
    sds			identifier, labelset;
    char		buf[PM_MAXLABELJSONLEN];
    int			len;

    len = instance_labelsets(ip, instance, buf, sizeof(buf), labels, NULL);
    if (len <= 0)
	len = pmsprintf(buf, sizeof(buf), "null");

    labelset = sdsnewlen(buf, len);
    identifier = sdscatfmt(sdsempty(),
		"{\"series\":\"instance\",\"name\":\"%S\",\"labels\":%S}",
		instance->name.sds, labelset);
    /* Calculate unique instance identifier 20-byte SHA1 hash */
    SHA1Init(&shactx);
    SHA1Update(&shactx, (unsigned char *)identifier, sdslen(identifier));
    SHA1Final(instance->name.hash, &shactx);
    sdsfree(identifier);
    sdsfree(labelset);
}
