/*
 * Copyright (c) 1997,2005 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2007-2008 Aconex.  All Rights Reserved.
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 */

#include <qmc_context.h>
#include <qmc_metric.h>
#include <limits.h>
#include <qvector.h>
#include <qstringlist.h>

QStringList *QmcContext::theStringList;

QmcContext::QmcContext(QmcSource* source)
{
    my.delta = 0.0;
    my.context = -1;
    my.source = source;
    my.needReconnect = false;

    if (my.source->status() >= 0)
	my.context = my.source->dupContext();
    else
	my.context = my.source->status();
}

QmcContext::~QmcContext()
{
    int i;

    for (i = 0; i < my.metrics.size(); i++)
	if (my.metrics[i])
	    delete my.metrics[i];
    for (i = 0; i < my.descs.size(); i++)
	if (my.descs[i])
	    delete my.descs[i];
    for (i = 0; i < my.indoms.size(); i++)
	if (my.indoms[i])
	    delete my.indoms[i];
    if (my.context >= 0)
	my.source->delContext(my.context);
}

int
QmcContext::lookupDesc(const char *name, pmID& id)
{
    int sts = 0;
    int i, len = strlen(name);

    for (i = 0; i < my.names.size(); i++) {
	const QmcNameToId &item = my.names[i];
	if (item.name.length() == len && item.name == name) {
	    sts = 1;
	    id = item.id;
	    if (pmDebug & DBG_TRACE_PMC) {
		QTextStream cerr(stderr);
		cerr << "QmcContext::lookupDesc: Matched \"" << name
		     << "\" to id " << pmIDStr(id) << endl;
	    }
	    break;
	}
    }

    if (i == my.names.size()) {
	sts = pmLookupName(1, (char **)(&name), &id);
	if (sts >= 0) {
	    QmcNameToId newName;
	    newName.name = name;
	    newName.id = id;
	    my.names.append(newName);
	}
    }

    return sts;
}

int
QmcContext::lookupDesc(const char *name, uint_t& desc, uint_t& indom)
{
    pmID id;
    int sts = lookupDesc(name, id);
    if (sts < 0)
	return sts;
    return lookupDesc(id, desc, indom);    
}

int
QmcContext::lookupDesc(pmID pmid, uint_t& desc, uint_t& indom)
{
    int i, sts = 0;
    QmcDesc *descPtr;
    QmcIndom *indomPtr;

    desc = UINT_MAX;
    indom = UINT_MAX;

    for (i = 0; i < my.descs.size(); i++)
	if (my.descs[i]->desc().pmid == pmid)
	    break;

    if (i == my.descs.size()) {
	descPtr = new QmcDesc(pmid);
	if (descPtr->status() < 0) {
	    sts = descPtr->status();
	    delete descPtr;
	    return sts;
	}
	my.descs.append(descPtr);
	desc = my.descs.size() - 1;
	if (pmDebug & DBG_TRACE_PMC) {
	    QTextStream cerr(stderr);
	    cerr << "QmcContext::lookupDesc: Add descriptor for "
		 << pmIDStr(descPtr->id()) << endl;
	}
    }
    else {
	descPtr = my.descs[i];
	desc = i;
	if (pmDebug & DBG_TRACE_PMC) {
	    QTextStream cerr(stderr);
	    cerr << "QmcContext::lookupDesc: Reusing descriptor "
		 << pmIDStr(descPtr->id()) << endl;
	}
    }
	
    if (descPtr->desc().indom != PM_INDOM_NULL) {
	for (i = 0; i < my.indoms.size(); i++)
	    if (my.indoms[i]->id() == (int)descPtr->desc().indom)
		break;
	if (i == my.indoms.size()) {
	    indomPtr = new QmcIndom(my.source->type(), *descPtr);
	    if (indomPtr->status() < 0) {
		sts = indomPtr->status();
		delete indomPtr;
		return sts;
	    }
	    my.indoms.append(indomPtr);
	    indom = my.indoms.size() - 1;
	    if (pmDebug & DBG_TRACE_PMC) {
		QTextStream cerr(stderr);
		cerr << "QmcContext::lookupDesc: Add indom for "
		     << pmInDomStr(indomPtr->id()) << endl;
	    }
	}
	else {
	    indomPtr = my.indoms[i];
	    indom = i;
	    if (pmDebug & DBG_TRACE_PMC) {
		QTextStream cerr(stderr);
		cerr << "QmcContext::lookupDesc: Reusing indom "
		     << pmInDomStr(indomPtr->id()) << endl;
	    }
	}
    }
    return sts;
}

int
QmcContext::useTZ()
{
    if (my.source->tzHandle() >= 0)
	return pmUseZone(my.source->tzHandle());
    return 0;
}

QTextStream&
operator<<(QTextStream &stream, const QmcContext &context)
{
    stream << context.source().desc() << " has "
	   << context.numMetrics() << " metrics";
    return stream;
}

void
QmcContext::dump(QTextStream &stream)
{
    stream << "Context " << my.context << " has " << my.names.size()
       << " metric names for source:" << endl;
    my.source->dump(stream);
}

void
QmcContext::dumpMetrics(QTextStream &stream)
{
    for (int i = 0; i < my.metrics.size(); i++)
	stream << "        [" << i << "] "
	       << my.metrics[i]->spec(false, true) << endl;
}

void
QmcContext::addMetric(QmcMetric *metric)
{
    pmID id;
    int i;

    my.metrics.append(metric);
    if (metric->status() >= 0) {
	id = metric->desc().desc().pmid;
	for (i = 0; i < my.pmids.size(); i++)
	    if (my.pmids[i] == (int)id)
		break;
	if (i == my.pmids.size())
	    my.pmids.append(id);
	metric->setIdIndex(i);
    }
}

int
QmcContext::fetch(bool update)
{
    int i, sts;
    pmResult *result;

    for (i = 0; i < my.metrics.size(); i++) {
	QmcMetric *metric = my.metrics[i];
	if (metric->status() < 0)
	    continue;
	metric->shiftValues();
    }

    // Inform each indom that we are about to do a new fetch so any
    // indom changes are now irrelevant
    for (i = 0; i < my.indoms.size(); i++)
	my.indoms[i]->newFetch();

    sts = pmUseContext(my.context);
    if (sts >= 0) {
	for (i = 0; i < my.indoms.size(); i++) {
	    if (my.indoms[i]->diffProfile())
		sts = my.indoms[i]->genProfile();
	}
    }
    else if (pmDebug & DBG_TRACE_OPTFETCH) {
	QTextStream cerr(stderr);
	cerr << "QmcContext::fetch: Unable to switch to this context: "
	     << pmErrStr(sts) << endl;
    }

    if (sts >= 0 && my.needReconnect) {
	sts = pmReconnectContext(my.context);
	if (sts >= 0) {
	    my.needReconnect = false;
	    if (pmDebug & DBG_TRACE_PMC) {
		QTextStream cerr(stderr);
		cerr << "QmcContext::fetch: Reconnected context \""
		     << *my.source << endl;
	    }
	}
	else if (pmDebug & DBG_TRACE_PMC) {
	    QTextStream cerr(stderr);
	    cerr << "QmcContext::fetch: Reconnect failed: "
		 << pmErrStr(sts) << endl;
	}
    }

    if (sts >= 0 && my.pmids.size()) {
	if (pmDebug & DBG_TRACE_OPTFETCH) {
	    QTextStream cerr(stderr);
	    cerr << "QmcContext::fetch: fetching context " << *this << endl;
	}

	sts = pmFetch(my.pmids.size(), 
		      (pmID *)(my.pmids.toVector().data()), &result);
	if (sts >= 0) {
	    my.previousTime = my.currentTime;
	    my.currentTime = result->timestamp;
	    my.delta = __pmtimevalSub(&my.currentTime, &my.previousTime);
	    for (i = 0; i < my.metrics.size(); i++) {
		QmcMetric *metric = my.metrics[i];
		if (metric->status() < 0)
		    continue;
		Q_ASSERT((int)metric->idIndex() < result->numpmid);
		metric->extractValues(result->vset[metric->idIndex()]);
	    }
	    pmFreeResult(result);
	}
	else {
	    if (pmDebug & DBG_TRACE_OPTFETCH) {
		QTextStream cerr(stderr);
		cerr << "QmcContext::fetch: pmFetch: " << pmErrStr(sts) << endl;
	    }
	    for (i = 0; i < my.metrics.size(); i++) {
		QmcMetric *metric = my.metrics[i];
		if (metric->status() < 0)
		    continue;
		metric->setError(sts);
	    }
	    if (sts == PM_ERR_IPC || sts == PM_ERR_TIMEOUT)
		my.needReconnect = true;
	}

	if (update) {
	    if (pmDebug & DBG_TRACE_OPTFETCH) {
		QTextStream cerr(stderr);
		cerr << "QmcContext::fetch: Updating metrics" << endl;
	    }
	    for (i = 0; i < my.metrics.size(); i++) {
		QmcMetric *metric = my.metrics[i];
		if (metric->status() < 0)
		    continue;
		metric->update();
	    }
	}
    }
    else if (pmDebug & DBG_TRACE_OPTFETCH) {
	QTextStream cerr(stderr);
	cerr << "QmcContext::fetch: nothing to fetch" << endl;
    }

    return sts;
}

void
QmcContext::dometric(const char *name)
{
    theStringList->append(name);
}

int
QmcContext::traverse(const char *name, QStringList &list)
{
    int	sts;

    theStringList = &list;
    theStringList->clear();

    sts = pmTraversePMNS(name, QmcContext::dometric);

    if (pmDebug & DBG_TRACE_PMC) {
	QTextStream cerr(stderr);
	if (sts >= 0) {
	    cerr << "QmcContext::traverse: Found " << list.size()
		<< " names from " << name << endl;
	}
	else
	    cerr << "QmcContext::traverse: Failed: " << pmErrStr(sts)
		<< endl;
    }	    

    return sts;
}
