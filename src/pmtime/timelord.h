/*
 * Copyright (c) 2007, Aconex.  All Rights Reserved.
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
#ifndef TIMELORD_H
#define TIMELORD_H

#include <QVariant>
#include <QTimer>
#include <QList>
#include <QTextStream>
#include <QTcpSocket>
#include <QTcpServer>
#include <QApplication>

#include "console.h"
#include "pmtimelive.h"
#include "pmtimearch.h"

// The TimeClient class provides a socket that is connected with a client.
// For every client that connects to the server, the server creates a new
// instance of this class.
//
class TimeClient : public QObject
{
    Q_OBJECT

public:
    // State transitions for a pmtime client.  Basic SET/ACK protocol is:
    // - client connects and sends initial (global) SET
    // - server responds with ACK (beware live mode, with timers SETs here)
    // - server now sends SETs with optional ACKs, until first ACK recv'd
    // - after first ACK recv'd by server, all subsequent SETs must be ACK'd.
    // The other messages can be sent/recv'd any time after the server has
    // ACK'd the initial connection.
    //
    typedef enum {
	Disconnected = 1,
	ClientConnectSET,
	ServerConnectACK,
	ServerNeedACK,
	ClientReady,
    } State;

public:
    TimeClient(QTcpSocket *socket, QObject *parent);
    ~TimeClient();
    void reset();

    void setContext(PmTimeArch *ac, PmTimeLive *hc) { my.ac = ac; my.hc = hc; }
    bool writeClient(PmTime::Packet *k, char *tz = NULL, int tzlen = 0,
				char *label = NULL, int llen = 0);

signals:
    void endConnect(TimeClient *);

public slots:
    void readClient();
    void disconnectClient();

private:
    struct {
	QTcpSocket *socket;
	TimeClient::State state;
	PmTime::Source source;
	struct timeval acktime;	// time position @ last STEP
	PmTimeLive *hc;
	PmTimeArch *ac;
    } my;
};

// The TimeLord class is a QTcpServer which randomly travels the space-
// time continuim, servicing and connecting up clients with the server;
// it also hooks up logging of the action to the wide-screen console in
// the Tardis.
//
// For each client that connects it creates a new TimeClient (maintained
// in a QList) - the new instance is responsible for communication with
// that TCP client.
//
class TimeLord : public QTcpServer
{
    Q_OBJECT

public:
    TimeLord(QApplication *app);
    void setContext(PmTimeLive *live, PmTimeArch *archive);

signals:
    void lastClientExit();

public slots:
    void quit();
    void newConnection();
    void endConnect(TimeClient *client);
    void timePulse(PmTime::Packet *k);
    void boundsPulse(PmTime::Packet *k);
    void vcrModePulse(PmTime::Packet *k, int drag);
    void tzPulse(PmTime::Packet *k, char *t, int tlen, char *l, int llen);

private:
    struct {
	PmTimeLive *hc;
	PmTimeArch *ac;
	QList<TimeClient*> clientlist;
    } my;
};

#endif // TIMELORD_H
