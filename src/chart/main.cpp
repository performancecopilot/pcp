/*
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
 * Copyright (c) 2007, Aconex.  All Rights Reserved.
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
 */
#include <QtCore/QSettings>
#include <QtGui/QApplication>
#include <QtGui/QStatusBar>
#include "main.h"
#include "openviewdialog.h"

#define DESPERATE 0

int Cflag;
QFont globalFont;
Settings globalSettings;

// Globals used to provide single instances of classes used across kmchart
Tab *activeTab;		// currently active Tab
QList<Tab*> tabs;	// list of Tabs (pages)
QmcGroup *liveGroup;	// one metrics class group for all hosts
QmcGroup *archiveGroup;	// one metrics class group for all archives
QmcGroup *activeGroup;	// currently active metric fetchgroup
Source *liveSources;	// one source class for all host sources
Source *archiveSources;	// one source class for all archive sources
Source *activeSources;	// currently active list of sources
TimeControl *kmtime;	// one timecontrol class for kmtime
KmChart *kmchart;

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
	pmProgname, globalSettings.sampleHistory, globalSettings.chartDelta,
	globalSettings.visibleHistory);
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

// debugging, display seconds-since-epoch in human readable format
char *timeString(double seconds)
{
    static char string[32];
    time_t secs = (time_t)seconds;
    char *s;

    s = pmCtime(&secs, string);
    s[strlen(s)-1] = '\0';
    return s;
}

double KmTime::secondsToUnits(double value, KmTime::DeltaUnits units)
{
    switch (units) {
    case Milliseconds:
	value = value * 1000.0;
	break;
    case Minutes:
	value = value / 60.0;
	break;
    case Hours:
	value = value / (60.0 * 60.0);
	break;
    case Days:
	value = value / (60.0 * 60.0 * 24.0);
	break;
    case Weeks:
	value = value / (60.0 * 60.0 * 24.0 * 7.0);
	break;
    case Seconds:
    default:
	break;
    }
    return value;
}

double KmTime::deltaValue(QString delta, KmTime::DeltaUnits units)
{
    return KmTime::secondsToUnits(delta.trimmed().toDouble(), units);
}

QString KmTime::deltaString(double value, KmTime::DeltaUnits units)
{
    QString delta;

    value = KmTime::secondsToUnits(value, units);
    if ((double)(int)value == value)
	delta.sprintf("%.2f", value);
    else
	delta.sprintf("%.6f", value);
    return delta;
}

void nomem(void)
{
    // no point trying to report anything ... dump core is the best bet
    abort();
}

void setupEnvironment(void)
{
    QString confirm = pmGetConfig("PCP_BIN_DIR");
    confirm.append("/kmquery");
    setenv("PCP_XCONFIRM_PROG", (const char *)confirm.toAscii(), 1);
    setenv("PCP_STDERR", "DISPLAY", 0);	// do not overwrite, for QA

    QCoreApplication::setOrganizationName("PCP");
    QCoreApplication::setApplicationName("kmchart");
}

void writeSettings(void)
{
    QString path = QDir::homePath();
    path.append("/.pcp/kmchart");
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, path);

    QSettings userSettings;
    userSettings.beginGroup("kmchart");
    if (globalSettings.chartDeltaModified)
	userSettings.setValue("chartDelta", globalSettings.chartDelta);
    if (globalSettings.loggerDeltaModified)
	userSettings.setValue("loggerDelta", globalSettings.loggerDelta);
    if (globalSettings.sampleHistoryModified)
	userSettings.setValue("sampleHistory", globalSettings.sampleHistory);
    if (globalSettings.visibleHistoryModified)
	userSettings.setValue("visibleHistory", globalSettings.visibleHistory);
    if (globalSettings.defaultColorsModified)
	userSettings.setValue("plotDefaultColors",
				globalSettings.defaultColorNames);
    if (globalSettings.chartBackgroundModified)
	userSettings.setValue("chartBackgroundColor",
				globalSettings.chartBackgroundName);
    if (globalSettings.chartHighlightModified)
	userSettings.setValue("chartHighlightColor",
				globalSettings.chartHighlightName);
    if (globalSettings.initialToolbarModified)
	userSettings.setValue("initialToolbar", globalSettings.initialToolbar);
    if (globalSettings.toolbarLocationModified)
	userSettings.setValue("toolbarLocation",
				globalSettings.toolbarLocation);
    if (globalSettings.toolbarActionsModified)
	userSettings.setValue("toolbarActions", globalSettings.toolbarActions);
    userSettings.endGroup();
}

void checkHistory(int samples, int visible)
{
    // sanity checking on sample sizes
    if (samples < KmChart::minimumPoints()) {
	globalSettings.sampleHistory = KmChart::minimumPoints();
	globalSettings.sampleHistoryModified = 1;
    }
    if (samples > KmChart::maximumPoints()) {
	globalSettings.sampleHistory = KmChart::maximumPoints();
	globalSettings.sampleHistoryModified = 1;
    }
    if (visible < KmChart::minimumPoints()) {
	globalSettings.visibleHistory = KmChart::minimumPoints();
	globalSettings.visibleHistoryModified = 1;
    }
    if (visible > KmChart::maximumPoints()) {
	globalSettings.visibleHistory = KmChart::maximumPoints();
	globalSettings.visibleHistoryModified = 1;
    }
    if (samples < visible) {
	globalSettings.sampleHistory = globalSettings.visibleHistory;
	globalSettings.sampleHistoryModified = 1;
    }
}

void readSettings(void)
{
    QString home = QDir::homePath();
    home.append("/.pcp/kmchart");
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, home);

    QSettings userSettings;
    userSettings.beginGroup("kmchart");

    //
    // Parameters related to sampling
    //
    globalSettings.chartDelta = userSettings.value("chartDelta",
				KmChart::defaultChartDelta()).toDouble();
    globalSettings.loggerDelta = userSettings.value("loggerDelta",
				KmChart::defaultLoggerDelta()).toDouble();
    globalSettings.sampleHistory = userSettings.value("sampleHistory",
				KmChart::defaultSampleHistory()).toInt();
    globalSettings.visibleHistory = userSettings.value("visibleHistory",
				KmChart::defaultVisibleHistory()).toInt();
    checkHistory(globalSettings.sampleHistory, globalSettings.visibleHistory);
    if (globalSettings.sampleHistoryModified)
	userSettings.setValue("samplePoints", globalSettings.sampleHistory);
    if (globalSettings.visibleHistoryModified)
	userSettings.setValue("visiblePoints", globalSettings.visibleHistory);

    //
    // Everything colour related
    //
    QStringList colorList;
    if (userSettings.contains("plotDefaultColors") == true)
	colorList = userSettings.value("plotDefaultColors").toStringList();
    else
	colorList << "yellow" << "blue" << "red" << "green" << "violet";
    globalSettings.defaultColorNames = colorList;
    for (int i = 0; i < colorList.size(); i++)
	globalSettings.defaultColors << QColor(colorList.at(i));
    globalSettings.chartBackgroundName = userSettings.value(
		"chartBackgroundColor", "black").toString();
    globalSettings.chartBackground = QColor(globalSettings.chartBackgroundName);

    globalSettings.chartHighlightName = userSettings.value(
		"chartHighlightColor", "blue").toString();
    globalSettings.chartHighlight = QColor(globalSettings.chartHighlightName);

    //
    // Toolbar user preferences
    //
    globalSettings.initialToolbar = userSettings.value(
					"initialToolbar", 1).toInt();
    globalSettings.toolbarLocation = userSettings.value(
					"toolbarLocation", 0).toInt();
    QStringList actionList;
    if (userSettings.contains("toolbarActions") == true)
	globalSettings.toolbarActions =
			userSettings.value("toolbarActions").toStringList();
    // else: (defaults come from the kmchart.ui interface specification)

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
    struct timeval	delta;
    struct timeval	origin;
    struct timeval	logStartTime;
    struct timeval	logEndTime;
    struct timeval	realStartTime;
    struct timeval	realEndTime;
    struct timeval	position;
    QList<Tab *>	defaultTabs;
    QStringList		hosts;
    QStringList		archives;
    QStringList		configs;
    QString		tzLabel;
    QString		tzString;

    gettimeofday(&origin, NULL);
    QApplication a(argc, argv);
    pmProgname = basename(argv[0]);
    setupEnvironment();
    readSettings();

    fromsec(globalSettings.chartDelta, &delta);

    liveGroup = new QmcGroup();
    liveSources = new Source(liveGroup);
    archiveGroup = new QmcGroup();
    archiveSources = new Source(archiveGroup);
    defaultTabs.append(new Tab);	// default Live Tab
    defaultTabs.append(new Tab);	// default Archive Tab

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
	}
    }

    if (errflg)
	usage();

    console = new Console(origin);

    //
    // Deal with user requested sample/visible points globalSettings.  These
    // (command line) override the QSettings values, for this instance
    // of kmchart.  They should not be written though, unless requested
    // later via the Settings dialog.
    //
    if (vh != -1 || sh != -1) {
	if (sh == -1)
	    sh = globalSettings.sampleHistory;
	if (vh == -1)
	    vh = globalSettings.visibleHistory;
	checkHistory(sh, vh);
	if (globalSettings.sampleHistoryModified ||
	    globalSettings.visibleHistoryModified) {
	    pmprintf("%s: invalid sample/visible history\n", pmProgname);
	    pmflush();
	    exit(1);
	}
	globalSettings.sampleHistory = sh;
	globalSettings.visibleHistory = vh;
    }
    console->post("Global settings setup complete");

    if (pmnsfile && (sts = pmLoadNameSpace(pmnsfile)) < 0) {
	pmprintf("%s: %s\n", pmProgname, pmErrStr(sts));
	pmflush();
	exit(1);
    }

    // Create all of the sources
    for (c = 0; c < hosts.size(); c++) {
	if (liveGroup->use(PM_CONTEXT_HOST, hosts[c]) < 0)
	    hosts.removeAt(c);
	else
	    liveSources->add(liveGroup->which());
    }
    for (c = 0; c < archives.size(); c++) {
	if (archiveGroup->use(PM_CONTEXT_ARCHIVE, archives[c]) < 0)
	    hosts.removeAt(c);
	else
	    archiveSources->add(archiveGroup->which());
    }
    if (hosts.size() == 0 && archives.size() == 0) {
	liveGroup->createLocalContext();
	liveSources->add(liveGroup->which());
    }
    pmflush();
    console->post("Sources setup complete (%d hosts, %d archives)",
			hosts.size(), archives.size());

    if (zflag) {
	if (archives.size() > 0)
	    archiveGroup->useTZ();
	liveGroup->useTZ();
    }
    else if (Tflag) {
	if (archives.size() > 0)
	    archiveGroup->useTZ(QString(tz));
	liveGroup->useTZ(QString(tz));
	if ((sts = pmNewZone(tz)) < 0) {
	    pmprintf("%s: cannot set timezone to \"%s\": %s\n",
		    pmProgname, (char *)tz, pmErrStr(sts));
	    pmflush();
	    exit(1);
	}
    }

    //
    // Choose which Tab will be displayed initially - archive/live.
    // If any archives given on command line, we go Archive mode;
    // otherwise Live mode wins.  Our initial kmtime connection is
    // set in that mode too.  Later we'll make a second connection
    // in the other mode (and only "on-demand").
    //
    if (archives.size() > 0) {
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
	}
	// move position to account for initial visible points
	for (c = 0; c < globalSettings.sampleHistory - 2; c++)
	    tadd(&position, &delta);
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
	}
    }
    console->post("Timezones and time window setup complete");

#ifdef IS_DARWIN
    c = 9;
#else
    c = 7;
#endif
    globalFont = QFont("Sans Serif", c);
    fileIconProvider = new FileIconProvider();
    kmchart = new KmChart;
    kmtime = new TimeControl;
    console->post("Phase1 user interface constructors complete");

    // Start kmtime process for time management
    kmtime->init(port, archives.size() == 0, &delta, &position,
		 &realStartTime, &realEndTime, tzString, tzLabel);

    kmchart->init();
    defaultTabs.at(0)->init(kmchart->tabWidget(),
		globalSettings.sampleHistory, globalSettings.visibleHistory,
		liveGroup, KmTime::HostSource, "Live",
		kmtime->liveInterval(), kmtime->livePosition());
    defaultTabs.at(1)->init(kmchart->tabWidget(),
		globalSettings.sampleHistory, globalSettings.visibleHistory,
		archiveGroup, KmTime::ArchiveSource, "Archive",
		kmtime->archiveInterval(), kmtime->archivePosition());

    //
    // We setup the global tabs list late, so we don't have to deal
    // with kmtime messages reaching the Tabs until we're all setup.
    //
    tabs = defaultTabs;
    kmchart->setActiveTab(archives.size() > 0 ? 1 : 0, true);
    console->post("Phase2 user interface setup complete");

    for (c = 0; c < configs.size(); c++)
	OpenViewDialog::openView((const char *)configs[c].toAscii());

    if (Cflag)	// done with -c config, quit
	return 0;

    kmchart->enableUi();
    kmchart->show();
    console->post("Top level window shown");

    a.connect(&a, SIGNAL(lastWindowClosed()), kmchart, SLOT(quit()));
    return a.exec();
}
