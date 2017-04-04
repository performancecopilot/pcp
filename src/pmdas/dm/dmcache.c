/*
 * Device Mapper PMDA - Cache (dm-cache) Stats
 *
 * Copyright (c) 2015 Red Hat.
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
#include "pmda.h"

#include "indom.h"
#include "dmcache.h"

#include <inttypes.h>

static char *dm_setup_cache;

/*
 * Fetches the value for the given metric item and then assigns to pmAtomValue.
 * We check to see if item is in valid range for the metric.
 */
int
dm_cache_fetch(int item, struct cache_stats *cache_stats, pmAtomValue *atom)
{
    if (item < 0 || item >= NUM_CACHE_STATS)
	return PM_ERR_PMID;

    switch(item) {
        case CACHE_SIZE:
            atom->ull = cache_stats->size;
            break;
        case CACHE_META_BLOCKSIZE:
            atom->ul = cache_stats->meta_blocksize;
            break;
        case CACHE_META_USED:
            atom->ull = cache_stats->meta_used;
            break;
        case CACHE_META_TOTAL:
            atom->ull = cache_stats->meta_total;
            break;
        case CACHE_BLOCKSIZE:
            atom->ul = cache_stats->cache_blocksize;
            break;
        case CACHE_USED:
            atom->ull = cache_stats->cache_used;
            break;
        case CACHE_TOTAL:
            atom->ull = cache_stats->cache_total;
            break;
        case CACHE_READHITS:
            atom->ul = cache_stats->read_hits;
            break;
        case CACHE_READMISSES:
            atom->ul = cache_stats->read_misses;
            break;
        case CACHE_WRITEHITS:
            atom->ul = cache_stats->write_hits;
            break;
        case CACHE_WRITEMISSES:
            atom->ul = cache_stats->write_misses;
            break;
        case CACHE_DEMOTIONS:
            atom->ul = cache_stats->demotions;
            break;
        case CACHE_PROMOTIONS:
            atom->ul = cache_stats->promotions;
            break;
        case CACHE_DIRTY:
            atom->ul = cache_stats->dirty;
            break;
        case CACHE_IOMODE_CODE:
            atom->ul = cache_stats->io_mode_code;
            break;
        case CACHE_IOMODE:
            atom->cp = cache_stats->io_mode;
            break;
    }     
    return 1;
}

/* 
 * Grab output from dmsetup status (or read in from cat when under QA),
 * Match the data to the pool which we wish to update the metrics and
 * assign the values to pool_stats. 
 */
int
dm_refresh_cache(const char *cache_name, struct cache_stats *cache_stats)
{
    char *token;
    uint64_t size_start, size_end;
    uint32_t mbsize, cbsize;
    char buffer[BUFSIZ];
    FILE *fp;

    if ((fp = popen(dm_setup_cache, "r")) == NULL)
        return -oserror();

    while (fgets(buffer, sizeof(buffer) -1, fp)) {
        if (!strstr(buffer, ":") || strstr(buffer, "Fail"))
            continue;

        token = strtok(buffer, ":");
        if (strcmp(token, cache_name) != 0)
            continue;
        token = strtok(NULL, ":");

        /* Pattern match our output to the given cache status
         * output (minus cache name). 
         * The format is:
         * <name>: <start> <end> <target>
         *  <metadata block size> <used metadata blocks>/<total metadata blocks>
         *  <cache block size><used data blocks>/<total data blocks> <read hits>
         *  <read misses> <write hits> <write misses> <demotions> <promotions>
         *  <dirty> <#features> <features>*
         */
        sscanf(token, " %"SCNu64" %"SCNu64" cache %"SCNu32" %"SCNu64"/%"SCNu64" %"SCNu32" %"SCNu64"/%"SCNu64" %"SCNu32" %"SCNu32" %"SCNu32" %"SCNu32" %"SCNu32" %"SCNu32" %"SCNu64" %"SCNu32" %s %*d",
                &size_start,
                &size_end,
                &cache_stats->meta_blocksize,
                &cache_stats->meta_used,
                &cache_stats->meta_total,
                &cache_stats->cache_blocksize,
                &cache_stats->cache_used,
                &cache_stats->cache_total,
                &cache_stats->read_hits,
                &cache_stats->read_misses,
                &cache_stats->write_hits,
                &cache_stats->write_misses,
                &cache_stats->demotions,
                &cache_stats->promotions,
                &cache_stats->dirty,
                &cache_stats->io_mode_code,
                cache_stats->io_mode
        );
        cache_stats->size = (size_end - size_start);

        /* Extra math to work out block sizes and byte values */
        mbsize = cache_stats->meta_blocksize * 512;  /* sectors -> blocks */
        cbsize = cache_stats->cache_blocksize * 512; /* sectors -> blocks */

        cache_stats->meta_used = cache_stats->meta_used * (mbsize / 1024);
        cache_stats->meta_total = cache_stats->meta_total * (mbsize / 1024);

        cache_stats->cache_used = cache_stats->cache_used * (cbsize / 1024);
        cache_stats->cache_total = cache_stats->cache_total * (cbsize / 1024);

        cache_stats->dirty = cache_stats->dirty * (cbsize / 1024);
    }

    if (pclose(fp) != 0)
        return -oserror(); 

    return 0;
}

/*
 * Update the device mapper cache instance domain. This will change
 * as volumes are created, activated and removed.
 *
 * Using the pmdaCache interfaces simplifies things and provides us
 * with guarantees around consistent instance numbering in all of
 * those interesting corner cases.
 */
int
dm_cache_instance_refresh(void)
{
    int sts;
    FILE *fp;
    char buffer[BUFSIZ];
    struct cache_stats *cache;
    pmInDom indom = dm_indom(DM_CACHE_INDOM);

    pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

    /* 
     * update indom cache based off of thin pools listed by dmsetup
     */
    if ((fp = popen(dm_setup_cache, "r")) == NULL)
        return -oserror();

    while (fgets(buffer, sizeof(buffer) -1, fp)) {
        if (!strstr(buffer, ":"))
            continue;
        strtok(buffer, ":");

        /*
         * at this point buffer contains our thin volume names
         * this will be used to map stats to file-system instances
         */
        sts = pmdaCacheLookupName(indom, buffer, NULL, (void **)&cache);
        if (sts == PM_ERR_INST || (sts >= 0 && cache == NULL)) {
            cache = calloc(1, sizeof(*cache));
            if (cache == NULL) {
                if (pclose(fp) != 0)
                    return -oserror();
                return PM_ERR_AGAIN;
            }
        }   
	else if (sts < 0)
	    continue;

	/* (re)activate this entry for the current query */
	pmdaCacheStore(indom, PMDA_CACHE_ADD, buffer, (void *)cache);
    }

    if (pclose(fp) != 0)
        return -oserror(); 

    return 0;
}

void
dm_cache_setup(void)
{
    static char dmcache_command[] = "dmsetup status --target cache";
    char *env_command;

    /* allow override at startup for QA testing */
    if ((env_command = getenv("DM_SETUP_CACHE")) != NULL)
        dm_setup_cache = env_command;
    else
        dm_setup_cache = dmcache_command;
}
