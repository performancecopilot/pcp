/*
 * Copyright (c) 2006-2007, Nathan Scott.  All Rights Reserved.
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
 * Contact information: Nathan Scott, nathans At debian DoT org
 */

#define PCP_DEBUG 1

#include "main.h"

TimeControl::TimeControl(QObject *parent, const char *name)
    : QProcess(parent, name)
{
    _port = -1;
    _tzlength = 0;
    _tzdata = NULL;

    _livekmtime = (kmTime *)malloc(sizeof(kmTime));
    _archivekmtime = (kmTime *)malloc(sizeof(kmTime));
    if (!_livekmtime || !_archivekmtime) {
	QMessageBox::critical(0,
		QApplication::tr("Fatal error"),
		QApplication::tr("Cannot allocate kmtime message space."),
		QApplication::tr("Quit") );
	exit(1);
    }

    _livekmtime->magic = KMTIME_MAGIC;
    _livekmtime->source = KM_SOURCE_HOST;
    _livestate = DISCONNECTED_STATE;
    _livesocket = new QSocket(this);
    connect(_livesocket, SIGNAL(connected()),
    	SLOT(liveSocketConnected()));
    connect(_livesocket, SIGNAL(connectionClosed()),
    	SLOT(socketConnectionClosed()));
    connect(_livesocket, SIGNAL(readyRead()),
    	SLOT(liveProtocolMessage()));

    _archivekmtime->magic = KMTIME_MAGIC;
    _archivekmtime->source = KM_SOURCE_ARCHIVE;
    _archivestate = DISCONNECTED_STATE;
    _archivesocket = new QSocket(this);
    connect(_archivesocket, SIGNAL(connected()),
    	SLOT(archiveSocketConnected()));
    connect(_archivesocket, SIGNAL(connectionClosed()),
    	SLOT(socketConnectionClosed()));
    connect(_archivesocket, SIGNAL(readyRead()),
    	SLOT(archiveProtocolMessage()));
}

void TimeControl::init(int port, bool livemode,
		struct timeval *interval, struct timeval *position,
		struct timeval *starttime, struct timeval *endtime,
		char *tzstring, int tzlen, char *tzlabel, int lablen)
{
    struct timeval	now;

    _tzlength = tzlen+1 + lablen+1;
    _tzdata = (char *)realloc(_tzdata, _tzlength);

    _livekmtime->length = _archivekmtime->length = sizeof(kmTime) + _tzlength;
    _livekmtime->command = _archivekmtime->command = KM_TCTL_SET;
    _livekmtime->delta = _archivekmtime->delta = *interval;
    if (livemode) {
	_livekmtime->position = *position;
	_livekmtime->start = *starttime;
	_livekmtime->end = *endtime;
	memset(&_archivekmtime->position, 0, sizeof(_archivekmtime->position));
	memset(&_archivekmtime->start, 0, sizeof(_archivekmtime->start));
	memset(&_archivekmtime->end, 0, sizeof(_archivekmtime->end));
    } else {
	gettimeofday(&now, NULL);
	_archivekmtime->position = *position;
fprintf(stderr, "%s: ARCH position=%s\n", __func__, timestring(tosec(_archivekmtime->position)));
	_archivekmtime->start = *starttime;
	_archivekmtime->end = *endtime;
	_livekmtime->position = now;
	_livekmtime->start = now;
	_livekmtime->end = now;
    }
    strncpy(_tzdata, tzstring, tzlen + 1);
    strncpy(_tzdata + tzlen + 1, tzlabel, lablen + 1);

    if (port < 0) {
	startTimeServer();
    } else {
	_port = port;
	liveConnect();
	archiveConnect();
    }
}

void TimeControl::addArchive(
		struct timeval *starttime, struct timeval *endtime,
		char *tzstring, int tzlen, char *tzlabel, int lablen)
{
    kmTime	*message;
    int		sz = sizeof(*message) + tzlen + 1 + lablen + 1;

    message = (kmTime *)malloc(sz);
    *message = *_archivekmtime;
    message->command = KM_TCTL_BOUNDS;
    message->length = sz;
    message->start = *starttime;
    message->end = *endtime;
    strncpy(message->data, tzstring, tzlen + 1);
    strncpy(message->data + tzlen + 1, tzlabel, lablen + 1);
    if (_archivesocket->writeBlock((char *)message, sz) < 0)
    	QMessageBox::warning(0,
    		QApplication::tr("Error"),
    		QApplication::tr("Cannot update kmtime boundaries."),
    		QApplication::tr("Quit") );
    free(message);
}

void TimeControl::showLiveTimeControl(void)
{
    _livekmtime->command = KM_TCTL_GUISHOW;
    _livekmtime->length = sizeof(kmTime);
    if (_livesocket->writeBlock((char *)_livekmtime, sizeof(kmTime)) < 0)
        QMessageBox::warning(0,
                QApplication::tr("Error"),
                QApplication::tr("Cannot get kmtime to show itself."),
                QApplication::tr("Quit") );
}

void TimeControl::showArchiveTimeControl(void)
{
    _archivekmtime->command = KM_TCTL_GUISHOW;
    _archivekmtime->length = sizeof(kmTime);
    if (_archivesocket->writeBlock((char *)_archivekmtime, sizeof(kmTime)) < 0)
    	QMessageBox::warning(0,
    		QApplication::tr("Error"),
    		QApplication::tr("Cannot get kmtime to show itself."),
    		QApplication::tr("Quit") );
}

void TimeControl::hideLiveTimeControl()
{
    _livekmtime->command = KM_TCTL_GUIHIDE;
    _livekmtime->length = sizeof(kmTime);
    if (_livesocket->writeBlock((char *)_livekmtime, sizeof(kmTime)) < 0)
    	QMessageBox::warning(0,
    		QApplication::tr("Error"),
    		QApplication::tr("Cannot get kmtime to hide itself."),
    		QApplication::tr("Quit") );
}

void TimeControl::hideArchiveTimeControl()
{
    _archivekmtime->command = KM_TCTL_GUIHIDE;
    _archivekmtime->length = sizeof(kmTime);
    if (_archivesocket->writeBlock((char *)_archivekmtime, sizeof(kmTime)) < 0)
    	QMessageBox::warning(0,
    		QApplication::tr("Error"),
    		QApplication::tr("Cannot get kmtime to hide itself."),
    		QApplication::tr("Quit") );
}

void TimeControl::styleTimeControl(char *style)
{
    int		sz = sizeof(kmTime) + strlen(style) + 1;
    kmTime	*message = (kmTime *)calloc(1, sz);

    if (message == NULL)	// *shrug* - cosmetic change, not really fatal
	return;
    message->magic = KMTIME_MAGIC;
    message->source = KM_SOURCE_HOST;
    message->command = KM_TCTL_GUISTYLE;
    message->length = sz;
    strcpy(message->data, style);
    if (_livesocket->writeBlock((char *)message, sz) < 0)
    	QMessageBox::warning(0,
    		QApplication::tr("Error"),
    		QApplication::tr("Cannot get kmtime to change style."),
    		QApplication::tr("Quit") );
    free(message);
}

void TimeControl::endTimeControl(void)
{
    QMessageBox::warning(0,
    		QApplication::tr("Error"),
    		QApplication::tr("Time Control process kmtime has exited."),
    		QApplication::tr("Quit") );
    exit(-1);
}

void TimeControl::liveCloseConnection()
{
    _livesocket->close();
    if (_livesocket->state() == QSocket::Closing) {
	connect(_livesocket, SIGNAL(delayedCloseFinished()),
    			 SLOT(socketClosed()));
    } else { // The socket is closed.
	socketClosed();
    }
}

void TimeControl::archiveCloseConnection()
{
    _archivesocket->close();
    if (_archivesocket->state() == QSocket::Closing) {
	connect(_archivesocket, SIGNAL(delayedCloseFinished()),
    			 SLOT(socketClosed()));
    } else { // The socket is closed.
	socketClosed();
    }
}

void TimeControl::liveSocketConnected()
{
    int sts;

    sts = _livesocket->writeBlock((char *)_livekmtime, sizeof(kmTime));
    if (sts < 0) {
	QMessageBox::critical(0,
    	QApplication::tr("Fatal error"),
    	QApplication::tr("Failed socket write in kmtime negotiation."),
    	QApplication::tr("Quit") );
	exit(1);
    }
    sts = _livesocket->writeBlock((char *)_tzdata, _tzlength);
    if (sts < 0) {
	QMessageBox::critical(0,
    	QApplication::tr("Fatal error"),
    	QApplication::tr("Failed tzdata write in kmtime negotiation."),
    	QApplication::tr("Quit") );
	exit(1);
    }
    _livestate = AWAITING_ACK_STATE;
}

void TimeControl::archiveSocketConnected()
{
    int sts;

    sts = _archivesocket->writeBlock((char *)_archivekmtime, sizeof(kmTime));
    if (sts < 0) {
	QMessageBox::critical(0,
    	QApplication::tr("Fatal error"),
    	QApplication::tr("Failed socket write in kmtime negotiation."),
    	QApplication::tr("Quit") );
	exit(1);
    }
    sts = _archivesocket->writeBlock((char *)_tzdata, _tzlength);
    if (sts < 0) {
	QMessageBox::critical(0,
    	QApplication::tr("Fatal error"),
    	QApplication::tr("Failed tzdata write in kmtime negotiation."),
    	QApplication::tr("Quit") );
	exit(1);
    }
    _archivestate = AWAITING_ACK_STATE;
}

//
// Start a shiny new kmtime process.
// The one process serves time for all (live and archive) tabs.
// We do have to specify which form will be used first, however.
//
void TimeControl::startTimeServer()
{
    addArgument("kmtime");
    if (settings.style != settings.defaultStyle) {
	addArgument("-style");
	addArgument(settings.styleName);
    }
    if (pmDebug & DBG_TRACE_TIMECONTROL) {
	addArgument("-D");
	addArgument("all");
    }

    connect(this, SIGNAL(processExited()), this, SLOT(endTimeControl()));
    connect(this, SIGNAL(readyReadStdout()), this, SLOT(readPortFromStdout()));
    if (!start()) {
	QMessageBox::critical(0,
		QApplication::tr("Fatal error"),
		QApplication::tr("Could not start time controls."),
		QApplication::tr("Quit") );
	exit(1);
    }
}

//
// When kmtime starts in "port probe" mode, port# is written to
// stdout.  We can only complete negotiation once we have that...
//
void TimeControl::readPortFromStdout(void)
{
    bool ok;

    _port = readLineStdout().remove("port=").toInt(&ok, 10);
    if (!ok) {
	QMessageBox::critical(0,
    	QApplication::tr("Fatal error"),
    	QApplication::tr("Bad port number from kmtime program."),
    	QApplication::tr("Quit") );
	exit(1);
    }

fprintf(stderr, "%s: Connecting to kmtime live\n", __FUNCTION__);
    liveConnect();
fprintf(stderr, "%s: Connecting to kmtime archive\n", __FUNCTION__);
    archiveConnect();
}

void TimeControl::protocolMessage(bool livemode, kmTime *_kmtime,
			QSocket *_socket, ProtocolState *_protostate)
{
    int sts;
    kmTime *msg;
    char buffer[8192];	// ick, but simple (TODO: use 2 reads)

    sts = _socket->readBlock(buffer, sizeof(buffer));
    if (sts < 0) {
	QMessageBox::critical(0,
		QApplication::tr("Fatal error"),
		QApplication::tr("Failed socket read in kmtime negotiation."),
		QApplication::tr("Quit") );
	exit(1);
    }
    msg = (kmTime *)buffer;
    if (msg->magic != KMTIME_MAGIC) {
	QMessageBox::critical(0,
		QApplication::tr("Fatal error"),
		QApplication::tr("Bad client message magic number."),
		QApplication::tr("Quit") );
	exit(1);
    }
    switch (*_protostate) {
    case AWAITING_ACK_STATE:
fprintf(stderr, "%s: ARCH1 position=%s\n", __func__, timestring(tosec(_kmtime->position)));
	if (msg->command != KM_TCTL_ACK) {
	    QMessageBox::critical(0,
		QApplication::tr("Fatal error"),
		QApplication::tr("Initial ACK not received from kmtime."),
		QApplication::tr("Quit") );
	    exit(1);
	}
	if (msg->source != _kmtime->source) {
	    QMessageBox::critical(0,
		QApplication::tr("Fatal error"),
		QApplication::tr("kmtime not serving same metric source."),
		QApplication::tr("Quit") );
	    exit(1);
	}
	*_protostate = CLIENT_READY_STATE;
	if (msg->length > _kmtime->length) {
	    _kmtime = (kmTime *)realloc(_kmtime, msg->length);
	    if (!_kmtime) {
		QMessageBox::critical(0,
		    QApplication::tr("Fatal error"),
		    QApplication::tr("Cannot allocate local message area."),
		    QApplication::tr("Quit") );
		exit(1);
	    }
	}
	// Note: drive local state from the time control values,
	// not from the values that we initially sent to it.
	memcpy(_kmtime, msg, msg->length);
fprintf(stderr, "%s: ARCH2 position=%s\n", __func__, timestring(tosec(_kmtime->position)));
	kmchart->vcrmode(livemode, msg, true);
	break;

    case CLIENT_READY_STATE:
	if (msg->command == KM_TCTL_STEP) {
	    kmchart->step(livemode, msg);
	    msg->command = KM_TCTL_ACK;
	    msg->length = sizeof(kmTime);
	    sts = _socket->writeBlock((char *)msg, msg->length);
	    if (sts < 0 || sts != (int)msg->length) {
		QMessageBox::critical(0,
			QApplication::tr("Fatal error"),
			QApplication::tr("Failed kmtime write for STEP ACK."),
			QApplication::tr("Quit") );
		exit(1);
	    }
	} else if (msg->command == KM_TCTL_VCRMODE ||
	    msg->command == KM_TCTL_VCRMODE_DRAG) {
	    kmchart->vcrmode(livemode, msg, msg->command != KM_TCTL_VCRMODE);
	} else if (msg->command == KM_TCTL_TZ) {
	    kmchart->timezone(livemode, msg->data);
	} else if (msg->command == KM_TCTL_GUISTYLE) {
	    kmchart->setStyle(msg->data);
	}
	break;

    case DISCONNECTED_STATE:
    default:
	QMessageBox::critical(0,
		QApplication::tr("Fatal error"),
		QApplication::tr("Protocol error with kmtime."),
		QApplication::tr("Quit") );
	exit(1);
    }
}
