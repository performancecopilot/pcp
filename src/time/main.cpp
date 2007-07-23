/*
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
 * Copyright (c) 2006-2007, Nathan Scott.  All Rights Reserved.
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
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 * 
 * Contact information: Ken McDonell, kenj At internode DoT on DoT net
 *                      Nathan Scott, nathans At debian DoT org
 */

#include <qapplication.h>
#include <qsocketnotifier.h>
#include <qstatusbar.h>
#include <qeventloop.h>
#include <qdatetime.h>
#include <qpixmap.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include "timelord.h"
#include "main.h"

// test for not-zeroed timeval
int tnonzero(struct timeval *a)
{
    return (a->tv_sec != 0 || a->tv_usec != 0);
}

// a := a + b for struct timevals
void tadd(struct timeval *a, struct timeval *b)
{
    a->tv_usec += b->tv_usec;
    if (a->tv_usec > 1000000) {
	a->tv_usec -= 1000000;
	a->tv_sec++;
    }
    a->tv_sec += b->tv_sec;
}

// a := a - b for struct timevals, result is never less than zero
void tsub(struct timeval *a, struct timeval *b)
{
    a->tv_usec -= b->tv_usec;
    if (a->tv_usec < 0) {
	a->tv_usec += 1000000;
	a->tv_sec--;
    }
    a->tv_sec -= b->tv_sec;
    if (a->tv_sec < 0) {
	/* clip negative values at zero */
	a->tv_sec = 0;
	a->tv_usec = 0;
    }
}

// a : b for struct timevals ... <0 for a<b, ==0 for a==b, >0 for a>b
int tcmp(struct timeval *a, struct timeval *b)
{
    int res = (int)(a->tv_sec - b->tv_sec);
    if (res == 0)
	res = (int)(a->tv_usec - b->tv_usec);
    return res;
}

// conversion from seconds (double precision) to struct timeval
void secondsToTV(double value, struct timeval *tv)
{
    double usec = (value - (unsigned int)value) / 1000000.0;
    tv->tv_sec = (unsigned int)value;
    tv->tv_usec = (unsigned int)usec;
}

// conversion from struct timeval to seconds (double precision)
double secondsFromTV(struct timeval *tv)
{
    return (double)tv->tv_sec + ((double)tv->tv_usec / 1000000.0);
}

// conversion from other time units into seconds
double unitsToSeconds(double value, delta_units units)
{
    if (units == Msec)
	return value / 1000.0;
    else if (units == Min)
	return value * 60.0;
    else if (units == Hour)
	return value * (60.0 * 60.0);
    else if (units == Day)
	return value * (60.0 * 60.0 * 24.0);
    else if (units == Week)
	return value * (60.0 * 60.0 * 24.0 * 7.0);
    return value;
}

// conversion from seconds into other time units
double secondsToUnits(double value, delta_units units)
{
    if (units == Msec)
	return value * 1000.0;
    else if (units == Min)
	return value / 60.0;
    else if (units == Hour)
	return value / (60.0 * 60.0);
    else if (units == Day)
	return value / (60.0 * 60.0 * 24.0);
    else if (units == Week)
	return value / (60.0 * 60.0 * 24.0 * 7.0);
    return value;
}

extern QPixmap *pixmap(enum PixmapType type)
{
    static QPixmap pixmaps[PIXMAP_COUNT];
    static int setup;

    if (!setup) {
	setup = 1;
	pixmaps[PLAY_ON] = QPixmap::fromMimeSource("play_on.png");
	pixmaps[PLAY_OFF] = QPixmap::fromMimeSource("play_off.png");
	pixmaps[STOP_ON] = QPixmap::fromMimeSource("stop_on.png");
	pixmaps[STOP_OFF] = QPixmap::fromMimeSource("stop_off.png");
	pixmaps[BACK_ON] = QPixmap::fromMimeSource("back_on.png");
	pixmaps[BACK_OFF] = QPixmap::fromMimeSource("back_off.png");
	pixmaps[FASTFWD_ON] = QPixmap::fromMimeSource("fastfwd_on.png");
	pixmaps[FASTFWD_OFF] = QPixmap::fromMimeSource("fastfwd_off.png");
	pixmaps[FASTBACK_ON] = QPixmap::fromMimeSource("fastback_on.png");
	pixmaps[FASTBACK_OFF] = QPixmap::fromMimeSource("fastback_off.png");
	pixmaps[STEPFWD_ON] = QPixmap::fromMimeSource("stepfwd_on.png");
	pixmaps[STEPFWD_OFF] = QPixmap::fromMimeSource("stepfwd_off.png");
	pixmaps[STEPBACK_ON] = QPixmap::fromMimeSource("stepback_on.png");
	pixmaps[STEPBACK_OFF] = QPixmap::fromMimeSource("stepback_off.png");
    }
    return &pixmaps[type];
}

void setupEnvironment(void)
{
    QString confirm = pmGetConfig("PCP_BIN_DIR");
    confirm.append("/kmquery");
    setenv("PCP_XCONFIRM_PROG", confirm.ascii(), 1);
    setenv("PCP_STDERR", "DISPLAY", 1);
}

int main(int argc, char ** argv)
{
    int			c;
    int			sts;
    int			errflg = 0;
    int			port = -1, autoport = 0;
    char		*endnum, *envstr, portname[32];
    static char		usage[] = "Usage: %s [-a | -h] [-p port]\n";

    QApplication a(argc, argv);
    pmProgname = basename(argv[0]);
    setupEnvironment();

    while ((c = getopt(argc, argv, "ahp:D:?")) != EOF) {
	switch (c) {

	case 'a':
	case 'h':
	    break;

	case 'p':
	    port = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || port < 0) {
		fprintf(stderr, "%s: requires a numeric port (not %s)\n",
			pmProgname, optarg);
		errflg++;
	    }
	    break;

	case 'D':
	    sts = __pmParseDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr,
			"%s: unrecognized debug flag specification (%s)\n",
			pmProgname, optarg);
		errflg++;
	    }
	    else
		pmDebug |= sts;
	    break;

	case '?':
	    errflg++;
	    break;
	}
    }

    if (errflg || optind != argc) {
	fprintf(stderr, usage, pmProgname);
	exit(1);
    }

    if (port == -1) {
	autoport = 1;
	if ((envstr = getenv("KMTIME_PORT")) == NULL) {
	    port = KMTIME_PORT_BASE;
	} else {
	    port = strtol(envstr, &endnum, 0);
	    if (*endnum != '\0' || port < 0) {
		fprintf(stderr,
		    "%s: KMTIME_PORT must be a numeric port number (not %s)\n",
			pmProgname, envstr);
		exit(1);
	    }
	}
    }

    Console cons;
    c = 0;
    if (pmDebug & DBG_TRACE_APPL0) c |= DBG_APP;   /* kmtime apps internals */
    if (pmDebug & DBG_TRACE_APPL1) c |= DBG_PROTO; /* trace kmtime protocol */
    cons.init(c);

    TimeLord *tl;
    do {
	tl = new TimeLord(port, &cons, &a);
	if (!tl->ok())
	    port++;
	else
	    break;
	tl->~TimeLord();
	tl = NULL;
    } while (autoport && (port >= 0));

    if (!tl || !tl->ok()) {
	if (!autoport)
	    fprintf(stderr, "%s: cannot find an available port\n", pmProgname);
	else
	    fprintf(stderr, "%s: cannot connect to requested port (%d)\n",
		    pmProgname, port);
	exit(1);
    } else if (autoport) {	/* write to stdout for client */
	c = snprintf(portname, sizeof(portname), "port=%u\n", port);
	if (write(fileno(stdout), portname, c + 1) < 0) {
	    fprintf(stderr, "%s: cannot write port for client: %s\n",
		    pmProgname, strerror(errno));
	    exit(1);
	}
    }

    KmTimeLive hc;
    KmTimeArch ac;
    tl->setContext(&hc, &ac);

    hc.init(&cons);
    ac.init(&cons);

    delete hc.statusBar();
    if (!pmDebug) hc.disableConsole();
    else hc.popup(1);

    delete ac.statusBar();
    if (!pmDebug) ac.disableConsole();
    else ac.popup(1);

    a.exec();
    return 0;
}
