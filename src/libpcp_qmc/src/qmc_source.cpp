/*
 * Copyright (c) 2013,2016 Red Hat.
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

#include "qmc_source.h"

QString QmcSource::localHost;
QList<QmcSource*> QmcSource::sourceList;

QmcSource::QmcSource(int type, QString &source, int flags)
{
    my.status = -1;
    my.flags = flags;
    my.type = type;
    my.tz = 0;
    my.dupFlag = false;

    if (localHost.length() == 0)
	localHost = getLocalHost();

    my.attrs = QString();
    my.context_hostname = QString();
    my.context_container = QString();

    this->retryConnect(type, source);
}

QString
QmcSource::hostLabel(void) const
{
    if (my.context_container != QString()) {
	QString label;
	label.append(my.host);
	label.append(":");
	label.append(my.context_container);
	if (my.context_container != my.context_hostname) {
	    label.append("::");
	    label.append(my.context_hostname);
	}
	return label;
    }
    if (my.context_hostname != QString())
	return my.context_hostname;
    // fallback to the name extracted from the hostspec string
    return my.host;
}

void
QmcSource::retryConnect(int type, QString &source)
{
    int oldTZ;
    int oldContext;
    int offset;
    int sts;
    char *tzs;
    QString name;
    QString hostSpec;

    switch (type) {
    case PM_CONTEXT_LOCAL:
	my.desc = "Local context";
	my.host = my.source = localHost;
	my.proxy = "";
	break;

    case PM_CONTEXT_HOST:
	my.desc = "host \"";
	my.desc.append(source);
	my.desc.append(QChar('\"'));
	my.host = source;
	my.proxy = getenv("PMPROXY_HOST");
	if ((offset = my.host.indexOf('?')) >= 0) {
	    my.attrs = my.host;
	    my.attrs.remove(0, offset+1);
	    my.host.truncate(offset);
	    name = my.attrs.section(QString("container="), -1);
	    if (name != QString() && (offset = name.indexOf(',')) > 0)
		name.truncate(offset);
	    my.context_container = name;
	}
	if ((offset = my.host.indexOf('@')) >= 0) {
	    my.proxy = my.host;
	    my.proxy.remove(0, offset+1);
	}
	my.source = my.host;
	break;

    case PM_CONTEXT_ARCHIVE:
	my.desc = "archive \"";
	my.desc.append(source);
	my.desc.append(QChar('\"'));
	my.source = source;
	my.proxy = "";
	break;
    }

    oldContext = pmWhichContext();
    hostSpec = source;
    my.status = pmNewContext(type | my.flags, (const char *)hostSpec.toLatin1());
    if (my.status >= 0) {
	my.handles.append(my.status);

        // Fetch the server-side host name for this context, properly as of pcp 3.8.3+.
        my.context_hostname = pmGetContextHostName(my.status); // NB: may leak memory
        if (my.context_hostname == "") // may be returned for errors or PM_CONTEXT_LOCAL
            my.context_hostname = localHost;

	if (my.type == PM_CONTEXT_ARCHIVE) {
	    pmLogLabel lp;
	    struct timespec	tmp_ts;
	    sts = pmGetArchiveLabel(&lp);
	    if (sts < 0) {
		pmprintf("%s: Unable to obtain log label for \"%s\": %s\n",
			 pmGetProgname(), (const char *)my.desc.toLatin1(),
			 pmErrStr(sts));
		my.host = "unknown?";
		my.status = sts;
		goto done;
	    }
	    else {
		my.host = lp.hostname;
		TVfromTS(my.start, lp.start);
	    }
	    sts = pmGetArchiveEnd(&tmp_ts);
	    my.end.tv_sec = tmp_ts.tv_sec;
	    my.end.tv_usec = tmp_ts.tv_nsec / 1000;
	    if (sts < 0) {
		pmprintf("%s: Unable to determine end of \"%s\": %s\n",
			 pmGetProgname(), (const char *)my.desc.toLatin1(),
			 pmErrStr(sts));
		my.status = sts;
		goto done;
	    }
	}
	else {
	    gettimeofday(&my.start, NULL);
	    my.end = my.start;
	}

	if (pmDebugOptions.pmc) {
	    QTextStream cerr(stderr);
	    cerr << "QmcSource::QmcSource: Created context "
		 << my.handles.last() << " to " << my.desc << Qt::endl;
	}

	oldTZ = pmWhichZone(&tzs);
	my.tz = pmNewContextZone();
	if (my.tz < 0)
	    pmprintf("%s: Warning: Unable to obtain timezone for %s: %s\n",
		     pmGetProgname(), (const char *)my.desc.toLatin1(),
		     pmErrStr(my.tz));
	else {
	    sts = pmWhichZone(&tzs);
	    if (sts >= 0)
		my.timezone = tzs;
	    else
		pmprintf("%s: Warning: Unable to obtain timezone for %s: %s\n",
			 pmGetProgname(), (const char *)my.desc.toLatin1(),
			 pmErrStr(sts));
	}

	if (oldTZ >= 0) {
	    sts = pmUseZone(oldTZ);
	    if (sts < 0) {
		pmprintf("%s: Warning: Unable to switch timezones."
			 " Using timezone for %s: %s\n",
			 pmGetProgname(), (const char *)my.desc.toLatin1(),
			 pmErrStr(sts));
	    }	
	}
    }
    else if (pmDebugOptions.pmc) {
	QTextStream cerr(stderr);
	cerr << "QmcSource::QmcSource: Context to " << source
	     << " failed: " << pmErrStr(my.status) << Qt::endl;
    }

 done:
    sourceList.append(this);

    if (oldContext >= 0) {
	sts = pmUseContext(oldContext);
	if (sts < 0) {
	    pmprintf("%s: Warning: Unable to switch contexts."
		     " Using context to %s: %s\n",
		     pmGetProgname(), (const char *)my.desc.toLatin1(),
		     pmErrStr(sts));
	}
    }
}

QmcSource::~QmcSource()
{
    int i;

    for (i = 0; i < sourceList.size(); i++)
	if (sourceList[i] == this)
	    break;
    if (i < sourceList.size())
	sourceList.removeAt(i);
}

QString
QmcSource::timeString(const struct timeval *timeval)
{
    char timebuf[32], datebuf[32], *ddmm, *year;
    struct tm tmp;
    time_t secs = (time_t)timeval->tv_sec;

    ddmm = pmCtime(&secs, datebuf);
    ddmm[10] = '\0';
    year = &ddmm[20];
    year[4] = '\0';
    pmLocaltime(&secs, &tmp);

    pmsprintf(timebuf, sizeof(timebuf), "%02d:%02d:%02d.%03d",
	tmp.tm_hour, tmp.tm_min, tmp.tm_sec, (int)(timeval->tv_usec/1000));
    QString timestring = QString(timebuf);
    timestring.prepend(" ");
    timestring.prepend(ddmm);
    timestring.append(" ");
    timestring.append(year);
    return timestring;
}

QString
QmcSource::timeStringBrief(const struct timeval *timeval)
{
    char timestring[32];
    struct tm tmp;
    time_t secs = (time_t)timeval->tv_sec;

    pmLocaltime(&secs, &tmp);
    pmsprintf(timestring, sizeof(timestring), "%02d:%02d:%02d.%03d",
	tmp.tm_hour, tmp.tm_min, tmp.tm_sec, (int)(timeval->tv_usec/1000));
    return QString(timestring);
}

bool
QmcSource::compare(int type, QString &source, int flags)
{
    if (this->type() != type)
	return false;
    if (this->flags() != flags)
	return false;
    return this->source() == source;
}

const char *
QmcSource::getLocalHost()
{
    // Respect the setting of PMPROXY_HOST
    char *envstr;
    if ((envstr = getenv("PMPROXY_HOST")) != NULL) {
	if (*envstr != '\0')
	    return envstr;
    }

    // Otherwise, use local:
    return "local:";
}

QmcSource*
QmcSource::getSource(int type, QString &source, int flags, bool matchHosts)
{
    int i;
    QmcSource *src = NULL;

    for (i = 0; i < sourceList.size(); i++) {
	src = sourceList[i];
	if (matchHosts && type == PM_CONTEXT_HOST) {
	    if (src->type() == PM_CONTEXT_ARCHIVE && src->host() == source) {
		if (pmDebugOptions.pmc) {
		    QTextStream cerr(stderr);
		    cerr << "QmcSource::getSource: Matched host "
			 << source << " to archive " << src->source()
			 << " (source " << i << ")" << Qt::endl;
		}
		break;
	    }
	}
	else if (src->compare(type, source, flags)) {
	    if (pmDebugOptions.pmc) {
		QTextStream cerr(stderr);
		cerr << "QmcSource::getSource: Matched " << source
		     << " to source " << i << Qt::endl;
	    }
	    if (src->status() < 0)
		src->retryConnect(type, source);
	    break;
	}
    }

    if (i == sourceList.size() && 
	!(matchHosts == true && type == PM_CONTEXT_HOST)) {
	if (pmDebugOptions.pmc) {
	    QTextStream cerr(stderr);
	    if (type != PM_CONTEXT_LOCAL)
		cerr << "QmcSource::getSource: Creating new source for "
		     << source << Qt::endl;
	    else
		cerr << "QmcSource::getSource: Creating new local context"
		     << Qt::endl;
	}
	src = new QmcSource(type, source, flags);
    }

    if (src == NULL && pmDebugOptions.pmc) {
	QTextStream cerr(stderr);
	cerr << "QmcSource::getSource: Unable to map host "
	     << source << " to an arch context" << Qt::endl;
    }

    return src;
}

int
QmcSource::dupContext()
{
    int sts = 0;

    if (my.status < 0)
	return my.status;

    if (my.dupFlag == false && my.handles.size() == 1) {
	sts = pmUseContext(my.handles[0]);
	if (sts >= 0) {
	    sts = my.handles[0];
	    my.dupFlag = true;
	    if (pmDebugOptions.pmc) {
		QTextStream cerr(stderr);
		cerr << "QmcSource::dupContext: Using original context for "
		     << my.desc << Qt::endl;
	    }
	}
	else
	    pmprintf("%s: Error: Unable to switch to context for \"%s\": %s\n",
		     pmGetProgname(), (const char *)my.desc.toLatin1(),
		     pmErrStr(sts));
    }
    else if (my.handles.size()) {
	sts = pmUseContext(my.handles[0]);
	if (sts >= 0) {
	    sts = pmDupContext();
	    if (sts >= 0) {
		my.handles.append(sts);
		if (pmDebugOptions.pmc) {
		    QTextStream cerr(stderr);
		    cerr << "QmcSource::dupContext: " << my.desc
			 << " duplicated, handle[" << my.handles.size() - 1
			 << "] = " << sts << Qt::endl;
		}
	    }
	    else
		pmprintf("%s: Error: "
			 "Unable to duplicate context to \"%s\": %s\n",
			 pmGetProgname(), (const char *)my.desc.toLatin1(),
			 pmErrStr(sts));
	}
	else
	    pmprintf("%s: Error: Unable to switch to context for \"%s\": %s\n",
		     pmGetProgname(), (const char *)my.desc.toLatin1(),
		     pmErrStr(sts));
    }
    // No active contexts, create a new context
    else {
	sts = pmNewContext(my.type, sourceAscii());
	if (sts >= 0) {
	    my.handles.append(sts);
	    if (pmDebugOptions.pmc) {
		QTextStream cerr(stderr);
		cerr << "QmcSource::dupContext: new context to " << my.desc
		     << " created, handle = " << sts << Qt::endl;
	    }
	}
    }

    if (sts < 0 && pmDebugOptions.pmc) {
	QTextStream cerr(stderr);
	cerr << "QmcSource::dupContext: context to " << my.desc
	     << " failed: " << pmErrStr(my.status) << Qt::endl;
    }

    return sts;
}

int
QmcSource::delContext(int handle)
{
    int i;
    int sts;

    for (i = 0; i < my.handles.size(); i++)
	if (my.handles[i] == handle)
	    break;

    if (i == my.handles.size()) {
	if (pmDebugOptions.pmc) {
	    QTextStream cerr(stderr);
	    cerr << "QmcSource::delContext: Attempt to delete " << handle
		 << " from list for " << my.desc << ", but it is not listed"
		 << Qt::endl;
	}
	return PM_ERR_NOCONTEXT;
    }

    sts = pmDestroyContext(my.handles[i]);
    my.handles.removeAt(i);

    // If this is a valid source, but no more contexts remain,
    // then we should delete ourselves
    if (my.handles.size() == 0 && my.status >= 0) {
	if (pmDebugOptions.pmc) {
	    QTextStream cerr(stderr);
	    cerr << "QmcSource::delContext: No contexts remain, removing "
		 << my.desc << Qt::endl;
	}
	delete this;
    }

    return sts;
}

QTextStream&
operator<<(QTextStream &stream, const QmcSource &rhs)
{
    stream << rhs.my.desc;
    return stream;
}

void
QmcSource::dump(QTextStream &stream)
{
    stream << "  sts = " << my.status << ", type = " << my.type
	   << ", source = " << my.source << Qt::endl
	   << "  host = " << my.host << ", timezone = " << my.timezone
	   << ", tz hndl = " << my.tz << Qt::endl;
    if (my.status >= 0)
	stream << "  start = " << timeString(&my.start) << ", end = "
	       << timeString(&my.end) << ", dupFlag = "
	       << (my.dupFlag == true ? "true" : "false") << Qt::endl << "  " 
	       << my.handles.size() << " contexts: ";
    for (int i = 0; i < my.handles.size(); i++)
	stream << my.handles[i] << ' ';
    stream << Qt::endl;
}

void
QmcSource::dumpList(QTextStream &stream)
{
    stream << sourceList.size() << " sources:" << Qt::endl;
    for (int i = 0; i < sourceList.size(); i++) {
	stream << '[' << i << "] " << *(sourceList[i]) << Qt::endl;
	sourceList[i]->dump(stream);
    }
}
