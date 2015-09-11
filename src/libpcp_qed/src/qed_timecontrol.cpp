/*
 * Copyright (c) 2014, Red Hat.
 * Copyright (c) 2006-2007, Aconex.  All Rights Reserved.
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

#include <QtGui/QMessageBox>
#include <QtGui/QApplication>
#include <QtNetwork/QHostAddress>
#include <pcp/pmapi.h>
#include <pcp/impl.h>

#include "qed_timecontrol.h"
#include "qed_console.h"
#include "qed_app.h"

QedTimeControl::QedTimeControl() : QProcess(NULL)
{
    my.tcpPort = -1;
    my.tzLength = 0;
    my.tzData = NULL;
    my.bufferLength = sizeof(QmcTime::Packet);

    console->post("TimeControl::TimeControl: created");
    my.buffer = (char *)malloc(my.bufferLength);
    my.livePacket = (QmcTime::Packet *)malloc(sizeof(QmcTime::Packet));
    my.archivePacket = (QmcTime::Packet *)malloc(sizeof(QmcTime::Packet));
    if (!my.buffer || !my.livePacket || !my.archivePacket)
	QedApp::nomem();
    my.livePacket->magic = QmcTime::Magic;
    my.livePacket->source = QmcTime::HostSource;
    my.liveState = QedTimeControl::Disconnected;
    my.liveSocket = new QTcpSocket(this);
    connect(my.liveSocket, SIGNAL(connected()),
				SLOT(liveSocketConnected()));
    connect(my.liveSocket, SIGNAL(disconnected()),
				SLOT(liveCloseConnection()));
    connect(my.liveSocket, SIGNAL(readyRead()),
				SLOT(liveProtocolMessage()));

    my.archivePacket->magic = QmcTime::Magic;
    my.archivePacket->source = QmcTime::ArchiveSource;
    my.archiveState = QedTimeControl::Disconnected;
    my.archiveSocket = new QTcpSocket(this);
    connect(my.archiveSocket, SIGNAL(connected()),
				SLOT(archiveSocketConnected()));
    connect(my.archiveSocket, SIGNAL(disconnected()),
				SLOT(archiveCloseConnection()));
    connect(my.archiveSocket, SIGNAL(readyRead()),
				SLOT(archiveProtocolMessage()));
}

void QedTimeControl::quit()
{
    disconnect(this, SIGNAL(finished(int, QProcess::ExitStatus)), this,
		    SLOT(endTimeControl()));
    if (my.liveSocket) {
	my.liveSocket->close();
	my.liveSocket = NULL;
    }
    if (my.archiveSocket) {
	my.archiveSocket->close();
	my.archiveSocket = NULL;
    }
    terminate();
}

void QedTimeControl::init(int port, bool live,
		struct timeval *interval, struct timeval *position,
		struct timeval *starttime, struct timeval *endtime,
		QString tzstring, QString tzlabel)
{
    struct timeval now;
    int tzlen = tzstring.length(), lablen = tzlabel.length();

    my.tzLength = tzlen+1 + lablen+1;
    my.tzData = (char *)realloc(my.tzData, my.tzLength);
    if (!my.tzData)
	QedApp::nomem();

    my.livePacket->length = my.archivePacket->length =
				sizeof(QmcTime::Packet) + my.tzLength;
    my.livePacket->command = my.archivePacket->command = QmcTime::Set;
    my.livePacket->delta = my.archivePacket->delta = *interval;
    if (live) {
	my.livePacket->position = *position;
	my.livePacket->start = *starttime;
	my.livePacket->end = *endtime;
	memset(&my.archivePacket->position, 0, sizeof(struct timeval));
	memset(&my.archivePacket->start, 0, sizeof(struct timeval));
	memset(&my.archivePacket->end, 0, sizeof(struct timeval));
    } else {
	gettimeofday(&now, NULL);
	my.archivePacket->position = *position;
	my.archivePacket->start = *starttime;
	my.archivePacket->end = *endtime;
	my.livePacket->position = now;
	my.livePacket->start = now;
	my.livePacket->end = now;
    }
    strncpy(my.tzData, (const char *)tzstring.toAscii(), tzlen+1);
    strncpy(my.tzData + tzlen+1, (const char *)tzlabel.toAscii(), lablen+1);

    if (port < 0) {
	startTimeServer();
    } else {
	my.tcpPort = port;
	liveConnect();
	archiveConnect();
    }
}

void QedTimeControl::addArchive(
		struct timeval starttime, struct timeval endtime,
		QString tzstring, QString tzlabel, bool atEnd)
{
    QmcTime::Packet *message;
    int tzlen = tzstring.length(), lablen = tzlabel.length();
    int sz = sizeof(QmcTime::Packet) + tzlen + 1 + lablen + 1;

    if (my.archivePacket->position.tv_sec == 0) {	// first archive
	my.archivePacket->position = atEnd ? endtime : starttime;
	my.archivePacket->start = starttime;
	my.archivePacket->end = endtime;
    }

    if ((message = (QmcTime::Packet *)malloc(sz)) == NULL)
	QedApp::nomem();
    *message = *my.archivePacket;
    message->command = QmcTime::Bounds;
    message->length = sz;
    message->start = starttime;
    message->end = endtime;
    strncpy((char *)message->data, (const char *)tzstring.toAscii(), tzlen+1);
    strncpy((char *)message->data + tzlen+1,
				(const char *)tzlabel.toAscii(), lablen+1);
    if (my.archiveSocket->write((const char *)message, sz) < 0)
	QMessageBox::warning(0,
		QApplication::tr("Error"),
		QApplication::tr("Cannot update pmtime boundaries."),
		QApplication::tr("Quit") );
    free(message);
}

void QedTimeControl::liveConnect()
{
    console->post("Connecting to pmtime, live source");
    my.liveSocket->connectToHost(QHostAddress::LocalHost, my.tcpPort);
}

void QedTimeControl::archiveConnect()
{
    console->post("Connecting to pmtime, archive source");
    my.archiveSocket->connectToHost(QHostAddress::LocalHost, my.tcpPort);
}

void QedTimeControl::showLiveTimeControl(void)
{
    my.livePacket->command = QmcTime::GUIShow;
    my.livePacket->length = sizeof(QmcTime::Packet);
    if (my.liveSocket->write((const char *)my.livePacket,
					sizeof(QmcTime::Packet)) < 0)
	QMessageBox::warning(0,
                QApplication::tr("Error"),
                QApplication::tr("Cannot get pmtime to show itself."),
                QApplication::tr("Quit") );
}

void QedTimeControl::showArchiveTimeControl(void)
{
    my.archivePacket->command = QmcTime::GUIShow;
    my.archivePacket->length = sizeof(QmcTime::Packet);
    if (my.archiveSocket->write((const char *)my.archivePacket,
					sizeof(QmcTime::Packet)) < 0)
	QMessageBox::warning(0,
    		QApplication::tr("Error"),
    		QApplication::tr("Cannot get pmtime to show itself."),
    		QApplication::tr("Quit") );
}

void QedTimeControl::hideLiveTimeControl()
{
    my.livePacket->command = QmcTime::GUIHide;
    my.livePacket->length = sizeof(QmcTime::Packet);
    if (my.liveSocket->write((const char *)my.livePacket,
					sizeof(QmcTime::Packet)) < 0)
	QMessageBox::warning(0,
		QApplication::tr("Error"),
		QApplication::tr("Cannot get pmtime to hide itself."),
		QApplication::tr("Quit") );
}

void QedTimeControl::hideArchiveTimeControl()
{
    my.archivePacket->command = QmcTime::GUIHide;
    my.archivePacket->length = sizeof(QmcTime::Packet);
    if (my.archiveSocket->write((const char *)my.archivePacket,
					sizeof(QmcTime::Packet)) < 0)
	QMessageBox::warning(0,
		QApplication::tr("Error"),
		QApplication::tr("Cannot get pmtime to hide itself."),
		QApplication::tr("Quit") );
}

void QedTimeControl::endTimeControl(void)
{
    QMessageBox::warning(0,
		QApplication::tr("Error"),
		QApplication::tr("Time Control process pmtime has exited."),
		QApplication::tr("Quit") );
    exit(-1);
}

void QedTimeControl::liveCloseConnection()
{
    my.liveSocket->close();
    my.liveSocket = NULL;
    emit done();
    exit(0);
}

void QedTimeControl::archiveCloseConnection()
{
    my.archiveSocket->close();
    my.archiveSocket = NULL;
    emit done();
    exit(0);
}

void QedTimeControl::liveSocketConnected()
{
    if (my.liveSocket->write((const char *)my.livePacket,
				sizeof(QmcTime::Packet)) < 0) {
	QMessageBox::critical(0,
		QApplication::tr("Fatal error"),
		QApplication::tr(
			"Failed socket write in live pmtime negotiation."),
		QApplication::tr("Quit") );
	exit(1);
    }
    if (my.liveSocket->write((const char *)my.tzData, my.tzLength) < 0) {
	QMessageBox::critical(0,
		QApplication::tr("Fatal error"),
		QApplication::tr(
			"Failed to send timezone in live pmtime negotiation."),
		QApplication::tr("Quit"));
	exit(1);
    }
    my.liveState = QedTimeControl::AwaitingACK;
}

void QedTimeControl::archiveSocketConnected()
{
    if (my.archiveSocket->write((const char *)my.archivePacket,
				sizeof(QmcTime::Packet)) < 0) {
	QMessageBox::critical(0,
		QApplication::tr("Fatal error"),
		QApplication::tr(
			"Failed socket write in archive pmtime negotiation."),
		QApplication::tr("Quit") );
	exit(1);
    }
    if (my.archiveSocket->write((const char *)my.tzData, my.tzLength) < 0) {
	QMessageBox::critical(0,
		QApplication::tr("Fatal error"),
		QApplication::tr(
			"Failed timezone send in archive pmtime negotiation."),
		QApplication::tr("Quit") );
	exit(1);
    }
    my.archiveState = QedTimeControl::AwaitingACK;
}

//
// Start a shiny new pmtime process.
// The one process serves time for all (live and archive) tabs.
// We do have to specify which form will be used first, however.
//
void QedTimeControl::startTimeServer()
{
    QStringList arguments;

    if (pmDebug & DBG_TRACE_TIMECONTROL)
	arguments << "-D" << "all";
    connect(this, SIGNAL(finished(int, QProcess::ExitStatus)), this,
		    SLOT(endTimeControl()));
    connect(this, SIGNAL(readyReadStandardOutput()), this,
		    SLOT(readPortFromStdout()));
    start("pmtime", arguments);
}

//
// When pmtime starts in "port probe" mode, port# is written to
// stdout.  We can only complete negotiation once we have that...
//
void QedTimeControl::readPortFromStdout(void)
{
    bool ok;
    QString data = readAllStandardOutput();

    my.tcpPort = data.remove("port=").toInt(&ok, 10);
    if (!ok) {
	QMessageBox::critical(0,
    	QApplication::tr("Fatal error"),
    	QApplication::tr("Bad port number from pmtime program."),
    	QApplication::tr("Quit") );
	exit(1);
    }

    liveConnect();
    archiveConnect();
}

void QedTimeControl::protocolMessage(bool live,
	QmcTime::Packet *packet, QTcpSocket *socket, QedProtocolState *state)
{
    int sts, need = sizeof(QmcTime::Packet), offset = 0;
    QmcTime::Packet *msg;

    // Read one pmtime packet, handling both small reads and large packets
    for (;;) {
	sts = socket->read(my.buffer + offset, need);
	if (sts < 0) {
	    QMessageBox::critical(0,
		QApplication::tr("Fatal error"),
		QApplication::tr("Failed socket read in pmtime transfer."),
		QApplication::tr("Quit") );
	    exit(1);
	}
	else if (sts != need) {
	    need -= sts;
	    offset += sts;
	    continue;
	}

	msg = (QmcTime::Packet *)my.buffer;
	if (msg->magic != QmcTime::Magic) {
	    QMessageBox::critical(0,
		QApplication::tr("Fatal error"),
		QApplication::tr("Bad client message magic number."),
		QApplication::tr("Quit") );
	    exit(1);
	}
	if (msg->length > my.bufferLength) {
	    my.bufferLength = msg->length;
	    my.buffer = (char *)realloc(my.buffer, my.bufferLength);
	    if (!my.buffer)
		QedApp::nomem();
	    msg = (QmcTime::Packet *)my.buffer;
	}
	if (msg->length > (uint)offset + sts) {
	    offset += sts;
	    need = msg->length - offset;
	    continue;
	}
	break;
    }

#if DESPERATE
    console->post(QedConsole::DebugProtocol,
		  "QedTimeControl::protocolMessage: recv pos=%s state=%d",
		  timeString(tosec(packet->position)), *state);
#endif

    switch (*state) {
    case QedTimeControl::AwaitingACK:
	if (msg->command != QmcTime::ACK) {
	    QMessageBox::critical(0,
		QApplication::tr("Fatal error"),
		QApplication::tr("Initial ACK not received from pmtime."),
		QApplication::tr("Quit") );
	    exit(1);
	}
	if (msg->source != packet->source) {
	    QMessageBox::critical(0,
		QApplication::tr("Fatal error"),
		QApplication::tr("pmtime not serving same metric source."),
		QApplication::tr("Quit") );
	    exit(1);
	}
	*state = QedTimeControl::ClientReady;
	if (msg->length > packet->length) {
	    packet = (QmcTime::Packet *)realloc(packet, msg->length);
	    if (!packet)
		QedApp::nomem();
	}
	//
	// Note: we drive the local state from the time control values,
	// and _not_ from the values that we initially sent to it.
	//
	memcpy(packet, msg, msg->length);
	emit VCRMode(live, msg, true);
	break;

    case QedTimeControl::ClientReady:
	if (msg->command == QmcTime::Step) {
	    emit step(live, msg);
	    msg->command = QmcTime::ACK;
	    msg->length = sizeof(QmcTime::Packet);
	    sts = socket->write((const char *)msg, msg->length);
	    if (sts < 0 || sts != (int)msg->length) {
		QMessageBox::critical(0,
			QApplication::tr("Fatal error"),
			QApplication::tr("Failed pmtime write for STEP ACK."),
			QApplication::tr("Quit") );
		exit(1);
	    }
	} else if (msg->command == QmcTime::VCRMode ||
		   msg->command == QmcTime::VCRModeDrag ||
		   msg->command == QmcTime::Bounds) {
	    emit VCRMode(live, msg, msg->command == QmcTime::VCRModeDrag);
	} else if (msg->command == QmcTime::TZ) {
	    emit timeZone(live, msg, (char *)msg->data);
	}
	break;

    default:
	QMessageBox::critical(0,
		QApplication::tr("Fatal error"),
		QApplication::tr("Protocol error with pmtime."),
		QApplication::tr("Quit") );
	// fall through
    case QedTimeControl::Disconnected:
	exit(1);
    }
}

void QedTimeControl::protocolMessageLoop(bool live,
	QmcTime::Packet *packet, QTcpSocket *socket, QedProtocolState *state)
{
    do {
	protocolMessage(live, packet, socket, state);
    } while (socket->bytesAvailable() >= (int)sizeof(QmcTime::Packet));
}
