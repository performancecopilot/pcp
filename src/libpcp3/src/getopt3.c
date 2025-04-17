/*
 * PMAPI v3 argument parsing for all PMAPI client tools.
 *
 * Copyright (c) 2014-2018,2020-2022 Red Hat.
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

#undef PMAPI_VERSION
#define PMAPI_VERSION 3		/* pmOptions with struct timespec */
#include "pmapi.h"
#include "libpcp.h"
#include "internal.h"

void
__pmParseTimeWindow3(pmOptions *opts,
	struct timespec *first_boundary, struct timespec *last_boundary)
{
    char *msg = NULL;

    if (pmParseHighResTimeWindow(
			opts->start_optarg, opts->finish_optarg,
			opts->align_optarg, opts->origin_optarg,
			first_boundary, last_boundary,
			&opts->start, &opts->finish, &opts->origin, &msg) < 0) {
	pmprintf("%s: invalid time window.\n%s\n", pmGetProgname(), msg);
	opts->errors++;
    }
    if (msg)
	free(msg);
}

void
__pmSetSampleInterval3(pmOptions *opts, char *arg)
{
    char *endnum;
    int sts;

    if ((sts = pmParseHighResInterval(arg, &opts->interval, &endnum)) < 0) {
	pmprintf("%s: -t argument not in %s(3) format:\n",
		pmGetProgname(), "pmParseHighResInterval");
	pmprintf("%s\n", endnum);
	opts->errors++;
	free(endnum);
    }
}
