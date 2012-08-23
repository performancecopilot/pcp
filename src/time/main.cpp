/*
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
 * Copyright (c) 2006-2007, Aconex.  All Rights Reserved.
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
#include <QtGui/QApplication>
#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pmtime.h>
#include "timelord.h"
#include "version.h"

static void setupEnvironment(void)
{
    QString confirm = pmGetConfig("PCP_BIN_DIR");
    confirm.prepend("PCP_XCONFIRM_PROG=");
    confirm.append(QChar(__pmPathSeparator()));
    confirm.append("pmquery");
    putenv(strdup((const char *)confirm.toAscii()));
    if (getenv("PCP_STDERR") == NULL)	// do not overwrite, for QA
	putenv(strdup("PCP_STDERR=DISPLAY"));

    QCoreApplication::setOrganizationName("PCP");
    QCoreApplication::setApplicationName("pmtime");
}

int main(int argc, char **argv)
{
    int			c;
    int			sts;
    int			errorFlag = 0;
    int			port = -1, autoport = 0;
    char		*endnum, *envstr, portname[32];
    static char		usage[] = "Usage: %s [-V] [-a | -h] [-p port]\n";

    QApplication a(argc, argv);
    __pmSetProgname(argv[0]);
    setupEnvironment();

    while ((c = getopt(argc, argv, "ahp:D:V?")) != EOF) {
	switch (c) {

	case 'a':
	case 'h':
	    break;

	case 'p':
	    port = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || port < 0) {
		pmprintf("%s: requires a numeric port (not %s)\n",
			pmProgname, optarg);
		errorFlag++;
	    }
	    break;

	case 'D':
	    sts = __pmParseDebug(optarg);
	    if (sts < 0) {
		pmprintf("%s: unrecognized debug flag specification (%s)\n",
			pmProgname, optarg);
		errorFlag++;
	    }
	    else
		pmDebug |= sts;
	    break;

	case 'V':		/* version */
	    printf("%s %s\n", pmProgname, VERSION);
	    exit(0);

	case '?':
	    errorFlag++;
	    break;
	}
    }

    if (errorFlag || optind != argc) {
	pmprintf(usage, pmProgname);
	pmflush();
	exit(1);
    }

    if (port == -1) {
	autoport = 1;
	if ((envstr = getenv("KMTIME_PORT")) == NULL) {
	    port = PmTime::BasePort;
	} else {
	    port = strtol(envstr, &endnum, 0);
	    if (*endnum != '\0' || port < 0) {
		pmprintf(
		    "%s: KMTIME_PORT must be a numeric port number (not %s)\n",
			pmProgname, envstr);
		pmflush();
		exit(1);
	    }
	}
    }

    console = new Console;
    TimeLord tl(&a);
    do {
	if (tl.listen(QHostAddress::LocalHost, port))
	    break;
	port++;
    } while (autoport && (port >= 0));

    if (!port || tl.isListening() == false) {
	if (!autoport)
	    pmprintf("%s: cannot find an available port\n", pmProgname);
	else
	    pmprintf("%s: cannot connect to requested port (%d)\n",
		    pmProgname, port);
	pmflush();
	exit(1);
    } else if (autoport) {	/* write to stdout for client */
	c = snprintf(portname, sizeof(portname), "port=%u\n", port);
	if (write(fileno(stdout), portname, c + 1) < 0) {
	    if (errno != EPIPE) {
		pmprintf("%s: cannot write port for client: %s\n",
		    pmProgname, strerror(errno));
		pmflush();
	    }
	    exit(1);
	}
    }

    PmTimeLive hc;
    PmTimeArch ac;
    tl.setContext(&hc, &ac);

    hc.init();
    if (!pmDebug) hc.disableConsole();
    else hc.popup(1);

    ac.init();
    if (!pmDebug) ac.disableConsole();
    else ac.popup(1);

    a.exec();
    return 0;
}

//
// Map icon type name to QIcon
//
extern QIcon *PmTime::icon(PmTime::Icon type)
{
    static QIcon icons[PmTime::IconCount];
    static int setup;

    if (!setup) {
	setup = 1;
	icons[PmTime::ForwardOn] = QIcon(":play_on.png");
	icons[PmTime::ForwardOff] = QIcon(":play_off.png");
	icons[PmTime::StoppedOn] = QIcon(":stop_on.png");
	icons[PmTime::StoppedOff] = QIcon(":stop_off.png");
	icons[PmTime::BackwardOn] = QIcon(":back_on.png");
	icons[PmTime::BackwardOff] = QIcon(":back_off.png");
	icons[PmTime::FastForwardOn] = QIcon(":fastfwd_on.png");
	icons[PmTime::FastForwardOff] = QIcon(":fastfwd_off.png");
	icons[PmTime::FastBackwardOn] = QIcon(":fastback_on.png");
	icons[PmTime::FastBackwardOff] = QIcon(":fastback_off.png");
	icons[PmTime::StepForwardOn] = QIcon(":stepfwd_on.png");
	icons[PmTime::StepForwardOff] = QIcon(":stepfwd_off.png");
	icons[PmTime::StepBackwardOn] = QIcon(":stepback_on.png");
	icons[PmTime::StepBackwardOff] = QIcon(":stepback_off.png");
    }
    return &icons[type];
}

//
// Test for not-zeroed timeval
//
int PmTime::timevalNonZero(struct timeval *a)
{
    return (a->tv_sec != 0 || a->tv_usec != 0);
}

//
// a := a + b for struct timevals
//
void PmTime::timevalAdd(struct timeval *a, struct timeval *b)
{
    a->tv_usec += b->tv_usec;
    if (a->tv_usec > 1000000) {
	a->tv_usec -= 1000000;
	a->tv_sec++;
    }
    a->tv_sec += b->tv_sec;
}

//
// a := a - b for struct timevals, result is never less than zero
//
void PmTime::timevalSub(struct timeval *a, struct timeval *b)
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

//
// a : b for struct timevals ... <0 for a<b, ==0 for a==b, >0 for a>b
//
int PmTime::timevalCompare(struct timeval *a, struct timeval *b)
{
    int res = (int)(a->tv_sec - b->tv_sec);
    if (res == 0)
	res = (int)(a->tv_usec - b->tv_usec);
    return res;
}

//
// Conversion from seconds (double precision) to struct timeval
//
void PmTime::secondsToTimeval(double value, struct timeval *tv)
{
    tv->tv_sec = (time_t)value;
    tv->tv_usec = (long)(((value - (double)tv->tv_sec) * 1000000.0));
}

//
// Conversion from struct timeval to seconds (double precision)
//
double PmTime::secondsFromTimeval(struct timeval *tv)
{
    return (double)tv->tv_sec + ((double)tv->tv_usec / 1000000.0);
}

//
// Conversion from other time units into seconds
//
double PmTime::unitsToSeconds(double value, PmTime::DeltaUnits units)
{
    if (units == PmTime::Milliseconds)
	return value / 1000.0;
    else if (units == PmTime::Minutes)
	return value * 60.0;
    else if (units == PmTime::Hours)
	return value * (60.0 * 60.0);
    else if (units == PmTime::Days)
	return value * (60.0 * 60.0 * 24.0);
    else if (units == PmTime::Weeks)
	return value * (60.0 * 60.0 * 24.0 * 7.0);
    return value;
}

//
// Conversion from seconds into other time units
//
double PmTime::secondsToUnits(double value, PmTime::DeltaUnits units)
{
    if (units == PmTime::Milliseconds)
	return value * 1000.0;
    else if (units == PmTime::Minutes)
	return value / 60.0;
    else if (units == PmTime::Hours)
	return value / (60.0 * 60.0);
    else if (units == PmTime::Days)
	return value / (60.0 * 60.0 * 24.0);
    else if (units == PmTime::Weeks)
	return value / (60.0 * 60.0 * 24.0 * 7.0);
    return value;
}
