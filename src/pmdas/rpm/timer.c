/*
 * Copyright (c) 2013 Red Hat.
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

#include <pmapi.h>
#include <impl.h>
#include <sys/time.h>
#include <sys/resource.h>

static struct rusage start_rsrc, final_rsrc;
static struct timeval start_time, final_time;
static double user, kernel, elapsed;

double get_user_timer() { return user; }
double get_kernel_timer() { return kernel; }
double get_elapsed_timer() { return elapsed; }

void
start_timing(void)
{
    getrusage(RUSAGE_SELF, &start_rsrc);
    gettimeofday(&start_time, NULL);
}

void
stop_timing(void)
{
    gettimeofday(&final_time, NULL);
    getrusage(RUSAGE_SELF, &final_rsrc);

    /* accumulate the totals as we go */
    user += __pmtimevalSub(&final_rsrc.ru_utime, &start_rsrc.ru_utime);
    kernel += __pmtimevalSub(&final_rsrc.ru_stime, &start_rsrc.ru_stime);
    elapsed += __pmtimevalSub(&final_time, &start_time);
}
