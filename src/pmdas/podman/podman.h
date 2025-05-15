/*
 * Copyright (c) 2018,2021 Red Hat.
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
#ifndef PCP_PODMAN_H
#define PCP_PODMAN_H

#include "pmapi.h"
#include "pmda.h"

typedef struct {
    int32_t		name;		/* string mapping for name */
    int32_t		command;	/* string mapping for running command */
    int32_t		status;		/* string mapping for status */
    unsigned int	running;
    int32_t		labelmap;	/* string mapping for labels */
    unsigned int	nlabels;	/* number of labels in labelmap */
    const char		*labels;	/* pointer to start of all labels */
    int32_t		image;		/* string mapping for image name */
    int32_t		podid;		/* string mapping for pod name */
} container_info_t;

typedef struct {
    int64_t		net_input;
    int64_t		net_output;
    int64_t		block_input;
    int64_t		block_output;
    double		cpu;
    int64_t		cpu_nano;
    int64_t		system_nano;
    int64_t		mem_usage;
    int64_t		mem_limit;
    double		mem_perc;
    uint32_t		nprocesses;
} container_stats_t;

typedef enum {
    STATE_NONE		= 0x0,
    STATE_INFO		= 0x1,
    STATE_STATS		= 0x2,
    STATE_POD		= 0x4,
} state_flags_t;

typedef struct {
    state_flags_t	flags;
    container_info_t	info;
    container_stats_t	stats;
} container_t;

typedef struct {
    int32_t		name;		/* string mapping for pod name */
    int32_t		cgroup;		/* string mapping for cgroup name */
    unsigned int	running;
    int32_t		labelmap;	/* string mapping for label names */
    unsigned int	nlabels;	/* number of labels in labelmap */
    const char		*labels;	/* pointer to start of all labels */
    int32_t		status;		/* string mapping for status info */
    unsigned int	ncontainers;
} pod_info_t;

typedef struct {
    state_flags_t	flags;
    pod_info_t		info;
} pod_t;

typedef enum {
    /* pmID item fields */
    STATS_NET_INPUT	= 0,
    STATS_NET_OUTPUT	= 1,
    STATS_BLOCK_INPUT	= 2,
    STATS_BLOCK_OUTPUT	= 3,
    STATS_CPU		= 4,
    STATS_CPU_NANO	= 5,
    STATS_SYSTEM_NANO	= 6,
    STATS_MEM_USAGE	= 7,
    STATS_MEM_LIMIT	= 8,
    STATS_MEM_PERC	= 9,
    STATS_PIDS		= 10,
    NUM_CONTAINER_STATS,

    /* internal fields */
    STATS_ID,
} container_stats_e;

typedef enum {
    /* pmID item fields */
    INFO_NAME		= 0,
    INFO_COMMAND	= 1,
    INFO_STATUS		= 2,
    INFO_RUNNING	= 5,
    INFO_IMAGE		= 6,
    INFO_POD		= 7,
    NUM_CONTAINER_INFO,

    /* internal fields */
    INFO_ID,
    INFO_LABELS,
} container_info_e;

typedef enum {
    /* pmID item fields */
    POD_NAME		= 0,
    POD_CGROUP		= 1,
    POD_STATUS		= 2,
    POD_CONTAINERS	= 3,
    NUM_POD_INFO,

    /* internal fields */
    POD_ID,
    POD_LABELS,
} pod_info_e;

enum {
    CLUSTER_STATS	= 0,
    CLUSTER_INFO	= 1,
    CLUSTER_POD		= 2,
    NUM_CLUSTERS
};

enum {
    CONTAINER_INDOM	= 0,
    POD_INDOM		= 1,
    STRINGS_INDOM	= 2,	/* virtual indom for string mappings */
    NUM_INDOMS
};

/* General routines */

extern void podman_refresh(unsigned int[]);
extern char *podman_strings_lookup(int);
extern int podman_strings_insert(const char *);

/* Parsing routines */

extern int podman_parse_init(void);
extern void podman_parse_end(void);

/* Instance domains */

extern pmdaIndom podman_indomtab[NUM_INDOMS];
#define INDOM(x) (podman_indomtab[x].it_indom)

/* System paths */
extern char *podman_rundir;

#endif /* PCP_PODMAN_H */
