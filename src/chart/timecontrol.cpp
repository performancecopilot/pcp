/*
 * Copyright (c) 2006-2007, Aconex.  All Rights Reserved.
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
 */
#include "main.h"
#include <kmtime.h>
#include "timecontrol.h"

#include <QtGui/QMessageBox>
#include <QtGui/QApplication>
#include <QtNetwork/QHostAddress>

TimeControl::TimeControl() : QProcess(NULL)
{
    my.tcpPort = -1;
    my.tzLength = 0;
    my.tzData = NULL;

    my.livePacket = (KmTime::Packet *)malloc(sizeof(KmTime::Packet));
    my.archivePacket = (KmTime::Packet *)malloc(sizeof(KmTime::Packet));
    if (!my.livePacket || !my.archivePacket)
	nomem();
    my.livePacket->magic = KmTime::Magic;
    my.livePacket->source = KmTime::HostSource;
    my.liveState = TimeControl::Disconnected;
    my.liveSocket = new QTcpSocket(this);
    connect(my.liveSocket, SIGNAL(connected()),
				SLOT(liveSocketConnected()));
    connect(my.liveSocket, SIGNAL(disconnected()),
				SLOT(liveCloseConnection()));
    connect(my.liveSocket, SIGNAL(readyRead()),
				SLOT(liveProtocolMessage()));

    my.archivePacket->magic = KmTime::Magic;
    my.archivePacket->source = KmTime::ArchiveSource;
    my.archiveState = TimeControl::Disconnected;
    my.archiveSocket = new QTcpSocket(this);
    connect(my.archiveSocket, SIGNAL(connected()),
				SLOT(archiveSocketConnected()));
    connect(my.archiveSocket, SIGNAL(disconnected()),
				SLOT(archiveCloseConnection()));
    connect(my.archiveSocket, SIGNAL(readyRead()),
				SLOT(archiveProtocolMessage()));
}

void TimeControl::init(int port, bool live,
		struct timeval *interval, struct timeval *position,
		struct timeval *starttime, struct timeval *endtime,
		char *tzstring, int tzlen, char *tzlabel, int lablen)
{
    struct timeval now;

    my.tzLength = tzlen+1 + lablen+1;
    my.tzData = (char *)realloc(my.tzData, my.tzLength);
    if (!my.tzData)
	nomem();

    my.livePacket->length = my.archivePacket->length =
				sizeof(KmTime::Packet) + my.tzLength;
    my.livePacket->command = my.archivePacket->command = KmTime::Set;
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
    strncpy(my.tzData, tzstring, tzlen + 1);
    strncpy(my.tzData + tzlen + 1, tzlabel, lablen + 1);

    if (port < 0) {
	startTimeServer();
    } else {
	my.tcpPort = port;
	liveConnect();
	archiveConnect();
    }
}

void TimeControl::addArchive(
		struct timeval *starttime, struct timeval *endtime,
		char *tzstring, int tzlen, char *tzlabel, int lablen)
{
    KmTime::Packet *message;
    int sz = sizeof(KmTime::Packet) + tzlen + 1 + lablen + 1;

    if ((message = (KmTime::Packet *)malloc(sz)) == NULL)
	nomem();
    *message = *my.archivePacket;
    message->command = KmTime::Bounds;
    message->length = sz;
    message->start = *starttime;
    message->end = *endtime;
    strncpy((char *)message->data, tzstring, tzlen + 1);
    strncpy((char *)message->data + tzlen + 1, tzlabel, lablen + 1);
    if (my.archiveSocket->write((const char *)message, sz) < 0)
	QMessageBox::warning(0,
		QApplication::tr("Error"),
		QApplication::tr("Cannot update kmtime boundaries."),
		QApplication::tr("Quit") );
    free(message);
}

void TimeControl::liveConnect()
{
    console->post("Connecting to kmtime, live source\n");
    my.liveSocket->connectToHost(QHostAddress::LocalHost, my.tcpPort);
}

void TimeControl::archiveConnect()
{
    console->post("Connecting to kmtime, archive source\n");
    my.archiveSocket->connectToHost(QHostAddress::LocalHost, my.tcpPort);
}

void TimeControl::showLiveTimeControl(void)
{
    my.livePacket->command = KmTime::GUIShow;
    my.livePacket->length = sizeof(KmTime::Packet);
    if (my.liveSocket->write((const char *)my.livePacket,
					sizeof(KmTime::Packet)) < 0)
	QMessageBox::warning(0,
                QApplication::tr("Error"),
                QApplication::tr("Cannot get kmtime to show itself."),
                QApplication::tr("Quit") );
}

void TimeControl::showArchiveTimeControl(void)
{
    my.archivePacket->command = KmTime::GUIShow;
    my.archivePacket->length = sizeof(KmTime::Packet);
    if (my.archiveSocket->write((const char *)my.archivePacket,
					sizeof(KmTime::Packet)) < 0)
	QMessageBox::warning(0,
    		QApplication::tr("Error"),
    		QApplication::tr("Cannot get kmtime to show itself."),
    		QApplication::tr("Quit") );
}

void TimeControl::hideLiveTimeControl()
{
    my.livePacket->command = KmTime::GUIHide;
    my.livePacket->length = sizeof(KmTime::Packet);
    if (my.liveSocket->write((const char *)my.livePacket,
					sizeof(KmTime::Packet)) < 0)
	QMessageBox::warning(0,
		QApplication::tr("Error"),
		QApplication::tr("Cannot get kmtime to hide itself."),
		QApplication::tr("Quit") );
}

void TimeControl::hideArchiveTimeControl()
{
    my.archivePacket->command = KmTime::GUIHide;
    my.archivePacket->length = sizeof(KmTime::Packet);
    if (my.archiveSocket->write((const char *)my.archivePacket,
					sizeof(KmTime::Packet)) < 0)
	QMessageBox::warning(0,
		QApplication::tr("Error"),
		QApplication::tr("Cannot get kmtime to hide itself."),
		QApplication::tr("Quit") );
}

void TimeControl::styleTimeControl(char *style)
{
    int sz = sizeof(KmTime::Packet) + strlen(style) + 1;
    KmTime::Packet *message = (KmTime::Packet *)calloc(1, sz);

    if (!message)
	nomem();
    message->magic = KmTime::Magic;
    message->source = KmTime::HostSource;
    message->command = KmTime::GUIStyle;
    message->length = sz;
    strcpy((char *)message->data, style);
    if (my.liveSocket->write((const char *)message, sz) < 0)
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
    my.liveSocket->close();
}

void TimeControl::archiveCloseConnection()
{
    my.archiveSocket->close();
}

void TimeControl::liveSocketConnected()
{
    if (my.liveSocket->write((const char *)my.livePacket,
				sizeof(KmTime::Packet)) < 0) {
	QMessageBox::critical(0,
		QApplication::tr("Fatal error"),
		QApplication::tr(
			"Failed socket write in live kmtime negotiation."),
		QApplication::tr("Quit") );
	exit(1);
    }
    if (my.liveSocket->write((const char *)my.tzData, my.tzLength) < 0) {
	QMessageBox::critical(0,
		QApplication::tr("Fatal error"),
		QApplication::tr(
			"Failed to send timezone in live kmtime negotiation."),
		QApplication::tr("Quit"));
	exit(1);
    }
    my.liveState = TimeControl::AwaitingACK;
}

void TimeControl::archiveSocketConnected()
{
    if (my.archiveSocket->write((const char *)my.archivePacket,
				sizeof(KmTime::Packet)) < 0) {
	QMessageBox::critical(0,
		QApplication::tr("Fatal error"),
		QApplication::tr(
			"Failed socket write in archive kmtime negotiation."),
		QApplication::tr("Quit") );
	exit(1);
    }
    if (my.archiveSocket->write((const char *)my.tzData, my.tzLength) < 0) {
	QMessageBox::critical(0,
		QApplication::tr("Fatal error"),
		QApplication::tr(
			"Failed timezone send in archive kmtime negotiation."),
		QApplication::tr("Quit") );
	exit(1);
    }
    my.archiveState = TimeControl::AwaitingACK;
}

//
// Start a shiny new kmtime process.
// The one process serves time for all (live and archive) tabs.
// We do have to specify which form will be used first, however.
//
void TimeControl::startTimeServer()
{
    QStringList arguments;

    if (globalSettings.style != globalSettings.defaultStyle)
	arguments << "-style" << globalSettings.styleName;
    if (pmDebug & DBG_TRACE_TIMECONTROL)
	arguments << "-D" << "all";
    connect(this, SIGNAL(finished(int, QProcess::ExitStatus)), this,
		    SLOT(endTimeControl()));
    connect(this, SIGNAL(readyReadStandardOutput()), this,
		    SLOT(readPortFromStdout()));
    start("kmtime", arguments);
}

//
// When kmtime starts in "port probe" mode, port# is written to
// stdout.  We can only complete negotiation once we have that...
//
void TimeControl::readPortFromStdout(void)
{
    bool ok;
    QString data = readAllStandardOutput();

    my.tcpPort = data.remove("port=").toInt(&ok, 10);
    if (!ok) {
	QMessageBox::critical(0,
    	QApplication::tr("Fatal error"),
    	QApplication::tr("Bad port number from kmtime program."),
    	QApplication::tr("Quit") );
	exit(1);
    }

    liveConnect();
    archiveConnect();
}

void TimeControl::protocolMessage(bool live,
	KmTime::Packet *packet, QTcpSocket *socket, ProtocolState *state)
{
    int sts;
    KmTime::Packet *msg;
    char buffer[8192];	// ick, but simple (TODO: use 2 reads)

    sts = socket->read(buffer, sizeof(buffer));
    if (sts < 0) {
	QMessageBox::critical(0,
		QApplication::tr("Fatal error"),
		QApplication::tr("Failed socket read in kmtime negotiation."),
		QApplication::tr("Quit") );
	exit(1);
    }
    msg = (KmTime::Packet *)buffer;
    if (msg->magic != KmTime::Magic) {
	QMessageBox::critical(0,
		QApplication::tr("Fatal error"),
		QApplication::tr("Bad client message magic number."),
		QApplication::tr("Quit") );
	exit(1);
    }
    switch (*state) {
    case TimeControl::AwaitingACK:
	if (!live)
	    console->post("TimeControl::protocolMessage: sent arch pos=%s\n",
			timeString(tosec(packet->position)));
	if (msg->command != KmTime::ACK) {
	    QMessageBox::critical(0,
		QApplication::tr("Fatal error"),
		QApplication::tr("Initial ACK not received from kmtime."),
		QApplication::tr("Quit") );
	    exit(1);
	}
	if (msg->source != packet->source) {
	    QMessageBox::critical(0,
		QApplication::tr("Fatal error"),
		QApplication::tr("kmtime not serving same metric source."),
		QApplication::tr("Quit") );
	    exit(1);
	}
	*state = TimeControl::ClientReady;
	if (msg->length > packet->length) {
	    packet = (KmTime::Packet *)realloc(packet, msg->length);
	    if (!packet)
		nomem();
	}
	//
	// Note: we drive the local state from the time control values,
	// and _not_ from the values that we initially sent to it.
	//
	memcpy(packet, msg, msg->length);
	if (!live)
	    console->post("TimeControl::protocolMessage: recv arch pos=%s\n",
			timeString(tosec(packet->position)));
	kmchart->VCRMode(live, msg, true);
	break;

    case TimeControl::ClientReady:
	if (msg->command == KmTime::Step) {
	    kmchart->step(live, msg);
	    msg->command = KmTime::ACK;
	    msg->length = sizeof(KmTime::Packet);
	    sts = socket->write((const char *)msg, msg->length);
	    if (sts < 0 || sts != (int)msg->length) {
		QMessageBox::critical(0,
			QApplication::tr("Fatal error"),
			QApplication::tr("Failed kmtime write for STEP ACK."),
			QApplication::tr("Quit") );
		exit(1);
	    }
	} else if (msg->command == KmTime::VCRMode ||
		   msg->command == KmTime::VCRModeDrag) {
	    kmchart->VCRMode(live, msg, msg->command != KmTime::VCRMode);
	} else if (msg->command == KmTime::TZ) {
	    kmchart->timeZone(live, (char *)msg->data);
	} else if (msg->command == KmTime::GUIStyle) {
	    kmchart->setStyle((char *)msg->data);
	}
	break;

    default:
	QMessageBox::critical(0,
		QApplication::tr("Fatal error"),
		QApplication::tr("Protocol error with kmtime."),
		QApplication::tr("Quit") );
	// fall through
    case TimeControl::Disconnected:
	exit(1);
    }
}
