/*
 * Copyright (c) 2014,2022 Red Hat.
 * Copyright (c) 1997,2004-2006 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2007 Aconex.  All Rights Reserved.
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
#include <math.h>
#include <float.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>
#include <QTextStream>
#include <QStringList>
#include <qmc_group.h>
#include <qmc_metric.h>
#include <qmc_context.h>

#include "libpcp.h"

// Temporary buffer
static char buffer[256];

// List of metrics
static QmcGroup *group;
static QList<QmcMetric*> metrics;
static bool isLive = false;
static int numValues;
static int doMetricType = PM_CONTEXT_HOST;
static bool doMetricFlag = true;
static double doMetricScale = 0.0;
static QString doMetricSource;

// Command line options
static bool dumpFlag = true;
static bool metricFlag;
static bool niceFlag;
static bool unitFlag;
static bool sourceFlag;
static bool timeFlag = true;
static bool timeOffsetFlag;
static bool rawFlag;
static bool shortFlag;
static bool descFlag;
static bool widthFlag;
static bool precFlag;
static bool normFlag;
static bool headerFlag;
static bool fullFlag;
static bool fullXFlag;

static QString errStr = "?";
static QString timeFormat;
static char delimiter = '\t';
static int precision = 3;
static int width = 6;
static int sampleCount;
static int repeatLines;

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("General options"), \
    PMOPT_ALIGN,
    PMOPT_ARCHIVE,
    PMOPT_DEBUG,
    PMOPT_HOST,
    PMOPT_NAMESPACE,
    PMOPT_ORIGIN,
    PMOPT_START,
    PMOPT_SAMPLES,
    PMOPT_FINISH,
    PMOPT_INTERVAL,
    PMOPT_TIMEZONE,
    PMOPT_HOSTZONE,
    PMOPT_VERSION,
    PMOPT_HELP,
    PMAPI_OPTIONS_HEADER("Reporting options"),
    { "config", 1, 'c', "FILE", "read list of metrics from FILE" },
    { "check", 0, 'C', 0, "exit before dumping any values" },
    { "delimiter", 1, 'd', "CHAR", "character separating each column" },
    { "time-format", 1, 'f', "FMT", "time format string" },
    { "fixed", 0, 'F', 0, "print fixed width values" },
    { "scientific", 0, 'G', 0, "print values in scientific format if shorter" },
    { "headers", 0, 'H', 0, "show all headers" },
    { "interactive", 0, 'i', 0, "format columns for interactive use" },
    { "source", 0, 'l', 0, "show source of metrics" },
    { "metrics", 0, 'm', 0, "show metric names" },
    { "", 0, 'M', 0, "show complete metrics names" },
    { "", 0, 'N', 0, "show normalizing factor" },
    { "offset", 0, 'o', 0, "prefix timestamp with offset in seconds" },
    { "precision", 1, 'P', "N", "floating point precision [default 3]" },
    { "repeat", 1, 'R', "N", "repeat the header after every N samples" },
    { "raw", 0, 'r', 0, "output raw values, no rate conversion" },
    { "unavailable", 1, 'U', "STR", "unavailable value string [default \"?\"]" },
    { "units", 0, 'u', 0, "show metric units" },
    { "width", 1, 'w', "N", "set column width" },
    { "extended", 0, 'X', 0, "show complete metrics names (extended form)" },
    PMAPI_OPTIONS_END
};

// Collection start time
static struct timeval logStartTime;

// This may be putenv, so make it static
static QString tzEnv = "TZ=";

static QTextStream cerr(stderr);
static QTextStream cout(stdout);

static void
checkUnits(QmcMetric *metric)
{
    pmUnits units;
    const pmDesc &desc = metric->desc().desc();

    // Only scale units if interactive and not raw
    if (rawFlag || !niceFlag)
	return;

    // Change to canonical bytes
    if (desc.units.dimTime == 0 &&
	     desc.units.dimSpace == 1 &&
	     desc.units.dimCount == 0 &&
	     desc.units.scaleSpace != PM_SPACE_BYTE) {
	units.dimSpace = 1;
	units.scaleSpace = PM_SPACE_BYTE;
	units.dimTime = units.dimCount = units.scaleTime = 
	    units.scaleCount = 0;
	metric->setScaleUnits(units);

	if (pmDebugOptions.appl0) {
	    cerr << "checkUnits: Changing " << metric->name()
		<< " to use bytes" << Qt::endl;
	}
    }
    // Change to canonical count
    else if (desc.units.dimTime == 0 &&
	     desc.units.dimSpace == 0 &&
	     desc.units.dimCount == 1 &&
	     desc.units.scaleCount != PM_COUNT_ONE) {
	units.dimCount = 1;
	units.scaleCount = PM_COUNT_ONE;
	units.dimTime = units.dimSpace = units.scaleTime = 
	    units.scaleSpace = 0;
	metric->setScaleUnits(units);

	if (pmDebugOptions.appl0) {
	    cerr << "checkUnits: Changing " << metric->name()
		<< " to use counts" << Qt::endl;
	}
    }
    else if (metric->desc().desc().sem == PM_SEM_COUNTER) {

	// Do time utilisation?
	if (desc.units.dimTime == 1 &&
	    desc.units.dimSpace == 0 &&
	    desc.units.dimCount == 0) {
	    units.dimTime = 1;
	    units.scaleTime = PM_TIME_SEC;
	    units.dimSpace = units.dimCount = units.scaleSpace = 
		units.scaleCount = 0;
	    metric->setScaleUnits(units);

	    if (pmDebugOptions.appl0) {
		cerr << "checkUnits: Changing " << metric->name()
		     << " to use time utilization" << Qt::endl;
	    }
	}
    }
}	    

static void
dometric(const char *name)
{
    QString	fullname = doMetricSource;

    if (fullname.length()) {
	if (doMetricType == PM_CONTEXT_ARCHIVE)
	    fullname.append(QChar('/'));
	else
	    fullname.append(QChar(':'));
    }
    fullname.append(name);

    QmcMetric* metric = group->addMetric((const char *)fullname.toLatin1(),
						doMetricScale);
    if (metric->status() >= 0) {
	checkUnits(metric);
	metrics.append(metric);
	numValues += metric->numValues();
    }
    else
	doMetricFlag = false;
}

static int
traverse(const char *str, double scale)
{
    pmMetricSpec	*theMetric;
    char		*msg;
    int			sts = 0;

    sts = pmParseMetricSpec((char *)str, 0, (char *)0, &theMetric, &msg);
    if (sts < 0) {
	pmprintf("%s: Error: Unable to parse metric spec:\n%s\n", 
		 pmGetProgname(), msg);
	free(msg);
	return sts;
    }

    // If the metric has instances, then it cannot be traversed
    if (theMetric->ninst) {
	QmcMetric *metric = group->addMetric(theMetric, scale);
	if (metric->status() >= 0) {
	    checkUnits(metric);
	    metrics.append(metric);
	    numValues += metric->numValues();
	}
	else
	    sts = -1;
    }
    else {
	if (theMetric->isarch == 0)
	    doMetricType = PM_CONTEXT_HOST;
	else if (theMetric->isarch == 1)
	    doMetricType = PM_CONTEXT_ARCHIVE;
	else if (theMetric->isarch == 2)
	    doMetricType = PM_CONTEXT_LOCAL;
	else {
	    pmprintf("%s: Error: invalid metric source (%d): %s\n",
			 pmGetProgname(), theMetric->isarch, theMetric->metric);
	    sts = -1;
	}
	doMetricSource = theMetric->source;
	if (sts >= 0)
	   sts = group->use(doMetricType, doMetricSource);
	if (sts >= 0) {
	    doMetricScale = scale;
	    sts = pmTraversePMNS(theMetric->metric, dometric);
	    if (sts >= 0 && doMetricFlag == false)
		sts = -1;
	    else if (sts < 0) {
		pmprintf("%s: Error: %s: %s\n",
			 pmGetProgname(), theMetric->metric, pmErrStr(sts));
	    }
	}
    }

    free(theMetric);

    return sts;
}

//
// parseConfig: parse list of metrics with optional scaling factor
//
static int
parseConfig(QString const& configName, FILE *configFile)
{
    char	buf[1024];
    char	*last;
    char	*msg;
    double	scale = 0.0;
    int		line = 0;
    int		len = 0;
    int		err = 0;

    while (!feof(configFile)) {
	if (fgets(buf, sizeof(buf), configFile) == NULL)
	    break;
	len = strlen(buf);
	if (len == 0 || buf[0] == '#' || buf[0] == '\n') {
	    line++;
	    continue;
	}
	last = &buf[len-1];
	if (*last != '\n' && !feof(configFile)) {
	    pmprintf("%s: Line %d of %s was too long, skipping.\n",
	    	     pmGetProgname(), line, (const char *)configName.toLatin1());
	    while(buf[len-1] != '\n') {
	    	if (fgets(buf, sizeof(buf), configFile) == NULL)
		    break;
		len = strlen(buf);
	    }
	    err++;
	    continue;
	}
	if (*last == '\n')
	    *last = '\0';
	line++;

	last = strrchr(buf, ']');
	if (last == NULL) {	// No instances
	    for (last = buf; *last != '\0' && isspace(*last); last++) { ; }
	    if (*last == '\0')
		continue;
	    for (; *last != '\0' && !isspace(*last); last++) { ; }
	    last--;
	}
	if (*(last + 1) == '\0') {
	    scale = 0.0;
	}
	else {
	    *(last+1)='\0';
	    scale = strtod(last+2, &msg);

	    if (*msg != '\0') {
	    	pmprintf("%s: Line %d of %s has an illegal scaling factor, assuming 0.\n",
			 pmGetProgname(), line, (const char *)configName.toLatin1());
		err++;
		scale = 0.0;
	    }
	}

	if (pmDebugOptions.appl0)
	    cerr << "parseConfig: Adding metric '" << buf << "' with scale = "
		 << scale << " from line " << line << Qt::endl;

	if (traverse(buf, scale) < 0)
	    err++;
    }

    if (configFile != stdin)
	fclose(configFile);

    return err;
}

static const char *
dumpTime(struct timeval const &curPos)
{
    time_t	curTime = (time_t)(curPos.tv_sec);
    char	*p;

    if (timeOffsetFlag) {
	double	o = pmtimevalSub(&curPos, &logStartTime);
	if (o < 10)
	    pmsprintf(buffer, sizeof(buffer), "%.2f ", o);
	else if (o < 100)
	    pmsprintf(buffer, sizeof(buffer), "%.1f ", o);
	else
	    pmsprintf(buffer, sizeof(buffer), "%.0f ", o);
	for (p = buffer; *p != ' '; p++)
	    ;
	*p++ = delimiter;
    }
    else
	p = buffer;

    if (timeFormat.length() > 0)
	strftime(p, sizeof(buffer) - (p-buffer),
		 (const char *)(timeFormat.toLatin1()), localtime(&curTime));
    else {
	// Use ctime as we have put the timezone into the environment
	strncpy(p, ctime(&curTime), 20);
	p[19] = '\0';
    }
    return buffer;
}

static void
dumpHeader()
{
    static bool	fullOnce = false;

    QmcMetric 	*metric;
    bool	instFlag = false;
    QString	noneStr = "none";
    QString	srcStr = "Source";
    QString	metricStr = "Metric";
    QString	instStr = "Inst";
    QString	normStr = "Normal";
    QString	unitStr = "Units";
    QString	columnStr = "Column";
    const char	*timeStr;
    int 	m;
    int		i;
    int		c;
    int		v;
    int		p;
    int		len = 0;

    if (niceFlag) {
    	struct timeval pos = { 0, 0 };
    	timeStr = dumpTime(pos);
	len = strlen(timeStr);
    }

    if (fullFlag) {
	fullOnce = true;

	for (m = 0, v = 1; m < metrics.size(); m++) {
	    metric = metrics[m];
	    for (i = 0; i < metric->numValues(); i++, v++) {
		cout << '[' << qSetFieldWidth(2) << v
		     << qSetFieldWidth(0) << "] "
		     << metric->spec(sourceFlag, true, i) << Qt::endl;
	    }
	}
	cout << Qt::endl;
    }

    if (fullOnce) {
	if (timeFlag) {
	    if (len < columnStr.length()) {
		columnStr.remove(len, columnStr.length() - len);
	    }
	    cout << qSetFieldWidth(len) << columnStr
		 << qSetFieldWidth(0) << delimiter;
	}

	for (m = 0, v = 1; m < metrics.size(); m++) {
	    metric = metrics[m];
	    for (i = 0; i < metric->numValues(); i++) {
		cout << qSetFieldWidth(width) << v << qSetFieldWidth(0);
		if (v < numValues) {
		    cout << delimiter;
		    v++;
		}
	    }
	}
	cout << Qt::endl;
    }

    if (niceFlag && sourceFlag) {
	if (timeFlag) {
	    if (len < srcStr.length()) {
		srcStr.remove(len, srcStr.length() - len);
	    }
	    cout << qSetFieldWidth(len) << srcStr
	         << qSetFieldWidth(0) << delimiter;
	}

	for (m = 0, v = 1; m < metrics.size(); m++) {
	    metric = metrics[m];
	    QString const& str = metric->context()->source().host();
	    strncpy(buffer, (const char *)str.toLatin1(), width);
	    buffer[width] = '\0';
	    for (i = 0; i < metric->numValues(); i++) {
		cout << qSetFieldWidth(width) << buffer << qSetFieldWidth(0);
		if (v < numValues) {
		    cout << delimiter;
		    v++;
		}
	    }
	}
	cout << Qt::endl;
    }

    if (metricFlag || (sourceFlag && !niceFlag)) {
	if (timeFlag) {
	    if (niceFlag) {
		if (len < metricStr.length()) {
		    metricStr.remove(len, metricStr.length() - len);
		}
		cout << qSetFieldWidth(len) << metricStr << qSetFieldWidth(0);
		cout << delimiter;
	    }
	    else
		cout << "Time" << delimiter;
	}

	for (m = 0, v = 1; m < metrics.size(); m++) {
	    metric = metrics[m];
	    if (niceFlag && !instFlag && metric->hasInstances())
	    	instFlag = true;
	    for (i = 0; i < metric->numValues(); i++) {
	    	if (niceFlag) {
		    QString const &str = metric->spec(false, false, i);
		    p = str.length() - width;
		    if (p > 0) {
			for (c = (p - 1 > 0 ? p - 1 : 0); c < str.length(); 
			     c++) {
			    if (str[c] == '.') {
				c++;
				break;
			    }
			}
			if (c < str.length())
			    cout << qSetFieldWidth(width)
				 << ((const char *)str.toLatin1() + c)
				 << qSetFieldWidth(0);
			else
			    cout << qSetFieldWidth(width)
				 << ((const char *)str.toLatin1() + p)
				 << qSetFieldWidth(0);
		    }
		    else {
			cout << qSetFieldWidth(width) << str
			     << qSetFieldWidth(0);
		    }
		}
		else
		    cout << metric->spec(sourceFlag, true, i);
	    	if (v < numValues) {
		    cout << delimiter;
		    v++;
		}
	    }
	}
	cout << Qt::endl;
    }

    if (instFlag) {
	if (timeFlag) {
	    if (niceFlag) {
		if (len < instStr.length()) {
		    instStr.remove(len, instStr.length() - len);
		}
		cout << qSetFieldWidth(len) << instStr
		     << qSetFieldWidth(0) << delimiter;
	    }
	    else {
		cout << qSetFieldWidth(len) << errStr
		     << qSetFieldWidth(0) << delimiter;
	    }
	}

	for (m = 0, v = 1; m < metrics.size(); m++) {
	    metric = metrics[m];
	    for (i = 0; i < metric->numValues(); i++) {
	    	if (metric->hasInstances()) {
		    QString const &str = metric->instName(i);
		    strncpy(buffer, (const char *)str.toLatin1(), width);
		    buffer[width] = '\0';
		    cout << qSetFieldWidth(width) << buffer
			 << qSetFieldWidth(0);
		}
		else
		    cout << qSetFieldWidth(width) << "n/a"
			 << qSetFieldWidth(0);

	    	if (v < numValues) {
		    cout << delimiter;
		    v++;
		}
	    }
	}
	cout << Qt::endl;
    }

    if (normFlag) {
	if (timeFlag) {
	    if (niceFlag) {
		if (len < normStr.length()) {
		    normStr.remove(len, normStr.length() - len);
		}
		cout << qSetFieldWidth(len) << normStr
		     << qSetFieldWidth(0) << delimiter;
	    }
	    else
		cout << errStr << delimiter;
	}

	for (m = 0, v = 1; m < metrics.size(); m++) {
	    metric = metrics[m];
	    for (i = 0; i < metric->numValues(); i++) {	      
		if (shortFlag)
		    cout << qSetRealNumberPrecision(precision)
			 << qSetFieldWidth(width)
			 << metric->scale()
			 << qSetFieldWidth(0);
		else if (descFlag)
		    cout << qSetFieldWidth(width) 
			 << QmcMetric::formatNumber(metric->scale())
			 << qSetFieldWidth(0);
		else
		    cout << Qt::fixed
			 << qSetRealNumberPrecision(precision)
			 << qSetFieldWidth(width)
			 << metric->scale()
			 << qSetFieldWidth(0);
	    	if (v < numValues) {
		    cout << delimiter;
		    v++;
		}
	    }
	}
	cout << Qt::endl;
    }

    if (unitFlag) {
	if (timeFlag) {
	    if (niceFlag) {
		if (len < unitStr.length()) {
		    unitStr.remove(len, unitStr.length() - len);
		}
		cout << qSetFieldWidth(len) << unitStr
		     << qSetFieldWidth(0) << delimiter;
	    }
	    else
		cout << noneStr << delimiter;
	}

	for (m = 0, v = 1; m < metrics.size(); m++) {
	    metric = metrics[m];
	    QString const &str = (niceFlag ? metric->desc().shortUnits()
				              : metric->desc().units());
	    for (i = 0; i < metric->numValues(); i++) {
	    	if (niceFlag) 
		    if (str.length() > width)
			cout << qSetFieldWidth(width)
			     << ((const char *)str.toLatin1() + str.length() - width)
			     << qSetFieldWidth(0);
		    else
			cout << qSetFieldWidth(width) << str
			     << qSetFieldWidth(0);
		else
		    cout << str;
	    	if (v < numValues) {
		    cout << delimiter;
		    v++;
		}
	    }
	}
	cout << Qt::endl;
    }
}

/*
 * Get Extended Time Base interval and Units from a timeval
 */
#define SECS_IN_24_DAYS 2073600.0

static int
getXTBintervalFromTimeval(int *mode, struct timeval *tval)
{
    double tmp_ival = pmtimevalToReal(tval);

    if (tmp_ival > SECS_IN_24_DAYS) {
	*mode = (*mode & 0x0000ffff) | PM_XTB_SET(PM_TIME_SEC);
	return ((int)tmp_ival);
    }
    else {
	*mode = (*mode & 0x0000ffff) | PM_XTB_SET(PM_TIME_MSEC);
	return ((int)(tmp_ival * 1000.0));
    }
}

static struct timeval
tadd(struct timeval t1, struct timeval t2)
{
    pmtimevalInc(&t1, &t2);
    return t1;
}

static struct timeval
tsub(struct timeval t1, struct timeval t2)
{
    pmtimevalDec(&t1, &t2);
    return t1;
}

static struct timespec *
tospec(struct timeval tv, struct timespec *ts)
{
    ts->tv_nsec = tv.tv_usec * 1000;
    ts->tv_sec = tv.tv_sec;
    return ts;
}

static void
sleeptill(struct timeval sched)
{
    int sts;
    struct timeval curr;	/* current time */
    struct timespec delay;	/* interval to sleep */
    struct timespec left;	/* remaining sleep time */

    pmtimevalNow(&curr);
    tospec(tsub(sched, curr), &delay);
    for (;;) {		/* loop to catch early wakeup by nanosleep */
	sts = nanosleep(&delay, &left);
	if (sts == 0 || (sts < 0 && errno != EINTR))
	    break;
	delay = left;
    }
}

static int
override(int opt, pmOptions *opts)
{
    (void)opts;
    if (opt == 'H' || opt == 'a' || opt == 'N')
	return 1;
    return 0;
}

static char *tz_in_env;		/* for valgrind */

int
main(int argc, char *argv[])
{
    char *endnum = NULL;
    int sts = 0;
    int c, l, m, i, v;
    int lines = 0;

    // Metrics
    QmcMetric *metric;
    double value = 0;

    // Config file
    QString configName;
    FILE *configFile = NULL;

    // Timing
    QString tzLabel;
    QString tzString;
    struct timeval logEndTime;
    double endTime;
    double delay;
    double pos;

    // Parse command line options
    //
    pmOptions opts;
    memset(&opts, 0, sizeof(opts));
    opts.flags = PM_OPTFLAG_MULTI;
    opts.short_options = "A:a:D:h:n:O:S:s:T:t:VZ:z?"
			 "c:Cd:f:FGHilmMNoP:rR:uU:Vw:X";
    opts.long_options = longopts;
    opts.short_usage = "[options] [metrics ...]";
    opts.override = override;

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {
	case 'a':       // archive name
	    endnum = strtok(opts.optarg, ", \t");
	    while (endnum) {
		__pmAddOptArchive(&opts, endnum);
		endnum = strtok(NULL, ", \t");
	    }
	    break;

	case 'c':	// config file
	    configName = opts.optarg;
	    break;

	case 'C':	// parse config, output metrics and units only
	    dumpFlag = false;
	    break;

	case 'd':	// delimiter
	    if (strlen(opts.optarg) == 2 && opts.optarg[0] == '\\') {
	    	switch (opts.optarg[1]) {
		    case 'n':
			delimiter = '\n';
			break;
		    case 't':
			delimiter = '\t';
			break;
		    default:
			delimiter = ' ';
		}
	    }
	    else if (strlen(opts.optarg) > 1) {
		pmprintf("%s: delimiter must be one character\n", pmGetProgname());
		opts.errors++;
	    }
	    else
	    	delimiter = opts.optarg[0];
	    break;

	case 'f':	// Time format
	    timeFormat = opts.optarg;
	    if (timeFormat.length() == 0)
	    	timeFlag = false;
	    else
	        timeFlag = true;
	    break;

	case 'F':	// Fixed width values
	    if (shortFlag) {
		pmprintf("%s: -F and -G options may not be used together\n",
			 pmGetProgname());
		opts.errors++;
	    }
	    else
		descFlag = true;
	    break;

	case 'G':	// Shortest format
	    if (descFlag) {
		pmprintf("%s: -F and -G may not be used together\n", 
			 pmGetProgname());
		opts.errors++;
	    }
	    else if (niceFlag) {
		pmprintf("%s: -i and -G may not be used together\n",
			 pmGetProgname());
		opts.errors++;
	    }
	    else
		shortFlag = true;
	    break;

	case 'H':	// show all headers
	    headerFlag = true;
	    break;

	case 'i':	// abbreviate metric names
	    if (precFlag) {
		pmprintf("%s: -i and -P may not be used together\n",
			 pmGetProgname());
		opts.errors++;
	    }
	    else if (shortFlag) {
		pmprintf("%s: -i and -G may not be used together\n",
			 pmGetProgname());
		opts.errors++;
	    }
	    else
		niceFlag = true;
	    break;

	case 'l':	// show source of metrics
	    sourceFlag = true;
	    break;

	case 'm':	// show metric names
	    metricFlag = true;
	    break;

	case 'M':	// show full metric names
	    fullFlag = true;
	    break;

	case 'X':	// show full metric names (extended mode)
	    fullFlag = true;
	    fullXFlag = true;
	    break;

	case 'N':	// show normalization values
	    normFlag = true;
	    break;

	case 'o':	// report timeOffset
	    timeOffsetFlag = true;
	    break;

        case 'P':       // precision
	    if (widthFlag) {
		pmprintf("%s: -P and -w may not be used together\n",
			 pmGetProgname());
		opts.errors++;
	    }
	    else if (niceFlag) {
		pmprintf("%s: -i and -P may not be used together\n",
			 pmGetProgname());
		opts.errors++;
	    }
	    else {
		precision = (int)strtol(opts.optarg, &endnum, 10);
		precFlag = true;
		if (*endnum != '\0' || precision < 0) {
		    pmprintf("%s: -P requires a positive numeric argument\n",
			     pmGetProgname());
		    opts.errors++;
		}
	    }
            break;

	    
	case 'r':	// output raw values
	    rawFlag = true;
	    break;

	case 'R':	// repeat header
	    repeatLines = (int)strtol(opts.optarg, &endnum, 10);
            if (*endnum != '\0' || repeatLines <= 0) {
                pmprintf("%s: -R requires a positive numeric argument\n",
			 pmGetProgname());
                opts.errors++;
            }
            break;

	case 'u':	// show units
	    unitFlag = true;
	    break;

	case 'U':	// error string
	    errStr = opts.optarg;
	    break;

        case 'w':       // width
	    if (precFlag) {
		pmprintf("%s: -P and -w may not be used together\n",
			 pmGetProgname());
		opts.errors++;
	    }
	    else {
		width = (int)strtol(opts.optarg, &endnum, 10);
		widthFlag = true;
		if (*endnum != '\0' || width < 0) {
		    pmprintf("%s: -w requires a positive numeric argument\n",
			     pmGetProgname());
		    opts.errors++;
		}
		else if (width < 3) {
		    pmprintf("%s: -w must be greater than 2\n", pmGetProgname());
		    opts.errors++;
		}
	    }
            break;
	}
    }

    if (opts.context == PM_CONTEXT_HOST) {
	if (opts.nhosts > 1) {
	    pmprintf("%s: only one host may be specified\n", pmGetProgname());
	    opts.errors++;
	}
    }

    if (opts.errors || (opts.flags & PM_OPTFLAG_EXIT)) {
	sts = !(opts.flags & PM_OPTFLAG_EXIT);
	pmUsageMessage(&opts);
	exit(sts);
    }

    // Default update interval is 1 second
    if (opts.interval.tv_sec == 0 && opts.interval.tv_usec == 0)
	opts.interval.tv_sec = 1;

    if (headerFlag)
	metricFlag = unitFlag = sourceFlag = normFlag = true;

    if (fullXFlag)
	niceFlag = true;

    // Create the metric fetch group
    group = new QmcGroup(true);

    // Create archive contexts
    if (opts.narchives > 0) {
	for (c = 0; c < opts.narchives; c++)
	    if (group->use(PM_CONTEXT_ARCHIVE, opts.archives[c]) < 0)
		opts.errors++;
    }
    // Create live context
    else if (opts.nhosts > 0) {
	if (group->use(PM_CONTEXT_HOST, opts.hosts[0]) < 0)
	    opts.errors++;
    }

    if (opts.errors) {
	pmflush();
	exit(1);
    }

    // Set up cout to use the required formatting
    //
    if (niceFlag) {
	width = width < 6 ? 6 : width;
	widthFlag = true;
	descFlag = true;
    }

    if (shortFlag) {
	if (widthFlag) {
	    width = width < 3 ? 3 : width;
	    precision = width - 1;
	}
	else {
	    precision = precision < 2 ? 2 : precision;
	    width = precision + 1;
	}
    }
    else {
	if (widthFlag) {
	    width = width < 3 ? 3 : width;
	    precision = width - 2;
	}
	else {
	    precision = precision < 2 ? 2 : precision;
	    width = precision + 2;
	}
    }

    if (pmDebugOptions.appl0)
	cerr << "main: optind = " << opts.optind << ", argc = " << argc
	     << ", width = " << width << ", precision = " << precision
	     << Qt::endl;

    if (opts.optind == argc) {
	if (configName.length() == 0) {
	    configFile = stdin;
	    configName = "(stdin)";
	}
	else {
	    configFile = fopen((const char *)configName.toLatin1(), "r");
	    if (configFile == NULL) {
		pmprintf("%s: Unable to open %s: %s\n", pmGetProgname(),
			(const char *)configName.toLatin1(), strerror(errno));
	    	pmflush();
		exit(1);
	    }
	}
    }
    else if (configName.length()) {
	pmprintf("%s: configuration file cannot be specified with metrics\n",
		 pmGetProgname());
	exit(1);
    }
    
    if (configFile != NULL) {
	opts.errors = parseConfig(configName, configFile);
    }
    else {
	for (c = opts.optind; c < argc; c++) {
	    if (traverse(argv[c], 0.0) < 0)
		opts.errors++;
	}
    }

    if (metrics.size() == 0 || numValues == 0) {
	pmprintf("%s: no valid metrics, exiting.\n", pmGetProgname());
	pmflush();
	exit(1);
    }
    else if (opts.errors)
        pmprintf("%s: Warning: Some metrics ignored, continuing with valid metrics\n",
		 pmGetProgname());

    pmflush();

    if (pmDebugOptions.appl0)
	cerr << "main: parsed " << metrics.size() << " metrics"
	     << Qt::endl;

    group->useDefault();

    if (group->context()->source().type() != PM_CONTEXT_ARCHIVE)
	isLive = true;

    if (pmDebugOptions.appl0)
	cerr << "main: default source is " << *(group->context()) << Qt::endl;

    if (opts.tzflag)
	group->useTZ();
    else if (opts.timezone) {
	sts = group->useTZ(opts.timezone);
        if ((sts = pmNewZone(opts.timezone)) < 0) {
	    pmprintf("%s: cannot set timezone to \"%s\": %s\n", pmGetProgname(),
			opts.timezone, pmErrStr(sts));
	    pmflush();
	    exit(1);
        }
    }

    group->defaultTZ(tzLabel, tzString);

    if (pmDebugOptions.appl0) {
	cerr << "main: Using timezone \"" << tzString << "\" from " << tzLabel
	     << Qt::endl;
    }

    // putenv timezone into TZ as we may use strftime or ctime later
    //
    if (group->defaultTZ() != QmcGroup::localTZ) {
	tzEnv.append(tzString);
	sts = putenv((tz_in_env = strdup((const char *)tzEnv.toLatin1())));
	if (sts < 0 || tz_in_env == NULL) {
	    pmprintf("%s: Warning: Unable to set timezone in environment\n",
		     pmGetProgname());
	    sts = 0;
	}
	else if (pmDebugOptions.appl0)
	    cerr << "main: Changed environment with \""
		 << tzEnv << '"' << Qt::endl;
    }

    if (isLive) {
	pmtimevalNow(&logStartTime);
	logEndTime.tv_sec = PM_MAX_TIME_T;
	logEndTime.tv_usec = 0;
    }
    else {
	group->updateBounds();

	logStartTime = group->logStart();
	logEndTime = group->logEnd();
	if (pmtimevalToReal(&logEndTime) <= pmtimevalToReal(&logStartTime)) {
	    logEndTime.tv_sec = PM_MAX_TIME_T;
	    logEndTime.tv_usec = 0;
	}
    }

    if (pmDebugOptions.appl0) {
        cerr << "main: start = "
             << pmtimevalToReal(&logStartTime) << ", end = "
             << pmtimevalToReal(&logEndTime)
             << Qt::endl;
    }

    sts = pmParseTimeWindow(opts.start_optarg, opts.finish_optarg,
			    opts.align_optarg, opts.origin_optarg,
			    &logStartTime, &logEndTime, &opts.start,
			    &opts.finish, &opts.origin, &endnum);
    if (sts < 0) {
	pmprintf("%s\n", endnum);
	pmUsageMessage(&opts);
	exit(1);
    }

    pos = pmtimevalToReal(&opts.origin);
    endTime = pmtimevalToReal(&opts.finish);
    delay = (int)(pmtimevalToReal(&opts.interval) * 1000.0);

    if (endTime < pos && opts.finish_optarg == NULL)
	endTime = DBL_MAX;

    if (pmDebugOptions.appl0) {
	cerr << "main: realStartTime = " << pmtimevalToReal(&opts.start)
	     << ", endTime = " << endTime << ", pos = " << pos 
	     << ", delay = " << delay << Qt::endl;
    }

    pmflush();
    dumpHeader();

    // Only dump full names once
    if (fullXFlag == false)
	fullFlag = false;

    if (!dumpFlag)
	goto done;

    if (!isLive) {
	int tmp_mode = PM_MODE_INTERP;
	int tmp_delay = getXTBintervalFromTimeval(&tmp_mode, &opts.interval);
	group->setArchiveMode(tmp_mode, &opts.origin, tmp_delay);
    }

    if (shortFlag) {
	cout.setRealNumberPrecision(precision);
    }
    else if (!descFlag) {
	cout.setRealNumberPrecision(precision);
	cout.setRealNumberNotation(QTextStream::FixedNotation);
    }

    while (pos <= endTime && 
	   ((opts.samples > 0 && sampleCount < opts.samples) ||
	     opts.samples == 0)) {

	group->fetch();
	sampleCount++;

	if (timeFlag)
	    cout << dumpTime(opts.origin) << delimiter;

	for (m = 0, v = 1; m < metrics.size(); m++) {
	    metric = metrics[m];

	    for (i = 0; i < metric->numValues(); i++) {
		if (rawFlag) {
		    if (metric->currentError(i) < 0) {
			if (niceFlag)
			    cout << qSetFieldWidth(width) << errStr
				 << qSetFieldWidth(0);
			else
			    cout << errStr;
			goto next;
		    }
		    else if (metric->real())
			value = metric->currentValue(i);
		}
		else if (metric->error(i) < 0) {
		    if (niceFlag)
			cout << qSetFieldWidth(width) << errStr
			     << qSetFieldWidth(0);
		    else
			cout << errStr;
		    goto next;
		}
		else if (metric->real())
		    value = metric->value(i);

		if (metric->real()) {
		    if (descFlag)
			if (niceFlag)
			    cout << qSetFieldWidth(width) 
				 << QmcMetric::formatNumber(value)
				 << qSetFieldWidth(0);
			else
			    cout << QmcMetric::formatNumber(value);
		    else if (niceFlag)
			cout << qSetFieldWidth(width) << value
			     << qSetFieldWidth(0);
		    else
			cout << value;
		}
		// String
		else {
		    l = metric->stringValue(i).length();
		    buffer[0] = '\"';
		    if (niceFlag) {
			if (l > width - 2) {
			    strncpy(buffer+1, (const char *)metric->stringValue(i).toLatin1(), 
				    width - 2);
			    buffer[width - 1] = '\"';
			    buffer[width] = '\0';
			    cout << qSetFieldWidth(width) << buffer
				 << qSetFieldWidth(0);
			}
			else {
			    strcpy(buffer+1, (const char *)metric->stringValue(i).toLatin1());
			    buffer[l + 1] = '\"';
			    buffer[l + 2] = '\0';
			    cout << qSetFieldWidth(width) << buffer;
			}
		    }
		    else if (widthFlag) {
			if (l > width - 2 && width > 5) {
			    strncpy(buffer+1, (const char *)metric->stringValue(i).toLatin1(),
				    width - 5);
			    strcpy(buffer + width - 4, "...\"");
			    buffer[width] = '\0';
			    cout << qSetFieldWidth(width) << buffer
				 << qSetFieldWidth(0);
			}
			else {
			    strncpy(buffer+1, (const char *)metric->stringValue(i).toLatin1(),
				    width - 2);
			    buffer[width - 1] = '\"';
			    buffer[width] = '\0';
			    cout << qSetFieldWidth(width) << buffer
				 << qSetFieldWidth(0);
			}
		    }
		    else
			cout << '\"' << metric->stringValue(i) << '\"';
		}
	
	next:
		if (v < numValues) {
		    cout << delimiter;
		    v++;
		}
	    }
	}
	cout << Qt::endl;

//	if (opts.samples > 0 && sampleCount == opts.samples)
//	    continue;	/* do not sleep needlessly */

	opts.origin = tadd(opts.origin, opts.interval);

	if (isLive)
	    sleeptill(opts.origin);

	pos = pmtimevalToReal(&opts.origin);
	lines++;
	if (repeatLines > 0 && repeatLines == lines) {
	    cout << Qt::endl;
	    dumpHeader();
	    lines = 0;
	}
    }

done:

    /*
     * We muck with opts->archives[] indirectly, so clean up to make
     * valgind happy ... to see why the direct free()s, are needed
     * you'll need to inspect the source of __pmAddOptArchive() to
     * discover that opts.archives[] contains strings that may have been
     * malloc'd (as in this case) or may be something quite different,
     * which is why pmFreeOptions() must take the lame path.
     */
    for (i = 0; i < opts.narchives; i++)
	free(opts.archives[i]);
    pmFreeOptions(&opts);

    return 0;
}
