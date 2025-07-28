/*
 * Copyright (c) 2025 Red Hat.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */
#include "opentelemetry.h"
#include "libpcp.h"
#include "util.h"

static void
labelname(const pmLabel *lp, const char *json, __pmHashCtl *lc,
		const char **name, int *length)
{
    __pmHashNode	*hp;

    if (!(lp->flags & PM_LABEL_COMPOUND) || lc == NULL ||
	(hp = __pmHashSearch(lp->name, lc)) == NULL) {
	*name = json + lp->name;
	*length = lp->namelen;
    } else {
	*name = hp->data;       /* compound "a.b.c" style label name */
	*length = strlen(hp->data);
    }
}

static void
labelvalue(const pmLabel *lp, const char *json, const char **value, int *length)
{
    const char		*offset;
    int			bytes;

    bytes = lp->valuelen;
    offset = json + lp->value;
    if (*offset == '\"' && bytes >= 2 && offset[bytes-1] == '\"') {
	bytes -= 2;
	offset++;
    }
    *value = offset;
    *length = bytes;
}

static void
labeladd(void *arg, const struct dictEntry *entry)
{
    sds			*buffer = (sds *)arg;
    sds			value = entry->v.val;
    const char		*type;

    if (value[0] == '"')
	type = "stringValue";
    else if (strchr(value, '.'))
	type = "doubleValue";
    else
	type = "intValue";

    if (*buffer)
	*buffer = sdscatfmt(*buffer,
				",{\"key\":\"%S\",\"value\":{\"%s\":%S}}",
				entry->key, type, value);
    else /* first label: alloc empty start string, and no leading comma */
	*buffer = sdscatfmt(sdsempty(),
				"{\"key\":\"%S\",\"value\":{\"%s\":%S}}",
				entry->key, type, value);
}

/* convert an array of PCP labelsets into Open Telemetry form */
void
open_telemetry_labels(pmWebLabelSet *labels, struct dict **context, sds *buffer)
{
    unsigned long	cursor = 0;
    pmLabelSet		*labelset;
    pmLabel		*label;
    dictEntry		*entry;
    const char		*offset;
    static sds		instname, instid;
    struct dict		*labeldict = *context;
    struct dict		*metric_labels;
    sds			key, value;
    int			i, j, length;

    if (instname == NULL)
	instname = sdsnewlen("instname", 8);
    if (instid == NULL)
	instid = sdsnewlen("instid", 6);

    /* setup resource attributues based on the PCP context labels */
    if (labeldict == NULL) {
	labeldict = dictCreate(&sdsOwnDictCallBacks, NULL);
	labelset = labels->sets[0];	/* context labels */
	for (j = 0; j < labelset->nlabels; j++) {
	    label = &labelset->labels[j];

	    /* extract the label name */
	    labelname(label, labelset->json, labelset->hash, &offset, &length);
	    key = sdsnewlen(offset, length);

	    /* extract the label value without any surrounding quotes */
	    labelvalue(label, labelset->json, &offset, &length);
	    value = sdscatrepr(sdsempty(), offset, length);

	    dictAdd(labeldict, key, value);	/* new entry */
	}
	*context = labeldict;
    }

    metric_labels = dictCreate(&sdsOwnDictCallBacks, NULL);

    /* walk remaining labelsets in order adding labels to */
    for (i = 1; i < labels->nsets; i++) {
	labelset = labels->sets[i];
	for (j = 0; j < labelset->nlabels; j++) {
	    label = &labelset->labels[j];

	    /* extract the label name */
	    labelname(label, labelset->json, labelset->hash, &offset, &length);
	    key = sdsnewlen(offset, length);

	    /* extract the label value without any surrounding quotes */
	    labelvalue(label, labelset->json, &offset, &length);
	    value = sdscatrepr(sdsempty(), offset, length);

	    /* overwrite entries from earlier passes: label hierarchy */
	    if ((entry = dictFind(metric_labels, key)) == NULL) {
		dictAdd(metric_labels, key, value);	/* new entry */
	    } else {
		sdsfree(key);
		sdsfree(dictGetVal(entry));
		dictSetVal(metric_labels, entry, value);
	    }
	}
    }

    /* if an instance with instname or instid labels missing, add them now */
    if (labels->instname && dictFind(metric_labels, instname) == NULL) {
	key = sdsdup(instname);
	value = labels->instname;
	value = sdscatrepr(sdsempty(), value, sdslen(value));
	dictAdd(metric_labels, key, value);	/* new entry */
    }
    if (labels->instid != PM_IN_NULL && dictFind(metric_labels, instid) == NULL) {
	key = sdsdup(instid);
	value = sdscatfmt(sdsempty(), "%u", labels->instid);
	dictAdd(metric_labels, key, value);	/* new entry */
    }

    /* finally produce the merged set of labels in the desired format */
    sdsfree(*buffer);
    *buffer = NULL;
    do {
	cursor = dictScan(metric_labels, cursor, labeladd, NULL, buffer);
    } while (cursor);
    dictRelease(metric_labels);
}

/* convert PCP metric name to Open Telemetry form */
sds
open_telemetry_name(sds metric)
{
    sds		p, name = sdsdup(metric);

    /* swap dots with underscores in name */
    for (p = name; p && *p; p++)
	if (*p == '.')
	    *p = '_';
    return name;
}

/* convert PCP metric semantics to Open Telemetry form */
const char *
open_telemetry_semantics(sds sem)
{
    if (strncmp(sem, "instant", 7) == 0 || strncmp(sem, "discrete", 8) == 0)
	return "gauge";
    return "sum";
}

/* convert PCP metric type has valid Open Telemetry form */
const char *
open_telemetry_type(sds type)
{
    static const char * const	asInt[] = { "u64", "64", "u32", "32" };
    static const char * const	asDouble[] = { "double", "float" };
    int		i;

    for (i = 0; i < sizeof(asDouble) / sizeof(asDouble[0]); i++)
	if (strcmp(type, asDouble[i]) == 0)
	    return "asDouble";
    for (i = 0; i < sizeof(asInt) / sizeof(asInt[0]); i++)
	if (strcmp(type, asInt[i]) == 0)
	    return "asInt";
    return NULL;
}

/*
 * Mapping Unified Code for Units of Measure (UCUM) to PCP pmUnits.
 * OpenTelemetry mandates the specification of units in UCUM form.
 */

/* Space unit prefixes (powers of 1024 for "binary" units) */
static const char *
UCUM_space_prefix(unsigned int scale)
{
    /* UCUM uses "Ki", "Mi", "Gi" for powers of 1024 */
    switch (scale) {
    case PM_SPACE_BYTE:  return "";   /* base bytes */
    case PM_SPACE_KBYTE: return "Ki"; /* Kibibyte */
    case PM_SPACE_MBYTE: return "Mi"; /* Mebibyte */
    case PM_SPACE_GBYTE: return "Gi"; /* Gibibyte */
    case PM_SPACE_TBYTE: return "Ti"; /* Tebibyte */
    case PM_SPACE_PBYTE: return "Pi"; /* Pebibyte */
    case PM_SPACE_EBYTE: return "Ei"; /* Exbibyte */
    case PM_SPACE_ZBYTE: return "Zi"; /* Zebibyte */
    case PM_SPACE_YBYTE: return "Yi"; /* Yobibyte */
    default:	/* should never happen */
	break;
    }
    return NULL;
}

/* Time unit prefixes */
static const char *
UCUM_time_prefix(unsigned int scale)
{
    switch (scale) {
    case PM_TIME_NSEC: return "n";  /* nano */
    case PM_TIME_USEC: return "u";  /* micro */
    case PM_TIME_MSEC: return "m";  /* milli */
    case PM_TIME_SEC:  return "";   /* base second */
    case PM_TIME_MIN:  return "min"; /* minute */
    case PM_TIME_HOUR: return "h";   /* hour */
    default:	/* should never happen */
	break;
    }
    return NULL;
}

/* Count unit prefixes (powers of 10) */
static const char*
UCUM_count_prefix(signed int scale)
{
    switch (scale) {
    case -24: return "y"; /* yocto */
    case -21: return "z"; /* zepto */
    case -18: return "a"; /* atto */
    case -15: return "f"; /* femto */
    case -12: return "p"; /* pico */
    case -9:  return "n"; /* nano */
    case -6:  return "u"; /* micro */
    case -3:  return "m"; /* milli */
    case 0:   return "";  /* no prefix (PM_COUNT_ONE) */
    case 3:   return "k"; /* kilo */
    case 6:   return "M"; /* mega */
    case 9:   return "G"; /* giga */
    case 12:  return "T"; /* tera */
    case 15:  return "P"; /* peta */
    case 18:  return "E"; /* exa */
    case 21:  return "Z"; /* zetta */
    case 24:  return "Y"; /* yotta */
    default:
	break;
    }
    return NULL;
}

/*
 * Convert pmUnits struct to UCUM string using a user-supplied bytes buffer
 * of at least 42 bytes.
 *
 * Maximum reasonable length for a UCUM string:
 * e.g.  'YiBy.h-3.{count}6'
 * Prefixes (2-3 chars), base units (2-4 chars), exponents (2-3 chars),
 *	 and multiplicating dot '.' (1 char)
 * So, 3 components * (3+4+3+1) + null terminator = ~30-40 chars.
 */
static char *
pmUnits_to_UCUM(pmUnits pmunits, char *ucum_string, size_t bytes)
{
    const char*	prefix = UCUM_space_prefix(pmunits.scaleSpace);
    char	component[32]; /* temporary buffer for each component */
    int		first_unit = 1;

    if (bytes < 42)
	return NULL;

    ucum_string[0] = '\0';

    /* Process the space dimension */
    if (pmunits.dimSpace != 0) {
	if (!first_unit)
	    strcat(ucum_string, "."); /* multiplication dot */
	first_unit = 0;

	prefix = UCUM_space_prefix(pmunits.scaleSpace);
	snprintf(component, sizeof(component), "%sBy", prefix);

	strcat(ucum_string, component);
	if (pmunits.dimSpace != 1) { /* add exponent if not 1 */
	    snprintf(component, sizeof(component), "%d", pmunits.dimSpace);
	    strcat(ucum_string, component);
	}
    }

    /* Process the time dimension */
    if (pmunits.dimTime != 0) {
	if (!first_unit)
	    strcat(ucum_string, "."); /* multiplication dot */
	first_unit = 0;

	prefix = UCUM_time_prefix(pmunits.scaleTime);
	/*
	 * UCUM base time units: s, min, h (seconds, minutes, hours),
	 * and n, u, m (nano, micro, milli) are standard s prefixes.
	 */
	if (pmunits.scaleTime <= PM_TIME_SEC)
	    snprintf(component, sizeof(component), "%ss", prefix);
	else /* min, h (minutes, hours) are standalone UCUM units */
	    snprintf(component, sizeof(component), "%s", prefix);
	strcat(ucum_string, component);
	if (pmunits.dimTime != 1) { /* add exponent if not 1 */
	    snprintf(component, sizeof(component), "%d", pmunits.dimTime);
	    strcat(ucum_string, component);
	}
    }

    /* Process the count dimension */
    if (pmunits.dimCount != 0) {
	if (!first_unit)
	    strcat(ucum_string, "."); /* multiplication dot */
	first_unit = 0;

	prefix = UCUM_count_prefix(pmunits.scaleCount);
	/*
	 * For count, UCUM can use "{count}" or similar annotations for base,
	 * then standard prefixes if applicable (e.g. "M{count}" for a million).
	 * As scaleCount specifies powers of 10, apply the prefix to "{count}".
	 * For powers of 10 that aren't in the standard prefixes: "10^X".
	 */
	if (prefix == NULL)
	    snprintf(component, sizeof(component), "10^%u", pmunits.scaleCount);
	else
	     snprintf(component, sizeof(component), "%s{count}", prefix);

	strcat(ucum_string, component);
	if (pmunits.dimCount != 1) { /* add exponent if not 1 */
	    snprintf(component, sizeof(component), "%d", pmunits.dimCount);
	    strcat(ucum_string, component);
	}
    }

    /* Finally, handle the dimensionless case */
    if (first_unit)
	strcpy(ucum_string, "1"); /* UCUM standard for dimensionless */

    return ucum_string;
}

/* convert PCP metric units to Open Telemetry form */
sds
open_telemetry_units(sds units)
{
    pmUnits	pmunits;
    double	unused;
    char	*error;
    char	buffer[64];

    if (pmParseUnitsStr(units, &pmunits, &unused, &error) == 0)
	return sdsnew(pmUnits_to_UCUM(pmunits, buffer, sizeof(buffer)));
    free(error);
    return sdsnew("1");
}
