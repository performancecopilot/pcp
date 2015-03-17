/*
 * Copyright (c) 2014 Red Hat.
 * Copyright (c) 2013 Ken McDonell.
 * Copyright (c) 1995-2002 Silicon Graphics, Inc.  All Rights Reserved.
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
#include "impl.h"
#include <limits.h>



static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    { "verbose", 0, 'v', 0, "verbose output" },
    PMOPT_TIMEZONE,
    PMOPT_HOSTZONE,
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "aD:dilLmst:vZ:z?",
    .long_options = longopts,
    .short_usage = "[options] archive [metricname ...]",
};

int
main(int argc, char *argv[])
{
    int			c;
    int			sts;
    int			zflag = 0;		/* for -z */
    char 		*tz = NULL;		/* for -Z timezone */

    exit(0);
}
