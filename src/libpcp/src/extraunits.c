/*
 * Data-driven implementation of "extra" units for metadata.
 *
 * Copyright (c) 2026 Ken McDonell.  All Rights Reserved.
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
#include "internal.h"
#include <ctype.h>

typedef struct {
    int		ident;		/* one of the PM_<unit>_* values */
    char	*text;		/* per-scale text to be inserted in unit description */
} scale_map_t;

typedef struct {
    int			type;		/* one of the PM_UNIT_* values */
    char		*name;		/* unit name */
    int			nscale;		/* number of scales */
    const scale_map_t	*scale;		/* scales map */
} unit_t;

const scale_map_t	temperature_scale[] = {
    { PM_TEMPERATURE_C,	"C" },
    { PM_TEMPERATURE_F,	"F" },
    { PM_TEMPERATURE_K,	"K" },
};

const scale_map_t	voltage_scale[] = {
    { PM_VOLTAGE_V,	"V" },
    { PM_VOLTAGE_mV,	"mV" },
    { PM_VOLTAGE_uV,	"uV" },
};

const scale_map_t	current_scale[] = {
    { PM_CURRENT_A,	"A" },
    { PM_CURRENT_mA,	"mA" },
    { PM_CURRENT_uA,	"uA" },
};

const scale_map_t	power_scale[] = {
    { PM_POWER_kW,	"kW" },
    { PM_POWER_W,	"W" },
    { PM_POWER_mW,	"mW" },
    { PM_POWER_uW,	"uW" },
};

static const unit_t extra[] = {
    { PM_UNIT_TEMPERATURE, "temperature", sizeof(temperature_scale)/sizeof(temperature_scale[0]), temperature_scale },
    { PM_UNIT_VOLTAGE, "voltage", sizeof(voltage_scale)/sizeof(voltage_scale[0]), voltage_scale },
    { PM_UNIT_CURRENT, "current", sizeof(current_scale)/sizeof(current_scale[0]), current_scale },
    { PM_UNIT_POWER, "power", sizeof(power_scale)/sizeof(power_scale[0]), power_scale },
};

static const int nextra = sizeof(extra)/sizeof(extra[0]);

/*
 * extraUnit + extraScale => string for printing
 *
 * On entry, pu->extraUnits != 0
 */
void
__pmExtraUnitsStr(const pmUnits *pu, char *buf, size_t buflen)
{
    int		u;	/* index into extra[] */
    int		s;	/* scale selector */

    for (u = 0; u < nextra; u++) {
	if (pu->extraUnit == extra[u].type ||
	    -pu->extraUnit == extra[u].type)
	    break;
    }
    if (u == nextra) {
	/* unknown extraUnit */
	pmsprintf(buf, buflen, "extra-%d", pu->extraUnit);
	return;
    }
    for (s = 0; s < extra[u].nscale; s++) {
	if (pu->extraScale == extra[u].scale[s].ident) {
	    pmsprintf(buf, buflen, "%s (%s)", extra[u].name, extra[u].scale[s].text);
	    return;
	}
    }
    /* unknown extraScale */
    pmsprintf(buf, buflen, "%s (scale-%d)", extra[u].name, pu->extraScale);
    return;
}

/*
 * parse units string to match extraUnit + extrScale encoding from
 * __pmExtraUnitsStr()
 *
 * return pointer updated to past end of parse in case of a match
 */
const char *
__pmParseExtraUnits(const char *buf, __pmUnits *pu)
{
    const char	*ptr = buf;
    int		u;	/* index into extra[] */
    int		s;	/* scale selector */

    for (u = 0; u < nextra; u++) {
	if (strncasecmp(ptr, extra[u].name, strlen(extra[u].name)) == 0) {
	    /* matched name */
	    ptr += strlen(extra[u].name);
	    /* check for " (" separator */
	    if (*ptr != ' ')
		continue;
	    ptr++;
	    if (*ptr != '(')
		continue;
	    ptr++;
	    for (s = 0; s < extra[u].nscale; s++) {
		if (strncasecmp(ptr, extra[u].scale[s].text, strlen(extra[u].scale[s].text)) == 0) {
		    /* matched scale */
		    if (ptr[strlen(extra[u].scale[s].text)] == '\0')
			continue;
		    ptr += strlen(extra[u].scale[s].text);
		    if (*ptr == ')') {
			/* matched trailing ) */
			ptr++;
			pu->extraUnit = extra[u].type;
			pu->extraScale = extra[u].scale[s].ident;
			/* and gobble any trailing space(s) */
			while (*ptr && !isspace(*ptr))
			    ptr++;
			return ptr;
		    }
		    else {
			ptr -= strlen(extra[u].scale[s].text);
		    }
		}
	    }
	}
    }

    return buf;
}

static int
append(char **buf, size_t *buflen, char *preamble, char *new)
{
    char	*buf_tmp;

    if (*buflen == 0) {
	if ((buf_tmp = (char *)malloc(1)) == NULL)
	    return -1;
	*buf = buf_tmp;
	**buf = '\0';
	*buflen = 1;
    }
    if (preamble != NULL)
	*buflen += strlen(preamble);
    *buflen += strlen(new);
    if ((buf_tmp = (char *)realloc(*buf, *buflen)) == NULL)
	return -1;
    *buf = buf_tmp;
    if (preamble != NULL)
	pmstrncat(*buf, *buflen, preamble);
    pmstrncat(*buf, *buflen, new);

    return 0;
}

/*
 * Check the integrity of a pmDesc
 *
 * Returns
 *   0 if OK else
 *   1 if most serious issue is a warning
 *   2 if most serious issue is an error
 *   < 0 for fatal error (most likely NOMEM)
 *
 * If an issue is found then errmsg also returns one or more lines of
 * explanation messages (each terminated by a newline) and the caller
 * must call free() to release the allocation for the message(s).
 * If preamble is not NULL then this text is prefixed before each
 * message line.
 */
int __pmCheckDesc(pmDesc *dp, char *preamble, char **errmsg)
{
    int			sts = 0;
    char		*err = NULL;
    size_t		errlen = 0;
    char		buf[MAXPATHLEN];
    static int		known_domain[512];
    static int		onetrip = 0;	/* state of known_domain[] */

    if (onetrip == 0) {
	/* load known_domain[] from $PCP_VAR_DIR/pmns/stdpmid */
	int	sep = pmPathSeparator();
	FILE	*f;
	char	*p;

	pmsprintf(buf, sizeof(buf), "%s%cpmns%cstdpmid", pmGetConfig("PCP_VAR_DIR"), sep, sep);
	if ((f = fopen(buf, "r")) == NULL) {
	    fprintf(stderr, "__pmCheckDesc: fopen(\"%s\", ...) failed: domain checks skipped\n", buf);
	    onetrip = -1;
	}
	else {
	    int		domain;
	    onetrip = 1;
	    while (fgets(buf, sizeof(buf), f) != NULL) {
		if (strncmp(buf, "#define ", 8) != 0)
		    continue;
		p = buf;
		/* #define */
		while (*p && !isspace(*p))
		    p++;
		/* white space */
		while (*p && isspace(*p))
		    p++;
		/* domain name */
		while (*p && !isspace(*p))
		    p++;
		/* white space */
		while (*p && isspace(*p))
		    p++;
		domain = atoi(p);
		if (domain > 0 && domain < 511)
		    known_domain[domain] = 1;
	    }
	    fclose(f);
	    known_domain[DYNAMIC_PMID] = 1;	/* dynamic metrics */
	}
    }

    if (onetrip == 1) {
	if (known_domain[pmID_domain(dp->pmid)] == 0) {
	    pmsprintf(buf, sizeof(buf), "Warning: domain (%d) in metric pmid (%s) is not recognized\n", pmID_domain(dp->pmid), pmIDStr(dp->pmid));
	    if (append(&err, &errlen, preamble, buf) < 0) {
		sts = -ENOMEM;
		goto done;
	    }
	    if (sts != 2)
		sts = 1;
	}
    }

    /* if PM_TYPE_NOSUPPORT => no other metadata can be checked */
    if (dp->type == PM_TYPE_NOSUPPORT)
	goto done;

    if (dp->indom != PM_INDOM_NULL) {
	if (onetrip == 1) {
	    if (known_domain[pmInDom_domain(dp->indom)] == 0) {
		pmsprintf(buf, sizeof(buf), "Warning: domain (%d) in metric indom (%s) is not recognized\n", pmInDom_domain(dp->indom), pmInDomStr(dp->indom));
		if (append(&err, &errlen, preamble, buf) < 0) {
		    sts = -ENOMEM;
		    goto done;
		}
		if (sts != 2)
		    sts = 1;
	    }
	}
	if (pmID_domain(dp->pmid) != DYNAMIC_PMID &&
	    pmID_domain(dp->pmid) != pmInDom_domain(dp->indom)) {
	    pmsprintf(buf, sizeof(buf), "Warning: domain (%d) in metric pmid (%s) different to domain (%d) in metric indom (%s)\n", pmID_domain(dp->pmid), pmIDStr(dp->pmid), pmInDom_domain(dp->indom), pmInDomStr(dp->indom));
	    if (append(&err, &errlen, preamble, buf) < 0) {
		sts = -ENOMEM;
		goto done;
	    }
	    if (sts != 2)
		sts = 1;
	}
    }

    if (dp->type < PM_TYPE_32 || dp->type > PM_TYPE_HIGHRES_EVENT) {
	pmsprintf(buf, sizeof(buf), "Error: metric type (%d) is not one of the valid PM_TYPE_* values\n", dp->type);
	if (append(&err, &errlen, preamble, buf) < 0) {
	    sts = -ENOMEM;
	    goto done;
	}
	sts = 2;
    }

    if (dp->sem != PM_SEM_COUNTER && dp->sem != PM_SEM_INSTANT && dp->sem != PM_SEM_DISCRETE) {
	pmsprintf(buf, sizeof(buf), "Error: metric semantics (%d) is not one of the valid PM_SEM_* values\n", dp->sem);
	if (append(&err, &errlen, preamble, buf) < 0) {
	    sts = -ENOMEM;
	    goto done;
	}
	sts = 2;
    }

    /*
     * Heuristic ... for regular units, dimension should really be
     * in the range -2,2 (inclusive)
     */
    if (dp->units.dimSpace < -2 || dp->units.dimSpace > 2) {
	pmsprintf(buf, sizeof(buf), "Warning: unusual dimension (%d) for Space in pmUnits\n", dp->units.dimSpace);
	if (append(&err, &errlen, preamble, buf) < 0) {
	    sts = -ENOMEM;
	    goto done;
	}
	if (sts != 2)
	    sts = 1;
    }
    if (dp->units.dimTime < -2 || dp->units.dimTime > 2) {
	pmsprintf(buf, sizeof(buf), "Warning: unusual dimension (%d) for Time in pmUnits\n", dp->units.dimTime);
	if (append(&err, &errlen, preamble, buf) < 0) {
	    sts = -ENOMEM;
	    goto done;
	}
	if (sts != 2)
	    sts = 1;
    }
    if (dp->units.dimCount < -2 || dp->units.dimCount > 2) {
	pmsprintf(buf, sizeof(buf), "Warning: unusual dimension (%d) for Count in pmUnits\n", dp->units.dimCount);
	if (append(&err, &errlen, preamble, buf) < 0) {
	    sts = -ENOMEM;
	    goto done;
	}
	if (sts != 2)
	    sts = 1;
    }

    /*
     * only Space and Time have sensible upper bounds, but if dimension
     * is 0, scale should also be 0
     */
    if (dp->units.dimSpace == 0 && dp->units.scaleSpace != 0) {
	pmsprintf(buf, sizeof(buf), "Error: non-zero scale (%d) with zero dimension for Space in pmUnits\n", dp->units.scaleSpace);
	if (append(&err, &errlen, preamble, buf) < 0) {
	    sts = -ENOMEM;
	    goto done;
	}
	sts = 2;
    }
    if (dp->units.scaleSpace > PM_SPACE_EBYTE) {
	pmsprintf(buf, sizeof(buf), "Error: scale (%d) for Space in pmUnits is not one of the valid PM_SPACE_* values\n", dp->units.scaleSpace);
	if (append(&err, &errlen, preamble, buf) < 0) {
	    sts = -ENOMEM;
	    goto done;
	}
	sts = 2;
    }
    if (dp->units.dimTime == 0 && dp->units.scaleTime != 0) {
	pmsprintf(buf, sizeof(buf), "Error: non-zero scale (%d) with zero dimension for Time in pmUnits\n", dp->units.scaleTime);
	if (append(&err, &errlen, preamble, buf) < 0) {
	    sts = -ENOMEM;
	    goto done;
	}
	sts = 2;
    }
    if (dp->units.scaleTime > PM_TIME_HOUR) {
	pmsprintf(buf, sizeof(buf), "Error: scale (%d) for Time in pmUnits is not one of the valid PM_SPACE_* values\n", dp->units.scaleTime);
	if (append(&err, &errlen, preamble, buf) < 0) {
	    sts = -ENOMEM;
	    goto done;
	}
	sts = 2;
    }
    if (dp->units.dimCount == 0 && dp->units.scaleCount != 0) {
	pmsprintf(buf, sizeof(buf), "Error: non-zero scale (%d) with zero dimension for Count in pmUnits\n", dp->units.scaleCount);
	if (append(&err, &errlen, preamble, buf) < 0) {
	    sts = -ENOMEM;
	    goto done;
	}
	sts = 2;
    }

    if (dp->units.extraUnit != 0) {
	int		u;	/* index into extra[] */
	int		s;	/* scale selector */

	for (u = 0; u < nextra; u++) {
	    if (dp->units.extraUnit == extra[u].type)
		break;
	}
	if (u < nextra) {
	    for (s = 0; s < extra[u].nscale; s++) {
		if (dp->units.extraScale == extra[u].scale[s].ident)
		    break;
	    }
	    if (s == extra[u].nscale) {
		char	macro[20];	/* 12 is enough for temperature\0 */
		char	*p, *q;
		for (q = extra[u].name, p = macro; *q; )
		    *p++ = toupper(*q++);
		*p = '\0';
		pmsprintf(buf, sizeof(buf), "Error: extraScale (%d) in pmUnits is not one of the valid PM_%s_* values\n", dp->units.extraScale, macro);
		if (append(&err, &errlen, preamble, buf) < 0) {
		    sts = -ENOMEM;
		    goto done;
		}
		sts = 2;
	    }
	}
	else {
	    pmsprintf(buf, sizeof(buf), "Error: extraUnit (%d) in pmUnits is not one of the valid PM_UNIT_* values\n", dp->units.extraUnit);
	    if (append(&err, &errlen, preamble, buf) < 0) {
		sts = -ENOMEM;
		goto done;
	    }
	    sts = 2;
	}
    }

done:
    if (err != NULL)
	*errmsg = err;
    return sts;
}

/*
 * map extra unit name -> type (PM_UNIT_*)
 */
int
__pmLookupExtraUnit(const char *name)
{
    int		u;	/* index into extra[] */

    for (u = 0; u < nextra; u++) {
	if (strcasecmp(name, extra[u].name) == 0) {
	    return extra[u].type;
	}
    }
    return -1;
}

/*
 * for extra unit type map text -> ident
 */
int
__pmLookupExtraScale(int type, const char *text)
{
    int		u;	/* index into extra[] */
    int		s;	/* scale selector */

    for (u = 0; u < nextra; u++) {
	if (type == extra[u].type) {
	    for (s = 0; s < extra[u].nscale; s++) {
		if (strcasecmp(text, extra[u].scale[s].text) == 0) {
		    return extra[u].scale[s].ident;
		}
	    }
	    return -1;
	}
    }
    return -1;
}

/*
 * Conversion for extra units is table driven using the generic
 * formula: out = a + b * (in + c) / d
 *
 * There is one table for each extra unit type.  Each table is terminated
 * with a iscale == -1 sentinal.
 */

typedef struct {
    int		iscale;
    int		oscale;
    double	a;
    double	b;
    double	c;
    double	d;
} convert_t;


static convert_t temp_conv[] = {
    						/* a */	/* b */	/* c */	/* d */
    { PM_TEMPERATURE_C,	PM_TEMPERATURE_F,	32,	18,	0,	10 },
    { PM_TEMPERATURE_F,	PM_TEMPERATURE_C,	0,	10,	-32,	18 },
    { PM_TEMPERATURE_C,	PM_TEMPERATURE_K,	273.15,	1,	0,	1 },
    { PM_TEMPERATURE_K,	PM_TEMPERATURE_C,	0,	1,	-273.15,1 },
    { PM_TEMPERATURE_F,	PM_TEMPERATURE_K,	273.15,	10,	-32,	18 },
    { PM_TEMPERATURE_K,	PM_TEMPERATURE_F,	32,	18,	-273.15,10 },
    { .iscale = -1 },
};

static convert_t volt_conv[] = {
    					/* a */	/* b */		/* c */	/* d */
    { PM_VOLTAGE_V,	PM_VOLTAGE_mV,	0,	1000,		0,	1 },
    { PM_VOLTAGE_V,	PM_VOLTAGE_uV,	0,	1000000,	0,	1 },
    { PM_VOLTAGE_mV,	PM_VOLTAGE_V,	0,	1,		0,	1000 },
    { PM_VOLTAGE_mV,	PM_VOLTAGE_uV,	0,	1000,		0,	1 },
    { PM_VOLTAGE_uV,	PM_VOLTAGE_V,	0,	1,		0,	1000000 },
    { PM_VOLTAGE_uV,	PM_VOLTAGE_mV,	0,	1,		0,	1000 },
    { .iscale = -1 },
};

/* out = a + b * (in + c) / d */
static convert_t curr_conv[] = {
    					/* a */	/* b */		/* c */	/* d */
    { PM_CURRENT_A,	PM_CURRENT_mA,	0,	1000,		0,	1 },
    { PM_CURRENT_A,	PM_CURRENT_uA,	0,	1000000,	0,	1 },
    { PM_CURRENT_mA,	PM_CURRENT_A,	0,	1,		0,	1000 },
    { PM_CURRENT_mA,	PM_CURRENT_uA,	0,	1000,		0,	1 },
    { PM_CURRENT_uA,	PM_CURRENT_A,	0,	1,		0,	1000000 },
    { PM_CURRENT_uA,	PM_CURRENT_mA,	0,	1,		0,	1000 },
    { .iscale = -1 },
};

static convert_t power_conv[] = {
    					/* a */	/* b */		/* c */	/* d */
    { PM_POWER_kW,	PM_POWER_W,	0,	1000,		0,	1 },
    { PM_POWER_kW,	PM_POWER_mW,	0,	1000000,	0,	1 },
    { PM_POWER_kW,	PM_POWER_uW,	0,	1000000000,	0,	1 },
    { PM_POWER_W,	PM_POWER_kW,	0,	1,		0,	1000 },
    { PM_POWER_W,	PM_POWER_mW,	0,	1000,		0,	1 },
    { PM_POWER_W,	PM_POWER_uW,	0,	1000000,	0,	1 },
    { PM_POWER_mW,	PM_POWER_kW,	0,	1,		0,	1000000 },
    { PM_POWER_mW,	PM_POWER_W,	0,	1,		0,	1000 },
    { PM_POWER_mW,	PM_POWER_uW,	0,	1000,		0,	1 },
    { PM_POWER_uW,	PM_POWER_kW,	0,	1,		0,	1000000000 },
    { PM_POWER_uW,	PM_POWER_W,	0,	1,		0,	1000000 },
    { PM_POWER_uW,	PM_POWER_mW,	0,	1,		0,	1000 },
    { .iscale = -1 },
};

/*
 * Rescale extra units in place (val).
 * On entry, extraUnit is the same for iunit and ounit, so for each possible
 * extra unit it is the N x N cases for the scale options, captured in the
 * convert_t tables above.
 *
 * Return 0 if ok, else -1 for badness (no diagnostics at this point)
 */
int
__pmConvExtraScale(int type, pmAtomValue *val, const pmUnits *iunit, const pmUnits *ounit)
{
    convert_t	*cp;

    if (iunit->extraScale == ounit->extraScale)
	return 0;

    if (iunit->extraUnit == PM_UNIT_TEMPERATURE)
	cp = temp_conv;
    else if (iunit->extraUnit == PM_UNIT_VOLTAGE)
	cp = volt_conv;
    else if (iunit->extraUnit == PM_UNIT_CURRENT)
	cp = curr_conv;
    else if (iunit->extraUnit == PM_UNIT_POWER)
	cp = power_conv;
    else
	return PM_ERR_UNIT;

    for ( ; ; cp++) {
	if (cp->iscale == -1)
	    return PM_ERR_UNIT;
	if (cp->iscale == iunit->extraScale && cp->oscale == ounit->extraScale)
	    break;
    }

    switch (type) {
	case PM_TYPE_32:
	    val->l = (__int32_t)(cp->a + cp->b * (val->l + cp->c) / cp->d);
	    break;
	case PM_TYPE_U32:
	    val->ul = (__uint32_t)(cp->a + cp->b * (val->ul + cp->c) / cp->d);
	    break;
	case PM_TYPE_64:
	    val->ll = (__int64_t)(cp->a + cp->b * (val->ll + cp->c) / cp->d);
	    break;
	case PM_TYPE_U64:
	    val->ull = (__uint64_t)(cp->a + cp->b * (val->ull + cp->c) / cp->d);
	    break;
	case PM_TYPE_FLOAT:
	    val->f = (float)(cp->a + cp->b * (val->f + cp->c) / cp->d);
	    break;
	case PM_TYPE_DOUBLE:
	    val->d = (float)(cp->a + cp->b * (val->d + cp->c) / cp->d);
	    break;
	default:
	    return PM_ERR_CONV;
    }

    return 0;
}
