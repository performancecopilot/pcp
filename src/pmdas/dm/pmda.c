/*
 * Device Mapper PMDA
 *
 * Copyright (c) 2015,2018 Red Hat.
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
#include "pmda.h"

#include "indom.h"
#include "domain.h"
#include "dmthin.h"
#include "dmcache.h"
#include "dmstats.h"
#include "vdo.h"

static int		_isDSO = 1; /* for local contexts */

enum {
    CLUSTER_CACHE = 0,		/* DM-Cache Caches */
    CLUSTER_POOL = 1,		/* DM-Thin Pools */
    CLUSTER_VOL = 2,		/* DM-Thin Volumes */
    CLUSTER_DM_COUNTER = 3,	/* Dmstats basic counter */
    CLUSTER_DM_HISTOGRAM = 4,	/* Dmstats latency histogram */
    CLUSTER_VDODEV = 5,		/* VDO per-device statistics */
    NUM_CLUSTERS
};

/*
 * all metrics supported in this PMDA - one table entry for each
 */
static pmdaMetric metrictable[] = {
    /* DMCACHE_STATS */
    { .m_desc =  {
        PMDA_PMID(CLUSTER_CACHE, CACHE_SIZE),
        PM_TYPE_U64, DM_CACHE_INDOM, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_CACHE, CACHE_META_BLOCKSIZE),
        PM_TYPE_U32, DM_CACHE_INDOM, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_CACHE, CACHE_META_USED),
        PM_TYPE_U64, DM_CACHE_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_CACHE, CACHE_META_TOTAL),
        PM_TYPE_U64, DM_CACHE_INDOM, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_CACHE, CACHE_BLOCKSIZE),
        PM_TYPE_U32, DM_CACHE_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_CACHE, CACHE_USED),
        PM_TYPE_U64, DM_CACHE_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_CACHE, CACHE_TOTAL),
        PM_TYPE_U64, DM_CACHE_INDOM, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_CACHE, CACHE_READHITS),
        PM_TYPE_U32, DM_CACHE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_CACHE, CACHE_READMISSES),
        PM_TYPE_U32, DM_CACHE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_CACHE, CACHE_WRITEHITS),
        PM_TYPE_U32, DM_CACHE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_CACHE, CACHE_WRITEMISSES),
        PM_TYPE_U32, DM_CACHE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_CACHE, CACHE_DEMOTIONS),
        PM_TYPE_U32, DM_CACHE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_CACHE, CACHE_PROMOTIONS),
        PM_TYPE_U32, DM_CACHE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_CACHE, CACHE_DIRTY),
        PM_TYPE_U32, DM_CACHE_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_CACHE, CACHE_IOMODE_CODE),
        PM_TYPE_U32, DM_CACHE_INDOM, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_CACHE, CACHE_IOMODE),
        PM_TYPE_STRING, DM_CACHE_INDOM, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0,0,1,0,0,0) }, },
    /* DMTHIN_POOL_STATS */
    { .m_desc =  {
        PMDA_PMID(CLUSTER_POOL, POOL_SIZE),
        PM_TYPE_U64, DM_THIN_POOL_INDOM, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
       { .m_desc =  {
        PMDA_PMID(CLUSTER_POOL, POOL_TRANS_ID),
        PM_TYPE_U64, DM_THIN_POOL_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_POOL, POOL_META_USED),
        PM_TYPE_U64, DM_THIN_POOL_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_POOL, POOL_META_TOTAL),
        PM_TYPE_U64, DM_THIN_POOL_INDOM, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_POOL, POOL_DATA_USED),
        PM_TYPE_U64, DM_THIN_POOL_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_POOL, POOL_DATA_TOTAL),
        PM_TYPE_U64, DM_THIN_POOL_INDOM, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_POOL, POOL_HELD_ROOT),
        PM_TYPE_STRING, DM_THIN_POOL_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_POOL, POOL_DISCARD_PASSDOWN),
        PM_TYPE_STRING, DM_THIN_POOL_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_POOL, POOL_READ_MODE),
        PM_TYPE_STRING, DM_THIN_POOL_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_POOL, POOL_NO_SPACE_MODE),
        PM_TYPE_STRING, DM_THIN_POOL_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    /* DMTHIN_VOL_STATS */
    { .m_desc = {
        PMDA_PMID(CLUSTER_VOL, VOL_SIZE),
        PM_TYPE_U64, DM_THIN_VOL_INDOM, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_VOL, VOL_NUM_MAPPED_SECTORS),
        PM_TYPE_U64, DM_THIN_VOL_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_VOL, VOL_HIGHEST_MAPPED_SECTORS),
        PM_TYPE_U64, DM_THIN_VOL_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    /* DM_STATS Basic Counters */
    { .m_desc = {
        PMDA_PMID(CLUSTER_DM_COUNTER, PM_DM_STATS_READS),
        PM_TYPE_U64, DM_STATS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DM_COUNTER, PM_DM_STATS_READS_MERGED),
        PM_TYPE_U64, DM_STATS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DM_COUNTER, PM_DM_STATS_READ_SECTORS),
        PM_TYPE_U64, DM_STATS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DM_COUNTER, PM_DM_STATS_READ_NSECS),
        PM_TYPE_U64, DM_STATS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DM_COUNTER, PM_DM_STATS_WRITES),
        PM_TYPE_U64, DM_STATS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DM_COUNTER, PM_DM_STATS_WRITES_MERGED),
        PM_TYPE_U64, DM_STATS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DM_COUNTER, PM_DM_STATS_WRITE_SECTORS),
        PM_TYPE_U64, DM_STATS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DM_COUNTER, PM_DM_STATS_WRITE_NSECS),
        PM_TYPE_U64, DM_STATS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DM_COUNTER, PM_DM_STATS_IO_IN_PROGRESS),
        PM_TYPE_U64, DM_STATS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DM_COUNTER, PM_DM_STATS_IO_NSECS),
        PM_TYPE_U64, DM_STATS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DM_COUNTER, PM_DM_STATS_WEIGHTED_IO_NSECS),
        PM_TYPE_U64, DM_STATS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DM_COUNTER, PM_DM_STATS_TOTAL_READ_NSECS),
        PM_TYPE_U64, DM_STATS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DM_COUNTER, PM_DM_STATS_TOTAL_WRITE_NSECS),
        PM_TYPE_U64, DM_STATS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) }, },
    /* DM STATS latency histogram */
    { .m_desc = {
        PMDA_PMID(CLUSTER_DM_HISTOGRAM, PM_DM_HISTOGRAM_COUNT),
        PM_TYPE_U64, DM_HISTOGRAM_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    /*
    { .m_desc = {
        PMDA_PMID(CLUSTER_DM_HISTOGRAM, PM_DM_HISTOGRAM_PERCENT),
        PM_TYPE_FLOAT, DM_HISTOGRAM_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    */
    { .m_desc = {
        PMDA_PMID(CLUSTER_DM_HISTOGRAM, PM_DM_HISTOGRAM_BIN),
        PM_TYPE_U64, DM_HISTOGRAM_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    /* VDODEV_STATS */
    { .m_user = (void *) "allocator_slab_count",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_ALLOCATOR_SLAB_COUNT),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "allocator_slabs_opened",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_ALLOCATOR_SLABS_OPENED),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "allocator_slabs_reopened",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_ALLOCATOR_SLABS_REOPENED),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_acknowledged_discard",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_ACKNOWLEDGED_DISCARD),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_acknowledged_flush",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_ACKNOWLEDGED_FLUSH),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_acknowledged_fua",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_ACKNOWLEDGED_FUA),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_acknowledged_partial_discard",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_ACKNOWLEDGED_PARTIAL_DISCARD),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_acknowledged_partial_flush",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_ACKNOWLEDGED_PARTIAL_FLUSH),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_acknowledged_partial_fua",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_ACKNOWLEDGED_PARTIAL_FUA),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_acknowledged_partial_read",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_ACKNOWLEDGED_PARTIAL_READ),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_acknowledged_partial_write",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_ACKNOWLEDGED_PARTIAL_WRITE),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_acknowledged_read",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_ACKNOWLEDGED_READ),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_acknowledged_write",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_ACKNOWLEDGED_WRITE),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_in_discard",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_IN_DISCARD),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_in_flush",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_IN_FLUSH),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_in_fua",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_IN_FUA),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_in_partial_discard",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_IN_PARTIAL_DISCARD),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_in_partial_flush",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_IN_PARTIAL_FLUSH),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_in_partial_fua",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_IN_PARTIAL_FUA),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_in_partial_read",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_IN_PARTIAL_READ),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_in_partial_write",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_IN_PARTIAL_WRITE),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_in_progress_discard",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_IN_PROGRESS_DISCARD),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_in_progress_flush",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_IN_PROGRESS_FLUSH),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_in_progress_fua",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_IN_PROGRESS_FUA),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_in_progress_read",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_IN_PROGRESS_READ),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_in_progress_write",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_IN_PROGRESS_WRITE),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_in_read",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_IN_READ),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_in_write",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_IN_WRITE),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_journal_completed_discard",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_JOURNAL_COMPLETED_DISCARD),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_journal_completed_flush",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_JOURNAL_COMPLETED_FLUSH),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_journal_completed_fua",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_JOURNAL_COMPLETED_FUA),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_journal_completed_read",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_JOURNAL_COMPLETED_READ),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_journal_completed_write",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_JOURNAL_COMPLETED_WRITE),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_journal_discard",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_JOURNAL_DISCARD),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_journal_flush",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_JOURNAL_FLUSH),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_journal_fua",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_JOURNAL_FUA),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_journal_read",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_JOURNAL_READ),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_journal_write",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_JOURNAL_WRITE),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_meta_completed_discard",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_META_COMPLETED_DISCARD),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_meta_completed_flush",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_META_COMPLETED_FLUSH),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_meta_completed_fua",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_META_COMPLETED_FUA),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_meta_completed_read",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_META_COMPLETED_READ),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_meta_completed_write",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_META_COMPLETED_WRITE),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_meta_discard",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_META_DISCARD),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_meta_flush",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_META_FLUSH),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_meta_fua",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_META_FUA),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_meta_read",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_META_READ),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_meta_write",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_META_WRITE),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_out_completed_discard",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_OUT_COMPLETED_DISCARD),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_out_completed_flush",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_OUT_COMPLETED_FLUSH),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_out_completed_fua",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_OUT_COMPLETED_FUA),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_out_completed_read",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_OUT_COMPLETED_READ),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_out_completed_write",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_OUT_COMPLETED_WRITE),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_out_discard",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_OUT_DISCARD),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_out_flush",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_OUT_FLUSH),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_out_fua",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_OUT_FUA),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_out_read",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_OUT_READ),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_out_write",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_OUT_WRITE),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_page_cache_completed_discard",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_PAGE_CACHE_COMPLETED_DISCARD),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_page_cache_completed_flush",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_PAGE_CACHE_COMPLETED_FLUSH),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_page_cache_completed_fua",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_PAGE_CACHE_COMPLETED_FUA),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_page_cache_completed_read",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_PAGE_CACHE_COMPLETED_READ),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_page_cache_completed_write",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_PAGE_CACHE_COMPLETED_WRITE),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_page_cache_discard",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_PAGE_CACHE_DISCARD),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_page_cache_flush",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_PAGE_CACHE_FLUSH),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_page_cache_fua",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_PAGE_CACHE_FUA),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_page_cache_read",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_PAGE_CACHE_READ),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "bios_page_cache_write",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BIOS_PAGE_CACHE_WRITE),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "block_map_cache_pressure",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BLOCK_MAP_CACHE_PRESSURE),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "block_map_cache_size",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BLOCK_MAP_CACHE_SIZE),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "block_map_clean_pages",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BLOCK_MAP_CLEAN_PAGES),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "block_map_dirty_pages",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BLOCK_MAP_DIRTY_PAGES),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "block_map_discard_required",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BLOCK_MAP_DISCARD_REQUIRED),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "block_map_failed_pages",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BLOCK_MAP_FAILED_PAGES),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "block_map_failed_reads",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BLOCK_MAP_FAILED_READS),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "block_map_failed_writes",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BLOCK_MAP_FAILED_WRITES),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "block_map_fetch_required",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BLOCK_MAP_FETCH_REQUIRED),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "block_map_flush_count",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BLOCK_MAP_FLUSH_COUNT),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "block_map_found_in_cache",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BLOCK_MAP_FOUND_IN_CACHE),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "block_map_free_pages",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BLOCK_MAP_FREE_PAGES),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "block_map_incoming_pages",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BLOCK_MAP_INCOMING_PAGES),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "block_map_outgoing_pages",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BLOCK_MAP_OUTGOING_PAGES),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "block_map_pages_loaded",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BLOCK_MAP_PAGES_LOADED),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "block_map_pages_saved",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BLOCK_MAP_PAGES_SAVED),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "block_map_read_count",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BLOCK_MAP_READ_COUNT),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "block_map_read_outgoing",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BLOCK_MAP_READ_OUTGOING),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "block_map_reclaimed",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BLOCK_MAP_RECLAIMED),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "block_map_wait_for_page",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BLOCK_MAP_WAIT_FOR_PAGE),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "block_map_write_count",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BLOCK_MAP_WRITE_COUNT),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "block_size",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_BLOCK_SIZE),
        PM_TYPE_U32, DM_VDODEV_INDOM, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
    { .m_user = (void *) "complete_recoveries",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_COMPLETE_RECOVERIES),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "curr_dedupe_queries",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_CURR_DEDUPE_QUERIES),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "currentVIOs_in_progress",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_CURRENTVIOS_IN_PROGRESS),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "data_blocks_used",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_DATA_BLOCKS_USED),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "dedupe_advice_stale",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_DEDUPE_ADVICE_STALE),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "dedupe_advice_timeouts",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_DEDUPE_ADVICE_TIMEOUTS),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "dedupe_advice_valid",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_DEDUPE_ADVICE_VALID),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "errors_invalid_advicePBNCount",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_ERRORS_INVALID_ADVICEPBNCOUNT),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "errors_no_space_error_count",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_ERRORS_NO_SPACE_ERROR_COUNT),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "errors_read_only_error_count",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_ERRORS_READ_ONLY_ERROR_COUNT),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "flush_out",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_FLUSH_OUT),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "in_recovery_mode",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_IN_RECOVERY_MODE),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "instance",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_INSTANCE),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "journal_blocks_committed",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_JOURNAL_BLOCKS_COMMITTED),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "journal_blocks_started",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_JOURNAL_BLOCKS_STARTED),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "journal_blocks_written",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_JOURNAL_BLOCKS_WRITTEN),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "journal_disk_full",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_JOURNAL_DISK_FULL),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "journal_entries_committed",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_JOURNAL_ENTRIES_COMMITTED),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "journal_entries_started",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_JOURNAL_ENTRIES_STARTED),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "journal_entries_written",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_JOURNAL_ENTRIES_WRITTEN),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "journal_slab_journal_commits_requested",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_JOURNAL_SLAB_JOURNAL_COMMITS_REQUESTED),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "logical_blocks",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_LOGICAL_BLOCKS),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "logical_block_size",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_LOGICAL_BLOCK_SIZE),
        PM_TYPE_U32, DM_VDODEV_INDOM, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
    { .m_user = (void *) "logical_blocks_used",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_LOGICAL_BLOCKS_USED),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "max_dedupe_queries",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_MAX_DEDUPE_QUERIES),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "maxVIOs",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_MAXVIOS),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "memory_usage_bios_used",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_MEMORY_USAGE_BIOS_USED),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "memory_usage_bytes_used",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_MEMORY_USAGE_BYTES_USED),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
    { .m_user = (void *) "memory_usage_peak_bio_count",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_MEMORY_USAGE_PEAK_BIO_COUNT),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "memory_usage_peak_bytes_used",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_MEMORY_USAGE_PEAK_BYTES_USED),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
    { .m_user = (void *) "mode",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_MODE),
        PM_TYPE_STRING, DM_VDODEV_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "overhead_blocks_used",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_OVERHEAD_BLOCKS_USED),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "packer_compressed_blocks_written",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_PACKER_COMPRESSED_BLOCKS_WRITTEN),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "packer_compressed_fragments_in_packer",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_PACKER_COMPRESSED_FRAGMENTS_IN_PACKER),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "packer_compressed_fragments_written",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_PACKER_COMPRESSED_FRAGMENTS_WRITTEN),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "physical_blocks",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_PHYSICAL_BLOCKS),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "read_cache_accesses",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_READ_CACHE_ACCESSES),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "read_cache_data_hits",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_READ_CACHE_DATA_HITS),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "read_cache_hits",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_READ_CACHE_HITS),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "read_only_recoveries",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_READ_ONLY_RECOVERIES),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "recovery_percentage",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_RECOVERY_PERCENTAGE),
        PM_TYPE_FLOAT, DM_VDODEV_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { .m_user = (void *) "ref_counts_blocks_written",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_REF_COUNTS_BLOCKS_WRITTEN),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "slab_journal_blocked_count",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_SLAB_JOURNAL_BLOCKED_COUNT),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "slab_journal_blocks_written",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_SLAB_JOURNAL_BLOCKS_WRITTEN),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "slab_journal_disk_full_count",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_SLAB_JOURNAL_DISK_FULL_COUNT),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "slab_journal_flush_count",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_SLAB_JOURNAL_FLUSH_COUNT),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "slab_journal_tail_busy_count",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_SLAB_JOURNAL_TAIL_BUSY_COUNT),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "slab_summary_blocks_written",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_SLAB_SUMMARY_BLOCKS_WRITTEN),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = (void *) "write_policy",
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_WRITE_POLICY),
        PM_TYPE_STRING, DM_VDODEV_INDOM, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* Derived metrics */
    { .m_user = NULL,
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_JOURNAL_BLOCKS_BATCHING),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = NULL,
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_JOURNAL_BLOCKS_WRITING),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = NULL,
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_JOURNAL_ENTRIES_BATCHING),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = NULL,
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_JOURNAL_ENTRIES_WRITING),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_user = NULL,
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_CAPACITY),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
    { .m_user = NULL,
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_USED),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
    { .m_user = NULL,
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_AVAILABLE),
        PM_TYPE_U64, DM_VDODEV_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
    { .m_user = NULL,
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_USED_PERCENTAGE),
        PM_TYPE_FLOAT, DM_VDODEV_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { .m_user = NULL,
      .m_desc = {
        PMDA_PMID(CLUSTER_VDODEV, VDODEV_SAVINGS_PERCENTAGE),
        PM_TYPE_FLOAT, DM_VDODEV_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
};

static pmdaIndom indomtable[] = {
    { .it_indom = DM_CACHE_INDOM },
    { .it_indom = DM_THIN_POOL_INDOM },
    { .it_indom = DM_THIN_VOL_INDOM },
    { .it_indom = DM_STATS_INDOM },
    { .it_indom = DM_HISTOGRAM_INDOM },
    { .it_indom = DM_VDODEV_INDOM },
};

pmInDom
dm_indom(int serial)
{
    return indomtable[serial].it_indom;
}

static int
dm_instance(pmInDom indom, int inst, char *name, pmInResult **result, pmdaExt *pmda)
{
    int privilege = 0;

    if (access("/dev/mapper/control", R_OK) == 0)
	privilege = 1;

    if (privilege) {
	dm_cache_instance_refresh();
	dm_thin_pool_instance_refresh();
	dm_thin_vol_instance_refresh();
    }

    (void)pm_dm_stats_instance_refresh();
    (void)pm_dm_histogram_instance_refresh();

    dm_vdodev_instance_refresh();

    return pmdaInstance(indom, inst, name, result, pmda);
}

static int
dm_fetch_refresh(pmdaExt *pmda, int *need_refresh)
{
    pmInDom indom;
    char *name;
    int privilege = 0;
    int sts;

    if (access("/dev/mapper/control", R_OK) == 0)
	privilege = 1;

    if (need_refresh[CLUSTER_CACHE] && privilege) {
        struct cache_stats *cache;

        if ((sts = dm_cache_instance_refresh()) < 0)
	    return sts;

        indom = dm_indom(DM_CACHE_INDOM);

        for (pmdaCacheOp(indom, PMDA_CACHE_WALK_REWIND);;) {
	    if ((sts = pmdaCacheOp(indom, PMDA_CACHE_WALK_NEXT)) < 0)
	        break;
	    if (!pmdaCacheLookup(indom, sts, &name, (void **)&cache) || !cache)
	        continue;
            if (need_refresh[CLUSTER_CACHE])
                dm_refresh_cache(name, cache);
        }
    }

    if (need_refresh[CLUSTER_POOL] && privilege) {
        struct pool_stats *pool;

        if ((sts = dm_thin_pool_instance_refresh()) < 0)
	    return sts;

        indom = dm_indom(DM_THIN_POOL_INDOM);

        for (pmdaCacheOp(indom, PMDA_CACHE_WALK_REWIND);;) {
	    if ((sts = pmdaCacheOp(indom, PMDA_CACHE_WALK_NEXT)) < 0)
	        break;
	    if (!pmdaCacheLookup(indom, sts, &name, (void **)&pool) || !pool)
	        continue;
            if (need_refresh[CLUSTER_POOL])
                dm_refresh_thin_pool(name, pool);
        }
    }

    if (need_refresh[CLUSTER_VOL] && privilege) {
        struct vol_stats *vol;

        if ((sts = dm_thin_vol_instance_refresh()) < 0)
	    return sts;

        indom = dm_indom(DM_THIN_VOL_INDOM);

        for (pmdaCacheOp(indom, PMDA_CACHE_WALK_REWIND);;) {
	    if ((sts = pmdaCacheOp(indom, PMDA_CACHE_WALK_NEXT)) < 0)
	        break;
	    if (!pmdaCacheLookup(indom, sts, &name, (void **)&vol) || !vol)
	        continue;
            if (need_refresh[CLUSTER_VOL])
                dm_refresh_thin_vol(name, vol);
        }
    }

    if (need_refresh[CLUSTER_DM_COUNTER]) {
        struct pm_wrap *pw;

        if ((sts = pm_dm_stats_instance_refresh()) < 0)
	    return sts;

        indom = dm_indom(DM_STATS_INDOM);

        for (pmdaCacheOp(indom, PMDA_CACHE_WALK_REWIND);;) {
	    if ((sts = pmdaCacheOp(indom, PMDA_CACHE_WALK_NEXT)) < 0)
	        break;
	    if (!pmdaCacheLookup(indom, sts, &name, (void **)&pw) || !pw)
	        continue;
            if (need_refresh[CLUSTER_DM_COUNTER])
                pm_dm_refresh_stats(pw, DM_STATS_INDOM);
        }
    }

    if (need_refresh[CLUSTER_DM_HISTOGRAM]) {
        struct pm_wrap *pw;

        if ((sts = pm_dm_histogram_instance_refresh()) < 0)
	    return sts;

        indom = dm_indom(DM_HISTOGRAM_INDOM);

        for (pmdaCacheOp(indom, PMDA_CACHE_WALK_REWIND);;) {
	    if ((sts = pmdaCacheOp(indom, PMDA_CACHE_WALK_NEXT)) < 0)
	        break;
	    if (!pmdaCacheLookup(indom, sts, &name, (void **)&pw) || !pw)
	        continue;
            if (need_refresh[CLUSTER_DM_HISTOGRAM])
                pm_dm_refresh_stats(pw, DM_HISTOGRAM_INDOM);
        }
    }

    if (need_refresh[CLUSTER_VDODEV]) {
	dm_vdodev_instance_refresh();
    }

    return 0;
}

static int
dm_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    int i, sts, need_refresh[NUM_CLUSTERS] = { 0 };

    for (i = 0; i < numpmid; i++) {
	unsigned int	cluster = pmID_cluster(pmidlist[i]);
	if (cluster < NUM_CLUSTERS)
	    need_refresh[cluster]++;
    }

    if ((sts = dm_fetch_refresh(pmda, need_refresh)) < 0)
	return sts;

    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

/*
 * callback provided to pmdaFetch
 */
static int
dm_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    unsigned int	item = pmID_item(mdesc->m_desc.pmid);
    struct cache_stats *cache;
    struct pool_stats *pool;
    struct vol_stats *vol;
    struct pm_wrap *pw;
    char *device;
    int sts;

    switch (pmID_cluster(mdesc->m_desc.pmid)) {
        case CLUSTER_CACHE:
            sts = pmdaCacheLookup(dm_indom(DM_CACHE_INDOM), inst, NULL, (void **)&cache);
            if (sts < 0)
                return sts;
            return dm_cache_fetch(item, cache, atom);

        case CLUSTER_POOL:
	    sts = pmdaCacheLookup(dm_indom(DM_THIN_POOL_INDOM), inst, NULL, (void **)&pool);
	    if (sts < 0)
	        return sts;
	    return dm_thin_pool_fetch(item, pool, atom);

        case CLUSTER_VOL:
	    sts = pmdaCacheLookup(dm_indom(DM_THIN_VOL_INDOM), inst, NULL, (void **)&vol);
	    if (sts < 0)
	        return sts;
	    return dm_thin_vol_fetch(item, vol, atom);

	case CLUSTER_DM_COUNTER:
	    sts = pmdaCacheLookup(dm_indom(DM_STATS_INDOM), inst, NULL, (void**)&pw);
	    if (sts < 0)
	        return sts;
	    return pm_dm_stats_fetch(item, pw, atom);

	case CLUSTER_DM_HISTOGRAM:
	    sts = pmdaCacheLookup(dm_indom(DM_HISTOGRAM_INDOM), inst, NULL, (void**)&pw);
	    if (sts < 0)
	        return sts;
	    return pm_dm_histogram_fetch(item, pw, atom);

	case CLUSTER_VDODEV:
	    sts = pmdaCacheLookup(dm_indom(DM_VDODEV_INDOM), inst, &device, NULL);
	    if (sts < 0)
	        return sts;
	    return dm_vdodev_fetch(mdesc, device, atom);

        default: /* unknown cluster */
	    return PM_ERR_PMID;
    }
    return 1;
}

/*
 * Initialise the agent (both daemon and DSO).
 */
void
__PMDA_INIT_CALL
dm_init(pmdaInterface *dp)
{
    if (_isDSO) {
	char helppath[MAXPATHLEN];
	int sep = pmPathSeparator();
	pmsprintf(helppath, sizeof(helppath), "%s%c" "dm" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
	pmdaDSO(dp, PMDA_INTERFACE_4, "DM DSO", helppath);
    }

    if (dp->status != 0)
	return;

    /* Check for environment variables allowing test injection */
    dm_cache_setup();
    dm_thin_setup();
    dm_vdo_setup();

    int	nindoms = sizeof(indomtable)/sizeof(indomtable[0]);
    int	nmetrics = sizeof(metrictable)/sizeof(metrictable[0]);

    if (dp->status != 0)
	return;

    dp->version.four.instance = dm_instance;
    dp->version.four.fetch = dm_fetch;
    pmdaSetFetchCallBack(dp, dm_fetchCallBack);

    pmdaSetFlags(dp, PMDA_EXT_FLAG_HASHED);
    pmdaInit(dp, indomtable, nindoms, metrictable, nmetrics);
}

static pmLongOptions longopts[] = {
    PMDA_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    PMDAOPT_DOMAIN,
    PMDAOPT_LOGFILE,
    PMOPT_HELP,
    PMDA_OPTIONS_END
};

static pmdaOptions opts = {
    .short_options = "D:d:l:?",
    .long_options = longopts,
};

/*
 * Set up the agent if running as a daemon.
 */
int
main(int argc, char **argv)
{
    int	sep = pmPathSeparator();
    pmdaInterface dispatch;
    char helppath[MAXPATHLEN];

    _isDSO = 0;
    pmSetProgname(argv[0]);
    pmsprintf(helppath, sizeof(helppath), "%s%c" "dm" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_4, pmGetProgname(), DM, "dm.log", helppath);

    pmdaGetOptions(argc, argv, &opts, &dispatch);
    if (opts.errors) {
	pmdaUsageMessage(&opts);
	exit(1);
    }

    pmdaOpenLog(&dispatch);
    dm_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);
    exit(0);
}
