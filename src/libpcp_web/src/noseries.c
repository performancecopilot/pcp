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
#include "series.h"

static void
noseries(pmSeriesSettings *settings, void *arg)
{
    settings->on_info(PMSERIES_ERROR, "no time series support", arg);
    settings->on_done(PM_ERR_GENERIC, arg);
}

void
pmSeriesDesc(pmSeriesSettings *s, int n, pmSeriesID *i, void *a)
{
    (void)n; (void)i;
    noseries(s, a);
}

void
pmSeriesLabel(pmSeriesSettings *s, int n, pmSeriesID *i, void *a)
{
    (void)n; (void)i;
    noseries(s, a);
}

void
pmSeriesMetric(pmSeriesSettings *s, int n, pmSeriesID *i, void *a)
{
    (void)n; (void)i;
    noseries(s, a);
}

void
pmSeriesInstance(pmSeriesSettings *s, int n, pmSeriesID *i, void *a)
{
    (void)n; (void)i;
    noseries(s, a);
}

void
pmSeriesQuery(pmSeriesSettings *s, const char *q, pmseries_flags f, void *a)
{
    (void)q; (void)f;
    noseries(s, a);
}

void
pmSeriesLoad(pmSeriesSettings *s, const char *q, pmseries_flags f, void *a)
{
    (void)q; (void)f;
    noseries(s, a);
}
