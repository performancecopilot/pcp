/*
 * Copyright (c) 2020 Ashwin Nayak.  All Rights Reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pcp/pmapi.h>
#include <pcp/import.h>

static int  myoverrides(int, pmOptions *);

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("Options"),
    { "hostname", 1, 'h', "HOST", "set hostname" },
    { "timezone", 1, 't', "TIMEZONE", "set timezone" },
    { "outfile", 1, 'o', "OUT", "set outfile" },
    { "metric_name", 1, 'm', "MNAME", "set metric name" },
    { "file_name", 1, 'f', "FILE", "set filename" },
    { "input", 1, 'i', "INPUT", " input to archive" },
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .flags = PM_OPTFLAG_DONE | PM_OPTFLAG_STDOUT_TZ,
    .short_options = "h:t:o:m:f:i:?",
    .long_options = longopts,
    .short_usage = "[options] archive",
    .override = myoverrides,
};

char  
*file_read(char *fn)
{
    /*
     * function reads a file given a input filename
     */
    char        *buffer = 0;
    long        length;
    FILE        *file = fopen (fn, "rb");

    if (file) {
        fseek (file, 0, SEEK_END);
        length = ftell (file);
        fseek (file, 0, SEEK_SET);
        buffer = malloc (length + 1);

        if (buffer){
            fread (buffer, 1, length, file);
        }
        fclose (file);
        buffer[length] = '\0';
    }

    return buffer;
}

void 
pmpaste ( char *fn, char *name, char *outfile, char *host_name, char *timezone, char *input)
{
    /*
     * function archives a string output from a tool
     * input: filename, metricname, outfile, hostname and timezone
     */
    int         code = 0;

    code = pmiStart(outfile, 0);
    if (code < 0) {
        fprintf(stderr, "%s: error starting pmi\n", pmGetProgname());
        exit(EXIT_FAILURE);
    }
    
    code = pmiSetHostname(host_name);
    if (code < 0) {
        fprintf(stderr, "%s: error setting pmi hostname\n", pmGetProgname());
        exit(EXIT_FAILURE);
    }

    code = pmiSetTimezone(timezone);
    if (code < 0) {
        fprintf(stderr, "%s: error setting pmi timezone\n", pmGetProgname());
        exit(EXIT_FAILURE);
    }

    code = pmiAddMetric(name, PM_ID_NULL, PM_TYPE_STRING, PM_INDOM_NULL, 
                        PM_SEM_DISCRETE, pmiUnits(0, 0, 0, 0, 0, 0));
    if (code < 0) {
        fprintf(stderr, "%s: error adding metric\n", pmGetProgname());
        exit(EXIT_FAILURE);
    }

    if (input == NULL) {
        code = pmiPutValue(name, "", file_read(fn));
    }
    else {
        code = pmiPutValue(name, "", "input");
    }
    
    if (code < 0) {
        fprintf(stderr, "%s: error putting value\n", pmGetProgname());
        exit(EXIT_FAILURE);
    }

    code = pmiWrite(time(NULL), 123456);
    if (code < 0) {
        fprintf(stderr, "%s: error in flushing data\n", pmGetProgname());
        exit(EXIT_FAILURE);
    }

    code = pmiEnd();
    if (code < 0) {
        fprintf(stderr, "%s: error in ending process\n", pmGetProgname());
        exit(EXIT_FAILURE);
    }
    
}

/*
 * pmlogpaste has a few options which do not follow the defacto standards
 */
static int
myoverrides(int opt, pmOptions *opts)
{
    if (opt == 'h' || opt == 't' || opt == 'o'|| opt == 'i')
	return 1;	/* we've claimed these, inform pmGetOptions */
    return 0;
}

int 
main (int argc, char *argv[]) 
{
    int         opt = 0;
    char        *file_name = NULL;
    char        *input = NULL;
    char        *metric_name = "output";
    char        *outfile = "archive";
    char        *host_name = "localhost";
    char        *timezone = "UTC";

    while ((opt = pmGetOptions(argc, argv, &opts)) != EOF) {
        switch(opt) {

        case 'f':
            file_name = opts.optarg;
            break;

        case 'm':
            metric_name = opts.optarg;
            break;

        case 'o':
            outfile = opts.optarg;
            break;

        case 'h':
            host_name = opts.optarg;
            break;

        case 't':
            timezone = opts.optarg;
            break;
        
        case 'i':
            input = opts.optarg;
            break;

        case '?':
            opts.errors++;
            break;

        break;
        }
    }

    if (file_name && input) {
	fprintf(stderr, "%s: -f and -i cannot be used together\n", pmGetProgname());
	opts.errors++;
    }

    if (!(file_name || input)) {
	fprintf(stderr, "%s: either filename(-f) should be provided or input(-i)\n", pmGetProgname());
	opts.errors++;
    }

    if (opts.errors) {
	pmUsageMessage(&opts);
	exit(EXIT_FAILURE);
    }

    pmpaste(file_name, metric_name, outfile, host_name, timezone, input);
    fprintf(stdout, "OK\n");

    return 0;
}
