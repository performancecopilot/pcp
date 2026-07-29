/*
 * Copyright (c) 2020-2021,2026 Red Hat.
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

#include "pmwebapi.h"

typedef enum search_flags {
    PMSEARCH_COLOUR	= (1<<0),	/* report in colour if possible */
    PMSEARCH_TOTALS	= (1<<1),	/* report total number of hits */
    PMSEARCH_TIMING	= (1<<2),	/* report time taken for search */
    PMSEARCH_DOCIDS	= (1<<3),	/* report every docs identifier */
    PMSEARCH_SCORES	= (1<<4),	/* report score for each result */

    PMSEARCH_OPT_INFO   = (1<<16),	/* -i, --info option */
    PMSEARCH_OPT_INDOM	= (1<<17),	/* -m, --indom option */
    PMSEARCH_OPT_QUERY	= (1<<18),	/* -q, --query option (default) */
    PMSEARCH_OPT_SUGGEST= (1<<19),	/* -s, --suggest option */
} search_flags;

#define ANSI_RESET	"\x1b[0m"
#define ANSI_FG_CYAN	"\x1b[36m"	/* highlights and statistics */

typedef struct search_data {
    pmSearchSettings	settings;
    pmSearchTextRequest	request;
    search_flags	flags;
    unsigned int	count;
    int			status;
} search_data;

static search_data *
search_data_init(search_flags flags, sds query, unsigned int count, unsigned int offset)
{
    search_data		*dp = calloc(1, sizeof(search_data));

    if (dp == NULL) {
	fprintf(stderr, "%s: out of memory allocating search data\n",
		pmGetProgname());
	exit(127);
    }
    dp->request.query = query;
    if ((flags & PMSEARCH_COLOUR)) {
	dp->request.highlight_name = 1;
	dp->request.highlight_oneline = 1;
	dp->request.highlight_helptext = 1;
    }
    dp->request.offset = offset;
    dp->request.count = count;
    dp->flags = flags;
    return dp;
}

static void
search_data_free(search_data *dp)
{
    sdsfree(dp->request.query);
    free(dp);
}

static void
on_search_info(pmLogLevel level, sds message, void *arg)
{
    search_data		*dp = (search_data *)arg;
    int			colour = (dp->flags & PMSEARCH_COLOUR);
    FILE		*fp = (level == PMLOG_INFO) ? stdout : stderr;

    if (level >= PMLOG_ERROR)
	dp->status = 1; /* exit pmsearch with error */
    if (level >= PMLOG_INFO || pmDebugOptions.search)
	pmLogLevelPrint(fp, level, message, colour);
}

static void
on_search_metrics(pmSearchMetrics *metrics, void *arg)
{
    search_data		*dp = (search_data *)arg;
    const char		*on, *off;

    if ((dp->flags & PMSEARCH_COLOUR)) {
	on = ANSI_FG_CYAN;
	off = ANSI_RESET;
    } else {
	on = off = "";
    }

    printf("Search index statistics:\n");
    printf("    Documents: %s%llu%s\n", on, metrics->docs, off);
    printf("        Terms: %s%llu%s\n", on, metrics->terms, off);
    printf("      Records: %s%llu%s\n", on, metrics->records, off);
}

/*
 * Print a value with an associated name.  Optional highlighting -
 * search for <b> ... </b> in given string and use colour if found.
*/
static void
printv(search_data *dp, const char *name, sds input)
{
    const char		*on, *off;
    char 		*start, *end, *tmp;
    sds			value;

    if (input == NULL || input[0] == '\0')
	return;

    if (!(dp->flags & PMSEARCH_COLOUR)) {
	value = input;
    } else {
	on = ANSI_FG_CYAN;
	off = ANSI_RESET;
	value = sdsempty();
	start = input;
	end = NULL;

	/* find highlighting and render it for the console */
	while ((tmp = strchr(start, '<')) != NULL) {
	    if (strncmp(tmp, "<b>", 3) == 0) {
		while ((end = strchr(tmp + 3, '<')) != NULL) {
		    if (strncmp(end, "</b>", 4) == 0) {
			/* found end - copy and replace */
			value = sdscatlen(value, start, tmp - start);
			value = sdscatlen(value, on, strlen(on));
			value = sdscatlen(value, tmp + 3, end - tmp - 3);
			value = sdscatlen(value, off, strlen(off));
			start = end += 4;
			break;
		    }
		    tmp = end + 1;
		}
		if (end == NULL) { /* not terminated */
		    value = sdscatlen(value, start, strlen(start));
		    break;
		}
	    } else {	/* not actually a start */
		value = sdscatlen(value, "<", 1);
		start = tmp + 1;
	    }
	}
	if (sdslen(value) == 0) {	/* no opening braces found */
	    sdsfree(value);
	    value = input;
	} else if (end != NULL) {
	    value = sdscatlen(value, end, strlen(end));
	}
    }

    printf("%s: %s\n", name, value);

    if (value != input)
	sdsfree(value);
}

static void
on_search_result(pmSearchTextResult *result, void *arg)
{
    search_data		*dp = (search_data *)arg;

    if (dp->count == 0) {
	if (dp->flags & PMSEARCH_TOTALS)
	    printf("%u hit%s", result->total, result->total == 1? "" : "s");
	if (dp->flags & PMSEARCH_TOTALS && dp->flags & PMSEARCH_TIMING)
	    printf(" in ");
	if (dp->flags & PMSEARCH_TIMING)
	    printf("%.5f second%s", result->timer, result->timer==1.0? "" : "s");
	if (dp->flags & (PMSEARCH_TOTALS | PMSEARCH_TIMING))
	    puts("\n");
    } else {
	puts("");
    }
    dp->count++;

    if (dp->flags & PMSEARCH_DOCIDS)
	printf("ID: %s\n", result->docid);
    if (dp->flags & PMSEARCH_SCORES)
	printf("Score: %.2f\n", result->score);
    if (result->type != PM_SEARCH_TYPE_UNKNOWN &&
	!(dp->flags & PMSEARCH_OPT_SUGGEST))
	printf("Type: %s\n", pmSearchTextTypeStr(result->type));
    if (result->name != NULL)
	printv(dp, "Name", result->name);
    if (!(dp->flags & PMSEARCH_OPT_SUGGEST)) {
	if (result->indom != NULL && result->indom[0] != '\0')
	    printv(dp, "InDom", result->indom);
	else if (result->type != PM_SEARCH_TYPE_UNKNOWN)
	    printv(dp, "InDom", "none");
    }
    if (result->oneline != NULL)
	printv(dp, "One line", result->oneline);
    if (result->helptext != NULL)
	printv(dp, "Help", result->helptext);
}

static int
pmsearch_overrides(int opt, pmOptions *opts)
{
    switch (opt) {
    case 'n': case 'N': case 'O': case 's': case 'S': case 't': case 'T':
	return 1;
    }
    return 0;
}

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("General Options"),
    { "config", 1, 'c', "FILE", "configuration file path"},
    { "no-colour", 0, 'C', 0, "no highlighting in results text" },
    { "docid", 0, 'd', 0, "report document ID of each result" },
    { "info", 0, 'i', 0, "report search index metrics" },
    { "indom", 0, 'n', 0, "perform an instance domain related entities search"},
    { "number", 1, 'N', "N", "return N search results at most" },
    { "offset", 1, 'O', "N", "paginated results from given offset" },
    { "query", 0, 'q', 0, "perform a general text search (default)" },
    { "suggest", 0, 's', 0, "perform a name suggestions search" },
    { "score", 0, 'S', 0, "report score (rank) of each result" },
    { "total", 0, 't', 0, "report total number of search results" },
    { "times", 0, 'T', 0, "report elapsed search execution time" },
    PMOPT_DEBUG,
    PMOPT_VERSION,
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "c:CdD:inN:O:qsStTvV?",
    .long_options = longopts,
    .short_usage = "[options] [query | indom]",
    .override = pmsearch_overrides,
};

static void
on_search_done(int sts, void *arg)
{
    search_data	*dp = (search_data *)arg;

    if (sts == 0) {
	if ((dp->flags & (PMSEARCH_OPT_QUERY | PMSEARCH_OPT_SUGGEST | PMSEARCH_OPT_INDOM)) &&
	    (dp->count == 0))
	    printf("0 search results\n");
    } else if (dp->flags & PMSEARCH_OPT_INFO)
	fprintf(stderr, "%s: %s failed - %s\n", pmGetProgname(),
			"pmSearchInfo", pmErrStr(sts));
    else if (dp->flags & PMSEARCH_OPT_QUERY)
	fprintf(stderr, "%s: %s failed - %s\n", pmGetProgname(),
			"pmSearchTextQuery", pmErrStr(sts));
    else if (dp->flags & PMSEARCH_OPT_SUGGEST)
	fprintf(stderr, "%s: %s failed - %s\n", pmGetProgname(),
			"pmSearchTextSuggest", pmErrStr(sts));
    else
	fprintf(stderr, "%s: %s failed - %s\n", pmGetProgname(),
			"pmSearchTextIndom", pmErrStr(sts));
}

static void
on_search_setup(void *arg)
{
    search_data	*dp = (search_data *)arg;
    int		sts;

    if ((dp->flags & PMSEARCH_OPT_INFO)) {
	sds key = sdsnew("text");
	sts = pmSearchInfo(&dp->settings, key, arg);
	sdsfree(key);
    }
    else if ((dp->flags & PMSEARCH_OPT_SUGGEST))
	sts = pmSearchTextSuggest(&dp->settings, &dp->request, arg);
    else if ((dp->flags & PMSEARCH_OPT_INDOM))
	sts = pmSearchTextInDom(&dp->settings, &dp->request, arg);
    else	/* flags & PMSEARCH_OPT_QUERY */
	sts = pmSearchTextQuery(&dp->settings, &dp->request, arg);

    if (sts < 0)
	on_search_done(sts, arg);
}

int
main(int argc, char *argv[])
{
    sds			query = NULL;
    int			c, colour = 1;
    unsigned int	search_count = 0;
    unsigned int	search_offset = 0;
    const char		*inifile = NULL;
    search_flags	flags = 0;
    search_data		*dp;
    struct dict		*config;

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {

	case 'C':	/* report matching text without highlight */
	    colour = 0;
	    break;

	case 'c':	/* path to .ini configuration file */
	    inifile = opts.optarg;
	    break;

	case 'd':	/* report document IDs with search hits */
	    flags |= PMSEARCH_DOCIDS;
	    break;

	case 'i':	/* report search engine info (metrics) */
	    flags |= PMSEARCH_OPT_INFO;
	    break;

	case 'n':	/* command line contains pmindom related entities query string */
	    flags |= PMSEARCH_OPT_INDOM;
	    break;

	case 'N':	/* number of results to report */
	    search_count = strtoul(opts.optarg, NULL, 10);
	    break;

	case 'O':	/* cursor - search starting point */
	    search_offset = strtoul(opts.optarg, NULL, 10);
	    break;

	case 'q':	/* command line contains query string */
	    flags |= PMSEARCH_OPT_QUERY;
	    break;

	case 's':	/* command line contains suggestion string */
	    flags |= PMSEARCH_OPT_SUGGEST;
	    break;

	case 'S':	/* report score (rank) with search hits */
	    flags |= PMSEARCH_SCORES;
	    break;

	case 't':	/* report total number of search hits */
	    flags |= PMSEARCH_TOTALS;
	    break;

	case 'T':	/* report elapsed search execution time */
	    flags |= PMSEARCH_TIMING;
	    break;

	default:
	    opts.errors++;
	    break;
	}
    }

    /*
     * Parse the configuration file for search index path and result
     * count settings.  Uses [pmsearch] section from pmproxy.conf or
     * a dedicated pmsearch.conf file.
     */
    if ((config = pmIniFileSetup(inifile)) == NULL) {
	if (inifile) {
	    pmprintf("%s: cannot setup from configuration file %s\n",
			    pmGetProgname(), inifile);
	    opts.errors++;
	}
    }

    if (!(flags & PMSEARCH_OPT_INFO) && opts.optind >= argc)
	opts.errors++;

    if (opts.errors || (opts.flags & PM_OPTFLAG_EXIT)) {
	int sts = !(opts.flags & PM_OPTFLAG_EXIT);
	pmUsageMessage(&opts);
	exit(sts);
    }

    if ((flags & (PMSEARCH_OPT_INFO | PMSEARCH_OPT_SUGGEST | PMSEARCH_OPT_INDOM)) == 0)
	flags |= PMSEARCH_OPT_QUERY;	/* default */

    if (colour && pmLogLevelIsTTY())
	flags |= PMSEARCH_COLOUR;

    if (opts.optind < argc)
	query = sdsjoin(&argv[opts.optind], argc - opts.optind, " ");

    dp = search_data_init(flags, query, search_count, search_offset);

    dp->settings.callbacks.on_text_result = on_search_result;
    dp->settings.callbacks.on_metrics = on_search_metrics;
    dp->settings.callbacks.on_done = on_search_done;

    dp->settings.module.on_info = on_search_info;
    dp->settings.module.on_setup = on_search_setup;

    if (config)
	pmSearchSetConfiguration(&dp->settings.module, config);

    pmSearchSetup(&dp->settings.module, dp);

    pmSearchClose(&dp->settings.module);

    c = dp->status;
    search_data_free(dp);
    return c;
}
