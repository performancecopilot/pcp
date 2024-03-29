/*
 * Copyright (c) 2014-2016 Red Hat.
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
#ifndef PMDA_ROOT_H
#define PMDA_ROOT_H

#include <sys/stat.h>
#include "pmjson.h"

enum {
    CONTAINERS_INDOM,
    NUM_INDOMS
};

enum {
    CONTAINERS_ENGINE,
    CONTAINERS_NAME,
    CONTAINERS_PID,
    CONTAINERS_RUNNING,
    CONTAINERS_PAUSED,
    CONTAINERS_RESTARTING,
    CONTAINERS_CGROUP,
    NUM_METRICS
};

enum {
    CONTAINERS_UPTODATE_NAME	= 0x1,
    CONTAINERS_UPTODATE_STATE	= 0x2,
    CONTAINERS_UPTODATE_CGROUP  = 0x4,
};

/*
 * General container services, abstracting individual implementations into
 * "engines" which are then instantiated one-per-container-technology.
 */

struct container;
struct container_engine;

typedef void (*container_setup_t)(struct container_engine *);
typedef int (*container_changed_t)(struct container_engine *);
typedef void (*container_insts_t)(struct container_engine *, pmInDom);
typedef int (*container_values_t)(struct container_engine *,
				  const char *, struct container *);
typedef int (*container_match_t)(struct container_engine *,
		const char *, const char *, const char *);

typedef struct container_engine {
    char		*name;		/* docker, lxc, rkt, etc. */
    int			state;		/* external driver states */
    char		*path;		/* suffix for cgroup path */
    container_setup_t	setup;
    container_changed_t	indom_changed;
    container_insts_t	insts_refresh;
    container_values_t	value_refresh;
    container_match_t	name_matching;
} container_engine_t;

typedef struct container {
    int			pid;
    unsigned int	flags : 8;	/* CONTAINER_FLAG bitwise */
    unsigned int	uptodate : 8;	/* refreshed values count */ 
    unsigned int	padding : 16;
    char		*name;		/* human-presentable name */
    char		cgroup[128];
    struct stat		stat;
    container_engine_t	*engine;
} container_t;

enum {
    CONTAINER_FLAG_RUNNING	= (1<<0),
    CONTAINER_FLAG_PAUSED	= (1<<1),
    CONTAINER_FLAG_RESTARTING	= (1<<2),
};

extern int root_stat_time_differs(struct stat *, struct stat *);

enum {
    ROOT_AGENT_SOCKET	= 1,
    ROOT_AGENT_PIPE	= 2,
};

extern int root_create_agent(int, char *, char *, int *, int *);
extern int root_agent_wait(int *);
extern int root_maximum_fd;

#endif	/* PMDA_ROOT_H */
