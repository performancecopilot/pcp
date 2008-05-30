/*
 * Copyright (c) 1997,2004-2006 Silicon Graphics, Inc.  All Rights Reserved.
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
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
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
#include <pcp/pmc/Group.h>
#include <pcp/pmc/Metric.h>
#include <pcp/pmc/Context.h>
#ifdef HAVE_IOSTREAM
#include <iostream>
using namespace std;
#include <iomanip>
#else
#include <iostream.h>
// Warning: piggy-backing iomanip conditional on iostream
#include <iomanip.h>
#endif

// Temporary buffer
char		buffer[256];

// List of metrics
PMC_Group*	group;
PMC_MetricList	metrics;
int		numValues;
PMC_Bool	doMetricFlag = PMC_true;
double		doMetricScale = 0.0;
PMC_String	doMetricSource;
int		doMetricType = PM_CONTEXT_HOST;
PMC_Bool	isLive = PMC_false;

// Command line flags
PMC_Bool	dumpFlag = PMC_true;
PMC_Bool	metricFlag = PMC_false;
PMC_Bool	niceFlag = PMC_false;
PMC_Bool	unitFlag = PMC_false;
PMC_Bool	sourceFlag = PMC_false;
PMC_Bool	timeFlag = PMC_true;
PMC_Bool	timeOffsetFlag = PMC_false;
PMC_Bool	rawFlag = PMC_false;
PMC_Bool	zflag = PMC_false;
PMC_Bool	shortFlag = PMC_false;
PMC_Bool	descFlag = PMC_false;
PMC_Bool	widthFlag = PMC_false;
PMC_Bool	precFlag = PMC_false;
PMC_Bool	normFlag = PMC_false;
PMC_Bool	headerFlag = PMC_false;
PMC_Bool	fullFlag = PMC_false;
PMC_Bool	fullXFlag = PMC_false;

// Command line options
PMC_String	errStr = "?";
PMC_String	timeFormat;
PMC_String	pmnsFile;
PMC_String	timeZone;
char		delimiter = '\t';
int		precision = 3;
int		width = 6;
int		numSamples = 0;
int		sampleCount = 0;
int		repeatLines = 0;

// Collection start time
struct timeval	logStartTime;

// This may be putenv, so make it static
static PMC_String tzEnv = "TZ=";

static
void
checkUnits(PMC_Metric *metric)
{
    pmUnits	units;
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

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0) {
	    cerr << "checkUnits: Changing " << metric->name()
		<< " to use bytes" << endl;
	}
#endif
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

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0) {
	    cerr << "checkUnits: Changing " << metric->name()
		<< " to use counts" << endl;
	}
#endif
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

#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0) {
		cerr << "checkUnits: Changing " << metric->name()
		     << " to use time utilization" << endl;
	    }
#endif
	}
    }
}	    

static void
dometric(const char *name)
{
    PMC_String	fullname = doMetricSource;

    if (fullname.length())
	if (doMetricType == PM_CONTEXT_ARCHIVE)
	    fullname.appendChar('/');
	else
	    fullname.appendChar(':');
    fullname.append(name);

    PMC_Metric* metric = group->addMetric(fullname.ptr(), doMetricScale);
    if (metric->status() >= 0) {
	checkUnits(metric);
	metrics.append(metric);
	numValues += metric->numValues();
    }
    else
	doMetricFlag = PMC_false;
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
	PMC_Metric *metric = group->addMetric(theMetric, scale);
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

	doMetricSource = theMetric->source;

	sts = group->use(doMetricType, theMetric->source);
	
	if (sts >= 0) {
	    doMetricScale = scale;
	    sts = pmTraversePMNS(theMetric->metric, dometric);
	    if (sts >= 0 && doMetricFlag == PMC_false)
		sts = -1;
	    else if (sts < 0)
		pmprintf("%s: Error: %s%c%s: %s\n",
			 pmProgname, 
			 group->which()->source().type() == PM_CONTEXT_LOCAL ? "@" : group->which()->source().source().ptr(),
			 (group->which()->source().type() == PM_CONTEXT_ARCHIVE ?
			  '/' : ':'),
			 theMetric->metric,
			 pmErrStr(sts));
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
  -g                      print values in scientific format if shorter\n\
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
parseConfig(PMC_String const& configName, FILE *configFile)
{
    char	buf[1024];
    char	*last = NULL;
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
	if (buf[len-1] != '\n') {
	    pmprintf("%s: Line %d of %s was too long, skipping.\n",
	    	     pmProgname, line, configName.ptr());
	    while(buf[len-1] != '\n') {
	    	if (fgets(buf, sizeof(buf), configFile) == NULL)
		    break;
		len = strlen(buf);
	    }
	    err++;
	    continue;
	}
	line++;
	buf[len-1] = '\0';

	last = strrchr(buf, ']');
	if (last == NULL) {	// No instances
	    for (last = buf; *last != '\0' && isspace(*last); last++);
	    if (*last == '\0')
		continue;
	    for (; *last != '\0' && !isspace(*last); last++);
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
			 pmProgname, line, configName.ptr());
		err++;
		scale = 0.0;
	    }
	}

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0)
	    cerr << "parseConfig: Adding metric '" << buf << "' with scale = "
		 << scale << " from line " << line << endl;
#endif

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

    if ((int)timeFormat.length() > 0)
	strftime(p, sizeof(buffer) - (p-buffer), (char *)(timeFormat.ptr()), localtime(&curTime));
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
    static PMC_Bool	fullOnce = PMC_false;

    PMC_Metric 	*metric;
    PMC_Bool	instFlag = PMC_false;
    PMC_String	noneStr = "none";
    PMC_String	srcStr = "Source";
    PMC_String	metricStr = "Metric";
    PMC_String	instStr = "Inst";
    PMC_String	normStr = "Normal";
    PMC_String	unitStr = "Units";
    PMC_String	columnStr = "Column";
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
	fullOnce = PMC_true;

	for (m = 0, v = 1; m < (int)metrics.length(); m++) {
	    metric = metrics[m];
	    for (i = 0; i < (int)metric->numValues(); i++, v++) {
		cout << '[' << setw(2) << v << "] "
		     << metric->spec(sourceFlag, PMC_true, i) << endl;
	    }
	}
	cout << endl;
    }

    if (fullOnce) {
	if (timeFlag) {
	    if (len < (int)columnStr.length()) {
		columnStr.remove(len, columnStr.length() - len);
	    }
	    cout << setw(len) << columnStr << delimiter;
	}

	for (m = 0, v = 1; m < (int)metrics.length(); m++) {
	    metric = metrics[m];
	    for (i = 0; i < (int)metric->numValues(); i++) {
		cout << setw(width) << v;
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
	    if (len < (int)srcStr.length()) {
		srcStr.remove(len, srcStr.length() - len);
	    }
	    cout << setw(len) << srcStr << delimiter;
	}
	
	for (m = 0, v = 1; m < (int)metrics.length(); m++) {
	    metric = metrics[m];
	    PMC_String const& str = metric->context().source().host();
	    strncpy(buffer, str.ptr(), width);
	    buffer[width] = '\0';
	    for (i = 0; i < (int)metric->numValues(); i++) {
		cout << setw(width) << buffer;
		if (v < numValues) {
		    cout << delimiter;
		    v++;
		}
	    }
	}
	cout << endl;
    }

    if (metricFlag || (sourceFlag && !niceFlag)) {
	if (timeFlag)
	    if (niceFlag) {
		if (len < (int)metricStr.length()) {
		    metricStr.remove(len, metricStr.length() - len);
		}
		cout << setw(len) << metricStr << delimiter;
	    }
	    else
		cout << "Time" << delimiter;

	for (m = 0, v = 1; m < (int)metrics.length(); m++) {
	    metric = metrics[m];
	    if (niceFlag && !instFlag && metric->hasInstances())
	    	instFlag = PMC_true;
	    for (i = 0; i < (int)metric->numValues(); i++) {
	    	if (niceFlag) {
		    PMC_String const &str = metric->spec(PMC_false, 
		    					 PMC_false, i);
		    p = str.length() - width;
		    if (p > 0) {
			for (c = (p - 1 > 0 ? p - 1 : 0); c < (int)str.length(); 
			     c++) {
			    if (str[c] == '.') {
				c++;
				break;
			    }
			}
			if (c < (int)str.length())
			    cout << setw(width) << (str.ptr() + c);
			else
			    cout << setw(width) << (str.ptr() + p);
		    }
		    else
			cout << setw(width) << str;
		}
		else
		    cout << metric->spec(sourceFlag, PMC_true, i);
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
		if (len < (int)instStr.length()) {
		    instStr.remove(len, instStr.length() - len);
		}
		cout << setw(len) << instStr << delimiter;
	    }
	    else
		cout << setw(len) << errStr << delimiter;
	}

	for (m = 0, v = 1; m < (int)metrics.length(); m++) {
	    metric = metrics[m];
	    for (i = 0; i < (int)metric->numValues(); i++) {
	    	if (metric->hasInstances()) {
		    PMC_String const &str = metric->instName(i);
		    strncpy(buffer, str.ptr(), width);
		    buffer[width] = '\0';
		    cout << setw(width) << buffer;
		}
		else
		    cout << setw(width) << "n/a";

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
		if (len < (int)normStr.length()) {
		    normStr.remove(len, normStr.length() - len);
		}
		cout << setw(len) << normStr << delimiter;
	    }
	    else
		cout << errStr << delimiter;
	}

	for (m = 0, v = 1; m < (int)metrics.length(); m++) {
	    metric = metrics[m];
	    for (i = 0; i < (int)metric->numValues(); i++) {	      
		if (shortFlag)
		    cout << setprecision(precision)
			 << setw(width)
			 << metric->scale();		
		else if (descFlag)
		    cout << setw(width) 
			 << PMC_Metric::formatNumber(metric->scale());
		else
#ifdef HAVE_IOSTREAM
// Warning: piggy-backing iomanip conditional on iostream
		    cout << setiosflags(ios_base::fixed)
			 << setprecision(precision)
			 << setw(width)
			 << metric->scale();
#else
		    cout << setiosflags(ios::fixed)
			 << setprecision(precision)
			 << setw(width)
			 << metric->scale();
#endif
	    	if (v < numValues) {
		    cout << delimiter;
		    v++;
		}
	    }
	}
	cout << endl;
    }

    if (unitFlag) {
	if (timeFlag)
	    if (niceFlag) {
		if (len < (int)unitStr.length()) {
		    unitStr.remove(len, unitStr.length() - len);
		}
		cout << setw(len) << unitStr << delimiter;
	    }
	    else
		cout << noneStr << delimiter;

	for (m = 0, v = 1; m < (int)metrics.length(); m++) {
	    metric = metrics[m];
	    PMC_String const &str = (niceFlag ? metric->desc().abvUnits()
				              : metric->desc().units());
	    for (i = 0; i < (int)metric->numValues(); i++) {
	    	if (niceFlag) 
		    if ((int)str.length() > width)
			cout << setw(width) 
			    << (str.ptr() + str.length() - width);
		    else
			cout << setw(width) << str;
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

void
tv_add(const struct timeval *a, const struct timeval *b, struct timeval *r)
{
    struct timeval t;

    t.tv_sec = a->tv_sec + b->tv_sec;
    t.tv_usec = a->tv_usec + b->tv_usec;
    while (t.tv_usec > 1000000) {
        t.tv_sec++;
        t.tv_usec -= 1000000;
    }

    *r = t;
}

int
main(int argc, char *argv[])
{
    char	*endnum = NULL;
    char	*msg;
    int		errflag = 0;
    int		sts = 0;
    int		c;
    int		l;
    int		m;
    int		i;
    int		v;
    int		lines = 0;

    // Metrics
    PMC_StrList	archives;
    PMC_String	host;
    PMC_Metric 	*metric;
    double	value;

    // Config file
    PMC_String	configName;
    FILE	*configFile = NULL;

    // Flags for setting time boundaries.
    int		Aflag = 0;		// 0 ctime, 1 +offset, 2 -offset
    char	*Atime = NULL;		// tm from -A flag
    int		Sflag = 0;		// 0 ctime, 1 +offset, 2 -offset
    char	*Stime = NULL;		// tm from -S flag
    int		Oflag = 0;		// 0 ctime, 1 +offset, 2 -offset
    char	*Otime = NULL;		// tm from -S flag
    int		Tflag = 0;		// 0 ctime, 1 +offset, 2 -offset
    char	*Ttime = NULL;		// tm from -T flag

    // Timing
    PMC_String		tzLabel;
    PMC_String		tzString;
    struct timeval	logEndTime;
    struct timeval	realStartTime;
    struct timeval	realEndTime;
    struct timeval	position;
    struct timeval	interval;
    struct timeval	curPos;
    double		pos;
    double		endTime;
    double		delay;
    double		diff;

    // Set the program name for error messages
    __pmSetProgname(argv[0]);

    // Default update interval is 1 second
    interval.tv_sec = 1;
    interval.tv_usec = 0;

    // Create the metric fetch group
    group = new PMC_Group(PMC_true);

//
// Parse command line options
//

    while((c = getopt(argc, argv, 
		      "A:a:c:Cd:D:f:Fgh:HilmMn:NO:oP:rR:s:S:t:T:uU:w:XZ:z?")) != EOF) {
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
		    archives.append(PMC_String(endnum));
		    endnum = strtok(NULL, ", \t");
		}
	    }
	    break;

	case 'c':	// config file
	    configName = optarg;
	    break;

	case 'C':	// parse config, output metrics and units only
	    dumpFlag = PMC_false;
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

	case 'g':	// Shortest format
	    if (descFlag) {
		pmprintf("%s: -F and -g may not be used together\n", 
			 pmProgname);
		errflag++;
	    }
	    else if (niceFlag) {
		pmprintf("%s: -i and -g may not be used togther\n",
			 pmProgname);
		errflag++;
	    }
	    else
		shortFlag = PMC_true;
	    break;

	case 'f':	// Time format
	    timeFormat = optarg;
	    if (timeFormat.length() == 0)
	    	timeFlag = PMC_false;
	    else
	        timeFlag = PMC_true;
	    break;

	case 'F':	// Fixed width values
	    if (shortFlag) {
		pmprintf("%s: -F and -g options may not be used together\n",
			 pmProgname);
		errflag++;
	    }
	    else
		descFlag = PMC_true;
	    break;

        case 'h':       // contact PMCD on this hostname
	    if (archives.length() > 0) {
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
	    headerFlag = PMC_true;
	    break;

	case 'i':	// abbreviate metric names
	    if (precFlag) {
		pmprintf("%s: -i and -P may not be used togther\n",
			 pmProgname);
		errflag++;
	    }
	    else if (shortFlag) {
		pmprintf("%s: -i and -g may not be used togther\n",
			 pmProgname);
		errflag++;
	    }
	    else
		niceFlag = PMC_true;
	    break;

	case 'l':	// show source of metrics
	    sourceFlag = PMC_true;
	    break;

	case 'm':	// show metric names
	    metricFlag = PMC_true;
	    break;

	case 'M':	// show full metric names
	    fullFlag = PMC_true;
	    break;

	case 'X':	// show full metric names repeatedly (extended)
	    fullFlag = PMC_true;
	    fullXFlag = PMC_true;
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
	    normFlag = PMC_true;
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
	    timeOffsetFlag = PMC_true;
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
		precFlag = PMC_true;
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
	    rawFlag = PMC_true;
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
	    unitFlag = PMC_true;
	    break;

	case 'U':	// error string
	    errStr = optarg;
	    break;

        case 'w':       // width
	    if (precFlag) {
		pmprintf("%s: -P and -w may not be used together\n",
			 pmProgname);
		errflag++;
	    }
	    else {
		width = (int)strtol(optarg, &endnum, 10);
		widthFlag = PMC_true;
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
            zflag = PMC_true;
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
	metricFlag = unitFlag = sourceFlag = normFlag = PMC_true;
    }
    if (fullXFlag) {
	niceFlag = PMC_true;
    }

    // Get local namespace is requested before opening any contexts
    //
    if (pmnsFile.length()) {
	sts = pmLoadNameSpace(pmnsFile.ptr());
	if (sts < 0) {
	    pmprintf("%s: %s\n", pmProgname, pmErrStr(sts));
	    pmflush();
	    exit(1);
	}
    }

    // Create archive contexts
    //
    if (archives.length() > 0) {
	for (c = 0; c < (int)archives.length(); c++)
	    if (group->use(PM_CONTEXT_ARCHIVE, archives[c].ptr()) < 0)
		errflag++;
    }
    // Create live context
    //
    else if (host.length() > 0) {
	if (group->use(PM_CONTEXT_HOST, host.ptr()) < 0)
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
	widthFlag = PMC_true;
	descFlag = PMC_true;
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

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	cerr << "main: optind = " << optind << ", argc = " << argc
	     << ", width = " << width << ", precision = " << precision
	     << endl;
#endif

    if (optind == argc) {
	if (configName.length() == 0) {
	    configFile = stdin;
	    configName = "(stdin)";
	}
	else {
	    configFile = fopen(configName.ptr(), "r");
	    if (configFile == NULL) {
		pmprintf("%s: Unable to open %s: %s\n",
			 pmProgname, configName.ptr(), strerror(errno));
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

    if (metrics.length() == 0 || numValues == 0) {
	pmprintf("%s: no valid metrics, exiting.\n", pmProgname);
	pmflush();
	exit(1);
    }
    else if (errflag)
        pmprintf("%s: Warning: Some metrics ignored, continuing with valid metrics\n",
		 pmProgname);

    pmflush();

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	cerr << "main: parsed " << metrics.length() << " metrics"
	     << endl;
#endif

    group->useDefault();

    if (group->which()->source().type() != PM_CONTEXT_ARCHIVE)
	isLive = PMC_true;

    if (isLive && (Aflag || Oflag || Sflag)) {
	pmprintf("%s: Error: -A, -O and -S options ignored in live mode\n",
		 pmProgname);
	pmflush();
	exit(1);
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	cerr << "main: default source is " << *(group->which()) << endl;
#endif

    if (zflag)
	group->useTZ();
    else if (timeZone.length()) {
	sts = group->useTZ(timeZone);
        if ((sts = pmNewZone(timeZone.ptr())) < 0) {
	    pmprintf("%s: cannot set timezone to \"%s\": %s\n",
		     pmProgname, (char *)(timeZone.ptr()), pmErrStr(sts));
	    pmflush();
	    exit(1);
        }
    }

    group->defaultTZ(tzLabel, tzString);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0) {
	cerr << "main: Using timezone \"" << tzString << "\" from " << tzLabel
	     << endl;
    }
#endif

    // putenv timezone into TZ as we may use strftime or ctime later
    //
    if (group->defaultTZ() != PMC_Group::localTZ) {
	tzEnv.append(tzString);
	sts = putenv(tzEnv.ptr());
	if (sts < 0) {
	    pmprintf("%s: Warning: Unable to set timezone in environment\n",
		     pmProgname);
	    sts = 0;
	}
#ifdef PCP_DEBUG
	else if (pmDebug & DBG_TRACE_APPL0)
	    cerr << "main: Changed environment with \""
		 << tzEnv << '"' << endl;
#endif
    }

    if (isLive) {
	gettimeofday(&logStartTime, NULL);
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

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0) {
        cerr << "main: start = "
             << __pmtimevalToReal(&logStartTime) << ", end = "
             << __pmtimevalToReal(&logEndTime)
             << endl;
    }
#endif

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

    if (endTime < pos && Tflag == PMC_false)
    	endTime = DBL_MAX;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0) {
	cerr << "main: realStartTime = " << __pmtimevalToReal(&realStartTime)
	     << ", endTime = " << endTime << ", pos = " << pos 
	     << ", delay = " << delay << endl;
    }
#endif

    pmflush();
    dumpHeader();

    if (fullXFlag == PMC_false) {
	// Only dump full names once
	fullFlag = PMC_false;
    }

    if (!dumpFlag) {
	exit(0);
    }

    if (!isLive) {
	int tmp_mode = PM_MODE_INTERP;
	int tmp_delay = getXTBintervalFromTimeval(&tmp_mode, &interval);
	group->setArchiveMode(tmp_mode, &position, tmp_delay);
    }

    if (shortFlag) {
	cout.precision(precision);
    }
    else if (!descFlag) {
	cout.precision(precision);
	cout.setf(ios::fixed);
    }

    while(pos <= endTime && 
	  ((numSamples > 0 && sampleCount < numSamples) || numSamples == 0)) {

	group->fetch();
	sampleCount++;

	if (timeFlag)
	    cout << dumpTime(position) << delimiter;

	for (m = 0, v = 1; m < (int)metrics.length(); m++) {
	    metric = metrics[m];

	    for (i = 0; i < (int)metric->numValues(); i++) {
		if (rawFlag) {
		    if (metric->currError(i) < 0) {
			if (niceFlag)
			    cout << setw(width) << errStr;
			else
			    cout << errStr;
			goto next;
		    }
		    else if (metric->real())
			value = metric->currValue(i);
		}
		else if (metric->error(i) < 0) {
		    if (niceFlag)
			cout << setw(width) << errStr;
		    else
			cout << errStr;
		    goto next;
		}
		else if (metric->real())
		    value = metric->value(i);

		if (metric->real()) {
		    if (descFlag)
			if (niceFlag)
			    cout << setw(width) 
				 << PMC_Metric::formatNumber(value);
			else
			    cout << PMC_Metric::formatNumber(value);
		    else if (niceFlag)
			cout << setw(width) << value;
		    else
			cout << value;
		}
		// String
		else {
		    l = metric->strValue(i).length();
		    buffer[0] = '\"';
		    if (niceFlag) {
			if (l > width - 2) {
			    strncpy(buffer+1, metric->strValue(i).ptr(), 
				    width - 2);
			    buffer[width - 1] = '\"';
			    buffer[width] = '\0';
			    cout << setw(width) << buffer;
			}
			else {
			    strcpy(buffer+1, metric->strValue(i).ptr());
			    buffer[l + 1] = '\"';
			    buffer[l + 2] = '\0';
			    cout << setw(width) << buffer;
			}
		    }
		    else if (widthFlag) {
			if (l > width - 2 && width > 5) {
			    strncpy(buffer+1, metric->strValue(i).ptr(),
				    width - 5);
			    strcpy(buffer + width - 4, "...\"");
			    buffer[width] = '\0';
			    cout << setw(width) << buffer;
			}
			else {
			    strncpy(buffer+1, metric->strValue(i).ptr(),
				    width - 2);
			    buffer[width - 1] = '\"';
			    buffer[width] = '\0';
			    cout << setw(width) << buffer;
			}
		    }
		    else
			cout << '\"' << metric->strValue(i) << '\"';
		}
	
	next:
		if (v < numValues) {
		    cout << delimiter;
		    v++;
		}
	    }
	}
	cout << endl;

	tv_add (&position, &interval, &position);

	if (isLive) {
	    gettimeofday(&curPos, NULL);
	    diff = __pmtimevalSub(&position, &curPos);

	    if (diff < 0.0) {	// We missed an update
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_APPL0) {
		    cerr << "Need to slip " << -diff << " seconds" << endl;
		}
#endif
		do {
		    tv_add (&position, &interval, &position);
		    diff = __pmtimevalSub(&position, &curPos);
		} while (diff < 0.0);
	    }

#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL1) {
	    	cerr << "Napping for " << diff << " seconds" << endl;
	    }
#endif
	    sginap((long)(diff * (double)CLK_TCK));
	}

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
