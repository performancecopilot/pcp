/*
 * Copyright (c) 2014-2015 Red Hat.
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
#ifndef _ROOT_H
#define _ROOT_H

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
    NUM_METRICS
};

enum {
    CONTAINERS_UPTODATE_NAME,
    CONTAINERS_UPTODATE_STATE,
    NUM_UPTODATE
};

/*
 * General container services, abstracting individual implementations into
 * "engines" which are then instantiated one-per-container-technology.
 */

struct stat;
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
    char		*name;
    int			state;
    char		path[60];
    container_setup_t	setup;
    container_changed_t	indom_changed;
    container_insts_t	insts_refresh;
    container_values_t	value_refresh;
    container_match_t	name_matching;
} container_engine_t;

typedef struct container {
    int			pid;
    int			flags : 8;	/* CONTAINER_FLAG bitwise */
    int			state : 8;	/* internal driver states */
    int			uptodate : 8;	/* refreshed values count */ 
    int			padding : 8;
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

#endif
