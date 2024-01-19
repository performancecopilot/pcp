/*
 * Copyright (c) 2020 Ashwin Nayak.  All Rights Reserved.
 * Copyright (c) 2020 Red Hat.  All Rights Reserved.
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
#include "libpcp.h"
#include "import.h"
#include <string.h>

static int  myoverrides(int, pmOptions *);

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("Options"),
    { "hostname", 1, 'h', "HOST", "set hostname" },
    { "timezone", 1, 't', "TZ", "set timezone" },
    { "outfile", 1, 'o', "OUT", "set outfile" },
    { "metric", 1, 'm', "NAME", "set metric name" },
    { "file", 1, 'f', "FILE", "set filename" },
	{ "label", 1, 'l', "LABEL", "set label"},
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .flags = PM_OPTFLAG_DONE | PM_OPTFLAG_STDOUT_TZ,
    .short_options = "f:h:m:o:t:l:?",
    .long_options = longopts,
    .short_usage = "[options] archive",
    .override = myoverrides,
};

static char *input;
static size_t input_length;
static char hostname_buffer[MAXHOSTNAMELEN];

/*
 * Append the given buffer to a global (accumulating) string.
 * No assumptions are made about buffer termination (i.e. not
 * explictly NULL terminated here - callers ensure this if it
 * is needed, depending on the context).
 */
static void
append_input(const char *buffer, size_t length)
{
    void	*p;

    if ((p = realloc(input, input_length + length)) == NULL) {
	fprintf(stderr, "%s: out of memory on input\n", pmGetProgname());
	exit(EXIT_FAILURE);
    }
    memcpy(p + input_length, buffer, length);
    input_length += length;
    input = p;
}

/*
 * Reads the contents of a file given an input filename (or '-' for stdin),
 * returns a single buffer.
 */
char *
slurp(const char *filename)
{
    char	buffer[BUFSIZ];
    size_t	length;
    FILE	*file;

    if (strcmp(filename, "-") != 0)
	file = fopen(filename, "r");
    else
	file = stdin;

    if (file) {
	while (!feof(file)) {
	    if ((length = fread(buffer, 1, BUFSIZ, file)) > 0)
		append_input(buffer, length);
	}
	if (strcmp(filename, "-") != 0)
	    fclose(file);
	append_input("\0", 1);	/* ensure string termination */
    }
    return input;
}

/*
 * archives the output from a tool
 */
void 
pmlogpaste(const char *filename, const char *metric,
	   const char *hostname, const char *tz,
	   const char *value, char **labels, int nlabels,
	   struct timespec *timestamp)
{
    int		i, sts;

    if ((sts = pmiStart(filename, 0)) < 0) {
	fprintf(stderr, "%s: error starting log import: %s\n",
			pmGetProgname(), pmiErrStr(sts));
	exit(EXIT_FAILURE);
    }

    if ((sts = pmiSetHostname(hostname)) < 0) {
	fprintf(stderr, "%s: error setting log hostname: %s\n",
			pmGetProgname(), pmiErrStr(sts));
	exit(EXIT_FAILURE);
    }

    if ((sts = pmiSetTimezone(tz)) < 0) {
	fprintf(stderr, "%s: error setting log timezone: %s\n",
			pmGetProgname(), pmiErrStr(sts));
	exit(EXIT_FAILURE);
    }

    if ((sts = pmiAddMetric(metric, PM_ID_NULL, PM_TYPE_STRING, PM_INDOM_NULL,
			    PM_SEM_DISCRETE, pmiUnits(0, 0, 0, 0, 0, 0))) < 0) {
	fprintf(stderr, "%s: error adding metric descriptor: %s\n",
			pmGetProgname(), pmiErrStr(sts));
	exit(EXIT_FAILURE);
    }

    if ((sts = pmiPutValue(metric, NULL, value)) < 0) {
	fprintf(stderr, "%s: error adding metric value: %s\n",
			pmGetProgname(), pmiErrStr(sts));
	exit(EXIT_FAILURE);
    }

    for (i = 0; i < nlabels; i++) {
	char 	*temp = strdup(*(labels + i));
	char	*name = strtok(temp, ":");
	char	*data = strtok(NULL, ":");

	if (name == NULL) {
	    fprintf(stderr, "%s: invalid label token %s: %s\n",
			pmGetProgname(), temp, pmiErrStr(sts));
	    exit(EXIT_FAILURE);
	}
	if ((sts = pmiPutLabel(PM_LABEL_CONTEXT, PM_ID_NULL, PM_IN_NULL, name, data)) < 0) {
	    fprintf(stderr, "%s: error adding label %s:%s: %s\n",
			pmGetProgname(), name, data, pmiErrStr(sts));
	    exit(EXIT_FAILURE);
	}
	free(temp);
    }
    
    if ((sts = pmiWrite(timestamp->tv_sec, timestamp->tv_nsec / 1000)) < 0) {
	fprintf(stderr, "%s: error writing archive: %s\n",
			pmGetProgname(), pmiErrStr(sts));
	exit(EXIT_FAILURE);
    }

    if ((sts = pmiEnd()) < 0) {
	fprintf(stderr, "%s: error in ending log writing: %s\n",
			pmGetProgname(), pmiErrStr(sts));
	exit(EXIT_FAILURE);
    }
}

/*
 * pmlogpaste has a few options which do not follow the defacto standards
 */
static int
myoverrides(int opt, pmOptions *options)
{
    if (opt == 'h' || opt == 't')
	return 1;	/* we've claimed these, inform pmGetOptions */
    return 0;
}

int 
main(int argc, char *argv[])
{
    int		opt, exitsts;
    char	*filename = NULL;
    char	*metric = NULL;
    char	*outfile = NULL;
    char	*hostname = NULL;
    char	*tz = NULL;
    char	**labels = NULL;
    int 	nlabels = 0;
    struct timespec timestamp = {0};

    while ((opt = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch(opt) {

	case 'f':
	    filename = opts.optarg;
	    break;

	case 'm':
	    metric = opts.optarg;
	    break;

	case 'o':
	    outfile = opts.optarg;
	    break;

	case 'h':
	    hostname = opts.optarg;
	    break;

	case 't':
	    tz = opts.optarg;
	    break;
	
	case 'l':
	    nlabels++;
	    labels = (char **)realloc(labels, nlabels * (sizeof(char *)));
	    if (labels == NULL)
		pmNoMem("pmlogpaste.labels", nlabels * sizeof(char *), PM_FATAL_ERR);
	    *(labels + nlabels - 1) = opts.optarg;
	    break;

	case '?':
	    break;

	default:
	    opts.errors++;
	    break;
	}
    }

    if (filename && opts.optind < argc) {
	fprintf(stderr,
		"%s: -f/--file cannot be used with command line input\n",
		pmGetProgname());
	opts.errors++;
    }

    if (opts.errors || (opts.flags & PM_OPTFLAG_EXIT)) {
	exitsts = !(opts.flags & PM_OPTFLAG_EXIT);
	pmUsageMessage(&opts);
	exit(exitsts);
    }

    if (filename == NULL && opts.optind < argc) {
	for (opt = opts.optind; opt < argc; opt++) {
	    append_input(argv[opt], strlen(argv[opt]));
	    append_input(" ", 1);
	}
	input[input_length-1] = '\0';
    } else {
	if (filename == NULL)
	    filename = "-";	/* read from standard input when nowhere else */
	slurp(filename);
    }

    if (metric == NULL)
	metric = "paste.value";		/* default metric name */

    if (outfile == NULL)
	outfile = "paste";		/* default archive name */

    if (tz == NULL)
	tz = __pmTimezone();

    if (__pmGetTimespec(&timestamp) < 0)	/* high resolution timestamp */
	timestamp.tv_sec = time(NULL);

    if (hostname == NULL) {
	if ((gethostname(hostname_buffer, sizeof(hostname_buffer))) < 0)
	    hostname = "localhost";
	else
	    hostname = &hostname_buffer[0];
    }

    pmlogpaste(outfile, metric, hostname, tz, input, labels, nlabels, &timestamp);

    if (labels)
	free(labels);
    return 0;
}
