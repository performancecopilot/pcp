/*
 * Copyright (c) 2016 Red Hat.
 * Copyright (c) 2009 Aconex.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include <pcp/mmv_stats.h>

static mmv_instances2_t dogs[] = {
    {  0, "Fido" },
    {  1, "Brutus" },
};

static mmv_instances2_t cats[] = {
    {  10, "Tom" },
};

static mmv_instances2_t mice[] = {
    {  20, "Jerry" },
};

static mmv_instances2_t film[] = {	/* long-named instances */
    {  30, "Dumbo - an animated film produced by Walt Disney Productions, premiered October 23, 1941" },
    {  31, "Bambi - an animated drama film produced by Walt Disney, released August 13, 1942" },
};

static mmv_indom2_t indoms[] = {
    {	.serial = 2,
	.shorttext = "Set of animal names",
	.helptext = "Animal names - cats, dogs, and more",
    },
};

static mmv_metric2_t metrics[] = {
    {	.name = "animals2.tag",
	.item = 33,
	.type = MMV_TYPE_U32,	
	.semantics = MMV_SEM_DISCRETE,
	.dimension = MMV_UNITS(0,0,0,0,0,0),
	.indom = 2,
	.shorttext = "Animal tag identifiers",
	.helptext = "Tags associated with each animal we have found",
    },
};

static inline int dogs_count() { return sizeof(dogs)/sizeof(dogs[0]); }
static inline int cats_count() { return sizeof(cats)/sizeof(cats[0]); }
static inline int mice_count() { return sizeof(mice)/sizeof(mice[0]); }
static inline int film_count() { return sizeof(film)/sizeof(film[0]); }

static inline int indom_count() { return sizeof(indoms)/sizeof(indoms[0]); }
static inline int metric_count() { return sizeof(metrics)/sizeof(metrics[0]); }

static inline int pets_cluster() { return 421; }

int 
main(int argc, char * argv[])
{
    void * addr;
    int i, error = 0;

    if (argc == 2) {
	if (strcmp(argv[1], "dogs2") == 0) {
	    indoms[0].instances = dogs;
	    indoms[0].count = dogs_count();
	}
	else if (strcmp(argv[1], "cats2") == 0) {
	    indoms[0].instances = cats;
	    indoms[0].count = cats_count();
	}
	else if (strcmp(argv[1], "mice2") == 0) {
	    indoms[0].instances = mice;
	    indoms[0].count = mice_count();
	}
	else if (strcmp(argv[1], "film2") == 0) {
	    indoms[0].instances = film;
	    indoms[0].count = film_count();
	}
	else
	    error++;
    }
    else
	error++;

    if (error) {
	fprintf(stderr, "Usage: mmv2_instances <cats|dogs|mice|film>2\n");
	exit(1);
    }

    addr = mmv_stats2_init(argv[1], pets_cluster(), MMV_FLAG_NOPREFIX,
			metrics, metric_count(), indoms, indom_count());
    if (!addr) {
	fprintf(stderr, "mmv_stats_init failed: %s\n", strerror(errno));
	return 1;
    }

    for (i = 0; i < indoms[0].count; i++) {
	mmv_stats_set(addr, "animals2.tag",
		indoms[0].instances[i].external, indoms[0].instances[i].internal);
    }

    return 0;
}
