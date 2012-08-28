/*
 * Copyright (c) 1998-2005 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2007 Aconex.  All Rights Reserved.
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 */
#ifndef QMC_CONTEXT_H
#define QMC_CONTEXT_H

#include <qmc.h>
#include <qmc_desc.h>
#include <qmc_indom.h>
#include <qmc_source.h>

#include <qlist.h>
#include <qstring.h>
#include <qtextstream.h>

struct QmcNameToId {
    QString	name;
    pmID	id;
};

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

    unsigned int numDesc() const	// Number of descriptors
	{ return my.descs.size(); }

    QmcDesc const& desc(unsigned int index) const
	{ return *(my.descs[index]); }	// Access to each descriptor
    QmcDesc& desc(unsigned int index)	// Access to each descriptor
	{ return *(my.descs[index]); }

    unsigned int numIndoms() const	// Number of indom descriptors
	{ return my.indoms.size(); }	// requested from this context

    QmcIndom const& indom(unsigned int index) const
	{ return *(my.indoms[index]); }
    QmcIndom& indom(unsigned int index)	// Access to each indom
	{ return *(my.indoms[index]); }

    // Lookup the descriptor and indom for metric <name>
    int lookupDesc(const char *name, pmID& id);
    int lookupDesc(const char *name, unsigned int& desc, unsigned int& indom);
    int lookupDesc(pmID pmid, unsigned int& desc, unsigned int& indom);

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
	QList<QmcNameToId> names;	// Mapping between names and PMIDs
	QList<int> pmids;		// List of valid PMIDs to be fetched
	QList<QmcDesc*> descs;		// List of requested metric descs
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
