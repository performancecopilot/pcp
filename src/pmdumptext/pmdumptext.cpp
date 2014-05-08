/*
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
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <QTextStream>
#include <QStringList>
#include <qmc_group.h>
#include <qmc_metric.h>
#include <qmc_context.h>

// Temporary buffer
char buffer[256];

// List of metrics
QmcGroup *group;
QList<QmcMetric*> metrics;
bool isLive = false;
int numValues;
int doMetricType = PM_CONTEXT_HOST;
bool doMetricFlag = true;
double doMetricScale = 0.0;
QString doMetricSource;

// Command line flags
bool dumpFlag = true;
bool metricFlag = false;
bool niceFlag = false;
bool unitFlag = false;
bool sourceFlag = false;
bool timeFlag = true;
bool timeOffsetFlag = false;
bool rawFlag = false;
bool zflag = false;
bool shortFlag = false;
bool descFlag = false;
bool widthFlag = false;
bool precFlag = false;
bool normFlag = false;
bool headerFlag = false;
bool fullFlag = false;
bool fullXFlag = false;

// Command line options
QString errStr = "?";
QString timeFormat;
QString pmnsFile;
QString timeZone;
char delimiter = '\t';
int precision = 3;
int width = 6;
int numSamples = 0;
int sampleCount = 0;
int repeatLines = 0;

// Collection start time
struct timeval logStartTime;

// This may be putenv, so make it static
static QString tzEnv = "TZ=";

QTextStream cerr(stderr);
QTextStream cout(stdout);

static
void
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

	if (pmDebug & DBG_TRACE_APPL0) {
	    cerr << "checkUnits: Changing " << metric->name()
		<< " to use bytes" << endl;
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

	if (pmDebug & DBG_TRACE_APPL0) {
	    cerr << "checkUnits: Changing " << metric->name()
		<< " to use counts" << endl;
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

	    if (pmDebug & DBG_TRACE_APPL0) {
		cerr << "checkUnits: Changing " << metric->name()
		     << " to use time utilization" << endl;
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

    QmcMetric* metric = group->addMetric((const char *)fullname.toAscii(),
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
		 pmProgname, msg);
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
			 pmProgname, theMetric->isarch, theMetric->metric);
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
			 pmProgname, theMetric->metric, pmErrStr(sts));
	    }
	}
    }

    free(theMetric);

    return sts;
}

//
// usage
//

void usage()
{
    pmprintf("Usage: %s [options] [metrics ...]\n\n", pmProgname);

    pmprintf(
"Options:\n\
  -A align                align sample times on natural boundaries\n\
  -a archive[,archive...] metric sources are PCP log archive(s)\n\
  -C                      exit before dumping any values\n\
  -c config               read list of metrics from config\n\
  -d delimiter            character separating each column\n\
  -f format               time format string\n\
  -F                      print fixed width values\n\
  -G                      print values in scientific format if shorter\n\
  -H                      show all headers\n\
  -h host                 metrics source is PMCD on host\n\
  -i                      format columns for interactive use\n\
  -l                      show source of metrics\n\
  -m                      show metric names\n\
  -M                      show complete metrics names\n\
  -N                      show normalizing factor\n\
  -n pmnsfile             use an alternative PMNS\n\
  -O offset               initial offset into the time window\n\
  -o			  prefix timestamp with offset in seconds\n\
  -P precision            floating point precision [default 3]\n\
  -R lines                repeat header every number of lines\n\
  -r                      output raw values, no rate conversion\n\
  -S starttime            start of the time window\n\
  -s sample               terminate after this many samples\n\
  -T endtime              end of the time window\n\
  -t interval             sample interval [default 1.0 second]\n\
  -U string               unavailable value string [default \"?\"]\n\
  -u                      show metric units\n\
  -w width                set column width\n\
  -X                      show complete metrics names (extended form)\n\
  -Z timezone             set reporting timezone\n\
  -z                      set reporting timezone to local time of metrics source\n");

    pmflush();
    exit(1);
}

//
// parseConfig: parse list of metrics with optional scaling factor
//

int
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
	    	     pmProgname, line, (const char *)configName.toAscii());
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
			 pmProgname, line, (const char *)configName.toAscii());
		err++;
		scale = 0.0;
	    }
	}

	if (pmDebug & DBG_TRACE_APPL0)
	    cerr << "parseConfig: Adding metric '" << buf << "' with scale = "
		 << scale << " from line " << line << endl;

	if (traverse(buf, scale) < 0)
	    err++;
    }

    if (configFile != stdin)
	fclose(configFile);

    return err;
}

//
// dumptime
//

const char *
dumpTime(struct timeval const &curPos)
{
    time_t	curTime = (time_t)(curPos.tv_sec);
    char	*p;

    if (timeOffsetFlag) {
	double	o = __pmtimevalSub(&curPos, &logStartTime);
	if (o < 10)
	    sprintf(buffer, "%.2f ", o);
	else if (o < 100)
	    sprintf(buffer, "%.1f ", o);
	else
	    sprintf(buffer, "%.0f ", o);
	for (p = buffer; *p != ' '; p++)
	    ;
	*p++ = delimiter;
    }
    else
	p = buffer;

    if (timeFormat.length() > 0)
	strftime(p, sizeof(buffer) - (p-buffer),
		 (const char *)(timeFormat.toAscii()), localtime(&curTime));
    else {
	// Use ctime as we have put the timezone into the environment
	strcpy(p, ctime(&curTime));
	p[19] = '\0';
    }
    return buffer;
}

//
// dumpHeader
//

void
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
		     << metric->spec(sourceFlag, true, i) << endl;
	    }
	}
	cout << endl;
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
	cout << endl;
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
	    strncpy(buffer, (const char *)str.toAscii(), width);
	    buffer[width] = '\0';
	    for (i = 0; i < metric->numValues(); i++) {
		cout << qSetFieldWidth(width) << buffer << qSetFieldWidth(0);
		if (v < numValues) {
		    cout << delimiter;
		    v++;
		}
	    }
	}
	cout << endl;
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
				 << ((const char *)str.toAscii() + c)
				 << qSetFieldWidth(0);
			else
			    cout << qSetFieldWidth(width)
				 << ((const char *)str.toAscii() + p)
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
	cout << endl;
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
		    strncpy(buffer, (const char *)str.toAscii(), width);
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
	cout << endl;
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
		    cout << fixed
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
	cout << endl;
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
			     << ((const char *)str.toAscii() + str.length() - width)
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
	cout << endl;
    }
}

/*
 * Get Extended Time Base interval and Units from a timeval
 */
#define SECS_IN_24_DAYS 2073600.0

static int
getXTBintervalFromTimeval(int *mode, struct timeval *tval)
{
    double tmp_ival = tval->tv_sec + tval->tv_usec / 1000000.0;

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
    t1.tv_sec += t2.tv_sec;
    t1.tv_usec += t2.tv_usec;
    if (t1.tv_usec > 1000000) {
	(t1.tv_sec)++;
	t1.tv_usec -= 1000000;
    }
    return t1;
}

static struct timeval
tsub(struct timeval t1, struct timeval t2)
{
    t1.tv_usec -= t2.tv_usec;
    if (t1.tv_usec < 0) {
	t1.tv_usec += 1000000;
	t1.tv_sec--;
    }
    t1.tv_sec -= t2.tv_sec;
    return t1;
}

static struct timespec *
tospec(struct timeval tv, struct timespec *ts)
{
    ts->tv_nsec = tv.tv_usec * 1000;
    ts->tv_sec = tv.tv_sec;
    return ts;
}

void
sleeptill(struct timeval sched)
{
    int sts;
    struct timeval curr;	/* current time */
    struct timespec delay;	/* interval to sleep */
    struct timespec left;	/* remaining sleep time */

    __pmtimevalNow(&curr);
    tospec(tsub(sched, curr), &delay);
    for (;;) {		/* loop to catch early wakeup by nanosleep */
	sts = nanosleep(&delay, &left);
	if (sts == 0 || (sts < 0 && errno != EINTR))
	    break;
	delay = left;
    }
}

int
main(int argc, char *argv[])
{
    char *msg, *endnum = NULL;
    int errflag = 0, sts = 0;
    int c, l, m, i, v;
    int lines = 0;

    // Metrics
    QStringList archives;
    QString host;
    QmcMetric *metric;
    double value = 0;

    // Config file
    QString configName;
    FILE *configFile = NULL;

    // Flags for setting time boundaries.
    int Aflag = 0;		// 0 ctime, 1 +offset, 2 -offset
    char *Atime = NULL;		// tm from -A flag
    int Sflag = 0;		// 0 ctime, 1 +offset, 2 -offset
    char *Stime = NULL;		// tm from -S flag
    int Oflag = 0;		// 0 ctime, 1 +offset, 2 -offset
    char *Otime = NULL;		// tm from -S flag
    int Tflag = 0;		// 0 ctime, 1 +offset, 2 -offset
    char *Ttime = NULL;		// tm from -T flag

    // Timing
    QString tzLabel;
    QString tzString;
    struct timeval logEndTime;
    struct timeval realStartTime;
    struct timeval realEndTime;
    struct timeval position;
    struct timeval interval;
    double endTime;
    double delay;
    double pos;

    // Set the program name for error messages
    __pmSetProgname(argv[0]);

    // Default update interval is 1 second
    interval.tv_sec = 1;
    interval.tv_usec = 0;

    // Create the metric fetch group
    group = new QmcGroup(true);

//
// Parse command line options
//

    while((c = getopt(argc, argv, 
	   "A:a:c:Cd:D:f:FgGh:HilmMn:NO:op:P:rR:s:S:t:T:uU:Vw:XZ:z?")) != EOF) {
	switch (c) {
	case 'A':       // alignment
            if (Aflag) {
		pmprintf("%s: at most one -A allowed\n", pmProgname);
                errflag++;
            }
            Atime = optarg;
            Aflag = 1;
            break;

        case 'a':       // archive name
	    if (host.length() > 0) {
		pmprintf("%s: -a and -h may not be used together\n", 
			 pmProgname);
		errflag++;
	    }
	    else {
		endnum = strtok(optarg, ", \t");
		while (endnum) {
		    archives.append(QString(endnum));
		    endnum = strtok(NULL, ", \t");
		}
	    }
	    break;

	case 'c':	// config file
	    configName = optarg;
	    break;

	case 'C':	// parse config, output metrics and units only
	    dumpFlag = false;
	    break;

	case 'd':	// delimiter
	    if (strlen(optarg) == 2 && optarg[0] == '\\') {
	    	switch (optarg[1]) {
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
	    else if (strlen(optarg) > 1) {
	    	pmprintf("%s: delimiter must be one character\n", pmProgname);
		errflag++;
	    }
	    else
	    	delimiter = optarg[0];
	    break;

	case 'D':	// debug flag
	    sts = __pmParseDebug(optarg);
	    if (sts < 0) {
                pmprintf("%s: unrecognized debug flag specification (%s)\n",
			 pmProgname, optarg);
		errflag++;
	    }
	    else
		pmDebug |= sts;
	    break;

	case 'f':	// Time format
	    timeFormat = optarg;
	    if (timeFormat.length() == 0)
	    	timeFlag = false;
	    else
	        timeFlag = true;
	    break;

	case 'F':	// Fixed width values
	    if (shortFlag) {
		pmprintf("%s: -F and -G options may not be used together\n",
			 pmProgname);
		errflag++;
	    }
	    else
		descFlag = true;
	    break;

	case 'g':	// GUI mode with pmtime
	    break;

	case 'G':	// Shortest format
	    if (descFlag) {
		pmprintf("%s: -F and -G may not be used together\n", 
			 pmProgname);
		errflag++;
	    }
	    else if (niceFlag) {
		pmprintf("%s: -i and -G may not be used togther\n",
			 pmProgname);
		errflag++;
	    }
	    else
		shortFlag = true;
	    break;

        case 'h':       // contact PMCD on this hostname
	    if (archives.size() > 0) {
		pmprintf("%s: -a and -h may not be used together\n", 
			 pmProgname);
		errflag++;
	    }
	    else if (host.length() > 0) {
		pmprintf("%s: only one host (-h) may be specifiedn\n",
			 pmProgname);
		errflag++;
	    }
	    else
		host = optarg;
	    break;

	case 'H':	// show all headers
	    headerFlag = true;
	    break;

	case 'i':	// abbreviate metric names
	    if (precFlag) {
		pmprintf("%s: -i and -P may not be used togther\n",
			 pmProgname);
		errflag++;
	    }
	    else if (shortFlag) {
		pmprintf("%s: -i and -G may not be used togther\n",
			 pmProgname);
		errflag++;
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

        case 'n':       // alternative namespace
	    if (pmnsFile.length() != 0) {
		pmprintf("%s: at most one -n option allowed\n", pmProgname);
		errflag++;
	    }
	    else
		pmnsFile = optarg;
	    break;

	case 'N':	// show normalization values
	    normFlag = true;
	    break;

        case 'O':	// offset sample time
            if (Oflag) {
                pmprintf("%s: at most one -O allowed\n", pmProgname);
                errflag++;
            }
            Otime = optarg;
            Oflag = 1;
            break;

	case 'o':	// report timeOffset
	    timeOffsetFlag = true;
	    break;

	case 'p':	// GUI port number for connecting to pmtime
	    break;

        case 'P':       // precision
	    if (widthFlag) {
		pmprintf("%s: -P and -w may not be used together\n",
			 pmProgname);
		errflag++;
	    }
	    else if (niceFlag) {
		pmprintf("%s: -i and -P may not be used together\n",
			 pmProgname);
		errflag++;
	    }
	    else {
		precision = (int)strtol(optarg, &endnum, 10);
		precFlag = true;
		if (*endnum != '\0' || precision < 0) {
		    pmprintf("%s: -P requires a positive numeric argument\n",
			     pmProgname);
		    errflag++;
		}
	    }
            break;

	case 's':	// number of samples
            numSamples = (int)strtol(optarg, &endnum, 10);
            if (*endnum != '\0' || numSamples <= 0) {
                pmprintf("%s: -s requires a positive numeric argument\n",
			 pmProgname);
                errflag++;
            }
            break;
	    
        case 'S':
            if (Sflag) {
                pmprintf("%s: at most one -S allowed\n", pmProgname);
                errflag++;
            }
            Stime = optarg;
            Sflag = 1;
            break;

	case 'r':	// output raw values
	    rawFlag = true;
	    break;

	case 'R':	// repeat header
	    repeatLines = (int)strtol(optarg, &endnum, 10);
            if (*endnum != '\0' || repeatLines <= 0) {
                pmprintf("%s: -R requires a positive numeric argument\n",
			 pmProgname);
                errflag++;
            }
            break;

        case 't':       // sampling interval
            if (pmParseInterval(optarg, &interval, &msg) < 0) {
		pmprintf("%s\n", msg);
                free(msg);
                errflag++;
            }
            break;


        case 'T':       // run time
            if (Tflag) {
                pmprintf("%s: at most one -T allowed\n", pmProgname);
                errflag++;
            }
            Ttime = optarg;
            Tflag = 1;
            break;

	case 'u':	// show units
	    unitFlag = true;
	    break;

	case 'U':	// error string
	    errStr = optarg;
	    break;

	case 'V':	// version
	    printf("%s %s\n", pmProgname, pmGetConfig("VERSION"));
	    exit(0);

        case 'w':       // width
	    if (precFlag) {
		pmprintf("%s: -P and -w may not be used together\n",
			 pmProgname);
		errflag++;
	    }
	    else {
		width = (int)strtol(optarg, &endnum, 10);
		widthFlag = true;
		if (*endnum != '\0' || width < 0) {
		    pmprintf("%s: -w requires a positive numeric argument\n",
			     pmProgname);
		    errflag++;
		}
		else if (width < 3) {
		    pmprintf("%s: -w must be greater than 2\n", pmProgname);
		    errflag++;
		}
	    }
            break;
        case 'z':       // timezone from host
            if (timeZone.length()) {
                pmprintf("%s: -z and -Z may not be used together\n",
			 pmProgname);
                errflag++;
            }
            zflag = true;
            break;

        case 'Z':       // $TZ timezone
            if (zflag) {
                pmprintf("%s: -z and -Z may not be used together\n",
			 pmProgname);
                errflag++;
            }
            timeZone = optarg;
            break;

        case '?':
        default:
            errflag++;
            break;
        }
    }

    if (errflag > 0) {
	usage();
    }

    if (headerFlag) {
	metricFlag = unitFlag = sourceFlag = normFlag = true;
    }
    if (fullXFlag) {
	niceFlag = true;
    }

    // Get local namespace is requested before opening any contexts
    //
    if (pmnsFile.length()) {
	sts = pmLoadNameSpace((const char *)pmnsFile.toAscii());
	if (sts < 0) {
	    pmprintf("%s: %s\n", pmProgname, pmErrStr(sts));
	    pmflush();
	    exit(1);
	}
    }

    // Create archive contexts
    //
    if (archives.size() > 0) {
	for (c = 0; c < archives.size(); c++)
	    if (group->use(PM_CONTEXT_ARCHIVE, archives[c]) < 0)
		errflag++;
    }
    // Create live context
    //
    else if (host.length() > 0) {
	if (group->use(PM_CONTEXT_HOST, host) < 0)
	    errflag++;
    }

    if (errflag) {
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

    if (pmDebug & DBG_TRACE_APPL0)
	cerr << "main: optind = " << optind << ", argc = " << argc
	     << ", width = " << width << ", precision = " << precision
	     << endl;

    if (optind == argc) {
	if (configName.length() == 0) {
	    configFile = stdin;
	    configName = "(stdin)";
	}
	else {
	    configFile = fopen((const char *)configName.toAscii(), "r");
	    if (configFile == NULL) {
		pmprintf("%s: Unable to open %s: %s\n", pmProgname,
			(const char *)configName.toAscii(), strerror(errno));
	    	pmflush();
		exit(1);
	    }
	}
    }
    else if (configName.length()) {
	pmprintf("%s: configuration file cannot be specified with metrics\n",
		 pmProgname);
	usage();
    }
    
    if (configFile != NULL) {
	errflag = parseConfig(configName, configFile);
    }
    else {
	for (c = optind; c < argc; c++) {
	    if (traverse(argv[c], 0.0) < 0)
		errflag++;
	}
    }

    if (metrics.size() == 0 || numValues == 0) {
	pmprintf("%s: no valid metrics, exiting.\n", pmProgname);
	pmflush();
	exit(1);
    }
    else if (errflag)
        pmprintf("%s: Warning: Some metrics ignored, continuing with valid metrics\n",
		 pmProgname);

    pmflush();

    if (pmDebug & DBG_TRACE_APPL0)
	cerr << "main: parsed " << metrics.size() << " metrics"
	     << endl;

    group->useDefault();

    if (group->context()->source().type() != PM_CONTEXT_ARCHIVE)
	isLive = true;

    if (isLive && (Aflag || Oflag || Sflag)) {
	pmprintf("%s: Error: -A, -O and -S options ignored in live mode\n",
		 pmProgname);
	pmflush();
	exit(1);
    }

    if (pmDebug & DBG_TRACE_APPL0)
	cerr << "main: default source is " << *(group->context()) << endl;

    if (zflag)
	group->useTZ();
    else if (timeZone.length()) {
	sts = group->useTZ(timeZone);
        if ((sts = pmNewZone((const char *)timeZone.toAscii())) < 0) {
	    pmprintf("%s: cannot set timezone to \"%s\": %s\n", pmProgname,
			(const char *)timeZone.toAscii(), pmErrStr(sts));
	    pmflush();
	    exit(1);
        }
    }

    group->defaultTZ(tzLabel, tzString);

    if (pmDebug & DBG_TRACE_APPL0) {
	cerr << "main: Using timezone \"" << tzString << "\" from " << tzLabel
	     << endl;
    }

    // putenv timezone into TZ as we may use strftime or ctime later
    //
    if (group->defaultTZ() != QmcGroup::localTZ) {
	tzEnv.append(tzString);
	sts = putenv(strdup((const char *)tzEnv.toAscii()));
	if (sts < 0) {
	    pmprintf("%s: Warning: Unable to set timezone in environment\n",
		     pmProgname);
	    sts = 0;
	}
	else if (pmDebug & DBG_TRACE_APPL0)
	    cerr << "main: Changed environment with \""
		 << tzEnv << '"' << endl;
    }

    if (isLive) {
	__pmtimevalNow(&logStartTime);
	logEndTime.tv_sec = INT_MAX;
	logEndTime.tv_usec = INT_MAX;
    }
    else {
	group->updateBounds();

	logStartTime = group->logStart();
	logEndTime = group->logEnd();
	if (__pmtimevalToReal(&logEndTime) <= __pmtimevalToReal(&logStartTime)) {
	    logEndTime.tv_sec = INT_MAX;
	    logEndTime.tv_usec = INT_MAX;	
	}
    }

    if (pmDebug & DBG_TRACE_APPL0) {
        cerr << "main: start = "
             << __pmtimevalToReal(&logStartTime) << ", end = "
             << __pmtimevalToReal(&logEndTime)
             << endl;
    }

    sts = pmParseTimeWindow(Stime, Ttime, Atime, Otime,
			    &logStartTime, &logEndTime, &realStartTime,
			    &realEndTime, &position,
			    &msg);

    if (sts < 0) {
	pmprintf("%s\n", msg);
	usage();
    }

    pos = __pmtimevalToReal(&position);
    endTime = __pmtimevalToReal(&realEndTime);
    delay = (int)(__pmtimevalToReal(&interval) * 1000.0);

    if (endTime < pos && Tflag == false)
    	endTime = DBL_MAX;

    if (pmDebug & DBG_TRACE_APPL0) {
	cerr << "main: realStartTime = " << __pmtimevalToReal(&realStartTime)
	     << ", endTime = " << endTime << ", pos = " << pos 
	     << ", delay = " << delay << endl;
    }

    pmflush();
    dumpHeader();

    if (fullXFlag == false) {
	// Only dump full names once
	fullFlag = false;
    }

    if (!dumpFlag)
	exit(0);

    if (!isLive) {
	int tmp_mode = PM_MODE_INTERP;
	int tmp_delay = getXTBintervalFromTimeval(&tmp_mode, &interval);
	group->setArchiveMode(tmp_mode, &position, tmp_delay);
    }

    if (shortFlag) {
	cout.setRealNumberPrecision(precision);
    }
    else if (!descFlag) {
	cout.setRealNumberPrecision(precision);
	cout.setRealNumberNotation(QTextStream::FixedNotation);
    }

    while(pos <= endTime && 
	  ((numSamples > 0 && sampleCount < numSamples) || numSamples == 0)) {

	group->fetch();
	sampleCount++;

	if (timeFlag)
	    cout << dumpTime(position) << delimiter;

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
			    strncpy(buffer+1, (const char *)metric->stringValue(i).toAscii(), 
				    width - 2);
			    buffer[width - 1] = '\"';
			    buffer[width] = '\0';
			    cout << qSetFieldWidth(width) << buffer
				 << qSetFieldWidth(0);
			}
			else {
			    strcpy(buffer+1, (const char *)metric->stringValue(i).toAscii());
			    buffer[l + 1] = '\"';
			    buffer[l + 2] = '\0';
			    cout << qSetFieldWidth(width) << buffer;
			}
		    }
		    else if (widthFlag) {
			if (l > width - 2 && width > 5) {
			    strncpy(buffer+1, (const char *)metric->stringValue(i).toAscii(),
				    width - 5);
			    strcpy(buffer + width - 4, "...\"");
			    buffer[width] = '\0';
			    cout << qSetFieldWidth(width) << buffer
				 << qSetFieldWidth(0);
			}
			else {
			    strncpy(buffer+1, (const char *)metric->stringValue(i).toAscii(),
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
	cout << endl;

	position = tadd(position, interval);

	if (isLive)
	    sleeptill(position);

	pos = __pmtimevalToReal(&position);
	lines++;
	if (repeatLines > 0 && repeatLines == lines) {
	    cout << endl;
	    dumpHeader();
	    lines = 0;
	}
    }

    return 0;
}
