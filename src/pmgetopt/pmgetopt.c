/*
 * Copyright (c) 2014 Red Hat.
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
#include <ctype.h>
#include "pmapi.h"
#include "impl.h"

static int lineno;
static int count;
static char buffer[4096];

static inline char *
skip_whitespace(char *buffer)
{
    while (buffer[0] && isspace(buffer[0]))
	buffer++;
    return buffer;
}

static inline char *
skip_nonwhitespace(char *buffer)
{
    while (buffer[0] && !isspace(buffer[0]))
	buffer++;
    return buffer;
}

static inline char *
seek_character(char *buffer, int seek)
{
    for (; *buffer; buffer++)
	if (seek == (int)(*buffer))
	    return buffer;
    return buffer;
}

/*
 * Parse non-option commands: getopts, (short)usage, or end.
 * The return code indicates whether the end directive was observed.
 */
static int
command(pmOptions *opts, char *buffer)
{
    char *start, *finish;

    start = skip_whitespace(buffer);

    if (strncasecmp(start, "getopt", sizeof("getopt")-1) == 0) {
	start = skip_whitespace(skip_nonwhitespace(start));
	finish = skip_nonwhitespace(start);
	*finish = '\0';
	if (pmDebug & DBG_TRACE_DESPERATE)
	    fprintf(stderr, "%s: getopt command: '%s'\n", pmProgname, start);
	if ((opts->short_options = strdup(start)) == NULL)
	    __pmNoMem("short_options", strlen(start), PM_FATAL_ERR);
	return 0;
    }

    if (strncasecmp(start, "usage", sizeof("usage")-1) == 0) {
	start = skip_whitespace(skip_nonwhitespace(start));
	if (pmDebug & DBG_TRACE_DESPERATE)
	    fprintf(stderr, "%s: usage command: '%s'\n", pmProgname, start);
	if ((opts->short_usage = strdup(start)) == NULL)
	    __pmNoMem("short_usage", strlen(start), PM_FATAL_ERR);
	return 0;
    }

    if (strncasecmp(start, "end", sizeof("end")-1) == 0) {
	if (pmDebug & DBG_TRACE_DESPERATE)
	    fprintf(stderr, "%s: end command\n", pmProgname);
	return 1;
    }

    fprintf(stderr, "%s: unrecognized command: '%s'\n", pmProgname, buffer);
    return 0;
}

static int
append_option(pmOptions *opts, pmLongOptions *longopt)
{
    pmLongOptions *entry;
    size_t size = sizeof(pmLongOptions);

    /* space for existing entries, new entry and the sentinal */
    size = (count + 1) * sizeof(pmLongOptions) + sizeof(pmLongOptions);
    if ((entry = realloc(opts->long_options, size)) == NULL)
	__pmNoMem("append", size, PM_FATAL_ERR);
    opts->long_options = entry;
    entry += count++;
    /* if not first entry: find current sentinal, overwrite with new option */
    memcpy(entry, longopt, sizeof(pmLongOptions));
    memset(entry + 1, 0, sizeof(pmLongOptions));    /* insert new sentinal */
    return 0;
}

static int
append_text(pmOptions *opts, char *buffer, size_t length)
{
    pmLongOptions text = PMAPI_OPTIONS_TEXT("");

    if (pmDebug & DBG_TRACE_DESPERATE)
	fprintf(stderr, "%s: append: '%s'\n", pmProgname, buffer);
    if ((text.message = strdup(buffer)) == NULL)
	__pmNoMem("append_text", length, PM_FATAL_ERR);
    return append_option(opts, &text);
}

static pmLongOptions *
search_long_options(pmLongOptions *stock, const char *name)
{
    pmLongOptions *entry;

    for (entry = stock; entry->long_opt != NULL; entry++)
	if (strcmp(entry->long_opt, name) == 0)
	    return entry;
    return NULL;
}

static pmLongOptions *
search_short_options(pmLongOptions *stock, int opt)
{
    pmLongOptions *entry;

    for (entry = stock; entry->long_opt != NULL; entry++)
	if (entry->short_opt == opt)
	    return entry;
    return NULL;
}

static int
standard_options(pmOptions *opts, char *start)
{
    pmLongOptions stock[] = {
	PMAPI_GENERAL_OPTIONS,
	PMOPT_SPECLOCAL,
	PMOPT_LOCALPMDA,
	PMOPT_HOSTSFILE,
	PMOPT_HOST_LIST,
	PMOPT_ARCHIVE_LIST,
	PMOPT_ARCHIVE_FOLIO,
	PMAPI_OPTIONS_END
    };
    pmLongOptions *entry;

    entry = (start[1] == '-') ?
		search_long_options(stock, start + 2) :
		search_short_options(stock, (int)start[1]);
    if (entry)
	return append_option(opts, entry);
    fprintf(stderr, "%s: cannot find PCP option \"%s\", line %d ignored\n",
		    pmProgname, start, lineno);
    return -EINVAL;
}

static int
options(pmOptions *opts, char *buffer, size_t length)
{
    char *start, *finish, *token;
    pmLongOptions option = { 0 };

    /*
     * Two cases to deal with here - a "standard" option e.g. --host
     * which is presented as a single word (no description) OR a full
     * description of a command line option, in one of these forms:
     *
     *     --label              dump the archive label
     *     --background=COLOR   render background with given color
     *     -d, --desc           get and print metric description
     *     -b=N, --batch=N	fetch N metrics at a time
     *     -L                   use a local context connection
     *     -X=N                 offset resulting values by N units
     */
    if (pmDebug & DBG_TRACE_DESPERATE)
	fprintf(stderr, "%s: parsing option: '%s'", pmProgname, buffer);

    start = skip_whitespace(skip_nonwhitespace(buffer));
    finish = skip_nonwhitespace(start);
    if (start[0] != '-') {
	*finish = '\0';
	return append_text(opts, buffer, length);
    }

    token = skip_whitespace(finish);
    *finish = '\0';

    /* if a single word, this is the standard PCP option case - find it */
    if (!token || *token == '\0')
	return standard_options(opts, start);

    /* handle the first two example cases above -- long option only */
    if (start[1] == '-') {
	token = seek_character(start, '=');
	if (*token == '=') {
	    *token = '\0';	/* e.g. --background=COLOR  render ... */
	    token++;
	    finish = skip_nonwhitespace(token);
	    *finish = '\0';
	    if ((option.argname = strdup(token)) == NULL)
		__pmNoMem("argname", strlen(token), PM_FATAL_ERR);
	    option.has_arg = 1;
	} /* else e.g. --label  dump the archive label */
	if ((option.long_opt = strdup(start + 2)) == NULL)
	    __pmNoMem("longopt", strlen(start), PM_FATAL_ERR);
	token = skip_whitespace(finish + 1);
	if ((option.message = strdup(token)) == NULL)
	    __pmNoMem("message", strlen(token), PM_FATAL_ERR);
	return append_option(opts, &option);
    }

    /* handle next two example cases above -- both long and short options */
    token = seek_character(start, ',');
    if (*token == ',') {
	/* e.g. -b=N, --batch=N   fetch N metrics at a time */
	option.short_opt = (int)start[1];

	/* move onto extracting --batch, [=N], and "fetch..." */
	token++;	/* move past the comma */
	if (*token == '\0' && token - buffer < length)	/* move past a null */
	    token++;
	token = skip_whitespace(token);
	if ((token = seek_character(token, '-')) == NULL ||
	    (token - buffer >= length) || (token[1] != '-')) {
	    fprintf(stderr, "%s: expected long option at \"%s\", line %d ignored\n",
		    pmProgname, token, lineno);
	    return -EINVAL;
	}
	start = token + 2;	/* skip double-dash */
	if ((token = seek_character(start, '=')) != NULL && *token == '=') {
	    *token++ = '\0';
	    option.has_arg = 1;	/* now extract the argument name */
	    finish = skip_nonwhitespace(token);
	    *finish = '\0';
	    if ((option.argname = strdup(token)) == NULL)
		__pmNoMem("argname", strlen(token), PM_FATAL_ERR);
	} else {
	    finish = skip_nonwhitespace(start);
	    *finish = '\0';
	}
	if ((option.long_opt = strdup(start)) == NULL)
	    __pmNoMem("longopt", strlen(start), PM_FATAL_ERR);
	start = skip_whitespace(finish + 1);
	if ((option.message = strdup(start)) == NULL)
	    __pmNoMem("message", strlen(start), PM_FATAL_ERR);
	return append_option(opts, &option);
    }

    /* handle final two example cases above -- short options only */
    if (isspace(start[1])) {
	fprintf(stderr, "%s: expected short option at \"%s\", line %d ignored\n",
		pmProgname, start, lineno);
	return -EINVAL;
    }
    option.long_opt = "";
    option.short_opt = start[1];
    if ((token = seek_character(start, '=')) != NULL && *token == '=') {
	*token++ = '\0';
	option.has_arg = 1;	/* now extract the argument name */
	finish = skip_nonwhitespace(token);
	*finish = '\0';
	if ((option.argname = strdup(token)) == NULL)
	    __pmNoMem("argname", strlen(token), PM_FATAL_ERR);
	/* e.g. -X=N  offset resulting values by N units */
	start = skip_whitespace(finish + 2);
    } else {
	/* e.g. -L    use a local context connection */
	start = skip_whitespace(start + 3);
    }
    if ((option.message = strdup(start)) == NULL)
	__pmNoMem("message", strlen(start), PM_FATAL_ERR);
    return append_option(opts, &option);
}

static char *
build_short_options(pmOptions *opts)
{
    pmLongOptions *entry;
    char *shortopts;
    size_t size;
    int opt, index = 0;

    /* allocate for maximal case - every entry has a short opt and an arg */
    size = 1 + sizeof(char) * 2 * count;
    if ((shortopts = malloc(size)) == NULL)
	__pmNoMem("shortopts", size, PM_FATAL_ERR);

    for (entry = opts->long_options; entry && entry->long_opt; entry++) {
	if ((opt = entry->short_opt) == 0)
	    continue;
	if (opt == '-' || opt == '|' || opt == '"')
	    continue;
	shortopts[index++] = (char)opt;
	if (entry->has_arg)
	    shortopts[index++] = ':';
    }
    shortopts[index] = '\0';
    return shortopts;
}

static int
setup(char *filename, pmOptions *opts)
{
    FILE *fp;
    size_t length;
    int sts = 0, ended = 0;

    if (filename)
	fp = fopen(filename, "r");
    else
	fp = fdopen(STDIN_FILENO, "r");
    if (!fp) {
	fprintf(stderr, "%s: cannot open %s for reading configuration\n",
		pmProgname, filename? filename : "<stdin>");
	return -oserror();
    }

    while (fgets(buffer, sizeof(buffer)-1, fp) != NULL) {
	lineno++;

	length = strlen(buffer);
	if (length > 0 && buffer[length-1] == '\n')
	    buffer[--length] = '\0';

	/*
	 * Check for special command character (hash) - if found process
	 * the few commands it can represent -> getopts/usage/end.
	 * If we're finished already, we just tack the text on unchanged.
	 * Otherwise, we must deal with regular long options/headers/text.
	 */
	if (ended)
	    sts = append_text(opts, buffer, length);
	else if (buffer[0] == '#')
	    sts = ended = command(opts, buffer + 1);
	else
	    sts = options(opts, buffer, length);
	if (sts < 0)
	    break;
    }
    fclose(fp);

    /* if not given a getopt string with short options, just make one */
    if (sts >= 0 && !opts->short_options)
	opts->short_options = build_short_options(opts);

    return sts;
}

static char *
shell_string(const char *string)
{
    const char *p;
    int i = 0;

    buffer[i] = '\0';
    for (p = string; *p != '\0' && i < sizeof(buffer)-6; p++) {
	if (*p == '\'') {
	    buffer[i++] = '\'';
	    buffer[i++] = '\\';
	    buffer[i++] = '\'';
	    buffer[i++] = '\'';
	} else {
	    buffer[i++] = *p;
	}
	buffer[i] = '\0';
    }
    return buffer;
}

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    { "usage", 0, 'u', 0, "generate usage message for calling script" },
    { "config", 1, 'c', "FILE", "read usage configuration from given FILE" },
    { "progname", 1, 'p', 0, "program name of calling script, for error reporting" },
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static pmOptions localopts = {
    .flags = PM_OPTFLAG_POSIX,
    .short_options = "c:D:p:u?",
    .long_options = longopts,
    .short_usage = "[options] -- [progname arguments]",
};

int
main(int argc, char **argv)
{
    pmOptions	opts = { .flags = PM_OPTFLAG_POSIX };
    char	*progname = NULL;
    char	*config = NULL;
    int		c, i, usage = 0;

    /* first parse our own arguments, up to a double-hyphen */
    while ((c = pmgetopt_r(argc, argv, &localopts)) != EOF) {
	switch (c) {
	case 'D':
	    if ((c = __pmParseDebug(localopts.optarg)) < 0) {
		pmprintf("%s: unrecognized debug flag specification (%s)\n",
			pmProgname, localopts.optarg);
		localopts.errors++;
	    } else {
		pmDebug |= c;
	    }
	    break;
	case 'c':
	    config = localopts.optarg;
	    break;
	case 'p':
	    progname = localopts.optarg;
	    break;
	case 'u':
	    usage = 1;
	    break;
	case '?':
	    localopts.errors++;
	    break;
	}
    }

    if (localopts.errors) {
	pmUsageMessage(&localopts);
	exit(1);
    }

    if (setup(config, &opts) < 0)
	exit(1);
    argc -= (localopts.optind - 1);
    argv += (localopts.optind - 1);
    argv[0] = progname ? progname : pmProgname;

    if (usage) {
	if (progname)
	    pmProgname = progname;
	pmUsageMessage(&opts);
	exit(1);
    }

    i = 0;
    while ((c = pmgetopt_r(argc, argv, &opts)) != EOF) {
	if (i++)
	    putc(' ', stdout);
	if (c == 0) {
	    if (opts.optarg)
		printf("--%s '%s'", opts.long_options[opts.index].long_opt,
			shell_string(opts.optarg));
	    else
		printf("--%s", opts.long_options[opts.index].long_opt);
	}
	else if (opts.optarg)
	    printf("-%c '%s'", c, shell_string(opts.optarg));
	else
	    printf("-%c", c);
    }

    /* finally we report any remaining arguments (after a double-dash) */
    if (opts.optind < argc) {
	if (i)
	    putc(' ', stdout);
	printf("--");
	for (i = opts.optind; i < argc; i++)
	    printf(" '%s'", shell_string(argv[i]));
    }
    printf("\n");
    return 0;
}
