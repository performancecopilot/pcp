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
#ifndef QED_TIMECONTROL_H
#define QED_TIMECONTROL_H

#include <QtCore/QString>
#include <QtCore/QProcess>
#include <QtNetwork/QTcpSocket>
#include <qmc_time.h>

class QedTimeControl : public QProcess
{
    Q_OBJECT

public:
    QedTimeControl();

    void init(int port, bool livemode,
	      struct timeval *interval, struct timeval *position,
	      struct timeval *starttime, struct timeval *endtime,
	      QString tzstring, QString tzlabel);
    void quit();

    void addArchive(struct timeval starttime, struct timeval endtime,
		    QString tzstring, QString tzlabel, bool atEnd);

    void liveConnect();
    void archiveConnect();

    int port() { return my.tcpPort; }
    struct timeval *liveInterval() { return &my.livePacket->delta; }
    struct timeval *livePosition() { return &my.livePacket->position; }
    struct timeval *archiveInterval() { return &my.archivePacket->delta; }
    struct timeval *archivePosition() { return &my.archivePacket->position; }
    struct timeval *archiveStart() { return &my.archivePacket->start; }
    struct timeval *archiveEnd() { return &my.archivePacket->end; }

signals:
    void done();
    void step(bool, QmcTime::Packet *);
    void VCRMode(bool, QmcTime::Packet *, bool);
    void timeZone(bool, QmcTime::Packet *, char *);

public slots:
    void showLiveTimeControl();
    void hideLiveTimeControl();
    void showArchiveTimeControl();
    void hideArchiveTimeControl();
    void endTimeControl();

    void readPortFromStdout();

    void liveCloseConnection();
    void liveSocketConnected();
    void liveProtocolMessage()
    {
	protocolMessageLoop(true, my.livePacket, my.liveSocket, &my.liveState);
    }

    void archiveCloseConnection();
    void archiveSocketConnected();
    void archiveProtocolMessage()
    {
	protocolMessageLoop(false, my.archivePacket, my.archiveSocket,
				&my.archiveState);
    }

private:
    typedef enum {
	Disconnected = 1,
	AwaitingACK = 2,
	ClientReady = 3,
    } QedProtocolState;

    void startTimeServer();
    void protocolMessage(bool live, QmcTime::Packet *pmtime,
			 QTcpSocket *socket, QedProtocolState *state);
    void protocolMessageLoop(bool live, QmcTime::Packet *pmtime,
			 QTcpSocket *socket, QedProtocolState *state);

    struct {
	int tcpPort;
	int tzLength;
	char *tzData;

	unsigned int bufferLength;
	char *buffer;

	QTcpSocket *liveSocket;
	QmcTime::Packet *livePacket;
	QedProtocolState liveState;

	QTcpSocket *archiveSocket;
	QmcTime::Packet *archivePacket;
	QedProtocolState archiveState;
    } my;
};

#endif	// QED_TIMECONTROL_H
