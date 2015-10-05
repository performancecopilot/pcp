/*
 * Copyright (c) 2013, Red Hat.
 * Copyright (c) 2007 Aconex.  All Rights Reserved.
 * Copyright (c) 1997-2005 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */
#include <limits.h>
#include <float.h>
#include <math.h>

#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include "qmc_group.h"
#include "qmc_source.h"
#include "qmc_context.h"
#include "qmc_metric.h"

int QmcGroup::tzLocal = -1;
bool QmcGroup::tzLocalInit = false;
QString	QmcGroup::tzLocalString;
QString	QmcGroup::localHost;

QmcGroup::QmcGroup(bool restrictArchives)
{
    my.restrictArchives = restrictArchives;
    my.mode = PM_CONTEXT_HOST;
    my.use = -1;
    my.localSource = 0;
    my.tzFlag = unknownTZ;
    my.tzDefault = -1;
    my.tzUser = -1;
    my.tzGroupIndex = 0;
    my.timeEndReal = 0.0;

    // Get timezone from environment
    if (tzLocalInit == false) {
	char buf[MAXHOSTNAMELEN];
	gethostname(buf, MAXHOSTNAMELEN);
	buf[MAXHOSTNAMELEN-1] = '\0';
	localHost = buf;

        char *tz = __pmTimezone();
	if (tz == NULL)
	    pmprintf("%s: Warning: Unable to get timezone from environment\n",
		     pmProgname);
	else {
	    tzLocal = pmNewZone(tz);
	    if (tzLocal < 0)
		pmprintf("%s: Warning: Timezone for localhost: %s\n",
			 pmProgname, pmErrStr(tzLocal));
	    else {
		tzLocalString = tz;
		my.tzDefault = tzLocal;
		my.tzFlag = localTZ;
	    }
	}
	tzLocalInit = true;
    }
}

QmcGroup::~QmcGroup()
{
    for (int i = 0; i < my.contexts.size(); i++)
	if (my.contexts[i])
	    delete my.contexts[i];
}

int
QmcGroup::use(int type, const QString &theSource, int flags)
{
    int sts = 0;
    unsigned int i;
    QString source(theSource);

    if (type == PM_CONTEXT_LOCAL) {
	for (i = 0; i < numContexts(); i++)
	    if (my.contexts[i]->source().type() == type)
		break;
    }
    else if (type == PM_CONTEXT_ARCHIVE) {
	if (source == QString::null) {
	    pmprintf("%s: Error: Archive context requires archive path\n",
			 pmProgname);
	    return PM_ERR_NOCONTEXT;
	}
	// This doesn't take into account {.N,.meta,.index,} ... but
	// then again, nor does pmNewContext.  More work ... useful?
	for (i = 0; i < numContexts(); i++) {
	    if (source == my.contexts[i]->source().source())
		break;
	}
    }
    else {
	if (source == QString::null) {
	    if (!defaultDefined()) {
		createLocalContext();
		if (!defaultDefined()) {
		    pmprintf("%s: Error: "
			 "Cannot connect to PMCD on localhost: %s\n",
			 pmProgname,
			 pmErrStr(my.localSource->status()));
		    return my.localSource->status();
		}
	    }
	    source = my.contexts[0]->source().source();
	}

	for (i = 0; i < numContexts(); i++) {
	    if (source == my.contexts[i]->source().source())
		break;
	}
    }

    if (i == numContexts()) {
	if (pmDebug & DBG_TRACE_PMC) {
	    QTextStream cerr(stderr);
	    cerr << "QmcGroup::use: No direct match for context \"" << source
		 << "\" (type " << type << ")." << endl;
	}

	// Determine live or archive mode by the first source
	if (i == 0)
	    my.mode = type;

	// If the assumed mode differs from the requested context type
	// we may need to map the host to an archive
	if (my.mode != type) {

	    if (my.mode == PM_CONTEXT_HOST && type == PM_CONTEXT_ARCHIVE) {
		pmprintf("%s: Error: Archive \"%s\" requested "
			 "after live mode was assumed.\n", pmProgname,
			 (const char *)source.toAscii());
		return PM_ERR_NOCONTEXT;
	    }

	    // If we are in archive mode, map hosts to archives of same host
	    if (my.mode == PM_CONTEXT_ARCHIVE && type == PM_CONTEXT_HOST) {
		QString chop1 = source.remove(PM_LOG_MAXHOSTLEN-1, INT_MAX);
		for (i = 0; i < numContexts(); i++) {
		    QString chop2 = my.contexts[i]->source().host();
		    chop2.remove(PM_LOG_MAXHOSTLEN-1, INT_MAX);
		    if (chop1 == chop2)
			break;
		}

		if (i == numContexts()) {
		    pmprintf("%s: Error: No archives were specified "
			     "for host \"%s\"\n", pmProgname,
			     (const char *)source.toAscii());
		    return PM_ERR_NOTARCHIVE;
		}
	    }
	}
    }

    if (i == numContexts()) {
	if (pmDebug & DBG_TRACE_PMC) {
	    QTextStream cerr(stderr);
	    cerr << "QmcGroup::use: Creating new context for \"" << source
		 << '\"' << endl;
	}

	QmcSource *src = QmcSource::getSource(type, source, flags, false);
	if (src == NULL) {
	    pmprintf("%s: Error: No archives were specified for host \"%s\"\n",
		     pmProgname, (const char *)source.toAscii());
	    return PM_ERR_NOTARCHIVE;
	}

	QmcContext *newContext = new QmcContext(src);
	if (newContext->handle() < 0) {
	    sts = newContext->handle();
	    pmprintf("%s: Error: %s: %s\n", pmProgname,
		     (const char *)source.toAscii(), pmErrStr(sts));
	    delete newContext;
	    return sts;
	}

	// If we are in archive mode and are adding an archive,
	// make sure another archive for the same host does not exist
	if (my.restrictArchives && type == PM_CONTEXT_ARCHIVE) {
	    for (i = 0; i < numContexts(); i++)
		// No need to restrict comparison here, both are from
		// log labels.
		if (my.contexts[i]->source().host() ==
			newContext->source().host()) {
		    pmprintf("%s: Error: Archives \"%s\" and \"%s\" are from "
			     "the same host \"%s\"\n", pmProgname,
			     my.contexts[i]->source().sourceAscii(),
			     newContext->source().sourceAscii(),
			     my.contexts[i]->source().hostAscii());
		    delete newContext;
		    return PM_ERR_NOCONTEXT;
		}
	}

	my.contexts.append(newContext);
	my.use = my.contexts.size() - 1;

	if (pmDebug & DBG_TRACE_PMC) {
	    QTextStream cerr(stderr);
	    cerr << "QmcGroup::use: Added context " << my.use << ": "
		 << *newContext << endl;
	}
    }

    // We found a match, do we need to use a different context?
    else if (i != (unsigned int)my.use) {
	my.use = i;
	sts = useContext();
	if (sts < 0) {
	    pmprintf("%s: Error: Unable to use context to %s: %s\n", pmProgname,
		     context()->source().sourceAscii(), pmErrStr(sts));
	    return sts;
	}

	if (pmDebug & DBG_TRACE_PMC) {
	    QTextStream cerr(stderr);
	    cerr << "QmcGroup::use: Using existing context " << my.use
		 << " for " << context()->source().desc() << endl;
	}
    }
    else if (pmDebug & DBG_TRACE_PMC) {
	QTextStream cerr(stderr);
	cerr << "QmcGroup::use: Using current context " << my.use
	     << " (handle = " << context()->handle() << ") for " 
	     << context()->source().desc() << endl;
    }

    return context()->handle();
}

int
QmcGroup::useTZ()
{
    int sts = context()->useTZ();

    if (sts >= 0) {
	my.tzDefault = context()->source().tzHandle();
	my.tzFlag = groupTZ;
	my.tzGroupIndex = my.use;
	if (pmDebug & DBG_TRACE_PMC) {
	    QTextStream cerr(stderr);
	    cerr << "QmcGroup::useTZ: Using timezone of "
		 << context()->source().desc()
		 << " (" << my.tzGroupIndex << ')' << endl;
	}
    }
    return sts;
}

int
QmcGroup::useTZ(const QString &tz)
{
    int sts = pmNewZone(tz.toAscii());

    if (sts >= 0) {
	my.tzUser = sts;
	my.tzUserString = tz;
	my.tzFlag = userTZ;
	my.tzDefault = sts;

	if (pmDebug & DBG_TRACE_PMC) {
	    QTextStream cerr(stderr);
	    cerr << "QmcGroup::useTZ: Switching timezones to \"" << tz
		 << "\" (" << my.tzUserString << ')' << endl;
	}
    }
    return sts;
}

int
QmcGroup::useLocalTZ()
{
    if (tzLocal >= 0) {
	int sts = pmUseZone(tzLocal);
	if (sts > 0) {
	    my.tzFlag = localTZ;
	    my.tzDefault = tzLocal;
	    if (pmDebug & DBG_TRACE_PMC) {
		QTextStream cerr(stderr);
		cerr << "QmcGroup::useTZ: Using timezone of host \"localhost\""
		     << endl;
	    }
	}
	return sts;
    }
    return tzLocal;
}

void
QmcGroup::defaultTZ(QString &label, QString &tz)
{
    if (my.tzFlag == userTZ) {
	label = my.tzUserString;
	tz = my.tzUserString;
    }
    else if (my.tzFlag == localTZ) {
	label = localHost;
	tz = tzLocalString;
    }
    else {
	label = my.contexts[my.tzGroupIndex]->source().host();
	tz = my.contexts[my.tzGroupIndex]->source().timezone();
    }
}

int
QmcGroup::useDefaultTZ()
{
    if (my.tzFlag == unknownTZ)
	return -1;
    return pmUseZone(my.tzDefault);
}

int
QmcGroup::useDefault()
{
    if (numContexts() == 0)
	createLocalContext();
    if (numContexts() == 0)
	return my.localSource->status();
    my.use = 0;
    return pmUseContext(context()->handle());
}

void
QmcGroup::createLocalContext()
{
    if (numContexts() == 0) {
	QTextStream cerr(stderr);
	QmcSource *localSource = QmcSource::getSource(PM_CONTEXT_HOST,
							localHost, 0, false);
	if (localSource->status() < 0 && pmDebug & DBG_TRACE_PMC)
	    cerr << "QmcGroup::createLocalContext: Default context to "
		 << localSource->desc() << " failed: " 
		 << pmErrStr(localSource->status()) << endl;
	else if (pmDebug & DBG_TRACE_PMC)
	    cerr << "QmcGroup::createLocalContext: Default context to "
		 << localSource->desc() << endl;

	QmcContext *newContext = new QmcContext(localSource);
	if (newContext->handle() < 0) {
	    pmprintf("%s: Error: %s: %s\n", pmProgname,
		     (const char *)localHost.toAscii(), pmErrStr(newContext->handle()));
	}
	my.contexts.append(newContext);
	my.use = my.contexts.size() - 1;
    }
}

void
QmcGroup::updateBounds()
{
    double newStart = DBL_MAX;
    double newEnd = 0.0;
    double startReal;
    double endReal;
    struct timeval startTv;
    struct timeval endTv;

    my.timeStart.tv_sec = 0;
    my.timeStart.tv_usec = 0;
    my.timeEnd = my.timeStart;

    for (unsigned int i = 0; i < numContexts(); i++) {
	if (my.contexts[i]->handle() >= 0 &&
	    my.contexts[i]->source().type() == PM_CONTEXT_ARCHIVE) {
	    startTv = my.contexts[i]->source().start();
	    endTv = my.contexts[i]->source().end();
	    startReal = __pmtimevalToReal(&startTv);
	    endReal = __pmtimevalToReal(&endTv);
	    if (startReal < newStart)
		newStart = startReal;
	    if (endReal > newEnd)
		newEnd = endReal;
	}
    }

    __pmtimevalFromReal(newStart, &my.timeStart);
    __pmtimevalFromReal(newEnd, &my.timeEnd);
    my.timeEndReal = newEnd;

    if (pmDebug & DBG_TRACE_PMC) {
	QTextStream cerr(stderr);
        cerr << "QmcGroup::updateBounds: start = " << my.timeStart.tv_sec 
	     << '.' << my.timeStart.tv_usec << ", end = "
             << my.timeEnd.tv_sec << '.' << my.timeEnd.tv_usec << endl;
    }
}

void
QmcGroup::dump(QTextStream &stream)
{
    stream << "mode: ";
    switch(my.mode) {
    case PM_CONTEXT_LOCAL:
	stream << "local";
	break;
    case PM_CONTEXT_HOST:
	stream << "live host";
	break;
    case PM_CONTEXT_ARCHIVE:
	stream << "archive";
	break;
    }

    stream << ", timezone: ";
    switch(my.tzFlag) {
    case QmcGroup::localTZ:
	stream << "local = \"" << tzLocalString;
	break;
    case QmcGroup::userTZ:
	stream << "user = \"" << my.tzUserString;
	break;
    case QmcGroup::groupTZ:
	stream << "group = \"" 
	       << my.contexts[my.tzGroupIndex]->source().timezone();
	break;
    case QmcGroup::unknownTZ:
	stream << "unknown = \"???";
	break;
    }
    stream << "\": " << endl;

    stream << "  " << numContexts() << " contexts:" << endl;
    for (unsigned int i = 0; i < numContexts(); i++) {
	stream << "    [" << i << "] " << *(my.contexts[i]) << endl;
	my.contexts[i]->dumpMetrics(stream);
    }
}

int
QmcGroup::useContext()
{
    int sts = 0;

    if ((context()->status() == 0) &&
	(sts = pmUseContext(context()->handle())) < 0)
	pmprintf("%s: Error: Unable to reuse context to %s: %s\n",
		 pmProgname, context()->source().sourceAscii(), pmErrStr(sts));
    return sts;
}

QmcMetric *
QmcGroup::addMetric(char const *string, double theScale, bool active)
{
    QmcMetric *metric = new QmcMetric(this, string, theScale, active);
    if (metric->status() >= 0)
	metric->context()->addMetric(metric);
    return metric;
}

QmcMetric *
QmcGroup::addMetric(pmMetricSpec *theMetric, double theScale, bool active)
{
    QmcMetric *metric = new QmcMetric(this, theMetric, theScale, active);
    if (metric->status() >= 0)
	metric->context()->addMetric(metric);
    return metric;
}

int
QmcGroup::fetch(bool update)
{
    int sts = 0;

    if (pmDebug & DBG_TRACE_PMC) {
	QTextStream cerr(stderr);
	cerr << "QmcGroup::fetch: " << numContexts() << " contexts" << endl;
    }

    for (unsigned int i = 0; i < numContexts(); i++)
	my.contexts[i]->fetch(update);

    if (numContexts())
	sts = useContext();

    if (pmDebug & DBG_TRACE_PMC) {
	QTextStream cerr(stderr);
	cerr << "QmcGroup::fetch: Done" << endl;
    }

    return sts;
}

int
QmcGroup::setArchiveMode(int mode, const struct timeval *when, int interval)
{
    int sts, result = 0;

    for (unsigned int i = 0; i < numContexts(); i++) {
	if (my.contexts[i]->source().type() != PM_CONTEXT_ARCHIVE)
	    continue;

	sts = pmUseContext(my.contexts[i]->handle());
	if (sts < 0) {
	    pmprintf("%s: Error: Unable to switch to context for %s: %s\n",
		     pmProgname, my.contexts[i]->source().sourceAscii(),
		     pmErrStr(sts));
	    result = sts;
	    continue;
	}
	sts = pmSetMode(mode, when, interval);
	if (sts < 0) {
	    pmprintf("%s: Error: Unable to set context mode for %s: %s\n",
		     pmProgname, my.contexts[i]->source().sourceAscii(),
		     pmErrStr(sts));
	    result = sts;
	}
    }
    sts = useContext();
    if (sts < 0)
	result = sts;
    return result;
}
