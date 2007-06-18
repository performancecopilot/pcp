/*
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
 * Copyright (c) 2007, Nathan Scott.  All Rights Reserved.
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
#include <qstatusbar.h>
#include <qsettings.h>
#include "main.h"

#define DESPERATE 0

int		Cflag;
Settings	settings;

// Globals used to provide single instances of classes used across kmchart
Tab 		*activeTab;	// currently active Tab
Tab 		**tabs;		// array of Tabs
int		ntabs;		// number of Tabs
PMC_Group	 *liveGroup;	// one libpcp_pmc group for all hosts
PMC_Group	 *archiveGroup;	// one libpcp_pmc group for all archives
PMC_Group	*activeGroup;	// currently active metric fetchgroup
Source 		*liveSources;	// one source class for all host sources
Source		*archiveSources;// one source class for all archive sources
Source		*activeSources;	// currently active list of sources
TimeControl	*kmtime;	// one timecontrol class for kmtime
KmChart		*kmchart;

static void usage(void)
{
    pmprintf("Usage: %s [options]\n\n"
"Options:\n"
"  -A align      align sample times on natural boundaries\n"
"  -a archive    add PCP log archive to metrics source list\n"
"  -c configfile initial view to load\n"
"  -C            with -c, parse config, report any errors and exit\n"
"  -h host       add host to list of live metrics sources\n"
"  -n pmnsfile   use an alternative PMNS\n"
"  -O offset     initial offset into the time window\n"
"  -p port       port name for connection to existing time control\n"
"  -s samples    sample history [default: %d points]\n"
"  -S starttime  start of the time window\n"
"  -T endtime    end of the time window\n"
"  -t interval   sample interval [default: %d seconds]\n"
"  -v samples    visible history [default: %d points]\n"
"  -Z timezone   set reporting timezone\n"
"  -z            set reporting timezone to local time of metrics source\n",
	pmProgname,
	DEFAULT_SAMPLE_POINTS, DEFAULT_SAMPLE_INTERVAL, DEFAULT_VISIBLE_POINTS);
    pmflush();
    exit(1);
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

// convert timeval to seconds
double tosec(struct timeval t)
{
    return t.tv_sec + (t.tv_usec / 1000000.0);
}

// create a time range in seconds from (delta x points)
double torange(struct timeval t, int points)
{
    return tosec(t) * points;
}

// conversion from seconds (double precision) to struct timeval
void fromsec(double value, struct timeval *tv)
{
    double usec = (value - (unsigned int)value) / 1000000.0;
    tv->tv_sec = (unsigned int)value;
    tv->tv_usec = (unsigned int)usec;
}

// conversion from struct timeval to seconds (double precision)
double secondsFromTV(struct timeval *tv)
{
    return (double)tv->tv_sec + ((double)tv->tv_usec / 1000.0);
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

// debugging, display seconds-since-epoch in human readable format
char *timestring(double seconds)
{
    static char string[32];
    time_t secs = (time_t)seconds;
    char *s;

    s = pmCtime(&secs, string);
    s[strlen(s)-1] = '\0';
    return s;
}

void nomem(void)
{
    // no point trying to report anything ... dump core is the best bet
    abort();
}

void writeSettings(void)
{
    QSettings userSettings;
    QString userHomeDir;

    userHomeDir = QDir::homeDirPath();
    userHomeDir.append("/.pcp/kmchart");
    userSettings.setPath(QApplication::tr("PCP"), QApplication::tr("kmchart"));
    userSettings.insertSearchPath(QSettings::Unix, userHomeDir);

    userSettings.beginGroup(QApplication::tr("kmchart"));
    if (settings.sampleHistoryModified)
	userSettings.writeEntry(QApplication::tr("sample_points"), 
				settings.sampleHistory);
    if (settings.visibleHistoryModified)
	userSettings.writeEntry(QApplication::tr("visible_points"),
				settings.visibleHistory);
    if (settings.defaultColorsModified)
	userSettings.writeEntry(QApplication::tr("plot_default_colors"),
				settings.defaultColorNames);
    if (settings.chartBackgroundModified)
	userSettings.writeEntry(QApplication::tr("chart_background_color"),
				settings.chartBackgroundName);
    if (settings.chartHighlightModified)
	userSettings.writeEntry(QApplication::tr("chart_highlight_color"),
				settings.chartHighlightName);
    if (settings.styleModified)
	userSettings.writeEntry(QApplication::tr("style"), settings.styleName);
    userSettings.endGroup();
}

void checkHistory(int samples, int visible)
{
    // sanity checking on sample sizes
    if (samples < visible) {
	settings.sampleHistory = settings.visibleHistory;
	settings.sampleHistoryModified = 1;
    }
    if (samples < 1) {
	settings.sampleHistory = 1;
	settings.sampleHistoryModified = 1;
    }
    if (samples > MAXIMUM_POINTS) {
	settings.sampleHistory = MAXIMUM_POINTS;
	settings.sampleHistoryModified = 1;
    }
    if (visible < 1) {
	settings.visibleHistory = 1;
	settings.visibleHistoryModified = 1;
    }
    if (visible > MAXIMUM_POINTS) {
	settings.visibleHistory = MAXIMUM_POINTS;
	settings.visibleHistoryModified = 1;
    }
}

void readSettings(void)
{
    QSettings	userSettings;
    QString	userHomeDir;
    bool	ok;

    userHomeDir = QDir::homeDirPath();
    userHomeDir.append("/.pcp/kmchart");
    userSettings.setPath(QApplication::tr("PCP"), QApplication::tr("kmchart"));
    userSettings.insertSearchPath(QSettings::Unix, userHomeDir);

    userSettings.beginGroup(QApplication::tr("kmchart"));

    //
    // Parameters related to numbers of samples
    //
    settings.sampleHistory = userSettings.readNumEntry(
	QApplication::tr("sample_points"), DEFAULT_SAMPLE_POINTS);
    settings.visibleHistory = userSettings.readNumEntry(
	QApplication::tr("visible_points"), DEFAULT_VISIBLE_POINTS);
    checkHistory(settings.sampleHistory, settings.visibleHistory);
    if (settings.sampleHistoryModified)
	userSettings.writeEntry(QApplication::tr("sample_points"), 
				settings.sampleHistory);
    if (settings.visibleHistoryModified)
	userSettings.writeEntry(QApplication::tr("visible_points"), 
				settings.visibleHistory);

    //
    // Everything colour related
    //
    QStringList colorList = userSettings.readListEntry(
		QApplication::tr("plot_default_colors"), &ok);
    if (!ok)
	colorList << QApplication::tr("yellow") << QApplication::tr("blue") <<
			QApplication::tr("red") << QApplication::tr("green") <<
			QApplication::tr("violet");
    settings.defaultColorNames = colorList;

    QStringList::Iterator it = colorList.begin();
    while (it != colorList.end()) {
	settings.defaultColors << QColor(*it);
	++it;
    }

    settings.chartBackgroundName = userSettings.readEntry(
		QApplication::tr("chart_background_color"),
		QApplication::tr("light steel blue"));
    settings.chartBackground = QColor(settings.chartBackgroundName);

    settings.chartHighlightName = userSettings.readEntry(
		QApplication::tr("chart_highlight_color"),
		QApplication::tr("blue"));
    settings.chartHighlight = QColor(settings.chartHighlightName);

    //
    // Application GUI Styles
    //
    settings.defaultStyle = settings.style = &QApplication::style();
    settings.styleName = userSettings.readEntry(QApplication::tr("style"),
						QString::null, &ok);
    if (ok)
	QApplication::setStyle(settings.styleName);

    userSettings.endGroup();
}

int
main(int argc, char ** argv)
{
    int			c, sts;
    int			errflg = 0;
    char		*endnum, *msg;
    char		*pmnsfile = NULL;	/* local namespace file */
    char		*Sflag = NULL;		/* argument of -S flag */
    char		*Tflag = NULL;		/* argument of -T flag */
    char		*Aflag = NULL;		/* argument of -A flag */
    char		*Oflag = NULL;		/* argument of -O flag */
    int			zflag = 0;		/* for -z */
    char		*tz = NULL;		/* for -Z timezone */
    int			sh = -1;		/* sample history length */
    int			vh = -1;		/* visible history length */
    int			port = -1;		/* kmtime port number */
    struct timeval	delta = { DEFAULT_SAMPLE_INTERVAL, 0 };
    struct timeval	logStartTime;
    struct timeval	logEndTime;
    struct timeval	realStartTime;
    struct timeval	realEndTime;
    struct timeval	position;
    PMC_StrList		hosts;
    PMC_StrList		archives;
    PMC_StrList		configs;
    PMC_String		tzLabel;
    PMC_String		tzString;

    QApplication a(argc, argv);
    pmProgname = basename(argv[0]);
    readSettings();

    liveGroup = new PMC_Group();
    liveSources = new Source(liveGroup);
    archiveGroup = new PMC_Group();
    archiveSources = new Source(archiveGroup);
    tabs = (Tab **)calloc(2, sizeof(Tab *));
    tabs[0] = new Tab();
    tabs[1] = new Tab();

    while ((c = getopt(argc, argv, "A:a:Cc:D:h:n:O:p:s:S:T:t:v:zZ:?")) != EOF) {
	switch (c) {

	case 'A':	/* sample alignment */
	    Aflag = optarg;
	    break;

	case 'a':
	    archives.append(optarg);
	    break;

	case 'C':
	    Cflag++;
	    break;

	case 'c':
	    configs.append(optarg);
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
	    hosts.append(optarg);
	    break;

	case 'n':		/* alternative PMNS */
	    pmnsfile = optarg;
	    break;

	case 'O':		/* sample offset */
	    Oflag = optarg;
	    break;

	case 'p':		/* existing kmtime port */
	    port = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || c < 0) {
		pmprintf("%s: -p requires a numeric argument\n", pmProgname);
		errflg++;
	    }
	    break;

	case 's':		/* sample history */
	    sh = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || sh < 1) {
		pmprintf("%s: -s requires a numeric argument, larger than 1\n",
			 pmProgname);
		errflg++;
	    }
	    break;

	case 'S':		/* start run time */
	    Sflag = optarg;
	    break;

	case 't':		/* sampling interval */
	    if (pmParseInterval(optarg, &delta, &msg) < 0) {
		pmprintf("%s: cannot parse interval\n%s", pmProgname, msg);
		free(msg);
		errflg++;
	    }
	    break;

	case 'T':		/* run time */
	    Tflag = optarg;
	    break;

	case 'v':		/* vertical history */
	    vh = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || vh < 1) {
		pmprintf("%s: -v requires a numeric argument, larger than 1\n",
			 pmProgname);
		errflg++;
	    }
	    break;

	case 'z':		/* timezone from host */
	    if (tz != NULL) {
		pmprintf("%s: at most one of -Z and/or -z allowed\n",
			pmProgname);
		errflg++;
	    }
	    zflag++;
	    break;

	case 'Z':		/* $TZ timezone */
	    if (zflag) {
		pmprintf("%s: at most one of -Z and/or -z allowed\n",
			pmProgname);
		errflg++;
	    }
	    tz = optarg;
	    break;

	case '?':
	default:
	    usage();
	    /* NOTREACHED */
	}
    }

    if (errflg) {
	usage();
	/* NOTREACHED */
    }

    //
    // Deal with user requested sample/visible points settings.  These
    // (command line) override the QSettings values, for this instance
    // of kmchart.  They should not be written though, unless requested
    // later via the Settings dialog.
    //
    if (vh != -1 || sh != -1) {
	if (sh == -1)
	    sh = settings.sampleHistory;
	if (vh == -1)
	    vh = settings.visibleHistory;
	checkHistory(sh, vh);
	if (settings.sampleHistoryModified || settings.visibleHistoryModified) {
	    pmprintf("%s: invalid sample/visible history\n", pmProgname);
	    pmflush();
	    exit(1);
	}
	settings.sampleHistory = sh;
	settings.visibleHistory = vh;
    }

    if (pmnsfile && (sts = pmLoadNameSpace(pmnsfile)) < 0) {
	pmprintf("%s: %s\n", pmProgname, pmErrStr(sts));
	pmflush();
	exit(1);
	/*NOTREACHED*/
    }

    // Create all of the sources
    for (c = 0; c < (int)hosts.length(); c++) {
	if (liveGroup->use(PM_CONTEXT_HOST, hosts[c].ptr()) < 0)
	    hosts.remove(c);
	else
	    liveSources->add(liveGroup->which());
    }
    for (c = 0; c < (int)archives.length(); c++) {
	if (archiveGroup->use(PM_CONTEXT_ARCHIVE, archives[c].ptr()) < 0)
	    hosts.remove(c);
	else
	    archiveSources->add(archiveGroup->which());
    }
    if (hosts.length() == 0 && archives.length() == 0) {
	liveGroup->createLocalContext();
	liveSources->add(liveGroup->which());
    }
    pmflush();

    if (zflag) {
	if (archives.length() > 0)
	    archiveGroup->useTZ();
	else
	    liveGroup->useTZ();
    }
    else if (Tflag) {
	if (archives.length() > 0)
	    archiveGroup->useTZ(PMC_String(tz));
	else
	    liveGroup->useTZ(PMC_String(tz));
	if ((sts = pmNewZone(tz)) < 0) {
	    pmprintf("%s: cannot set timezone to \"%s\": %s\n",
		    pmProgname, (char *)tz, pmErrStr(sts));
	    pmflush();
	    exit(1);
	    /*NOTREACHED*/
	}
    }

    //
    // Choose which Tab will be displayed initially - archive/live.
    // If any archives given on command line, we go Archive mode;
    // otherwise Live mode wins.  Our initial kmtime connection is
    // set in that mode too.  Later we'll make a second connection
    // in the other mode (and only "on-demand").
    //
    if (archives.length() > 0) {
	archiveGroup->defaultTZ(tzLabel, tzString);
	archiveGroup->updateBounds();
	logStartTime = archiveGroup->logStart();
	logEndTime = archiveGroup->logEnd();
	if ((sts = pmParseTimeWindow(Sflag, Tflag, Aflag, Oflag,
				     &logStartTime, &logEndTime,
				     &realStartTime, &realEndTime,
				     &position, &msg)) < 0) {
	    pmprintf("Cannot parse archive time window\n%s\n", msg);
	    free(msg);
	    usage();
	    /*NOTREACHED*/
	}
	// move position to account for initial visible points
	// TODO: pmchart had an option to start at archive end
fprintf(stderr, "RESET position from %s to ", timestring(tosec(position)));
	for (c = 0; c < settings.sampleHistory - 2; c++)
	    tadd(&position, &delta);
fprintf(stderr, "%s\n", timestring(tosec(position)));
    }
    else {
	liveGroup->defaultTZ(tzLabel, tzString);
	gettimeofday(&logStartTime, NULL);
	logEndTime.tv_sec = logEndTime.tv_usec = INT_MAX;
	if ((sts = pmParseTimeWindow(Sflag, Tflag, Aflag, Oflag,
					&logStartTime, &logEndTime,
					&realStartTime, &realEndTime,
					&position, &msg)) < 0) {
	    pmprintf("Cannot parse live time window\n%s\n", msg);
	    free(msg);
	    usage();
	    /*NOTREACHED*/
	}
    }

    kmchart = new KmChart();
    kmtime = new TimeControl();

    // Start kmtime process for time management
    kmtime->init(port, archives.length() == 0,
		 &delta, &position, &realStartTime, &realEndTime,
		 tzString.ptr(), tzString.length(),
		 tzLabel.ptr(), tzLabel.length());

    kmchart->init();
    tabs[0]->init(kmchart->tabWidget(),
			settings.sampleHistory, settings.visibleHistory,
			liveGroup, KM_SOURCE_HOST, "Live",
			kmtime->liveInterval(), kmtime->livePosition());
    tabs[1]->init(kmchart->tabWidget(),
			settings.sampleHistory, settings.visibleHistory,
			archiveGroup, KM_SOURCE_ARCHIVE, "Archive",
			kmtime->archiveInterval(), kmtime->archivePosition());
    kmchart->setActiveTab(archives.length() > 0 ? 1 : 0, true);

    for (c = 0; c < (int)configs.length(); c++)
	OpenViewDialog::openView(configs[c].ptr());

    ntabs = 2;	// last, so we don't react to kmtime step messages until now

    kmchart->enableUI();

    if (Cflag)	// done with -c config, quit
	return 0;

    delete kmchart->statusBar();
    kmchart->show();
    a.connect(&a, SIGNAL(lastWindowClosed()), &a, SLOT(quit()));

    FileIconProvider *provider = new FileIconProvider(kmchart, "fileIcons");
    QFileDialog::setIconProvider(provider);

    return a.exec();
}
