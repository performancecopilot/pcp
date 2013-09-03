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

#include <pcp/pmapi.h>
#include <pcp/mmv_stats.h>

static mmv_instances_t products[] = {
    {  0, "Anvils" },
    {  1, "Rockets" },
    {  2, "Giant_Rubber_Bands" },
    {  3, "Rocket_Powered_Anvils" },
};
#define ACME_PRODUCTS_INDOM 61
#define ACME_PRODUCTS_COUNT (sizeof(products)/sizeof(products[0]))

static mmv_indom_t indoms[] = {
    {   .serial = ACME_PRODUCTS_INDOM,
        .count = ACME_PRODUCTS_COUNT,
        .instances = products,
        .shorttext = "Acme products",
        .helptext = "Most popular products produced by the Acme Corporation",
    },
};

static mmv_metric_t metrics[] = {
    {   .name = "products.count",
        .item = 7,
        .type = MMV_TYPE_U64,
        .semantics = MMV_SEM_COUNTER,
        .dimension = MMV_UNITS(0,0,1,0,0,PM_COUNT_ONE),
        .indom = ACME_PRODUCTS_INDOM,
        .shorttext = "Acme factory product throughput",
        .helptext =
"Monotonic increasing counter of products produced in the Acme Corporation\n"
"factory since starting the Acme production application.  Quality guaranteed.",
    },
    {   .name = "products.time",
        .item = 8,
        .type = MMV_TYPE_U64,
        .semantics = MMV_SEM_COUNTER,
        .dimension = MMV_UNITS(0,1,0,0,PM_TIME_USEC,0),
        .indom = ACME_PRODUCTS_INDOM,
        .shorttext = "Machine time spent producing Acme products",
        .helptext =
"Machine time spent producing Acme Corporation products.  Does not include\n"
"time in queues waiting for production machinery.",
    },
    {   .name = "products.queuetime",
        .item = 10,
        .type = MMV_TYPE_U64,
        .semantics = MMV_SEM_COUNTER,
        .dimension = MMV_UNITS(0,1,0,0,PM_TIME_USEC,0),
        .indom = ACME_PRODUCTS_INDOM,
        .shorttext = "Queued time while producing Acme products",
        .helptext =
"Time spent in the queue waiting to build Acme Corporation products,\n"
"while some other Acme product was being built instead of this one.",
    },
};

#define INDOM_COUNT (sizeof(indoms)/sizeof(indoms[0]))
#define METRIC_COUNT (sizeof(metrics)/sizeof(metrics[0]))
#define ACME_CLUSTER 321        /* PMID cluster identifier */

int 
main(int argc, char * argv[])
{
    void *base;
    void *count[ACME_PRODUCTS_COUNT];
    void *machine[ACME_PRODUCTS_COUNT];
    void *inqueue[ACME_PRODUCTS_COUNT];
    unsigned int working;
    unsigned int product;
    unsigned int i;

    base = mmv_stats_init("acme", ACME_CLUSTER, 0,
                        metrics, METRIC_COUNT, indoms, INDOM_COUNT);
    if (!base) {
        perror("mmv_stats_init");
        return 1;
    }

    for (i = 0; i < ACME_PRODUCTS_COUNT; i++) {
        count[i] = mmv_lookup_value_desc(base,
                        "products.count", products[i].external);
        machine[i] = mmv_lookup_value_desc(base,
                        "products.time", products[i].external);
        inqueue[i] = mmv_lookup_value_desc(base,
                        "products.queuetime", products[i].external);
    }

    while (1) {
        /* choose a random number between 0-N -> product */
        product = rand() % ACME_PRODUCTS_COUNT;

        /* assign a time spent "working" on this product */
        working = rand() % 5000;

        /* pretend to "work" so process doesn't burn CPU */
        usleep(working);

        /* update the memory mapped values for this one: */
        /* one more product produced and work time spent */
        mmv_inc_value(base, count[product], 1);
        mmv_inc_value(base, machine[product], working);

        /* all other products are "queued" for this time */
        for (i = 0; i < ACME_PRODUCTS_COUNT; i++)
            if (i != product)
                mmv_inc_value(base, inqueue[i], working);
    }

    mmv_stats_stop("acme", base);
    return 0;
}
