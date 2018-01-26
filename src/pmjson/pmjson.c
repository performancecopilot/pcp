/*
 * Copyright (c) 2017 Red Hat.
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
#include "pmapi.h"
#include "pmjson.h"

static int override(int, pmOptions *);

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("Reporting options"),
    PMOPT_DEBUG,
    { "in", 1, 'i', "FILE", "read input from named FILE" },
    { "out", 1, 'o', "FILE", "write output to named FILE" },
    { "minimal", 0, 'm', 0, "report JSON with optional whitespace removed" },
/*  { "pointer", 1, 'P', "PATH", "report subset of JSON via pointer PATH" }, */
    { "pretty",  0, 'p', 0, "report neatly formatted JSON" },
    { "quiet", 0, 'q', 0, "quiet mode, parse only, do not write to stdout" },
    { "yaml", 0, 'y', 0, "parse JSON input, report a YAML style of output" },
    PMOPT_VERSION,
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "D:mi:o:pqyV?",
    .long_options = longopts,
    .override = override,
};

static int
override(int opt, pmOptions *opts)
{
    (void)opts;
    return (opt == 'p');
}

static int
read_json(char *buffer, int buflen, void *userdata)
{
    FILE	*input = (FILE *)userdata;

    return fread(buffer, 1, buflen, input);
}

int
main(int argc, char **argv)
{
    char	*pointer = NULL, *infile = NULL, *outfile = NULL;
    FILE	*in = stdin, *out = stdout;
    int		c, sts, flags = 0;

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {
	case 'i':	/* input from named file */
	    infile = opts.optarg;
	    break;
	case 'm':	/* no optional whitespace */
	    flags = pmjson_flag_minimal;
	    break;
	case 'o':	/* output to named file */
	    outfile = opts.optarg;
	    break;
	case 'P':	/* search via jsonpointer */
	    pointer = opts.optarg;
	    break;
	case 'p':	/* pretty-print output */
	    flags = pmjson_flag_pretty;
	    break;
	case 'q':	/* no stdout messages */
	    flags = pmjson_flag_quiet;
	    break;
	case 'y':	/* YAML-style output */
	    flags = pmjson_flag_yaml;
	    break;
	default:
	    opts.errors++;
	    break;
	}
    }

    if (opts.optind != argc)
	opts.errors++;

    if (opts.errors || (opts.flags & PM_OPTFLAG_EXIT)) {
	sts = !(opts.flags & PM_OPTFLAG_EXIT);
	pmUsageMessage(&opts);
	exit(sts);
    }

    if (infile != NULL && (infile[0] != '-' && infile[1] != '\0') &&
	(in = fopen(infile, "r")) == NULL) {
	perror(infile);
	exit(2);
    }

    if (outfile != NULL && (outfile[0] != '-' && outfile[1] != '\0') &&
	(out = fopen(outfile, "w")) == NULL) {
	if (in != stdin)
	    fclose(in);
	perror(outfile);
	exit(2);
    }

    if ((sts = pmjsonPrint(out, flags, pointer, read_json, (void *)in)) < 0)
	if (flags != pmjson_flag_quiet)
	    fprintf(stderr, "%s failed: %s\n", pmGetProgname(), pmErrStr(sts));

    if (out != stdout)
	fclose(out);
    if (in != stdin)
	fclose(in);

    return (sts != 0);
}
