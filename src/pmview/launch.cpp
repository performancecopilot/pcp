/*
 * Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2009 Aconex.  All Rights Reserved.
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
#include "main.h"
#include "launch.h"
#include "colorscale.h"
#include "metriclist.h"

#include <iostream>
using namespace std;

#define PM_LAUNCH_VERSION1 1
#define PM_LAUNCH_VERSION2 2

const QString Launch::theVersion1Str = "pmlaunch Version 1.0\n";
const QString Launch::theVersion2Str = "pmlaunch Version 2.0\n";

Launch::~Launch()
{
}

Launch::Launch(const QString &version)
: _strings(), 
  _groupMetric(-1),
  _groupCount(0),
  _groupHint(),
  _metricCount(0)
{
    if (version == "1.0")
	_version = PM_LAUNCH_VERSION1;
    else
	_version = PM_LAUNCH_VERSION2;
}

Launch::Launch(const Launch &rhs)
: _strings(rhs._strings),
  _groupMetric(rhs._groupMetric),
  _groupCount(rhs._groupCount),
  _groupHint(rhs._groupHint),
  _metricCount(rhs._metricCount),
  _version(rhs._version)
{
}

const Launch &
Launch::operator=(const Launch &rhs)
{
    if (this != &rhs) {
	 _strings = rhs._strings;
	 _groupMetric = rhs._groupMetric;
	 _groupCount = rhs._groupCount;
	 _groupHint = rhs._groupHint;
	 _metricCount = rhs._metricCount;
    }
    return *this;
}

void
Launch::setDefaultOptions(int interval,
			      int debug,
			      const char *timeport,
			      const char *starttime,
			      const char *endtime,
			      const char *offset,
			      const char *timezone,
			      const char *defsourcetype,
			      const char *defsourcename,
			      bool selected)
{
    addOption("interval", (interval < 0 ? -interval : interval));
    addOption("debug", debug);
    
    if (timeport != NULL)
	addOption("timeport", timeport);
    if (starttime != NULL)
	addOption("starttime", starttime);
    if (endtime != NULL)
	addOption("endtime", endtime);
    if (offset != NULL)
	addOption("offset", offset);
    if (timezone != NULL)
	addOption("timezone", timezone);
    if (defsourcetype != NULL)
	addOption("defsourcetype", defsourcetype);
    if (defsourcename != NULL)
	addOption("defsourcename", defsourcename);
    if (pmProgname != NULL)
	addOption("progname", pmProgname);
    addOption("pid", (int)getpid());

    if (selected)
	addOption("selected", "true");
    else
	addOption("selected", "false");
}

void 
Launch::addOption(const char *name, const char *value)
{
    QString	str = "option ";

    if (name == NULL)
	return;

    if (value == NULL) {
	str.append(name).append("=\n");
    }
    else {
	str.append(name).append('=').append(value).append('\n');
	str.append('\n');
    }
    _strings.append(str);
}

void 
Launch::addOption(const char *name, int value)
{
    QString	str = "option ";

    if (name == NULL)
	return;

    str.append(name).append('=').setNum(value).append('\n');
    _strings.append(str);
}

void
Launch::addMetric(const QmcMetric &metric, 
		      const SbColor &color,
		      int instance,
		      bool useSocks)
{
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1) {
	    cerr << "Launch::addMetric(1): Adding ";
	    metric.dump(cerr, true, instance);
	    cerr << " (" << _metricCount << ')' << endl;
	}
#endif

    QmcSource source = metric.context()->source();
    QByteArray ba;
    ba = metric.instName(instance).toLocal8Bit();

    addMetric(metric.context()->handle(), source.source(), source.host(), metric.name(),
	      metric.hasInstances() == 0 ? NULL : ba.data(),
	      metric.desc(), color, metric.scale(), useSocks);
}

void
Launch::addMetric(const QmcMetric &metric, 
		      const ColorScale &scale,
		      int instance,
		      bool useSocks)
{
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1) {
	    cerr << "Launch::addMetric(2): Adding ";
	    metric.dump(cerr, true, instance);
	    cerr << " (" << _metricCount << ')' << endl;
	}
#endif

    QmcSource source = metric.context()->source();
    QByteArray ba;
    ba = metric.instName(instance).toLocal8Bit();

    addMetric(metric.context()->handle(), source.source(), source.host(), metric.name(),
	      metric.hasInstances() == 0 ? NULL : ba.data(),
	      metric.desc(), scale, useSocks);
}

void 
Launch::addMetric(int context,
		      const QString &source,
		      const QString &host,
		      const QString &metric,
		      const char *instance,
		      const QmcDesc &desc,
		      const SbColor &color,
		      double scale,
		      bool useSocks)
{
    QString	str(256);
    QString	col;

    if (_groupMetric == -1) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1)
	    cerr << "Launch::addMetric: Called before startGroup."
		 << " Adding a group." << endl;
#endif
	startGroup();
    }

    preColor(context, source, host, metric, useSocks, str);

    str.append(" S ");
    MetricList::toString(color, col);
    str.append(col).append(',').setNum(scale).append(' ');

    postColor(desc, instance, str);
    _strings.append(str);
}

void 
Launch::addMetric(int context,
		      const QString &source,
		      const QString &host,
		      const QString &metric,
		      const char *instance,
		      const QmcDesc &desc,
		      const ColorScale &scale,
		      bool useSocks)
{
    int		i;
    QString	str(128);
    QString	col;

    if (_groupMetric == -1) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1)
	    cerr << "Launch::addMetric: Called before startGroup."
		 << " Adding a group." << endl;
#endif
	startGroup();
    }

    preColor(context, source, host, metric, useSocks, str);
    str.append(" D ");

    MetricList::toString(scale[0].color(), col);
    str.append(col).append(',').setNum(scale[0].min());
    for (i = 1; i < scale.numSteps(); i++) {
	MetricList::toString(scale[i].color(), col);
	str.append(',').append(col).append(',');
	str.setNum(scale[i].min());
    }
    str.append(' ');

    postColor(desc, instance, str);
    _strings.append(str);
}

void
Launch::preColor(int context,
		     const QString &source,
		     const QString &host,
		     const QString &metric,
		     bool useSocks,
		     QString &str)
{
    str.append("metric ").setNum(_metricCount++).append(' ');
    str.setNum(_groupCount).append(' ').append(_groupHint);

    switch(context) {
    case PM_CONTEXT_LOCAL:
	str.append(" l ");
	break;
    case PM_CONTEXT_HOST:
	str.append(" h ");
	break;
    case PM_CONTEXT_ARCHIVE:
	str.append(" a ");
	break;
    }

    str.append(source).append(' ');

    if (context == PM_CONTEXT_ARCHIVE)
	str.append(host);
    else if (useSocks)
	str.append("true");
    else
	str.append("false");

    str.append(' ').append(metric);
}

void
Launch::postColor(const QmcDesc &desc, 
		      const char *instance,
		      QString &str)
{
    const pmDesc d = desc.desc();

    if (_version == PM_LAUNCH_VERSION2) {
	str.setNum(d.type).append(' ');
	str.setNum(d.sem).append(' ');
	str.setNum(d.units.scaleSpace).append(' ');
	str.setNum(d.units.scaleTime).append(' ');
	str.setNum(d.units.scaleCount).append(' ');
    }

    str.setNum(d.units.dimSpace).append(' ');
    str.setNum(d.units.dimTime).append(' ');
    str.setNum(d.units.dimCount).append(' ');
    str.setNum((int)(d.indom)).append(" [");
    if (instance != NULL)
	str.append(instance);
    str.append("]\n");
}

void
Launch::startGroup(const char *hint)
{

    if (_groupMetric != -1)
    	cerr << "Launch::startGroup: Two groups started at once!" << endl;
    else {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1)
	    cerr << "Launch::startGroup: Starting group " << _groupCount 
	         << endl;
#endif

	_groupMetric = _metricCount;
	_groupHint = hint;
    }
}

void
Launch::endGroup()
{
    if (_groupMetric == -1)
    	cerr << "Launch::endGroup: No group to end!" << endl;
    else if (_groupMetric == _metricCount) {
    	cerr << "Launch::endGroup: No metrics added to group "
	     << _groupCount << endl;
	_groupMetric = -1;
    } else {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1)
	    cerr << "Launch::endGroup: ending group " << _groupCount 
	         << endl;
#endif

	_groupMetric = -1;
	_groupCount++;
    }
}

void
Launch::append(Launch const &rhs)
{
    if (rhs._groupMetric != -1) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1)
	    cerr << "Launch::append: Group not finished in appended object."
	         << " Completing group" << endl;
#endif

	// Cast away const, yuk.
	((Launch *)(&rhs))->endGroup();
    }
    _strings.append(rhs._strings);
}

QTextStream& 
operator<<(QTextStream& os, Launch const& rhs)
{
    int i;
    for (i = 0; i < rhs._strings.size(); i++)
	os << rhs._strings[i];
    return os;
}

const char *
Launch::launchPath()
{
    char *env;
    static char launch_path[MAXPATHLEN];

    if ((env = getenv("PM_LAUNCH_PATH")) != NULL)
	strncpy(launch_path, env, sizeof(launch_path));
    else
	pmsprintf(launch_path, sizeof(launch_path), "%s/config/pmlaunch", pmGetConfig("PCP_VAR_DIR"));
    return launch_path;
}

#include <iostream>

void
Launch::output(int fd) const
{
    QByteArray	ba;
    int		sts;
    const char	*str;

    if (_version == PM_LAUNCH_VERSION2)
	ba = theVersion2Str.toLocal8Bit();
    else
	ba = theVersion1Str.toLocal8Bit();

    str = ba.data();
    if ((sts = write(fd, str, strlen(str))) != (int)strlen(str)) {
	cerr << "Launch::output: version write->" << sts
	     << " not " << strlen(str) << endl;
    }

    for (int i = 0; i < _strings.length(); i++) {
	ba = _strings[i].toLocal8Bit();
	str = ba.data();
	if ((sts = write(fd, str, strlen(str))) != (int)strlen(str)) {
	    cerr << "Launch::output: string write->" << sts
		 << " not " << strlen(str)
		 << " for " << str << endl;
	}
    }
}
