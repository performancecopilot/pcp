/*
 * Copyright (c) 2018 Red Hat.
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
#include <stdbool.h>

typedef struct {
    uint32_t		name;		/* string mapping for name */
    uint32_t		command;	/* string mapping for running command */
    uint32_t		status;		/* string mapping for status */
    int64_t		rwsize;
    int64_t		rootfssize;
    bool		running;
    unsigned int	labels;
    uint32_t		*labelnames;	/* string mappings for label names */
    uint32_t		*labelvalues;	/* string mappings for label values */
} container_info_t;

typedef struct container_stats {
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
    int64_t		nprocesses;
    uint32_t		name;		/* string mapping for container name */
} container_stats_t;

typedef enum state_flags {
    STATE_NONE		= 0x0,
    STATE_INFO		= 0x1,
    STATE_STATS		= 0x2,
} state_flags_t;

typedef struct container {
    uint32_t		id;	/* string mapping for container hash */
    uint32_t		podmap;	/* string mapping for pod hash (optional) */
    state_flags_t	flags;
    container_info_t	info;
    container_stats_t	stats;
} container_t;

typedef struct pod_info {
    uint32_t		id;		/* string mapping for pod hash */
    state_flags_t	flags;
    uint32_t		name;		/* string mapping for pod name */
    uint32_t		cgroup;		/* string mapping for cgroup name */
    uint64_t		rwsize;
    uint64_t		rootfssize;
    bool		running;
    unsigned int	labels;
    uint32_t		*labelnames;	/* string mappings for label names */
    uint32_t		*labelvalues;	/* string mappings for label values */
    uint32_t		status;		/* string mapping for status info */
    uint32_t		ncontainers;
} pod_info_t;

enum {
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
    NUM_CONTAINER_STATS
};

enum {
    INFO_NAME		= 0,
    INFO_COMMAND	= 1,
    INFO_STATUS		= 2,
    INFO_RWSIZE		= 3,
    INFO_ROOTFSSIZE	= 4,
    INFO_RUNNING	= 5,
    NUM_CONTAINER_INFO
};

enum {
    POD_NAME		= 0,
    POD_CGROUP		= 1,
    POD_STATUS		= 2,
    POD_CONTAINERS	= 3,
    NUM_POD_INFO
};

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

extern void podman_context_set_container(int, pmInDom, const char *, int);
extern container_t *podman_context_container(int);
extern void podman_context_end(int);

extern void refresh_podman_containers(pmInDom, state_flags_t);
extern void refresh_podman_container(pmInDom, char *, state_flags_t);
extern void refresh_podman_pod_info(pmInDom, char *);
extern void refresh_podman_pods_info(pmInDom);

extern char *podman_strings_lookup(int);
extern int podman_strings_insert(const char *);

#endif /* PCP_PODMAN_H */
