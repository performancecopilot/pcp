/*
 * Copyright (c) 2013,2015 Red Hat, Inc.
 * Copyright (c) 2007 Aconex.  All Rights Reserved.
 * Copyright (c) 1998,2005 Silicon Graphics, Inc.  All Rights Reserved.
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
#ifndef QMC_SOURCE_H
#define QMC_SOURCE_H

#include "qmc.h"

#include <qlist.h>
#include <qstring.h>
#include <qtextstream.h>

class QmcSource
{
public:
    QmcSource(int type, QString &source, int flags = 0);
    ~QmcSource();

    // Get the source description by searching the list of existing sources
    // and returning a new source only if required.
    // If matchHosts is true, then it will attempt to map a live context
    // to an archive source. If no matching archive context is found,
    // a NULL pointer is returned.
    static QmcSource* getSource(int type, QString &source, int flags = 0,
				 bool matchHosts = false);

    int status() const { return my.status; }
    int flags() const { return my.flags; }
    int type() const { return my.type; }

    bool isArchive() const { return my.type == PM_CONTEXT_ARCHIVE; }
    bool isContainer() const { return my.context_container != QString::null; }
    QString hostLabel() const;

    QString source() const { return my.source; }
    char *sourceAscii() const { return strdup((const char*)my.source.toAscii()); }
    QString host() const { return my.host; }
    char *hostAscii() const { return strdup((const char *)my.host.toAscii()); }
    QString proxy() const { return my.proxy; }
    char *proxyAscii() const { return strdup((const char *)my.proxy.toAscii()); }
    int tzHandle() const { return my.tz; }
    QString attributes() const { return my.attrs; }
    QString timezone() const { return my.timezone; }
    struct timeval start() const { return my.start; }
    QString startTime() { return timeString(&my.start); }
    struct timeval end() const { return my.end; }
    QString endTime() { return timeString(&my.end); }
    QString desc() const { return my.desc; }
    char *descAscii() const { return strdup((const char *)my.desc.toAscii()); }

    // Number of active contexts to this source
    uint numContexts() const { return my.handles.size(); }

    // Create a new context to this source
    int dupContext();

    // Delete context to this source
    int delContext(int handle);

    // Output the source
    friend QTextStream &operator<<(QTextStream &os, const QmcSource &rhs);

    // Dump all info about a source
    void dump(QTextStream &os);

    // Dump list of known sources
    static void dumpList(QTextStream &os);

    // Local host name (from gethostname(2))
    static QString localHost;

    // Convert a time to a string (long and short forms)
    static QString timeString(const struct timeval *timeval);
    static QString timeStringBrief(const struct timeval *timeval);

protected:
    // retry context/connection (e.g. if it failed in the constructor)
    void retryConnect(int type, QString &source);

    // compare two sources - static so getSource() can make use of it
    bool compare(int type, QString &source, int flags);

private:
    struct {
	int status;
	int type;
	QString source;
	QString proxy;
	QString attrs;
	QString host;
	QString context_hostname; // from pmcd/archive, not from -h/-a argument
	QString context_container;
	QString	desc;
	QString timezone;
	QList<int> handles;	// Contexts created for this source
	struct timeval start;
	struct timeval end;
	int tz;
	bool dupFlag;		// Dup has been called and 1st context is in use
	int flags;
    } my;

    static QList<QmcSource*> sourceList;
};

#endif	// QMC_SOURCE_H
