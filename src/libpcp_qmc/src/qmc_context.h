/*
 * Copyright (c) 2012 Red Hat.
 * Copyright (c) 2007 Aconex.  All Rights Reserved.
 * Copyright (c) 1998-2005 Silicon Graphics, Inc.  All Rights Reserved.
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
#ifndef QMC_CONTEXT_H
#define QMC_CONTEXT_H

#include "qmc.h"
#include "qmc_desc.h"
#include "qmc_indom.h"
#include "qmc_source.h"

#include <qhash.h>
#include <qlist.h>
#include <qstring.h>
#include <qtextstream.h>

class QmcContext
{
public:
    QmcContext(QmcSource *source);
    ~QmcContext();

    int status() const			// Is this a valid context?
	{ return (my.context < 0) ? my.context : 0; }

    int handle() const			// The PMAPI context handle
	{ return my.context; }

    QmcSource const& source() const	// Source description
	{ return *my.source; }

    unsigned int numIDs() const		// Number of unique pmIDs in use
	{ return my.pmids.size(); }

    pmID id(unsigned int index) const	// Access to each unique pmID
	{ return my.pmids[index]; }

    QmcDesc const& desc(pmID pmid) const
	{ return *(my.descCache.value(pmid)); }
    QmcDesc& desc(pmID pmid)		// Access to each descriptor
	{ return *(my.descCache.value(pmid)); }

    unsigned int numIndoms() const	// Number of indom descriptors
	{ return my.indoms.size(); }	// requested from this context

    QmcIndom const& indom(unsigned int index) const
	{ return *(my.indoms[index]); }
    QmcIndom& indom(unsigned int index)	// Access to each indom
	{ return *(my.indoms[index]); }

    // Lookup the pmid or indom (implies descriptor) for metric <name>|<id>
    int lookupPMID(const char *name, pmID& id);
    int lookupInDom(const char *name, unsigned int& indom);
    int lookupInDom(QmcDesc *desc, uint_t& indom);

    // Lookup various structures using the pmid
    int lookupInDom(pmID pmid, unsigned int& indom);
    int lookupName(pmID pmid, QString **name);
    int lookupDesc(pmID pmid, QmcDesc **desc);
    int lookup(pmID pmid, QString **name, QmcDesc **desc, QmcIndom **indom);

    int useTZ();			// Use this timezone

    unsigned int numMetrics() const	// Number of metrics using this context
	{ return my.metrics.size(); }

    QmcMetric const& metric(unsigned int index) const
	{ return *(my.metrics[index]); }
    QmcMetric& metric(unsigned int index)	// Get a handle to a metric
	{ return *(my.metrics[index]); }

    void addMetric(QmcMetric* metric);	// Add a metric using this context

    int fetch(bool update);		// Fetch metrics using this context

    struct timeval const& timeStamp() const
	{ return my.currentTime; }

    double timeDelta() const		// Time since previous fetch
	{ return my.delta; }

    int traverse(const char *name, QStringList &list);	// Walk the namespace

    friend QTextStream &operator<<(QTextStream &stream, const QmcContext &rhs);
    void dump(QTextStream &stream);	// Dump debugging information
    void dumpMetrics(QTextStream &stream);	// Dump list of metrics

private:
    struct {
	int context;			// PMAPI Context handle
	bool needReconnect;		// Need to reconnect the context
	QmcSource *source;		// Handle to the source description
	QHash<QString, pmID> nameCache;	// Reverse map from names to PMIDs
	QHash<pmID, QString*> pmidCache;// Mapping between PMIDs and names
	QHash<pmID, QmcDesc*> descCache;// Mapping between PMIDs and descs
	QList<pmID> pmids;		// List of valid PMIDs to be fetched
	QList<QmcIndom*> indoms;	// List of requested indoms 
	QList<QmcMetric*> metrics;	// List of metrics using this context
	struct timeval currentTime;	// Time of current fetch
	struct timeval previousTime;	// Time of previous fetch
	double delta;			// Time between fetches
    } my;

    static QStringList *theStringList;	// List of metric names in traversal
    static void dometric(const char *);
};

#endif	// QMC_CONTEXT_H
