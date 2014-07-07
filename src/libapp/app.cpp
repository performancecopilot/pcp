/*
 * Copyright (c) 2007-2009, Aconex.  All Rights Reserved.
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
#include "app.h"
#include "console.h"
#include "version.h"

#include <math.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>

App::App(int &argc, char **argv) : QApplication(argc, argv)
{
    __pmSetProgname(argv[0]);
    my.argc = argc;
    my.argv = argv;
    my.pmnsfile = NULL;
    my.Lflag = 0;
    my.Sflag = NULL;
    my.Tflag = NULL;
    my.Aflag = NULL;
    my.Oflag = NULL;
    my.zflag = 0;
    my.tz = NULL;
    my.port = -1;

    QCoreApplication::setOrganizationName("PCP");
    QCoreApplication::setApplicationName(pmProgname);
    QCoreApplication::setApplicationVersion(VERSION);
    QString confirm = pmGetConfig("PCP_BIN_DIR");
    confirm.prepend("PCP_XCONFIRM_PROG=");
    confirm.append("/pmquery");
    putenv(strdup((const char *)confirm.toAscii()));
    if (getenv("PCP_STDERR") == NULL)   // do not overwrite, for QA
	putenv(strdup("PCP_STDERR=DISPLAY"));
}

QFont *App::globalFont()
{
    static QFont *globalFont;
    if (!globalFont)
	globalFont = new QFont("Sans Serif", App::globalFontSize());
    return globalFont;
}

int App::globalFontSize()
{
#ifdef IS_DARWIN
    return 9;
#else
    return 7;
#endif
}

int App::getopts(const char *options)
{
    int			unknown = 0;
    int			c, sts, errflg = 0;
    char		*endnum, *msg;

    do {
	switch ((c = getopt(my.argc, my.argv, options))) {

	case 'A':	/* sample alignment */
	    my.Aflag = optarg;
	    continue;

	case 'a':
	    my.archives.append(optarg);
	    break;

	case 'D':
	    sts = __pmParseDebug(optarg);
	    if (sts < 0) {
		pmprintf("%s: unrecognized debug flag specification (%s)\n",
			pmProgname, optarg);
		errflg++;
	    } else
		pmDebug |= sts;
	    break;

	case 'h':
	    my.hosts.append(optarg);
	    break;

	case 'L':		/* local context */
	    my.Lflag = 1;
	    break;

	case 'n':		/* alternative PMNS */
	    my.pmnsfile = optarg;
	    break;

	case 'O':		/* sample offset */
	    my.Oflag = optarg;
	    break;

	case 'p':		/* existing pmtime port */
	    my.port = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || c < 0) {
		pmprintf("%s: -p requires a numeric argument\n", pmProgname);
		errflg++;
	    }
	    break;

	case 'S':		/* start run time */
	    my.Sflag = optarg;
	    break;

	case 't':		/* sampling interval */
	    if (pmParseInterval(optarg, &my.delta, &msg) < 0) {
		pmprintf("%s: cannot parse interval\n%s", pmProgname, msg);
		free(msg);
		errflg++;
	    }
	    continue;

	case 'T':		/* run time */
	    my.Tflag = optarg;
	    break;

	case 'V':		/* version */
	    printf("%s %s\n", pmProgname, VERSION);
	    exit(0);

	case 'z':		/* timezone from host */
	    if (my.tz != NULL) {
		pmprintf("%s: at most one of -Z and/or -z allowed\n",
			pmProgname);
		errflg++;
	    }
	    my.zflag++;
	    break;

	case 'Z':		/* $TZ timezone */
	    if (my.zflag) {
		pmprintf("%s: at most one of -Z and/or -z allowed\n",
			pmProgname);
		errflg++;
	    }
	    my.tz = optarg;
	    break;

	default:
	    unknown = 1;
	    break;
	}
    } while (!unknown);

    return c;
}

// a := a + b for struct timevals
void App::timevalAdd(struct timeval *a, struct timeval *b)
{
    a->tv_usec += b->tv_usec;
    if (a->tv_usec > 1000000) {
	a->tv_usec -= 1000000;
	a->tv_sec++;
    }
    a->tv_sec += b->tv_sec;
}

//
// a : b for struct timevals ... <0 for a<b, ==0 for a==b, >0 for a>b
//
int App::timevalCmp(struct timeval *a, struct timeval *b)
{
    int res = (int)(a->tv_sec - b->tv_sec);
    if (res == 0)
	res = (int)(a->tv_usec - b->tv_usec);
    return res;
}

// convert timeval to seconds
double App::timevalToSeconds(struct timeval t)
{
    return t.tv_sec + (t.tv_usec / 1000000.0);
}

// conversion from seconds (double precision) to struct timeval
void App::timevalFromSeconds(double value, struct timeval *tv)
{
    tv->tv_sec = (time_t)value;
    tv->tv_usec = (long)(((value - (double)tv->tv_sec) * 1000000.0));
}

// debugging, display seconds-since-epoch in human readable format
char *App::timeString(double seconds)
{
    static char string[32];
    time_t secs = (time_t)seconds;
    char *s;

    s = pmCtime(&secs, string);
    s[strlen(s)-1] = '\0';
    return s;
}

// return a string containing hour and milliseconds
char *App::timeHiResString(double time)
{
    static char s[16];
    char m[8];
    time_t secs = (time_t)time;
    struct tm t;

    sprintf(m, "%.3f", time - floor(time));
    pmLocaltime(&secs, &t);
    sprintf(s, "%02d:%02d:%02d.%s", t.tm_hour, t.tm_min, t.tm_sec, m+2);
    s[strlen(s)-1] = '\0';
    return s;
}

void App::nomem(void)
{
    // no point trying to report anything ... dump core is the best bet
    abort();
}

QPixmap App::cached(const QString &image)
{
    if (QPixmap *p = QPixmapCache::find(image))
	return *p;

    QPixmap pm;
    pm = QPixmap::fromImage(QImage(image),
			    Qt::OrderedDither | Qt::OrderedAlphaDither);
    if (pm.isNull())
	return QPixmap();

    QPixmapCache::insert(image, pm);
    return pm;
}

// call startconsole() after command line args processed so pmDebug
// has a chance to be set
//
void App::startconsole(void)
{
    struct timeval origin;

    gettimeofday(&origin, NULL);
    console = new Console(origin);
}
