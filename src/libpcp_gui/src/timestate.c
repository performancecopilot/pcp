/*
 * Time control functions for pmval
 *
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2008-2009 Aconex.  All Rights Reserved.
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
#include "pmtime.h"
#include "pmapi.h"
#include "libpcp.h"

static enum {
    START	= -1,
    STANDBY	= 0,
    FORW	= 1,
    BACK	= 2,
    MOVING	= 3,
    RESTART	= 4,
    ENDLOG	= 5,
} state;

/*
 * a : b for struct timevals ... <0 for a<b, ==0 for a==b, >0 for a>b
 */
static int
__pmtimevalCmp(struct timeval *a, struct timeval *b)
{
    int res = (int)(a->tv_sec - b->tv_sec);

    if (res == 0)
	res = (int)(a->tv_usec - b->tv_usec);

    return res;
}

static void timeControlExited()
{
    fprintf(stderr, "\n%s: Time Control dialog exited, goodbye.\n", pmGetProgname());
}

static void timeControlRewind()
{
    printf("\n[Time Control] Rewind/Reverse ...\n");
}

static void timeControlPosition(struct timeval position)
{
    printf("\n[Time Control] Repositioned in archive ...\n");
}

static void timeControlInterval(struct timeval delta)
{
    printf("new interval:  %1.2f sec\n", pmtimevalToReal(&delta));
}

static void timeControlResume()
{
    printf("\n[Time Control] Resume ...\n");
}

static void timeControlBounds()
{
    printf("\n[Time Control] End of Archive ...\n");
}

static void timeControlStepped(struct timeval delta)
{
}

static void timeControlNewZone(char *timezone, char *label)
{
    int sts = pmNewZone(timezone);

    if (sts < 0)
	fprintf(stderr, "%s: Warning: cannot set timezone to \"%s\": %s\n",
		pmGetProgname(), timezone, pmErrStr(sts));
    else
	printf("new timezone: %s (%s)\n", timezone, label);
}

/*
 * Get Extended Time Base interval and Units from a timeval
 * in order to set archive mode.
 */
void
pmTimeStateMode(int mode, struct timeval delta, struct timeval *position)
{
    const int SECS_IN_24_DAYS = 2073600;
    int step, sts;

    if (delta.tv_sec > SECS_IN_24_DAYS) {
	step = delta.tv_sec;
	mode |= PM_XTB_SET(PM_TIME_SEC);
    } else {
	step = delta.tv_sec * 1e3 + delta.tv_usec / 1e3;
	mode |= PM_XTB_SET(PM_TIME_MSEC);
    }

    if ((sts = pmSetMode(mode, position, step)) < 0) {
	fprintf(stderr, "%s: pmSetMode: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(EXIT_FAILURE);
    }
}

pmTime *
pmTimeStateSetup(
	pmTimeControls *timecontrols, int ctxt, int port,
	struct timeval delta, struct timeval position,
	struct timeval first, struct timeval last, char *tz, char *tz_label)
{
    pmTime	*pmtime = malloc(sizeof(pmTime));
    pmTime	*pmtime_tmp;
    int		fd, sts, tzlen;

    if (pmtime == NULL) {
	fprintf(stderr, "%s: pmTimeConnect: malloc: %s\n", pmGetProgname(), osstrerror());
	exit(EXIT_FAILURE);
    }

    pmtime->magic = PMTIME_MAGIC;
    pmtime->length = sizeof(pmTime);
    pmtime->command = PM_TCTL_SET;
    pmtime->delta = delta;

    if (ctxt == PM_CONTEXT_ARCHIVE) {
	pmtime->source = PM_SOURCE_ARCHIVE;
	pmtime->position = position;
	pmtime->start = first;
	pmtime->end = last;
    } else {
	pmtime->source = PM_SOURCE_HOST;
	pmtimevalNow(&pmtime->position);
    }
    if (tz == NULL) {
	char	tzbuf[PM_TZ_MAXLEN];
	tz = __pmTimezone_r(tzbuf, sizeof(tzbuf));
	if (ctxt == PM_CONTEXT_ARCHIVE) {
	    if ((sts = pmNewZone(tz)) < 0) {
		fprintf(stderr, "%s: pmTimeConnect: Cannot set timezone to \"%s\": %s\n",
			pmGetProgname(), tz, pmErrStr(sts));
		exit(EXIT_FAILURE);
	    }
	}
    }
    tzlen = strlen(tz) + 1;
    if (tz_label == NULL)
	tz_label = "localhost";
    pmtime->length += tzlen + strlen(tz_label) + 1;
    pmtime_tmp = realloc(pmtime, pmtime->length);
    if (pmtime_tmp == NULL) {
	fprintf(stderr, "%s: pmTimeConnect: realloc: %s\n", pmGetProgname(), osstrerror());
	exit(EXIT_FAILURE);
    }
    pmtime = pmtime_tmp;
    strcpy(pmtime->data, tz);
    strcpy(pmtime->data + tzlen, tz_label);
    if ((fd = pmTimeConnect(port, pmtime)) < 0) {
	fprintf(stderr, "%s: pmTimeConnect: %s\n", pmGetProgname(), pmErrStr(fd));
	exit(EXIT_FAILURE);
    }

    pmtime->length = sizeof(pmTime); /* reduce size to header only */
    pmtime = realloc(pmtime, pmtime->length);	/* cannot fail! */

    /* setup default vectors */
    timecontrols->resume = timeControlResume;
    timecontrols->exited = timeControlExited;
    timecontrols->rewind = timeControlRewind;
    timecontrols->boundary = timeControlBounds;
    timecontrols->position = timeControlPosition;
    timecontrols->interval = timeControlInterval;
    timecontrols->stepped = timeControlStepped;
    timecontrols->newzone = timeControlNewZone;
    timecontrols->context = ctxt;
    timecontrols->showgui = 1;
    timecontrols->delta = delta;
    timecontrols->padding = 0;
    timecontrols->fd = fd;
    state = START;

    return pmtime;
}

void
pmTimeStateBounds(pmTimeControls *control, pmTime *pmtime)
{
    pmTimeStateAck(control, pmtime);
    if (state != ENDLOG)
	control->boundary();
    state = ENDLOG;
}

void
pmTimeStateAck(pmTimeControls *control, pmTime *pmtime)
{
    int sts = pmTimeSendAck(control->fd, &pmtime->position);

    if (sts < 0) {
	if (sts == -EPIPE)
	    control->exited();
	else
	    fprintf(stderr, "\n%s: pmTimeSendAck: %s\n",
			pmGetProgname(), pmErrStr(sts));
	exit(EXIT_FAILURE);
    }
}

int
pmTimeStateVector(pmTimeControls *control, pmTime *pmtime)
{
    int cmd, fetch = 0;

    if (control->showgui) {
	pmTimeShowDialog(control->fd, 1);
	control->showgui = 0;
    }

    do {
	cmd = pmTimeRecv(control->fd, &pmtime);
	if (cmd < 0) {
	    control->exited();
	    exit(EXIT_FAILURE);
	}

	switch (pmtime->command) {
	    case PM_TCTL_SET:
		if (state == ENDLOG)
		    state = STANDBY;
		else if (state == FORW)
		    state = RESTART;
		break;

	    case PM_TCTL_STEP:
		if (pmtime->state == PM_STATE_BACKWARD) {
		    if (state != BACK) {
			control->rewind();
			state = BACK;
		    }
		}
		else if (state != FORW && state != ENDLOG) {
		    if (control->context == PM_CONTEXT_ARCHIVE) {
			if (state == STANDBY)
			    control->resume();
			else if (state != START)
			    control->position(pmtime->position);
		    }
		    else if (state != START)
			control->resume();
		    if (state != START &&
			__pmtimevalCmp(&pmtime->delta, &control->delta) != 0)
			control->interval(pmtime->delta);

		    if (control->context == PM_CONTEXT_ARCHIVE)
			pmTimeStateMode(PM_MODE_INTERP,
					pmtime->delta, &pmtime->position);
		    control->delta = pmtime->delta;
		    control->stepped(pmtime->delta);
		    state = FORW;
		}

		if (state == BACK || state == ENDLOG) {
		    /*
		     * for EOL and reverse travel, no fetch, so ACK here
		     */
		    pmTimeStateAck(control, pmtime);
		    break;
		}
		fetch = 1;
		break;

	    case PM_TCTL_TZ:
		control->newzone(pmtime->data,
				 pmtime->data + strlen(pmtime->data) + 1);
		break;

	    case PM_TCTL_VCRMODE:
	    case PM_TCTL_VCRMODE_DRAG:
		/* something has changed ... suppress reporting */
		if (pmtime->command == PM_TCTL_VCRMODE_DRAG)
		    state = MOVING;
		else if (pmtime->state == PM_STATE_STOP)
		    state = STANDBY;
		else
		    state = RESTART;
		break;

	    /*
	     * safely and silently ignore these
	     */
	    case PM_TCTL_GUISHOW:
	    case PM_TCTL_GUIHIDE:
	    case PM_TCTL_BOUNDS:
	    case PM_TCTL_ACK:
		break;

	    default:
		if (pmDebugOptions.timecontrol)
		    fprintf(stderr, "pmTimeRecv: cmd %x?\n", cmd);
		break;
	}
    } while (!fetch);

    return 0;
}
