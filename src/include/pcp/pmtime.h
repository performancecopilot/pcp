/*
 * Copyright (C) 2006-2007 Aconex.  All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */
#ifndef PCP_PMTIME_H
#define PCP_PMTIME_H

#include <sys/time.h>

typedef enum pm_tctl_command {
    PM_TCTL_SET		= (1<<0),	// client -> server
    PM_TCTL_STEP	= (1<<1),	// server -> clients
    PM_TCTL_TZ		= (1<<2),	// server -> clients
    PM_TCTL_VCRMODE	= (1<<3),	// server -> clients
    PM_TCTL_VCRMODE_DRAG= (1<<4),	// server -> clients
    PM_TCTL_GUISHOW	= (1<<5),	// client -> server
    PM_TCTL_GUIHIDE	= (1<<6),	// client -> server
    PM_TCTL_BOUNDS	= (1<<7),	// client -> server
    PM_TCTL_ACK		= (1<<8),	// client -> server (except handshake)
} pm_tctl_command;

typedef enum pm_tctl_state {
    PM_STATE_STOP	= 0,
    PM_STATE_FORWARD	= 1,
    PM_STATE_BACKWARD	= 2,
} pm_tctl_state;

typedef enum {
    PM_MODE_STEP	= 0,
    PM_MODE_NORMAL	= 1,
    PM_MODE_FAST	= 2,
} pm_tctl_mode;

typedef enum pm_tctl_source {
    PM_SOURCE_NONE	= -1,
    PM_SOURCE_HOST	= 0,
    PM_SOURCE_ARCHIVE	= 1,
} pm_tctl_source;

#define PMTIME_MAGIC	0x54494D45	/* "TIME" */

typedef struct pmTime {
    unsigned int	magic;
    unsigned int	length;
    pm_tctl_command	command;
    pm_tctl_source	source;
    pm_tctl_state	state;
    pm_tctl_mode	mode;
    struct timeval	delta;
    struct timeval	position;
    struct timeval	start;		/* archive only */
    struct timeval	end;		/* archive only */
    char		data[0];	/* arbitrary length info (e.g. $TZ) */
} pmTime;

extern int pmTimeSendAck(int, struct timeval *);
extern int pmTimeConnect(int, pmTime *);
extern int pmTimeShowDialog(int, int);
extern int pmTimeRecv(int, pmTime **);

/*
 * Time state management API for simple clients
 */

typedef void (*pmTimeStateResume)(void);
typedef void (*pmTimeStateRewind)(void);
typedef void (*pmTimeStateExited)(void);
typedef void (*pmTimeStateBoundary)(void);
typedef void (*pmTimeStatePosition)(struct timeval);
typedef void (*pmTimeStateInterval)(struct timeval);
typedef void (*pmTimeStateStepped)(struct timeval);
typedef void (*pmTimeStateNewZone)(char *, char *);

typedef struct pmTimeControls {
    pmTimeStateResume	resume;
    pmTimeStateRewind	rewind;
    pmTimeStateExited	exited;
    pmTimeStateBoundary	boundary;
    pmTimeStatePosition	position;
    pmTimeStateInterval	interval;
    pmTimeStateStepped	stepped;
    pmTimeStateNewZone	newzone;
    struct timeval	delta;
    int			fd;
    int			showgui;
    int			context;
    int			padding;
} pmTimeControls;

extern pmTime *pmTimeStateSetup(pmTimeControls *, int, int,
			    struct timeval, struct timeval,
			    struct timeval, struct timeval, char *, char *);
extern void pmTimeStateAck(pmTimeControls *, pmTime *);
extern void pmTimeStateMode(int, struct timeval, struct timeval *);
extern int pmTimeStateVector(pmTimeControls *, pmTime *);
extern void pmTimeStateBounds(pmTimeControls *, pmTime *);

#endif /* PCP_PMTIME_H */
