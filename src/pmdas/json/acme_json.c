/*
 * Copyright (c) 2013, 2017 Red Hat.
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
#include <sys/stat.h>
#include <stdio.h>

char *products[] = {
    "Anvils",
    "Rockets",
    "Giant_Rubber_Bands"
};

#define ACME_PRODUCTS_COUNT (sizeof(products)/sizeof(products[0]))

typedef struct {
    char *name;
    char *type;
    char *units;
    char *shorttext;
    char *semantics;
    char *helptext;
} metric_t;

metric_t metrics[] = {
    {   .name = "count",
        .type = "integer",
	.units = "count",
        .semantics = "counter",
        .shorttext = "Acme factory product throughput",
        .helptext =
"Monotonic increasing counter of products produced in the Acme Corporation\n"
"factory since starting the Acme production application.  Quality guaranteed.",
    },
    {   .name = "time",
        .type = "integer",
	.units = "microsec",
        .semantics = "counter",
        .shorttext = "Machine time spent producing Acme products",
        .helptext =
"Machine time spent producing Acme Corporation products.  Does not include\n"
"time in queues waiting for production machinery.",
    },
    {   .name = "queuetime",
        .type = "integer",
	.units = "microsec",
        .semantics = "counter",
        .shorttext = "Queued time while producing Acme products",
        .helptext =
"Time spent in the queue waiting to build Acme Corporation products,\n"
"while some other Acme product was being built instead of this one.",
    },
};

#define METRIC_COUNT (sizeof(metrics)/sizeof(metrics[0]))

#define PRODUCT_COUNTER 0
#define TIME_COUNTER 1
#define QUEUE_COUNTER 2

static int values[ACME_PRODUCTS_COUNT][METRIC_COUNT];


void write_metadata(void)
{
    int i;
    FILE *meta_fd;

    meta_fd = fopen("metadata.json", "w+");
    if (meta_fd==NULL) goto fail;

    fprintf(meta_fd, "{\n\"prefix\" : \"acme\",\n");
    fprintf(meta_fd, "\t\"metrics\" : [\n");
    fprintf(meta_fd, "\t{\n");
    fprintf(meta_fd, "\t\t\"name\": \"products\",\n");
    fprintf(meta_fd, "\t\t\"pointer\": \"/productdata\",\n");
    fprintf(meta_fd, "\t\t\"type\": \"array\",\n");
    fprintf(meta_fd, "\t\t\"description\": \"acme product data\",\n");
    fprintf(meta_fd, "\t\t\"index\": \"/product\",\n");
    fprintf(meta_fd, "\t\t\"metrics\": [\n");
    for (i=0; i< METRIC_COUNT; ++i) {
	fprintf(meta_fd, "\t\t{\n");
	fprintf(meta_fd, "\t\t\t\"name\": \"%s\",\n", metrics[i].name);
	fprintf(meta_fd, "\t\t\t\"pointer\": \"/%s\",\n", metrics[i].name);
	fprintf(meta_fd, "\t\t\t\"type\": \"%s\",\n", metrics[i].type);
	fprintf(meta_fd, "\t\t\t\"units\": \"%s\",\n", metrics[i].units);
	fprintf(meta_fd, "\t\t\t\"semantics\": \"%s\",\n", metrics[i].semantics);
	fprintf(meta_fd, "\t\t\t\"description\": \"%s\"\n", metrics[i].shorttext);
	fprintf(meta_fd, "\t\t}");
	if(i< METRIC_COUNT-1) fprintf(meta_fd, ",");
	fprintf(meta_fd, "\n");
    }
    fprintf(meta_fd, "\t\t]\n");

    fprintf(meta_fd, "\t}\n");
    fprintf(meta_fd, "\t]\n");
    fprintf(meta_fd, "}\n");

    fclose(meta_fd);

    return;

 fail:
    printf("unable to open metadata.json file\n");
    exit(1);
    return;
}

void write_data(void)
{
    int i, j;
    FILE *data_fd;

    /* create fs for data.json */
    data_fd = fopen("data.json", "w+");
    if (data_fd==NULL) goto fail;
    
    fprintf(data_fd, "{\n");
    fprintf(data_fd, "\t\"productdata\": [\n");
    for (i=0; i<ACME_PRODUCTS_COUNT; ++i) {
	fprintf(data_fd, "\t\t{\n");
	fprintf(data_fd, "\t\t\t\"product\": \"%s\",\n", products[i]);
	for (j=0; j<METRIC_COUNT; ++j) {
	    fprintf(data_fd, "\t\t\t\"%s\": %d", metrics[j].name, values[i][j]);
	    if(j< METRIC_COUNT-1) fprintf(data_fd, ",");
	    fprintf(data_fd, "\n");
	}
	fprintf(data_fd, "\t\t}");
	if(i< ACME_PRODUCTS_COUNT-1) fprintf(data_fd, ",");
	fprintf(data_fd, "\n");
    }
    fprintf(data_fd, "\t]\n");
    fprintf(data_fd, "}\n");

    fclose(data_fd);

    return;

 fail:
    fprintf(stderr, "unable to write data.json file\n");
    exit(1);
}

int 
main(int argc, char * argv[])
{
    unsigned int working;
    unsigned int product;
    unsigned int i;

    /* create metadata.json and data.json */
    write_metadata();
    write_data();

    while (1) {
        /* choose a random number between 0-N -> product */
        product = rand() % ACME_PRODUCTS_COUNT;

        /* assign a time spent "working" on this product */
        working = rand() % 50000;

        /* pretend to "work" so process doesn't burn CPU */
        usleep(working);

        /* update the memory mapped values for this one: */
        /* one more product produced and work time spent */
        values[product][TIME_COUNTER] += working; /* API */
        values[product][PRODUCT_COUNTER] += 1;

        /* all other products are "queued" for this time */
        for (i = 0; i < ACME_PRODUCTS_COUNT; i++)
            if (i != product) {
                values[i][QUEUE_COUNTER] += working;
	    }
	write_data();
    }

    return 0;
}
