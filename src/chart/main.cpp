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
#define PM_USE_CONTEXT_LOCAL 1

int Cflag;
int Lflag;
int Wflag;
char *outfile;
char *outgeometry;
QFont globalFont;
Settings globalSettings;

// Globals used to provide single instances of classes used across kmchart
QmcGroup *liveGroup;	// one metrics class group for all hosts
QmcGroup *archiveGroup;	// one metrics class group for all archives
QmcGroup *activeGroup;	// currently active metric fetchgroup
TimeControl *kmtime;	// one timecontrol class for kmtime
KmChart *kmchart;

static void usage(void)
{
    pmprintf("Usage: %s [options] [sources]\n\n"
"Options:\n"
"  -A align      align sample times on natural boundaries\n"
"  -a archive    add PCP log archive to metrics source list\n"
"  -c configfile initial view to load\n"
"  -C            with -c, parse config, report any errors and exit\n"
"  -CC           like -C, but also connect to pmcd to check semantics\n"
"  -g geometry   image geometry Width x Height (WxH)\n"
"  -h host       add host to list of live metrics sources\n"
#ifdef PM_USE_CONTEXT_LOCAL
"  -L            directly fetch metrics from localhost, PMCD is not used\n"
#endif
"  -n pmnsfile   use an alternative PMNS\n"
"  -o outfile    export image to outfile\n"
"  -O offset     initial offset into the time window\n"
"  -p port       port name for connection to existing time control\n"
"  -s samples    sample history [default: %d points]\n"
"  -S starttime  start of the time window\n"
"  -T endtime    end of the time window\n"
"  -t interval   sample interval [default: %d seconds]\n"
"  -v visible    visible history [default: %d points]\n"
"  -V            display kmchart version number and exit\n"
"  -W            export images using an opaque (white) background\n"
"  -Z timezone   set reporting timezone\n"
"  -z            set reporting timezone to local time of metrics source\n",
	pmProgname, KmChart::defaultSampleHistory(),
	(int)KmChart::defaultChartDelta(), KmChart::defaultVisibleHistory());
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

//
// a : b for struct timevals ... <0 for a<b, ==0 for a==b, >0 for a>b
//
int tcmp(struct timeval *a, struct timeval *b)
{
    int res = (int)(a->tv_sec - b->tv_sec);
    if (res == 0)
	res = (int)(a->tv_usec - b->tv_usec);
    return res;
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
    tv->tv_sec = (time_t)value;
    tv->tv_usec = (long)(((value - (double)tv->tv_sec) * 1000000.0));
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

// return a string containing hour and milliseconds
char *timeHiResString(double time)
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
    if (globalSettings.defaultSchemeModified)
	userSettings.setValue("defaultColorScheme",
				globalSettings.defaultScheme.colorNames());
    if (globalSettings.colorSchemesModified) {
	userSettings.beginWriteArray("schemes");
	for (int i = 0; i < globalSettings.colorSchemes.size(); i++) {
	    userSettings.setArrayIndex(i);
	    userSettings.setValue("name",
				globalSettings.colorSchemes[i].name());
	    userSettings.setValue("colors",
				globalSettings.colorSchemes[i].colorNames());
	}
	userSettings.endArray();
    }
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
    if (globalSettings.sampleHistoryModified) {
	userSettings.setValue("samplePoints", globalSettings.sampleHistory);
	globalSettings.sampleHistoryModified = false;
    }
    if (globalSettings.visibleHistoryModified) {
	userSettings.setValue("visiblePoints", globalSettings.visibleHistory);
	globalSettings.visibleHistoryModified = false;
    }

    //
    // Everything colour (scheme) related
    //
    QStringList colorList;
    if (userSettings.contains("defaultColorScheme") == true)
	colorList = userSettings.value("defaultColorScheme").toStringList();
    else
	colorList << "yellow" << "blue" << "red" << "green" << "violet";
    globalSettings.defaultScheme.setName("#-cycle");
    globalSettings.defaultScheme.setModified(false);
    globalSettings.defaultScheme.setColorNames(colorList);

    int size = userSettings.beginReadArray("schemes");
    for (int i = 0; i < size; i++) {
	userSettings.setArrayIndex(i);
	ColorScheme scheme;
	scheme.setName(userSettings.value("name").toString());
	scheme.setModified(false);
	scheme.setColorNames(userSettings.value("colors").toStringList());
	globalSettings.colorSchemes.append(scheme);
    }
    userSettings.endArray();

    //
    // Everything (else) colour related
    //
    globalSettings.chartBackgroundName = userSettings.value(
		"chartBackgroundColor", "#6ca2c9").toString();
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

void readSchemes(void)
{
    QString schemes = pmGetConfig("PCP_VAR_DIR");
    QFileInfo fi(schemes.append("/config/kmchart/Schemes"));
    if (fi.exists())
	OpenViewDialog::openView(schemes.toAscii());
}

// Get next color from given scheme or from default colors for #-cycle
QColor nextColor(QString scheme, int *sequence)
{
    QList<QColor> colorList;
    int seq = (*sequence)++;

    for (int i = 0; i < globalSettings.colorSchemes.size(); i++) {
	if (globalSettings.colorSchemes[i].name() == scheme) {
	    colorList = globalSettings.colorSchemes[i].colors();
	    break;
	}
    }
    if (colorList.size() < 2)	// common case
	colorList = globalSettings.defaultScheme.colors();
    if (colorList.size() < 2)	// idiot user!?
	colorList << QColor("yellow") << QColor("blue") << QColor("red")
		  << QColor("green") << QColor("violet");
    seq %= colorList.size();
    return colorList.at(seq);
}

void setupViewGlobals()
{
    int w, h, points, x, y;

    OpenViewDialog::globals(&w, &h, &points, &x, &y);
    if (w || h) {
	QSize size = kmchart->size();
	kmchart->resize(size.expandedTo(QSize(w, h)));
    }
    if (x || y) {
	QPoint pos = kmchart->pos();
	if (x) pos.setX(x);
	if (y) pos.setY(y);
	kmchart->move(pos);
    }
    if (points) {
	if (kmchart->activeTab()->sampleHistory() < points)
	    kmchart->activeTab()->setSampleHistory(points);
	kmchart->activeTab()->setVisibleHistory(points);
    }
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
    int			zflag = 0;		/* for -z (source zone) */
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
    Tab			*tab;
    QStringList		hosts;
    QStringList		archives;
    QStringList		configs;
    QString		tzLabel;
    QString		tzString;
    static const char	*options = "A:a:Cc:D:g:h:Lo:n:O:p:s:S:T:t:Vv:WzZ:?";

    gettimeofday(&origin, NULL);
    pmProgname = basename(argv[0]);
    QApplication a(argc, argv);
    setupEnvironment();
    readSettings();

    fromsec(globalSettings.chartDelta, &delta);

    tab = new Tab;
    liveGroup = new QmcGroup();
    archiveGroup = new QmcGroup();

    while ((c = getopt(argc, argv, options)) != EOF) {
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

	case 'g':
	    outgeometry = optarg;
	    break;

#ifdef PM_USE_CONTEXT_LOCAL
	case 'L':		/* local context */
	    Lflag = 1;
	    break;
#endif

	case 'n':		/* alternative PMNS */
	    pmnsfile = optarg;
	    break;

	case 'o':		/* output image file */
	    outfile = optarg;
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

	case 'W':		/* white image background */
	    Wflag = 1;
	    break;

	case 'v':		/* vertical history */
	    vh = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || vh < 1) {
		pmprintf("%s: -v requires a numeric argument, larger than 1\n",
			 pmProgname);
		errflg++;
	    }
	    break;

	case 'V':		/* version */
	    printf("%s %s\n", pmProgname, VERSION);
	    exit(0);

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

    if (archives.size() > 0)
	while (optind < argc)
	    archives.append(argv[optind++]);
    else
	while (optind < argc)
	    hosts.append(argv[optind++]);

    if (optind != argc)
	errflg++;
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
    if (Lflag)
	liveGroup->use(PM_CONTEXT_LOCAL, QmcSource::localHost);
    for (c = 0; c < hosts.size(); c++) {
	if (liveGroup->use(PM_CONTEXT_HOST, hosts[c]) < 0)
	    hosts.removeAt(c);
    }
    for (c = 0; c < archives.size(); c++) {
	if (archiveGroup->use(PM_CONTEXT_ARCHIVE, archives[c]) < 0)
	    archives.removeAt(c);
    }
    if (!Lflag && hosts.size() == 0 && archives.size() == 0)
	liveGroup->createLocalContext();
    pmflush();
    console->post("Metric group setup complete (%d hosts, %d archives)",
			hosts.size(), archives.size());

    if (zflag) {
	if (archives.size() > 0)
	    archiveGroup->useTZ();
	if (hosts.size() > 0)
	    liveGroup->useTZ();
    }
    else if (tz != NULL) {
	if (archives.size() > 0)
	    archiveGroup->useTZ(QString(tz));
	if (hosts.size() > 0)
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
	if (tcmp(&position, &realStartTime) <= 0)
	    for (c = 0; c < globalSettings.sampleHistory - 2; c++)
		tadd(&position, &delta);
	if (tcmp(&position, &realEndTime) > 0)
	    position = realEndTime;
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

    globalFont = QFont("Sans Serif", KmChart::defaultFontSize());
    fileIconProvider = new FileIconProvider();
    kmchart = new KmChart;
    kmtime = new TimeControl;
    console->post("Phase1 user interface constructors complete");

    // Start kmtime process for time management
    kmtime->init(port, archives.size() == 0, &delta, &position,
		 &realStartTime, &realEndTime, tzString, tzLabel);

    //
    // We setup the kmchart tab list late, so we don't have to deal
    // with kmtime messages reaching the Tabs until we're all setup.
    //
    kmchart->init();
    if (archives.size() == 0) {
	tab->init(kmchart->tabWidget(),
		globalSettings.sampleHistory, globalSettings.visibleHistory,
		liveGroup, KmTime::HostSource, "Live",
		kmtime->liveInterval(), kmtime->livePosition());
    }
    else {
	tab->init(kmchart->tabWidget(),
		globalSettings.sampleHistory, globalSettings.visibleHistory,
		archiveGroup, KmTime::ArchiveSource, "Archive",
		kmtime->archiveInterval(), kmtime->archivePosition());
    }
    kmchart->tabWidget()->insertTab(tab);
    kmchart->setActiveTab(0, true);
    console->post("Phase2 user interface setup complete");

    readSchemes();
    for (c = 0; c < configs.size(); c++)
	OpenViewDialog::openView((const char *)configs[c].toAscii());
    setupViewGlobals();

    if (Cflag)	// done with -c config, quit
	return 0;

    kmchart->enableUi();
    kmchart->show();
    console->post("Top level window shown");

    a.connect(&a, SIGNAL(lastWindowClosed()), kmchart, SLOT(quit()));
    return a.exec();
}
