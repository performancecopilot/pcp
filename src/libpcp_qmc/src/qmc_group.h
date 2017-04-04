/*
 * Copyright (c) 2013, Red Hat.
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
#ifndef QMC_GROUP_H
#define QMC_GROUP_H

#include "qmc.h"
#include "qmc_context.h"

#include <qlist.h>
#include <qstring.h>
#include <qtextstream.h>

class QmcGroup
{
public:
    enum TimeZoneFlag { localTZ, userTZ, groupTZ, unknownTZ };

public:
    QmcGroup(bool restrictArchives = false);
    ~QmcGroup();

    int mode() const { return my.mode; }

    unsigned int numContexts() const { return my.contexts.size(); }

    // Return a handle to the contexts
    QmcContext* context() const { return my.contexts[my.use]; }
    QmcContext* context(unsigned int index) const { return my.contexts[index]; }

    // Index to the active context
    unsigned int contextIndex() const { return my.use; }

    int use(int type, const QString &source, int flags = 0);
    int use(unsigned int index) { my.use = index; return useContext(); }
    bool defaultDefined() const { return (numContexts() > 0); }
    int useDefault();

    void createLocalContext();

    // Add a new metric to the group
    QmcMetric* addMetric(char const* str, double theScale = 0.0,
			  bool active = false);
    QmcMetric* addMetric(pmMetricSpec* theMetric, double theScale = 0.0,
			  bool active = false);

    // Fetch all the metrics in this group
    // By default, do all rate conversions and counter wraps
    int fetch(bool update = true);

    // Set the archive position and mode
    int setArchiveMode(int mode, const struct timeval *when, int interval);

    int useTZ();			// Use TZ of current context as default
    int useTZ(const QString &tz);	// Use this TZ as default
    int useLocalTZ();			// Use local TZ as default
    void defaultTZ(QString &label, QString &tz);

    TimeZoneFlag defaultTZ() const { return my.tzFlag; }
    int useDefaultTZ();

    struct timeval const& logStart() const { return my.timeStart; }
    struct timeval const& logEnd() const { return my.timeEnd; }
    void updateBounds();	// Determine the archive start and finish times

    void dump(QTextStream &os);

private:
    struct {
	QList<QmcContext*> contexts;	// List of all contexts in this group
	bool restrictArchives;		// Only one archive per host
	int mode;			// Default context type
	int use;			// Context in use
	QmcSource *localSource;		// Localhost source desc

	TimeZoneFlag tzFlag;		// default TZ type
	int tzDefault;			// handle to default TZ
	int tzUser;			// handle to user defined TZ
	QString tzUserString;		// user defined TZ;
	int tzGroupIndex;		// index to group context used for
					// current timezone
	struct timeval timeStart;	// Start of first archive
	struct timeval timeEnd;		// End of last archive
	double timeEndReal;		// End of last archive
    } my;

    // Timezone for localhost from environment
    static bool tzLocalInit;	// got TZ from environment
    static int tzLocal;		// handle to environment TZ
    static QString tzLocalString;	// environment TZ string
    static QString localHost;	// name of localhost

    int useContext();
};

#endif	// QMC_GROUP_H
