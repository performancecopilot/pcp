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

#define DESPERATE 0

int		Cflag;
Settings	globalSettings;

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
	pmProgname, KmChart::defaultSamplePoints,
	KmChart::defaultSampleInterval, KmChart::defaultVisiblePoints);
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

void nomem(void)
{
    // no point trying to report anything ... dump core is the best bet
    abort();
}

void writeSettings(void)
{
    QString path = QDir::homePath();
    path.append("/.pcp/kmchart");
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, path);

    QSettings userSettings;
    userSettings.beginGroup("kmchart");
    if (globalSettings.sampleHistoryModified)
	userSettings.setValue("sample_points", globalSettings.sampleHistory);
    if (globalSettings.visibleHistoryModified)
	userSettings.setValue("visible_points", globalSettings.visibleHistory);
    if (globalSettings.defaultColorsModified)
	userSettings.setValue("plot_default_colors",
				globalSettings.defaultColorNames);
    if (globalSettings.chartBackgroundModified)
	userSettings.setValue("chart_background_color",
				globalSettings.chartBackgroundName);
    if (globalSettings.chartHighlightModified)
	userSettings.setValue("chart_highlight_color",
				globalSettings.chartHighlightName);
    if (globalSettings.styleModified)
	userSettings.setValue("style", globalSettings.styleName);
    userSettings.endGroup();
}

void checkHistory(int samples, int visible)
{
    // sanity checking on sample sizes
    if (samples < KmChart::minimumPoints) {
	globalSettings.sampleHistory = KmChart::minimumPoints;
	globalSettings.sampleHistoryModified = 1;
    }
    if (samples > KmChart::maximumPoints) {
	globalSettings.sampleHistory = KmChart::maximumPoints;
	globalSettings.sampleHistoryModified = 1;
    }
    if (visible < KmChart::minimumPoints) {
	globalSettings.visibleHistory = KmChart::minimumPoints;
	globalSettings.visibleHistoryModified = 1;
    }
    if (visible > KmChart::maximumPoints) {
	globalSettings.visibleHistory = KmChart::maximumPoints;
	globalSettings.visibleHistoryModified = 1;
    }
    if (samples < visible) {
	globalSettings.sampleHistory = globalSettings.visibleHistory;
	globalSettings.sampleHistoryModified = 1;
    }
}

void setupEnvironment(void)
{
    QString confirm = pmGetConfig("PCP_BIN_DIR");
    confirm.append("/kmquery");
    setenv("PCP_XCONFIRM_PROG", (const char *)confirm.toAscii(), 1);
    setenv("PCP_STDERR", "DISPLAY", 1);

    QCoreApplication::setOrganizationName("PCP");
    QCoreApplication::setApplicationName("kmchart");
}

void readSettings(void)
{
    QString home = QDir::homePath();
    home.append("/.pcp/kmchart");
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, home);

    QSettings userSettings;
    userSettings.beginGroup(QApplication::tr("kmchart"));

    //
    // Parameters related to numbers of samples
    //
    globalSettings.sampleHistory = userSettings.value("sample_points",
					KmChart::defaultSamplePoints).toInt();
    globalSettings.visibleHistory = userSettings.value("visible_points",
					KmChart::defaultVisiblePoints).toInt();
    checkHistory(globalSettings.sampleHistory, globalSettings.visibleHistory);
    if (globalSettings.sampleHistoryModified)
	userSettings.setValue("sample_points", globalSettings.sampleHistory);
    if (globalSettings.visibleHistoryModified)
	userSettings.setValue("visible_points", globalSettings.visibleHistory);

    //
    // Everything colour related
    //
    QStringList colorList;
    if (userSettings.contains("plot_default_colors") == true)
	colorList = userSettings.value("plot_default_colors").toStringList();
    else
	colorList << QApplication::tr("yellow") << QApplication::tr("blue") <<
			QApplication::tr("red") << QApplication::tr("green") <<
			QApplication::tr("violet");
    globalSettings.defaultColorNames = colorList;

    QStringList::Iterator it = colorList.begin();
    while (it != colorList.end()) {
	globalSettings.defaultColors << QColor(*it);
	++it;
    }

    globalSettings.chartBackgroundName = userSettings.value(
		"chart_background_color", "light steel blue").toString();
    globalSettings.chartBackground = QColor(globalSettings.chartBackgroundName);

    globalSettings.chartHighlightName = userSettings.value(
		"chart_highlight_color", "blue").toString();
    globalSettings.chartHighlight = QColor(globalSettings.chartHighlightName);

    //
    // Application GUI Styles
    //
    globalSettings.defaultStyle = globalSettings.style = QApplication::style();
    if (userSettings.contains("style") == true) {
	globalSettings.styleName = userSettings.value("style").toString();
	QApplication::setStyle(globalSettings.styleName);
    }

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
    struct timeval	delta = { KmChart::defaultSampleInterval, 0 };
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
    setupEnvironment();
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
	    /*NOTREACHED*/
	}
    }

    console = new Console();
    fileIconProvider = new FileIconProvider();

    kmchart = new KmChart;
    kmtime = new TimeControl;

    // Start kmtime process for time management
    kmtime->init(port, archives.length() == 0,
		 &delta, &position, &realStartTime, &realEndTime,
		 tzString.ptr(), tzString.length(),
		 tzLabel.ptr(), tzLabel.length());

    kmchart->init();
    tabs[0]->init(kmchart->tabWidget(),
			globalSettings.sampleHistory,
			globalSettings.visibleHistory,
			liveGroup, KmTime::HostSource, "Live",
			kmtime->liveInterval(), kmtime->livePosition());
    tabs[1]->init(kmchart->tabWidget(),
			globalSettings.sampleHistory,
			globalSettings.visibleHistory,
			archiveGroup, KmTime::ArchiveSource, "Archive",
			kmtime->archiveInterval(), kmtime->archivePosition());
    kmchart->setActiveTab(archives.length() > 0 ? 1 : 0, true);

    for (c = 0; c < (int)configs.length(); c++)
	OpenViewDialog::openView(configs[c].ptr());

    ntabs = 2;	// last, so we don't react to kmtime step messages until now

    kmchart->enableUi();

    if (Cflag)	// done with -c config, quit
	return 0;

    kmchart->show();
    a.connect(&a, SIGNAL(lastWindowClosed()), &a, SLOT(quit()));

    return a.exec();
}
