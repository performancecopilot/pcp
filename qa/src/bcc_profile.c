/*
 * Copyright (c) 2018 Andreas Gerstmayr.
 */

#include <ctype.h>
#include <pcp/pmapi.h>
#include "libpcp.h"

static time_t exit_time;
static long counter = 0;

void bcc_profile_fn2()
{
    counter++;
}

void bcc_profile_fn1()
{
    struct timeval t;
    while(1) {
        bcc_profile_fn2();

        gettimeofday(&t, 0);
        if (t.tv_sec >= exit_time) {
            exit(0);
        }
    }
}

int
main(int argc, char **argv)
{
    struct timeval t;
    long duration;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s duration\n", argv[0]);
        exit(1);
    }

    duration = strtol(argv[1], NULL, 10);
    gettimeofday(&t, 0);
    exit_time = t.tv_sec + duration;

    bcc_profile_fn1();
    return 0;
}
