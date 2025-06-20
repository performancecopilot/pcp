/*
 * Copyright (c) 2020-2021 Red Hat.  All Rights Reserved.
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
 * Debug flags:
 * appl0	group file operations
 * appl1	probe operations
 */
#include "pmlogconf.h"

static pmLongOptions longopts[] = {
    PMOPT_DEBUG,
    PMOPT_HOST,
    { "", 0, 'c', NULL, "add message and timestamp (not for interactive use)" },
    { "group", 1, 'g', "TAG", "report the logging state for a specific metrics group" },
    { "groups", 1, 'd', "DIR", "specify path to the pmlogconf groups directory" },
    { "quiet", 0, 'q', NULL, "quiet, suppress logging interval dialog" },
    { "reprobe", 0, 'r', NULL, "every group reconsidered for inclusion in configfile" },
    { "setup", 1, 's', "FILE", "report default logging state for a group file" },
    { "verbose", 0, 'v', NULL, "increase diagnostic verbosity" },
    PMOPT_VERSION,
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static int
override(int opt, pmOptions *opts)
{
    return (opt == 'g' || opt == 's');
}

static pmOptions opts = {
    .short_options = "D:h:cd:g:qrs:vV?",
    .long_options = longopts,
    .short_usage = "[options] configfile",
    .override = override,
};

int	autocreate;		/* generate message+timestamp */
int	prompt = 1;		/* user interaction */
int	deltas = 1;		/* support interval setting */
int	quick;			/* quiet, no interval dialog */
int	reprobe;
int	rewrite;		/* pmlogger file is modified */
int	verbose;
int	existing;		/* new file or updating existing */
char	groupdir[MAXPATHLEN];	/* path to pmlogconf groups files */
char	*tmpconfig;		/* in-situ configuration file */
char	*trailer;		/* immutable trailer section */
char	*header;		/* immutable header section */
char	*config;		/* pmlogger configuration file */
char	*host;			/* pmcd host specification */
char	*setupfile;
char	*finaltag;

unsigned	ngroups;
group_t	*groups;

int
resize_groups(unsigned int extras)
{
    size_t		size = (ngroups + extras) * sizeof(group_t);
    group_t		*tmpgroups;

    if ((tmpgroups = realloc(groups, size)) == NULL)
	return -ENOMEM;
    memset(tmpgroups + ngroups, 0, extras * sizeof(group_t));
    groups = tmpgroups;
    return 0;
}

static int
group_compare(const void *p, const void *q)
{
    group_t		*pp = (group_t *)p;
    group_t		*qq = (group_t *)q;

    return strcmp(pp->tag, qq->tag);
}

group_t *
group_free(group_t *group)
{
    unsigned int	i;

    if (group->tag)
	free(group->tag);
    if (group->ident)
	free(group->ident);
    if (group->delta)
	free(group->delta);
    if (group->metric)
	free(group->metric);
    if (group->force)
	free(group->force);
    if (group->probe)
	free(group->probe);
    if (group->value)
	free(group->value);
    for (i = 0; i < group->nmetrics; i++)
	free(group->metrics[i]);
    if (group->metrics)
	free(group->metrics);
    if (group->saved_delta)
	free(group->saved_delta);
    memset(group, 0, sizeof(group_t));

    free(group);
    return NULL;
}

static group_t *
group_create_pmlogger(const char *state, unsigned int line)
{
    char		*errmsg;
    size_t		length;
    group_t		*group;
    const char		*begin, *end;
    struct timespec	interval;
    enum { TAG, STATE, DELTA } parse;

    if ((group = calloc(1, sizeof(group_t))) == NULL) {
	fprintf(stderr, "%s: cannot create group for %s: %s\n",
			pmGetProgname(), state, osstrerror());
	exit(EXIT_FAILURE);
    }
    group->pmlogger = 1;

    /*
     * pmlogger group control lines have this format
     * #+ tag:on-off:delta
     * where
     *       tag     is arbitrary (no embedded :'s) and unique
     *       on-off  y or n to enable or disable this group, else
     *               x for groups excluded by probing when the group was added
     *               to the configuration file
     *       delta   delta argument for pmlogger "logging ... on delta" clause
     */

    parse = TAG;
    begin = end = state;
    while (*end != '\n' && *end != '\0') {
	if (*end != ':') {
	    end++;
	    continue;
	}
	length = end - begin;
	switch (parse) {
	case TAG:
	    if (length > 0) {
		if ((group->tag = strndup(begin, length)) == NULL) {
		    fprintf(stderr, "%s: out-of-memory parsing %s at line %u\n",
			    pmGetProgname(), config, line);
		    exit(EXIT_FAILURE);
		}
	    } else {
		fprintf(stderr, "%s: missing tag for group at line %u of %s\n",
			pmGetProgname(), line, config);
		return group_free(group);
	    }
	    break;

	case STATE:
	    if (strncmp(begin, "y", length) == 0)
		group->saved_state = STATE_INCLUDE;
	    else if (strncmp(begin, "n", length) == 0)
		group->saved_state = STATE_AVAILABLE;
	    else if (strncmp(begin, "x", length) == 0)
		group->saved_state = STATE_EXCLUDE;
	    else {
		fprintf(stderr, "%s: missing state for %s at line %u of %s\n",
			pmGetProgname(), group->tag, line, config);
		return group_free(group);
	    }
	    break;

	case DELTA:
	    if (length == 0)
		break;
	    if ((group->saved_delta = strndup(begin, length)) == NULL) {
		fprintf(stderr, "%s: out-of-memory on %s delta line %u of %s\n",
			pmGetProgname(), group->tag, line, config);
		exit(EXIT_FAILURE);
	    }
	    if (strcmp(group->saved_delta, "once") == 0 ||
		strcmp(group->saved_delta, "default") == 0)
		break;
	    if (pmParseHighResInterval(group->saved_delta, &interval, &errmsg) < 0) {
		fprintf(stderr, "%s: ignoring %s logging interval \"%s\" "
				"on line %u of %s: %s\n", pmGetProgname(),
			group->tag, group->saved_delta, line, config, errmsg);
		free(errmsg);
		free(group->saved_delta);
		group->saved_delta = NULL;
	    }
	    break;

	default:
	    break;
	}
	begin = ++end;
	parse++;
    }
    if (group->tag != NULL)
	return group;
    return group_free(group);
}

void
group_ident(group_t *group, const char *bytes)
{
    /* trim and add this ident line to group */
    group->ident = append(group->ident, trim(bytes), ' ');
}

static void
group_metric(group_t *group, const char *bytes)
{
    size_t		length;
    char		**metrics;

    /* trim and add this metric line to group */
    length = (group->nmetrics + 1) * sizeof(char *);
    if ((metrics = realloc(group->metrics, length)) == NULL) {
	fprintf(stderr, "%s: out of memory for metrics in group %s\n",
			pmGetProgname(), group->tag);
	exit(EXIT_FAILURE);
    }
    group->metrics = metrics;
    group->metrics[group->nmetrics++] = copy_string(trim(bytes));
}

static group_t *
group_search(const char *tag, unsigned int *position)
{
    unsigned int	i;

    /*
     * Using the array index hint first, attempt to locate the group
     * identified by tag.  If not found forward of the hint, attempt
     * a complete array scan and either reset the hint or fail.  The
     * position is one after the last array slot where a successful
     * lookup occurred; as the array is sorted and the incoming tags
     * we are searching for are as well, its a good place to start.
     */
    for (i = *position; i < ngroups; i++)
	if (strcmp(tag, groups[i].tag) == 0) {
	    *position = i + 1;
	    return &groups[i];
	}
    /* head back to the start and try the first half of the array */
    for (i = 0; i <= *position && i < ngroups; i++)
	if (strcmp(tag, groups[i].tag) == 0) {
	    *position = i + 1;
	    return &groups[i];
	}
    return NULL;
}

static group_t *
group_insert(group_t *group)
{
    if (resize_groups(1) < 0) {
	fprintf(stderr, "%s: out-of-memory adding logged group %s\n",
		pmGetProgname(), group->tag);
	exit(EXIT_FAILURE);
    }
    group->valid = 1;
    groups[ngroups++] = *group;
    qsort(groups, ngroups, sizeof(group_t), group_compare);
    return NULL;	/* this group insert is now complete */
}

static group_t *
group_merge(group_t *found, group_t *group)
{
    found->valid = 1;
    found->logging = group->logging;
    found->saved_state = group->saved_state;

    if (found->saved_delta)
	free(found->saved_delta);
    found->saved_delta = group->saved_delta;
    group->saved_delta = NULL;	/* moved a string pointer */

    group_free(group);
    return NULL;	/* this group update is now complete */
}

group_t *
group_finish(group_t *group, unsigned int *hint)
{
    group_t		*found;

    /* add this group to the global group data structure for tracking */
    if ((found = group_search(group->tag, hint)) != NULL)
	return group_merge(found, group);

    /* group is in pmlogger config but no pmlogconf definition found  */
    if (autocreate) {
	fprintf(stderr, "%s: Warning: cannot find group file (%s): %s\n",
		pmGetProgname(), group->tag, "deleting obsolete group");
	group_free(group);
	return NULL;
    }
    if (existing)
	fprintf(stderr, "%s: Warning: cannot find group file (%s): %s\n",
		pmGetProgname(), group->tag, "no change is possible");
    return group_insert(group);
}

static group_t *
group_pattern(unsigned int *position, const char *pattern)
{
    unsigned int	i, m;
    group_t		*group;

    /* search for a pattern string in given group (array index hint) */
    for (i = *position; i < ngroups; i++) {
	group = &groups[i];
	/* search first in the group description */
	if (strstr(group->ident, pattern) != NULL) {
	    *position = i;
	    return group;
	}
	/* then search through the group metrics */
	for (m = 0; m < group->nmetrics; m++) {
	    if (strstr(group->metrics[m], pattern) == NULL)
		continue;
	    *position = i;
	    return group;
	}
    }
    return NULL;
}

static int
cullv1(const_dirent *dep)
{
    return strcmp("v1.0", dep->d_name);
}

int
parse_groupfile(FILE *file, const char *tag)
{
    struct group	group;
    unsigned int	major, minor;
    char		*p, bytes[1024];

    if (pmDebugOptions.appl0)
	fprintf(stderr, "Parsing group with tag %s\n", tag);

    /* read in first line, bail out early if not ours or unknown version */
    if (fscanf(file, "#pmlogconf-setup %u.%u\n", &major, &minor) != 2)
	return -EINVAL;
    if (major != 2 && minor != 0)
	return -ENOTSUP;

    memset(&group, 0, sizeof(struct group));
    if ((group.tag = strdup(tag)) == NULL) {
	fprintf(stderr, "%s: out-of-memory adding pmlogconf tag %s\n",
		pmGetProgname(), tag);
	exit(EXIT_FAILURE);
    }
    group.pmlogconf = 1;
    while ((p = fgets(bytes, sizeof(bytes), file)) != NULL) {
	if (*p == '#')	/* skip comment lines (keep embedded comments) */
	    continue;
	p = trim(p);
	if (*p == '\n')	/* skip any empty lines (incl whitespace-only) */
	    continue;

	if (istoken(p, "ident", sizeof("ident")-1)) {	/* description */
	    p = trim(p + sizeof("ident"));
	    group.ident = append(group.ident, chop(p), ' ');
	}
	else if (istoken(p, "probe", sizeof("probe")-1)) {
	    p = trim(p + sizeof("probe"));
	    group.probe = copy_string(p);
	    group.metric = copy_token(p);
	}
	else if (istoken(p, "force", sizeof("force")-1)) {
	    p = trim(p + sizeof("force"));
	    group.force = copy_string(p);
	}
	else if (istoken(p, "delta", sizeof("delta")-1)) {
	    p = trim(p + sizeof("delta"));
	    group.delta = copy_string(p);
	}
	else {		/* a metric specification for this logging group */
	    group_metric(&group, p);
	}
    }

    /* save this group into the global set (structure copy) */
    group.valid = (group.force || group.probe);
    groups[ngroups++] = group;

    if (pmDebugOptions.appl0)
	fprintf(stderr, "Parsed group with tag %s valid=%u metric=%s\n",
			tag, group.valid, group.metric? group.metric : "");
    return 0;
}

/* recursive descent below groups dir (find 'tags'), sort, drop non-v2 files */
int
parse_groups(const char *path, const char *subdir)
{
    struct dirent	**files = NULL;
    struct stat		sbuf;
    char		filename[MAXPATHLEN];
    char		tag[256], *name;
    FILE		*file;
    int			i, sep, count;

    if (pmDebugOptions.appl0)
	fprintf(stderr, "parse_groups entry: path=%s subdir=%s\n",
			path, subdir? subdir:"");

    sep = pmPathSeparator();
    if (subdir)
	pmsprintf(filename, MAXPATHLEN, "%s%c%s", path, sep, subdir);
    else
	pmsprintf(filename, MAXPATHLEN, "%s", path);

    count = scandir(filename, &files, cullv1, alphasort);
    if (count < 0 && oserror() == ENOENT)
	count = 0;

    if (resize_groups(count) < 0) {
	for (i = 0; i < count; i++) free(files[i]);
	if (files)
	    free(files);
	return 1;
    }

    for (i = 0; i < count; i++) {
	name = files[i]->d_name;
	if (*name == '.')
	    continue;

	if (subdir == NULL) {
	    pmsprintf(filename, MAXPATHLEN, "%s%c%s", path, sep, name);
	    pmsprintf(tag, sizeof(tag), "%s", name);
	} else {
	    pmsprintf(filename, MAXPATHLEN, "%s%c%s%c%s",
			    path, sep, subdir, sep, name);
	    pmsprintf(tag, sizeof(tag), "%s%c%s", subdir, sep, name);
	}

	if ((file = fopen(filename, "r")) != NULL) {
	    if (fstat(fileno(file), &sbuf) >= 0 && S_ISDIR(sbuf.st_mode))
		parse_groups(path, tag);
	    else
		parse_groupfile(file, tag);
	    fclose(file);
	} else if (oserror() == EISDIR) {
	    parse_groups(path, tag);
	}
    }

    for (i = 0; i < count; i++) free(files[i]);
    if (files)
	free(files);
    return 0;
}

int
fetch_groups(void)
{
    static pmResult	*result;
    const char		**names;
    pmDesc		*descs;
    pmID		*pmids;
    int			i, n, sts, count;

    /* prepare arrays of names, descriptors and IDs for PMAPI metric lookup */
    if ((names = calloc(ngroups, sizeof(char *))) == NULL)
	return -ENOMEM;
    if ((descs = calloc(ngroups, sizeof(pmDesc))) == NULL) {
	free(names);
	return -ENOMEM;
    }
    if ((pmids = calloc(ngroups, sizeof(pmID))) == NULL) {
	free(descs);
	free(names);
	return -ENOMEM;
    }

    /* iterate over the groups to extract metric names */
    for (i = n = 0; i < ngroups; i++) {
	if (!groups[i].valid || groups[i].metric == NULL)
	    continue;
	names[n++] = (const char *)groups[i].metric;
    }
    count = n;

    if ((sts = pmLookupName(count, names, pmids)) < 0) {
	if (count == 1)
	    groups[0].pmid = PM_ID_NULL;
	else {
	    int		j;
	    fprintf(stderr, "%s: cannot lookup metric names: %s\n",
			    pmGetProgname(), pmErrStr(sts));
	    for (j = 0; j < count; j++) {
		fprintf(stderr, "name[%d] %s\n", j, names[j]);
	    }
	}
	free(descs);
    }
    else if ((sts = pmFetch(count, pmids, &result)) < 0) {
	int	j;
	fprintf(stderr, "%s: cannot fetch metric values: %s\n",
			pmGetProgname(), pmErrStr(sts));
	for (j = 0; j < count; j++) {
	    fprintf(stderr, "name[%d] %s (PMID %s)\n", j, names[j], pmIDStr(pmids[j]));
	}
	free(descs);
    }
    else {
	/* associate metric identifier with each logging group */
	for (i = n = 0; i < ngroups; i++) {
	    if (!groups[i].valid || groups[i].metric == NULL)
		groups[i].pmid = PM_ID_NULL;
	    else
		groups[i].pmid = pmids[n++];
	}
	/* descriptor lookup, descs_hash handles failure here */
	(void) pmLookupDescs(count, pmids, descs);

	/* create a hash over the descs for quick PMID lookup */
	if ((sts = descs_hash(count, descs)) < 0)
	    fprintf(stderr, "%s: cannot hash metric descs: %s\n",
			    pmGetProgname(), pmErrStr(sts));
	/* create a hash over the result for quick PMID lookup */
	if ((sts = values_hash(result)) < 0)
	    fprintf(stderr, "%s: cannot hash metric values: %s\n",
			    pmGetProgname(), pmErrStr(sts));
    }
    free(names);
    free(pmids);
    return sts;
}

static inline char *
pmlogger_group_delta(group_t *group)
{
    return group->delta ? group->delta : "default";
}

static void
ident_callback(const char *line, int length, void *arg)
{
    fprintf((FILE *)arg, "## %.*s\n", length, line);
}

static void
pmlogger_group_ident(FILE *file, group_t *group)
{
    char	buffer[128];

    fmt(group->ident, buffer, sizeof(buffer), 65, 75, ident_callback, file);
}

int
parse_group(group_t *group)
{
    size_t		length;
    char		*p;

    if (group->force && group->metric) {
	fprintf(stderr, "%s: Warning: %s/%s "
			"\"probe\" and \"force\" control lines ... "
			"ignoring \"force\\n",
		pmGetProgname(), groupdir, group->tag);
    }
    if (!group->force && !group->metric) {
	fprintf(stderr, "%s: Warning: %s/%s "
			"neither \"probe\" nor \"force\" control lines ... "
			"use \"force available\"\n",
		pmGetProgname(), groupdir, group->tag);
	group->force = strdup("available");
    }

    if ((p = group->force) != NULL) {
	if (istoken(p, AVAILABLE, AVAILABLE_LEN))
	    group->true_state = STATE_AVAILABLE;
	else if (istoken(p, INCLUDE, INCLUDE_LEN))
	    group->true_state = STATE_INCLUDE;
	else if (istoken(p, EXCLUDE, EXCLUDE_LEN))
	    group->true_state = STATE_EXCLUDE;
	else {
	    fprintf(stderr, "%s: Error: %s/%s "
			    "force state \"%s\" not recognized\n",
		    pmGetProgname(), groupdir, group->tag, p);
	    group->valid = 0;
	    return -EINVAL;
	}
    }

    if ((p = group->probe) != NULL) {
	if (group->metric)
	    p = trim(p + strlen(group->metric));
	if (istoken(p, EXISTS, EXISTS_LEN)) {
	    p = trim(p + EXISTS_LEN);
	    group->probe_style = PROBE_EXISTS;
	} else if (istoken(p, VALUES, VALUES_LEN)) {
	    p = trim(p + VALUES_LEN);
	    group->probe_style = PROBE_VALUES;
	} else if (istoken(p, EQUAL, EQUAL_LEN)) {
	    p = trim(p + EQUAL_LEN);
	    group->probe_style = PROBE_EQ;
	} else if (istoken(p, NOTEQUAL, NOTEQUAL_LEN)) {
	    p = trim(p + NOTEQUAL_LEN);
	    group->probe_style = PROBE_NEQ;
	} else if (istoken(p, GREATER, GREATER_LEN)) {
	    p = trim(p + GREATER_LEN);
	    group->probe_style = PROBE_GT;
	} else if (istoken(p, GTEQUAL, GTEQUAL_LEN)) {
	    p = trim(p + GTEQUAL_LEN);
	    group->probe_style = PROBE_GE;
	} else if (istoken(p, LESSER, LESSER_LEN)) {
	    p = trim(p + LESSER_LEN);
	    group->probe_style = PROBE_LT;
	} else if (istoken(p, LTEQUAL, LTEQUAL_LEN)) {
	    p = trim(p + LTEQUAL_LEN);
	    group->probe_style = PROBE_LE;
	} else if (istoken(p, REGEXP, REGEXP_LEN)) {
	    p = trim(p + REGEXP_LEN);
	    group->probe_style = PROBE_RE;
	} else if (istoken(p, NOTREGEXP, NOTREGEXP_LEN)) {
	    p = trim(p + NOTREGEXP_LEN);
	    group->probe_style = PROBE_NRE;
	}
	else if (*p != '\0' && *p != '\n' && *p != '?') {
	    fprintf(stderr, "%s: Error: %s/%s "
			    "condition operator \"%s\" not recognized\n",
		    pmGetProgname(), groupdir, group->tag, p);
	    group->valid = 0;
	    return -EINVAL;
	}
	else
	    group->probe_style = PROBE_EXISTS;

	if (group->probe_style != PROBE_EXISTS &&
	    group->probe_style != PROBE_VALUES) {	/* extract op -val- */
	    group->value = copy_token(p);
	    if ((length = strlen(group->value)) > 0)
		p = trim(p + length);
	    else {
		fprintf(stderr, "%s: Error: %s/%s "
				"missing condition operand after %s operator\n",
			pmGetProgname(), groupdir, group->tag,
			operandstr(group->probe_style));
		group->valid = 0;
		return -EINVAL;
	    }
	}
	if (*p == '\0' || *p == '\n') {
	    group->success = 1;
	    group->probe_state = STATE_AVAILABLE;
	    group->true_state = STATE_AVAILABLE;
	    group->false_state = STATE_EXCLUDE;
	} else if (*p == '?') {	/* -?- true_state : false_state */
	    p = trim(++p);
   
	    if (istoken(p, AVAILABLE, AVAILABLE_LEN)) {
		p = trim(p + AVAILABLE_LEN);
		group->true_state = STATE_AVAILABLE;
	    } else if (istoken(p, INCLUDE, INCLUDE_LEN)) {
		p = trim(p + INCLUDE_LEN);
		group->true_state = STATE_INCLUDE;
	    } else if (istoken(p, EXCLUDE, EXCLUDE_LEN)) {
		p = trim(p + EXCLUDE_LEN);
		group->true_state = STATE_EXCLUDE;
	    } else {
		fprintf(stderr, "%s: Error: %s/%s "
				"condition true state \"%s\" not recognized\n",
			pmGetProgname(), groupdir, group->tag, p);
		group->valid = 0;
		return -EINVAL;
	    }

	    if (*p != ':') {	/* ? true_state -:- false_state */
		fprintf(stderr, "%s: Error: %s/%s missing \":\" in state rule "
				"at \"%s\"\n",
			pmGetProgname(), groupdir, group->tag, p);
		group->valid = 0;
		return -EINVAL;
	    }
	    p = trim(++p);

	    if (istoken(p, AVAILABLE, AVAILABLE_LEN)) {
		p = trim(p + AVAILABLE_LEN);
		group->false_state = STATE_AVAILABLE;
	    } else if (istoken(p, INCLUDE, INCLUDE_LEN)) {
		p = trim(p + INCLUDE_LEN);
		group->false_state = STATE_INCLUDE;
	    } else if (istoken(p, EXCLUDE, EXCLUDE_LEN)) {
		p = trim(p + EXCLUDE_LEN);
		group->false_state = STATE_EXCLUDE;
	    } else if (*p) {
		fprintf(stderr, "%s: Error: %s/%s condition false state \"%s\" "
				"not recognized\n",
			pmGetProgname(), groupdir, group->tag, p);
		group->valid = 0;
		return -EINVAL;
	    } else {
		fprintf(stderr, "%s: Error: %s/%s missing false state\n",
			pmGetProgname(), groupdir, group->tag);
		group->valid = 0;
		return -EINVAL;
	    }
	    if (*p != '\0' && *p != '\n') {
		fprintf(stderr, "%s: Error: %s/%s extra state rule components: "
				"\"%s\"\n",
			pmGetProgname(), groupdir, group->tag, p);
		group->valid = 0;
		return -EINVAL;
	    }
	} else {
	    fprintf(stderr, "%s: Error: %s/%s expected \"?\" after condition, "
			    "found \"%s\"\n",
		    pmGetProgname(), groupdir, group->tag, p);
	    group->valid = 0;
	    return -EINVAL;
	}
    }
    return 0;
}

static int
evaluate_number_values(group_t *group, int type, numeric_cmp_t compare)
{
    int			i, found;
    pmValueSet		*vsp;
    pmValue		*vp;
    pmAtomValue		atom;
    double		given;
    char		*endnum;
    int			sts;

    if ((vsp = metric_values(group->pmid)) == NULL) {
	if (pmDebugOptions.appl1)
	    fprintf(stderr, "evaluate_number_values: PMID %s: not in values hash\n", pmIDStr(group->pmid));
	return 0;
    }

    given = strtod(group->value, &endnum);
    if (*endnum != '\0') {
	fprintf(stderr, "%s: Error: %s/%s operand \"%s\" is not a number\n",
		pmGetProgname(), groupdir, group->tag, group->value);
	return -EINVAL;
    }

    if (pmDebugOptions.appl1) {
	if (vsp->numval == 0)
	    fprintf(stderr, "evaluate_number_values: PMID %s: no values\n", pmIDStr(group->pmid));
	else if (vsp->numval < 0)
	    fprintf(stderr, "evaluate_number_values: PMID %s: numval error: %s\n", pmIDStr(group->pmid), pmErrStr(vsp->numval));
    }

    /* iterate over all available values looking for any possible match */
    for (i = found = 0; i < vsp->numval; i++) {
	int	lsts;
	vp = &vsp->vlist[i];
	if ((lsts = pmExtractValue(vsp->valfmt, vp, type, &atom, PM_TYPE_DOUBLE)) < 0) {
	    if (pmDebugOptions.appl1)
		fprintf(stderr, "evaluate_number_values: PMID %s: extract error: %s\n", pmIDStr(group->pmid), pmErrStr(lsts));
	    continue;
	}
	if ((sts = compare(atom.d, given)) == 0) {
	    if (pmDebugOptions.appl1)
		fprintf(stderr, "evaluate_number_values: PMID %s: %f no match %f\n", pmIDStr(group->pmid), atom.d, given);
	    continue;
	}
	if (pmDebugOptions.appl1)
	    fprintf(stderr, "evaluate_number_values: PMID %s: match %f\n", pmIDStr(group->pmid), given);
	found = sts;
	break;
    }
    return found;
}

static int
evaluate_string_values(group_t *group, string_cmp_t compare)
{
    int			i, found;
    pmValueSet		*vsp;
    pmValue		*vp;
    pmAtomValue		atom;
    const int		type = PM_TYPE_STRING;
    int			sts;

    if ((vsp = metric_values(group->pmid)) == NULL) {
	if (pmDebugOptions.appl1)
	    fprintf(stderr, "evaluate_string_values: PMID %s: not in values hash\n", pmIDStr(group->pmid));
	return 0;
    }

    if (pmDebugOptions.appl1) {
	if (vsp->numval == 0)
	    fprintf(stderr, "evaluate_string_values: PMID %s: no values\n", pmIDStr(group->pmid));
	else if (vsp->numval < 0)
	    fprintf(stderr, "evaluate_string_values: PMID %s: numval error: %s\n", pmIDStr(group->pmid), pmErrStr(vsp->numval));
    }
    /* iterate over all available values looking for any possible match */
    for (i = found = 0; i < vsp->numval; i++) {
	int	lsts;
	vp = &vsp->vlist[i];
	if ((lsts = pmExtractValue(vsp->valfmt, vp, type, &atom, type)) < 0) {
	    if (pmDebugOptions.appl1)
		fprintf(stderr, "evaluate_string_values: PMID %s: extract error: %s\n", pmIDStr(group->pmid), pmErrStr(lsts));
	    continue;
	}
	if ((sts = compare(atom.cp, group->value)) == 0) {
	    if (pmDebugOptions.appl1)
		fprintf(stderr, "evaluate_string_values: PMID %s: \"%s\" no match \"%s\"\n", pmIDStr(group->pmid), atom.cp, group->value);
	    free(atom.cp);
	    continue;
	}
	if (pmDebugOptions.appl1)
	    fprintf(stderr, "evaluate_string_values: PMID %s: match \"%s\"\n", pmIDStr(group->pmid), group->value);
	free(atom.cp);
	found = sts;
	break;
    }
    return found;
}

static char *
number_to_string(pmAtomValue *atom, int type)
{
    char		buffer[64];

    switch (type) {
    case PM_TYPE_32:
	pmsprintf(buffer, sizeof(buffer), "%d", atom->l);
	break;
    case PM_TYPE_U32:
	pmsprintf(buffer, sizeof(buffer), "%u", atom->ul);
	break;
    case PM_TYPE_64:
	pmsprintf(buffer, sizeof(buffer), "%" PRId64, atom->ll);
	break;
    case PM_TYPE_U64:
	pmsprintf(buffer, sizeof(buffer), "%" PRIu64, atom->ull);
	break;
    case PM_TYPE_FLOAT:
	pmsprintf(buffer, sizeof(buffer), "%f", (double)atom->f);
	break;
    case PM_TYPE_DOUBLE:
	pmsprintf(buffer, sizeof(buffer), "%f", atom->d);
	break;
    default:
	return NULL;
    }
    return strdup(buffer);
}

static int
evaluate_string_regexp(group_t *group, regex_cmp_t compare)
{
    int			i, found;
    pmValueSet		*vsp;
    pmValue		*vp;
    pmDesc		*dp;
    pmAtomValue		atom;
    regex_t		regex;
    int			sts, type;

    if ((vsp = metric_values(group->pmid)) == NULL) {
	if (pmDebugOptions.appl1)
	    fprintf(stderr, "evaluate_string_regexp: PMID %s: not in values hash\n", pmIDStr(group->pmid));
	return 0;
    }
    if ((dp = metric_desc(group->pmid)) == NULL) {
	if (pmDebugOptions.appl1)
	    fprintf(stderr, "evaluate_string_regexp: PMID %s: not in desc hash\n", pmIDStr(group->pmid));
	return 0;
    }

    type = dp->type;
    if (type < 0 || type > PM_TYPE_STRING) {
	fprintf(stderr, "%s: %s uses regular expression on non-scalar metric\n",
		pmGetProgname(), group->tag);
	return 0;
    }

    if (regcomp(&regex, group->value, REG_EXTENDED|REG_NOSUB) != 0) {
	fprintf(stderr, "%s: expression \"%s\" in group %s is invalid\n",
		pmGetProgname(), group->value, group->tag);
	return -EINVAL;
    }

    if (pmDebugOptions.appl1) {
	if (vsp->numval == 0)
	    fprintf(stderr, "evaluate_string_regexp: PMID %s: no values\n", pmIDStr(group->pmid));
	else if (vsp->numval < 0)
	    fprintf(stderr, "evaluate_string_regexp: PMID %s: numval error: %s\n", pmIDStr(group->pmid), pmErrStr(vsp->numval));
    }
    /* iterate over all available values looking for any possible match */
    for (i = found = 0; i < vsp->numval; i++) {
	int	lsts;
	vp = &vsp->vlist[i];
	if ((lsts = pmExtractValue(vsp->valfmt, vp, type, &atom, type)) < 0) {
	    if (pmDebugOptions.appl1)
		fprintf(stderr, "evaluate_string_regexp: PMID %s: extract error: %s\n", pmIDStr(group->pmid), pmErrStr(lsts));
	    continue;
	}
	if (type != PM_TYPE_STRING &&
	    (atom.cp = number_to_string(&atom, type)) == NULL) {
	    if (pmDebugOptions.appl1)
		fprintf(stderr, "evaluate_string_regexp: PMID %s: number_to_string error\n", pmIDStr(group->pmid));
	    continue;
	}
	if ((sts = compare(&regex, atom.cp)) == 0) {
	    if (pmDebugOptions.appl1)
		fprintf(stderr, "evaluate_string_regexp: PMID %s: \"%s\" no match \"%s\"\n", pmIDStr(group->pmid), atom.cp, group->value);
	    free(atom.cp);
	    continue;
	}
	if (pmDebugOptions.appl1)
	    fprintf(stderr, "evaluate_string_regexp: PMID %s: \"%s\" match \"%s\"\n", pmIDStr(group->pmid), atom.cp, group->value);
	free(atom.cp);
	found = sts;
	break;
    }
    regfree(&regex);
    return found;
}

static int
evaluate_values(group_t *group, numeric_cmp_t ncmp, string_cmp_t scmp)
{
    pmDesc		*dp;

    if ((dp = metric_desc(group->pmid)) == NULL)
	return 0;

    if (dp->type == PM_TYPE_STRING)
	return evaluate_string_values(group, scmp);
    return evaluate_number_values(group, dp->type, ncmp);
}

int
evaluate_group(group_t *group)
{
    pmValueSet		*vp;

    switch (group->probe_style) {
    case PROBE_VALUES:
	if ((vp = metric_values(group->pmid)) == NULL)
	    return 0;
	return vp->numval > 0;

    case PROBE_EQ:
	return evaluate_values(group, number_equal, string_equal);
    case PROBE_NEQ:
	return evaluate_values(group, number_nequal, string_nequal);
    case PROBE_GT:
	return evaluate_values(group, number_greater, string_greater);
    case PROBE_GE:
	return evaluate_values(group, number_gtequal, string_gtequal);
    case PROBE_LT:
	return evaluate_values(group, number_lessthan, string_lessthan);
    case PROBE_LE:
	return evaluate_values(group, number_ltequal, string_ltequal);

    case PROBE_RE:
	return evaluate_string_regexp(group, string_regexp);
    case PROBE_NRE:
	return evaluate_string_regexp(group, string_nregexp);

    case PROBE_EXISTS:
    default:
	break;
    }
    return group->pmid != PM_ID_NULL;
}

unsigned int
evaluate_state(group_t *group)
{
    unsigned int	state;

    if (group->force) {
	if ((state = group->saved_state) == 0)
	    state = group->true_state;
	group->probe_state = state;
	if (state == STATE_EXCLUDE)
	    return STATE_EXCLUDE;
	if (state == STATE_AVAILABLE &&
	    (!group->metric || !evaluate_group(group)))
	    return STATE_AVAILABLE;
	group->probe_state = STATE_INCLUDE;
	group->success = 1;
	return STATE_INCLUDE; /* STATE_INCLUDE || available */
    }
    if ((group->pmlogger || group->pmrep) && !group->pmlogconf) {
	state = group->saved_state;
    } else if (evaluate_group(group)) {	/* probe */
	if ((state = group->saved_state) != STATE_INCLUDE)
	    state = group->true_state;
	group->success = 1;
    } else {
	if ((state = group->saved_state) != STATE_EXCLUDE)
	    state = group->false_state;
	group->success = 0;
    }
    group->probe_state = state;
    return state;
}

static void
pmlogger_group_enable(FILE *file, group_t *group)
{
    unsigned int	i;

    fprintf(file, "#+ %s:y:%s:\n",
		    group->tag, pmlogger_group_delta(group));
    pmlogger_group_ident(file, group);
    fprintf(file, "log %s on %s {\n",
		    loggingstr(group->logging), pmlogger_group_delta(group));
    for (i = 0; i < group->nmetrics; i++)
	fprintf(file, "\t%s\n", group->metrics[i]);
    fputs("}\n", file);
}

static void
pmlogger_group_disable(FILE *file, group_t *group)
{
    fprintf(file, "#+ %s:n:%s:\n", group->tag, pmlogger_group_delta(group));
    pmlogger_group_ident(file, group);
}

static void
pmlogger_group_exclude(FILE *file, group_t *group)
{
    fprintf(file, "#+ %s:x::\n", group->tag);
}

static void
pmlogger_group_state(FILE *file, group_t *group)
{
    if (group->probe_state == STATE_INCLUDE)
	pmlogger_group_enable(file, group);
    else if (group->probe_state == STATE_EXCLUDE)
	pmlogger_group_exclude(file, group);
    else
	pmlogger_group_disable(file, group);
}

static void
unlink_tempfile(void)
{
    unlink(tmpconfig);
}

static void
sigintproc(int sig)
{
    (void)sig;
    unlink_tempfile();
    putchar('\n');
    exit(1);
}

void
create_tempfile(FILE *file, FILE **tempfile, struct stat *stat)
{
    int			fd, sts, mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH;
    FILE		*fp;
    char		bytes[BUFSIZ];
    mode_t		cur_umask;

    /* create a temporary file in the same directory as config (for rename) */
    pmsprintf(bytes, sizeof(bytes), "%s.new", config);
    if ((tmpconfig = strdup(bytes)) == NULL) {
	fprintf(stderr, "%s: cannot create file %s: %s\n",
			pmGetProgname(), bytes, osstrerror());
	exit(EXIT_FAILURE);
    }
    unlink(tmpconfig);
    cur_umask = umask(S_IWGRP|S_IWOTH);
    if ((fd = open(tmpconfig, O_CREAT|O_EXCL|O_RDWR, mode)) < 0 ||
	(fp = fdopen(fd, "r+")) == NULL) {
	fprintf(stderr, "%s: cannot create file %s: %s\n",
			pmGetProgname(), tmpconfig, osstrerror());
	exit(EXIT_FAILURE);
    }
    __pmSetSignalHandler(SIGTERM, sigintproc);
    __pmSetSignalHandler(SIGINT, sigintproc);
    atexit(unlink_tempfile);
    umask(cur_umask);

    if (stat) {
#if defined(HAVE_FCHOWN)
	sts = fchown(fd, stat->st_uid, stat->st_gid);
#elif defined(HAVE_CHOWN)
	sts = chown(tmpconfig, stat->st_uid, stat->st_gid);
#else
	sts = 0;
#endif
	if (sts < 0) {
	    fprintf(stderr, "%s: Warning: chown() failed: %s\n",
			pmGetProgname(), osstrerror());
	}
#if defined(HAVE_FCHMOD)
	sts = fchmod(fd, stat->st_mode);
#else
	sts = chmod(tmpconfig, stat->st_mode);
#endif
	if (sts < 0) {
	    fprintf(stderr, "%s: Warning: chmod() failed: %s\n",
			pmGetProgname(), osstrerror());
	}
    }

    *tempfile = fp;
}

static void
create_group(FILE *file, group_t *group)
{
    evaluate_state(group);

    if (!prompt && !autocreate && group->probe_state != STATE_EXCLUDE &&
	(group->pmid != PM_ID_NULL || group->metric == NULL))
	fputc('.', stdout);

    pmlogger_group_state(file, group);
    fputs("#----\n", file);
}

static void
pmlogger_header(FILE *f)
{
    if (header) {
	fputs(header, f);
	return;
    }

    fprintf(f,
"#pmlogconf 2.0\n"
"#\n"
"# pmlogger(1) config file created and updated by pmlogconf\n");

    if (autocreate) {
	time_t	timenow = time(NULL);
	fprintf(f,
"# Auto-generated by %s on: %s\n", pmGetProgname(), ctime(&timenow));
    }

    fprintf(f,
"#\n"
"# DO NOT UPDATE THE INITIAL SECTION OF THIS FILE.\n"
"# Any changes may be lost the next time pmlogconf is used\n"
"# on this file.\n"
"#\n"
"#+ groupdir %s\n"
"#\n", groupdir);
}

static void
pmlogger_trailer(FILE *f)
{
    if (trailer) {
	fputs(trailer, f);
	return;
    }

    fprintf(f,
"# DO NOT UPDATE THE FILE ABOVE THIS LINE\n"
"# Otherwise any changes may be lost the next time pmlogconf is\n"
"# used on this file.\n"
"#\n"
"# It is safe to make additions from here on ...\n"
"#\n"
"\n"
"[access]\n"
"disallow .* : all;\n"
"disallow :* : all;\n"
"allow local:* : enquire;\n");
}

static void
pmlogger_report_group(group_t *group)
{
    unsigned int	state;
    char		*delta = NULL;

    if (!group->valid)
	return;

    if (finaltag == NULL && setupfile == NULL)
	finaltag = "all";	/* verbose mode default */

    if (finaltag) {
	if (strcmp(finaltag, "all") != 0 && strcmp(finaltag, group->tag) != 0)
	    return;
	if ((state = group->saved_state) == 0)
	    state = group->probe_state;
	if (group->saved_delta)
	    delta = group->saved_delta;
	else
	    delta = pmlogger_group_delta(group);
    } else if (setupfile) {
	state = group->probe_state;
	delta = pmlogger_group_delta(group);
    } else
	return;

    if (verbose) {
	printf("%s/%s:\n", groupdir, group->tag);
	if (group->force)
	    printf("%s -> ", group->force);
	else
	    printf("%s -> probe=%s ", group->probe,
		    group->success? "success" : "failure");
	if (state == STATE_EXCLUDE)
	    printf("action=exclude\n");
	else if (state == STATE_INCLUDE)
	    printf("action=include\n");
	else
	    printf("action=available\n");
    }

    if (state == STATE_EXCLUDE)
	printf("#+ %s/%s:x::\n", groupdir, group->tag);
    else if (state == STATE_INCLUDE)
	printf("#+ %s/%s:y:%s:\n", groupdir, group->tag, delta);
    else
	printf("#+ %s/%s:n:%s:\n", groupdir, group->tag, delta);
}

static int
pmlogconf_setup(void)
{
    group_t		*group;
    FILE		*file;
    char		*tag;

    if (resize_groups(1) < 0) {
	fprintf(stderr, "%s: out-of-memory adding pmlogconf file %s\n",
		pmGetProgname(), setupfile);
	exit(EXIT_FAILURE);
    }

    if ((file = fopen(setupfile, "r")) != NULL) {
	if (*(tag = setupfile) == pmPathSeparator())
	    tag++;
	parse_groupfile(file, tag);
	fclose(file);
    } else {
	fprintf(stderr, "%s: Cannot open pmlogconf configuration \"%s\": %s\n",
		    pmGetProgname(), setupfile,  osstrerror());
	exit(EXIT_FAILURE);
    }

    /* reporting mode, and only on pmlogconf file state */
    group = &groups[0];
    if (parse_group(group) != 0)
	return 1;
    fetch_groups();
    evaluate_group(group);
    evaluate_state(group);
    pmlogger_report_group(group);
    return 0;
}

void
setup_groups(void)
{
    unsigned int	i;

    qsort(groups, ngroups, sizeof(group_t), group_compare);
    for (i = 0; i < ngroups; i++)
	parse_group(&groups[i]);
}

static void
pmlogger_create(FILE *file)
{
    FILE		*tempfile;
    unsigned int	i;

    printf("Creating config file \"%s\" using default settings ...\n", config);
    parse_groups(groupdir, NULL);
    fetch_groups();
    setup_groups();

    if (finaltag) {
	for (i = 0; i < ngroups; i++)
	    pmlogger_report_group(&groups[i]);
	return;
    }

    create_tempfile(file, &tempfile, NULL);
    pmlogger_header(tempfile);
    for (i = 0; i < ngroups; i++) {
	create_group(tempfile, &groups[i]);
	if (verbose)
	    pmlogger_report_group(&groups[i]);
    }
    pmlogger_trailer(tempfile);

    if (rename(tmpconfig, config) < 0) {
	fprintf(stderr, "%s: cannot rename file %s to %s: %s\n",
		pmGetProgname(), tmpconfig, config, osstrerror());
	exit(EXIT_FAILURE);
    }
    fclose(tempfile);
    fputc('\n', stdout);
}

static void
pmlogger_update_group(FILE *file, group_t *group)
{
    if (group->valid) {
	evaluate_state(group);
	pmlogger_group_state(file, group);
	fputs("#----\n", file);
    }
}


static void
update_callback(const char *line, int length, void *arg)
{
    unsigned int	*count = (unsigned int *)arg;

    if ((*count)++ == 0)
	printf("Group: %.*s\n", length, line);
    else
	printf("       %.*s\n", length, line);
}

static void
update_delta(group_t *group, const char *delta)
{
    free(group->delta);
    group->delta = copy_string(delta);
}

char *
update_groups(FILE *tempfile, const char *pattern)
{
    group_t		*group;
    struct timespec	interval;
    static char		*answer;
    static char		buffer[128]; /* returned 'answer' points into this */
    char		*state = NULL, *errmsg, *p;
    unsigned int	i, m, count;

    /* iterate over the groups array. */
    for (i = count = 0; i < ngroups; count = 0, i++) {
	group = &groups[i];
	if (!group->valid || group->saved_state == STATE_EXCLUDE)
	    continue;
	if (!prompt) {
	    if (!autocreate)
		putc('.', stdout);
	    continue;
	}
	if (group->force)
	    continue;
	if (autocreate)
	    state = "\n";
	else if (group->saved_state == STATE_INCLUDE)
	    state = "y";
	else
	    state = "n";
	if (!autocreate) {
	    putc('\n', stdout);
	    fmt(group->ident, buffer, sizeof(buffer), 64, 75,
			    update_callback, &count);
	    printf("Log this group? [%s] ", state);
	}
	answer = memset(buffer, 0, sizeof(buffer));
	if (autocreate || fgets(answer, sizeof(buffer), stdin) == NULL)
	    answer[0] = *state;
	else
	    answer = chop(trim(answer));
	if (answer[0] == '\0')	/* keep group as-is */
	    answer[0] = *state;

	switch (answer[0]) {
	case '?':
	    printf("Valid responses are:\n\
m         report the names of the metrics in this group\n\
n         do not log this group\n\
q         quit; no change for this or any of the following groups\n\
y         log this group\n\
/pattern  no change for this group and search for a group containing pattern\n\
          in the description or the metrics associated with the group\n");
	    break;

	case 'm':
	    printf("Metrics in this group (%s):\n", group->tag);
	    for (m = 0; m < group->nmetrics; m++)
		printf("    %s\n", group->metrics[m]);
	    i--;	/* stay on this group */
	    continue;

	case 'q':
	    prompt = 0;	/* finished editing */
	    continue;

	case 'n':
	    if (group->saved_state != STATE_AVAILABLE)
		rewrite = 1;
	    group->saved_state = STATE_AVAILABLE;
	    break;

	case 'y':
	    if (group->saved_state != STATE_INCLUDE)
		rewrite = 1;
	    group->saved_state = STATE_INCLUDE;
	    break;

	case '/':
	    if ((p = strrchr(answer + 1, '\n')) != NULL)
		*p = '\0';
	    printf("Searching for \"%s\"\n", answer + 1);
	    if ((group_pattern(&i, answer + 1)) != NULL) {
		i--;	/* prepare for loop iteration */
		continue;
	    }
	    return answer + 1;

	default:
	    printf("Error: you must answer \"m\", \"n\", \"q\", \"y\", \"?\" "
		   "or \"/pattern\" ... try again\n");
	    i--;
	    continue;
	}

	if (autocreate)
	    continue;

	if (prompt && deltas && group->saved_state == STATE_INCLUDE) {
	    if (quick) {
		printf("Logging interval: %s\n", pmlogger_group_delta(group));
	    } else {
		do {
		    printf("Logging interval? [%s] ", pmlogger_group_delta(group));
		    answer = memset(buffer, 0, sizeof(buffer));
		    if (fgets(answer, sizeof(buffer), stdin) == NULL)
			break;
		    answer = chop(trim(answer));
		    if (answer == NULL || answer[0] == '\0')
			answer = "default";
		    if (strcmp(answer, "once") == 0 ||
			strcmp(answer, "default") == 0) {
			update_delta(group, answer);
			break;
		    }
		    if (pmParseHighResInterval(answer, &interval, &errmsg) >= 0) {
			update_delta(group, answer);
			break;
		    }
		    free(errmsg);
		    printf("Error: logging interval must be of the form "
			   "\"once\" or \"default\" or \"<integer> <scale>\","
			   "where <scale> is one of \"sec\", \"secs\", \"min\","
			   "\"mins\", etc ... try again\n");
		} while (1);
		rewrite = 1;
	    }
	}
    }
    return NULL;
}

static void
update_pmlogger_tempfile(FILE *tempfile)
{
    unsigned int	i;
    char		*pattern = NULL;
    char		answer[16] = {0};

    /* Interactive loop (unless 'prompt' variable is cleared) */
    do {
	if ((pattern = update_groups(tempfile, pattern)) == NULL)
	    break;
	printf(" not found.\n");
	for (;;) {
	    printf("Continue searching from start of the file? [y] ");
	    if (fgets(answer, sizeof(answer), stdin) == NULL)
		answer[0] = 'y';
	    if (answer[0] == 'y' || answer[0] == 'n')
		break;
	    printf("Error: you must answer \"y\" or \"n\" ... try again\n");
	}
	if (answer[0] == 'n') {
	    pattern = NULL;
	    prompt = 1;
	} else {
	    printf("Searching for \"%s\"\n", pattern);
	}
    } while (1);

    /*
     * If we are reprobing, or any group exists in pmlogconf files but not
     * the pmlogger file, make sure we rewrite the pmlogger configuration.
     */
    if (reprobe) {
	rewrite = 1;
    } else if (rewrite == 0) {
	for (i = 0; i < ngroups; i++)
	    if (groups[i].pmlogconf && !groups[i].pmlogger) {
		rewrite = 1;
		break;
	    }
    }

    /*
     * Finally write out the pmlogger configuration if anything has been
     * modified - either by the user or by new pmlogconf groups arriving.
     */
    if (rewrite) {
	if (ftruncate(fileno(tempfile), 0L) < 0)
	    fprintf(stderr, "%s: cannot truncate temporary file: %s\n",
			pmGetProgname(), osstrerror());
	if (fseek(tempfile, 0L, SEEK_SET) < 0)
	    fprintf(stderr, "%s: cannot fseek to temporary file start: %s\n",
			pmGetProgname(), osstrerror());
	prompt = 0;
	pmlogger_header(tempfile);
	for (i = 0; i < ngroups; i++)
	    pmlogger_update_group(tempfile, &groups[i]);
	pmlogger_trailer(tempfile);
	fflush(tempfile);
    }
}

void
group_dircheck(const char *path)
{
    size_t		length;
    char		*end;

    if ((end = strrchr(path, '\n')) == NULL)
	return;
    length = end - path;
    if (strncmp(groupdir, path, length) != 0)
	fprintf(stderr,
		"%s: Warning: using base directory for group files from command"
		" line (%s) which is different from that in %s (%.*s)\n", 
		pmGetProgname(), groupdir, config, (int)length, path);
}

static void
copy_and_parse_tempfile(FILE *file, FILE *tempfile)
{
    char		bytes[BUFSIZ];
    group_t		*group = NULL;
    unsigned int	tail = 0, head = 0, line = 0, count = 0;

    /*
     * Copy the contents of config into tempfile, parsing and extracting
     * existing group state as we go, and stashing the immutable trailer
     * as well.
     */
    fseek(file, 0L, SEEK_SET);
    while (fgets(bytes, sizeof(bytes), file) != NULL) {
	fputs(bytes, tempfile);	/* copy into temporary configuration file */
	line++;

	if (strncmp(bytes, "#pmlogconf 2", 12) == 0)
	    head = 1;
	if (strncmp(bytes, "# DO NOT UPDATE THE FILE ABOVE THIS LINE", 40) == 0)
	    tail = 1;

	if (tail) {
	    trailer = append(trailer, bytes, '\n');
	} else if (strncmp("#+ groupdir ", bytes, 12) == 0) {
	    group_dircheck(bytes + 12);
	} else if (strncmp("#+ ", bytes, 3) == 0) {
	    if (group)
	    	group_free(group);
	    group = group_create_pmlogger(bytes + 3, line);
	    head = 0;
	} else if (group) {
	    if (strncmp("#----", bytes, 5) == 0)
		group = group_finish(group, &count);
	    else if (strncmp("## ", bytes, 3) == 0)
		group_ident(group, bytes + 3);
	    else if (strncmp("log mandatory", bytes, 13) == 0)
		group->logging = LOG_MANDATORY;
	    else if (strncmp("log advisory", bytes, 12) == 0)
		group->logging = LOG_ADVISORY;
	    else if (strncmp("}", bytes, 1) != 0)
		group_metric(group, bytes);
	    head = 0;
	}

	if (head)
	    header = append(header, bytes, '\n');
    }
    if (group)	/* missing end marker? - we'll optimistically fix this up */
	group_finish(group, &count);
    fflush(tempfile);
}

void
diff_tempfile(FILE *tempfile)
{
    char		bytes[512];
    char		answer[16] = {0};
    __pmExecCtl_t	*argp = NULL;
    FILE		*diff;
    int			sts = 0;

    sts |= __pmProcessAddArg(&argp, "diff");
    sts |= __pmProcessAddArg(&argp, "-c");
    sts |= __pmProcessAddArg(&argp, config);
    sts |= __pmProcessAddArg(&argp, tmpconfig);
    if ((sts < 0) ||
        (sts = __pmProcessPipe(&argp, "r", PM_EXEC_TOSS_NONE, &diff)) < 0) {
	fprintf(stderr, "%s: cannot execute diff command: %s\n",
			pmGetProgname(), pmErrStr(sts));
	unlink(tmpconfig);
	fclose(tempfile);
	exit(EXIT_FAILURE);
    }
    if (fgets(bytes, sizeof(bytes), diff) == NULL) {
	__pmProcessPipeClose(diff);
	printf("\nNo changes\n");
	unlink(tmpconfig);
    } else {
	printf("\nDifferences ...\n");
	do {
	    fputs(bytes, stdout);
	} while (fgets(bytes, sizeof(bytes), diff) != NULL);
	__pmProcessPipeClose(diff);
	for (;;) {
	    if (!autocreate)
		printf("Keep changes? [y] ");
	    if (fgets(answer, sizeof(answer), stdin) == NULL || autocreate)
		answer[0] = 'y';
	    if (answer[0] == '\n' || answer[0] == '\0')
		answer[0] = 'y';
	    if (answer[0] == 'y' || answer[0] == 'n')
		break;
	    printf("Error: you must answer \"y\" or \"n\" ... try again\n");
	}
	if (answer[0] == 'y')
	    rename(tmpconfig, config);
	else
	    unlink(tmpconfig);
    }
    fclose(tempfile);
}

static void
pmlogger_update(FILE *file, struct stat *stat)
{
    FILE		*tempfile;
    unsigned int	i;

    if (host && !reprobe)
	fprintf(stderr,
		"%s: Warning: existing config file, -h %s will be ignored\n",
		pmGetProgname(), host);

    parse_groups(groupdir, NULL);
    fetch_groups();
    setup_groups();

    create_tempfile(file, &tempfile, stat);
    copy_and_parse_tempfile(file, tempfile);

    if (finaltag) {
	for (i = 0; i < ngroups; i++)
	    pmlogger_report_group(&groups[i]);
	unlink(tmpconfig);
	fclose(tempfile);
	return;
    }

    update_pmlogger_tempfile(tempfile);

    if (verbose) {
	for (i = 0; i < ngroups; i++)
	    pmlogger_report_group(&groups[i]);
    }
    diff_tempfile(tempfile);
}

void
pmapi_setup(pmOptions *options)
{
    const char	*hostspec;
    int		sts;

    /* prepare the environment - no derived metrics, POSIX sorting */
    unsetenv("PCP_DERIVED_CONFIG");
    setenv("LC_COLLATE", "POSIX", 1);

    /* setup connection to pmcd in order to query metric states */
    if (options->nhosts > 0)
	hostspec = options->hosts[0];
    else
	hostspec = "local:";
    if ((sts = pmNewContext(PM_CONTEXT_HOST, hostspec)) < 0) {
	fprintf(stderr, "%s: Cannot connect to pmcd on host \"%s\": %s\n",
		pmGetProgname(), hostspec,  pmErrStr(sts));
	exit(EXIT_FAILURE);
    }
}

void
group_setup(void)
{
    struct stat	sbuf;
    int		c, sts;

    /* location of pmlogconf template files we will be using */
    if (groupdir[0] == '\0') {
	c = pmPathSeparator();
	pmsprintf(groupdir, sizeof(groupdir), "%s%cconfig%cpmlogconf",
			pmGetConfig("PCP_VAR_DIR"), c, c);
    }
    if ((sts = stat(groupdir, &sbuf)) < 0)
	sts = -oserror();
    else if (!S_ISDIR(sbuf.st_mode))
	sts = -ENOTDIR;
    if (sts < 0) {
	fprintf(stderr, "%s: Cannot open pmlogconf groups path \"%s\": %s\n",
		    pmGetProgname(), groupdir, pmErrStr(sts));
	exit(EXIT_FAILURE);
    }
}

int
pmlogconf(int argc, char **argv)
{
    struct stat	sbuf;
    FILE	*file;
    int		c;

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {

	case 'c':	/* auto-generated */
	    autocreate = 1;
	    prompt = 0;
	    break;

	case 'd':
	    pmsprintf(groupdir, MAXPATHLEN, "%s", opts.optarg);
	    break;

	case 'g':	/* group (report final configured states) */
	    finaltag = opts.optarg;
	    prompt = 0;
	    break;

	case 'q':	/* quiet/quick */
	    quick = 1;
	    break;

	case 'r':	/* reprobe */
	    reprobe = 1;
	    break;

	case 's':	/* setup (report initial pmlogconf states) */
	    setupfile = opts.optarg;
	    prompt = 0;
	    break;

	case 'v':	/* verbose */
	    verbose = 1;
	    break;	
	}
    }

    if (autocreate && (finaltag || setupfile)) {
	pmprintf("Option -c cannot be used with -g,--group or -s,--setup\n");
	opts.errors++;
    }

    if (groupdir[0] && setupfile) {
	pmprintf("Options -d,--groupdir and -s,--setup are mutually exclusive\n");
	opts.errors++;
    }

    if (finaltag && setupfile) {
	pmprintf("Options -g,--group and -s,--setup are mutually exclusive\n");
	opts.errors++;
    }

    if (opts.errors || (opts.optind != argc - 1 && !setupfile)) {
	pmUsageMessage(&opts);
	exit(EXIT_FAILURE);
    }

    if (opts.flags & PM_OPTFLAG_EXIT) {
	pmflush();
	pmUsageMessage(&opts);
	exit(0);
    }

    pmapi_setup(&opts);

    if (setupfile)
	return pmlogconf_setup();

    group_setup();

    /* given pmlogger configuration file to be created or modified */
    config = argv[opts.optind];
    setoserror(0);
    if ((file = fopen(config, finaltag ? "r" : "r+")) == NULL) {
	if (oserror() == ENOENT)
	    file = fopen(config, "w+");
	if (file == NULL) {
	    fprintf(stderr, "%s: Cannot open pmlogger configuration \"%s\": %s\n",
			pmGetProgname(), config,  osstrerror());
	    exit(EXIT_FAILURE);
	}
    }
    if (fstat(fileno(file), &sbuf) < 0)
	existing = 0;
    else if (sbuf.st_size > 0)
	existing = 1;
    else
	existing = 0;
    if (existing == 0)
	prompt = 0;

    if (!existing)
	pmlogger_create(file);
    else
	pmlogger_update(file, &sbuf);
    fclose(file);
    return 0;
}

int
main(int argc, char **argv)
{
    pmSetProgname(argv[0]);

    if (strcmp(pmGetProgname(), "pmrepconf") == 0)
	return pmrepconf(argc, argv);

    return pmlogconf(argc, argv);
}
