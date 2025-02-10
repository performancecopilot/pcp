/*
 * Copyright (c) 2012 Red Hat.
 * Copyright (c) 2007-2008 Aconex.  All Rights Reserved.
 * Copyright (c) 1997,2005 Silicon Graphics, Inc.  All Rights Reserved.
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

#include "qmc_context.h"
#include "qmc_metric.h"
#include <limits.h>
#include <QVector>
#include <QStringList>
#include <QHashIterator>

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
    while (my.metrics.isEmpty() == false) {
	delete my.metrics.takeFirst();
    }
    QHashIterator<pmID, QmcDesc*> descs(my.descCache);
    while (descs.hasNext()) {
	descs.next();
	delete descs.value();
    }
    while (my.indoms.isEmpty() == false) {
	delete my.indoms.takeFirst();
    }
    if (my.context >= 0)
	my.source->delContext(my.context);
}

int
QmcContext::lookupName(pmID pmid, QString **name)
{
    char *value;
    int sts = 0;

    if ((sts = pmUseContext(my.context)) < 0)
	return sts;

    if (my.pmidCache.contains(pmid) == false) {
	if ((sts = pmNameID(pmid, &value)) >= 0) {
	    *name = new QString(value);
	    my.pmidCache.insert(pmid, *name);
	    free(value);
	}
    } else {
	QString *np = my.pmidCache.value(pmid);
	if (pmDebugOptions.pmc) {
	    QTextStream cerr(stderr);
	    cerr << "QmcContext::lookupName: Matched id "
		 << pmIDStr(pmid) << " to \"" << *np << "\"" << Qt::endl;
	}
	*name = np;
    }
    return sts;
}

int
QmcContext::lookupPMID(const char *name, pmID& id)
{
    QString key = name;
    int sts;

    if ((sts = pmUseContext(my.context)) < 0)
	return sts;

    if (my.nameCache.contains(key) == false) {
        if ((sts = pmLookupName(1, &name, &id)) >= 0)
	    my.nameCache.insert(key, id);
    } else {
	id = my.nameCache.value(key);
	if (pmDebugOptions.pmc) {
	    QTextStream cerr(stderr);
	    cerr << "QmcContext::lookupPMID: Matched \"" << name
		 << "\" to id " << pmIDStr(id) << Qt::endl;
	}
	sts = 1;
    }
    return sts;
}

int
QmcContext::lookupInDom(const char *name, uint32_t& indom)
{
    pmID pmid;
    int sts = lookupPMID(name, pmid);
    if (sts < 0)
	return sts;
    return lookupInDom(pmid, indom);    
}

int
QmcContext::lookupDesc(pmID pmid, QmcDesc **descriptor)
{
    int sts;
    QmcDesc *descPtr;

    if ((sts = pmUseContext(my.context)) < 0)
	return sts;

    if (my.descCache.contains(pmid) == false) {
	descPtr = new QmcDesc(pmid);
	if (descPtr->status() < 0) {
	    sts = descPtr->status();
	    delete descPtr;
	    return sts;
	}
	my.descCache.insert(pmid, descPtr);
	if (pmDebugOptions.pmc) {
	    QTextStream cerr(stderr);
	    cerr << "QmcContext::lookupDesc: Add descriptor for "
		 << pmIDStr(descPtr->id()) << Qt::endl;
	}
    }
    else {
	descPtr = my.descCache.value(pmid);
	if (pmDebugOptions.pmc) {
	    QTextStream cerr(stderr);
	    cerr << "QmcContext::lookupDesc: Reusing descriptor "
		 << pmIDStr(descPtr->id()) << Qt::endl;
	}
    }
    *descriptor = descPtr;
    return 0;
}

int
QmcContext::lookup(pmID pmid, QString **namePtr, QmcDesc **descPtr, QmcIndom **indomPtr)
{
    uint32_t indom;
    int sts;

    if ((sts = lookupName(pmid, namePtr)) < 0)
	return sts;
    if ((sts = lookupDesc(pmid, descPtr)) < 0)
	return sts;
    if ((sts = lookupInDom(pmid, indom)) < 0)
	return sts;
    *indomPtr = (indom == UINT_MAX) ? NULL : my.indoms[indom];
    return 0;
}

int
QmcContext::lookupInDom(pmID pmid, uint32_t& indom)
{
    QmcDesc *descPtr;
    int sts;

    if ((sts = lookupDesc(pmid, &descPtr)) < 0)
	return sts;
    return lookupInDom(descPtr, indom);
}

int
QmcContext::lookupInDom(QmcDesc *descPtr, uint32_t& indom)
{
    int i, sts;
    QmcIndom *indomPtr;

    if ((sts = pmUseContext(my.context)) < 0)
	return sts;

    indom = UINT_MAX;
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
	    if (pmDebugOptions.pmc) {
		QTextStream cerr(stderr);
		cerr << "QmcContext::lookupInDom: Add indom for "
		     << pmInDomStr(indomPtr->id()) << Qt::endl;
	    }
	}
	else {
	    indomPtr = my.indoms[i];
	    indom = i;
	    if (pmDebugOptions.pmc) {
		QTextStream cerr(stderr);
		cerr << "QmcContext::lookupInDom: Reusing indom "
		     << pmInDomStr(indomPtr->id()) << Qt::endl;
	    }
	}
    }
    return 0;
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
    stream << "Context " << my.context << " has " << my.nameCache.size()
       << " metric names for source:" << Qt::endl;
    my.source->dump(stream);
}

void
QmcContext::dumpMetrics(QTextStream &stream)
{
    for (int i = 0; i < my.metrics.size(); i++)
	stream << "        [" << i << "] "
	       << my.metrics[i]->spec(false, true) << Qt::endl;
}

void
QmcContext::addMetric(QmcMetric *metric)
{
    pmID pmid;
    int i;

    my.metrics.append(metric);
    if (metric->status() >= 0) {
	pmid = metric->desc().desc().pmid;
	for (i = 0; i < my.pmids.size(); i++)
	    if (my.pmids[i] == pmid)
		break;
	if (i == my.pmids.size())
	    my.pmids.append(pmid);
	metric->setIdIndex(i);
    }
}

int
QmcContext::fetch(bool update)
{
    int i, sts;
    pmResult *result;

    if (pmDebugOptions.pmc) {
	QTextStream cerr(stderr);
	cerr << "QmcContext::fetch: update=" << update << Qt::endl;
    }

    for (i = 0; i < my.metrics.size(); i++) {
	QmcMetric *metric = my.metrics[i];
	if (metric->status() < 0)
	    continue;
	metric->shiftValues();
	if (pmDebugOptions.pmc && pmDebugOptions.desperate) {
	    QTextStream cerr(stderr);
	    cerr << "QmcContext::fetch: shiftValues " << &metric << " metric[" << i << "] status=" << metric->status() << Qt::endl;
	}
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
    else if (pmDebugOptions.pmc) {
	QTextStream cerr(stderr);
	cerr << "QmcContext::fetch: Unable to switch to this context: "
	     << pmErrStr(sts) << Qt::endl;
    }

    if (sts >= 0 && my.needReconnect) {
	sts = pmReconnectContext(my.context);
	if (sts >= 0) {
	    my.needReconnect = false;
	    if (pmDebugOptions.pmc) {
		QTextStream cerr(stderr);
		cerr << "QmcContext::fetch: Reconnected context \""
		     << *my.source << Qt::endl;
	    }
	}
	else if (pmDebugOptions.pmc) {
	    QTextStream cerr(stderr);
	    cerr << "QmcContext::fetch: Reconnect failed: "
		 << pmErrStr(sts) << Qt::endl;
	}
    }

    if (sts >= 0 && my.pmids.size()) {
	if (pmDebugOptions.pmc) {
	    QTextStream cerr(stderr);
	    cerr << "QmcContext::fetch: fetching context " << *this << Qt::endl;
	}

	sts = pmFetch(my.pmids.size(), 
		      (pmID *)(my.pmids.toVector().data()), &result);
	if (sts >= 0) {
	    my.previousTime = my.currentTime;
	    my.currentTime = result->timestamp;
	    my.delta = pmtimevalSub(&my.currentTime, &my.previousTime);
	    for (i = 0; i < my.metrics.size(); i++) {
		QmcMetric *metric = my.metrics[i];
		if (metric->status() < 0) {
		    if (pmDebugOptions.pmc && pmDebugOptions.desperate) {
			QTextStream cerr(stderr);
			cerr << "QmcContext::fetch: " << metric << " metric[" << i << "] status=" << metric->status() << Qt::endl;
		    }
		    continue;
		}
		Q_ASSERT((int)metric->idIndex() < result->numpmid);
		metric->extractValues(result->vset[metric->idIndex()]);
		if (pmDebugOptions.pmc && pmDebugOptions.desperate) {
		    int	j;
		    QTextStream cerr(stderr);
		    cerr << "QmcContext::fetch: " << metric << " metric[" << i << "]" << Qt::endl;
		    for (j = 0; j < metric->numValues(); j++) {
			cerr << "  ";
			metric->dump(cerr, false, j);
		    }
		}
	    }
	    pmFreeResult(result);
	}
	else {
	    if (pmDebugOptions.pmc) {
		QTextStream cerr(stderr);
		cerr << "QmcContext::fetch: pmFetch: " << pmErrStr(sts) << Qt::endl;
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
	    if (pmDebugOptions.pmc && pmDebugOptions.desperate) {
		QTextStream cerr(stderr);
		cerr << "QmcContext::fetch: Updating metrics" << Qt::endl;
	    }
	    for (i = 0; i < my.metrics.size(); i++) {
		QmcMetric *metric = my.metrics[i];
		if (metric->status() < 0)
		    continue;
		metric->update();
	    }
	}
    }
    else if (pmDebugOptions.pmc) {
	QTextStream cerr(stderr);
	cerr << "QmcContext::fetch: nothing to fetch" << Qt::endl;
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

    if ((sts = pmUseContext(my.context)) < 0)
	return sts;

    sts = pmTraversePMNS(name, QmcContext::dometric);

    if (pmDebugOptions.pmc) {
	QTextStream cerr(stderr);
	if (sts >= 0) {
	    cerr << "QmcContext::traverse: Found " << list.size()
		<< " names from " << name << Qt::endl;
	}
	else
	    cerr << "QmcContext::traverse: Failed: " << pmErrStr(sts)
		<< Qt::endl;
    }	    

    return sts;
}
