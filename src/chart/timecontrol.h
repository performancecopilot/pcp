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
#ifndef TIMECONTROL_H
#define TIMECONTROL_H

#include <qprocess.h>
#include <qsocket.h>
#include <qstring.h>
#include <qmessagebox.h>
#include <qapplication.h>
#include "kmtime.h"
#include "main.h"

enum ProtocolState {
    DISCONNECTED_STATE = 1,
    AWAITING_ACK_STATE,
    CLIENT_READY_STATE,
};

class TimeControl : public QProcess
{
    Q_OBJECT
public:
    TimeControl(QObject *parent = 0, const char *name = 0);

    void init(int port, bool livemode,
	      struct timeval *interval, struct timeval *position,
	      struct timeval *starttime, struct timeval *endtime,
	      char *tzstring, int tzlen, char *tzlabel, int lablen);
    void addArchive(struct timeval *starttime, struct timeval *endtime,
		    char *tzstring, int tzlen, char *tzlabel, int lablen);

    int port(void) { return _port; }
    struct timeval *liveInterval(void) { return &_livekmtime->delta; }
    struct timeval *livePosition(void) { return &_livekmtime->position; }
    struct timeval *archiveInterval(void) { return &_archivekmtime->delta; }
    struct timeval *archivePosition(void) { return &_archivekmtime->position; }
    struct timeval *archiveStart(void) { return &_archivekmtime->start; }
    struct timeval *archiveEnd(void) { return &_archivekmtime->end; }

    void liveConnect() { _livesocket->connectToHost("localhost", _port); }
    void archiveConnect() { _archivesocket->connectToHost("localhost", _port);}

public slots:
    void showLiveTimeControl(void);
    void showArchiveTimeControl(void);
    void hideLiveTimeControl(void);
    void hideArchiveTimeControl(void);
    void styleTimeControl(char *);
    void endTimeControl(void);

private slots:
    void liveCloseConnection(void);
    void liveSocketConnected(void);
    void liveProtocolMessage(void)
    {
	protocolMessage(true, _livekmtime, _livesocket, &_livestate);
    }

    void archiveCloseConnection(void);
    void archiveSocketConnected(void);
    void archiveProtocolMessage(void)
    {
	protocolMessage(false, _archivekmtime, _archivesocket, &_archivestate);
    }

    void readPortFromStdout(void);

    void socketConnectionClosed() { fprintf(stderr, "Connection closed\n"); }
    void delayedCloseFinished() { fprintf(stderr, "Delayed connect closed\n"); }
    void socketClosed() { fprintf(stderr, "Connection closed\n"); }

private:
    void startTimeServer();
    void protocolMessage(bool livemode, kmTime *_kmtime,
			 QSocket *_socket, ProtocolState *_protostate);

    ProtocolState	_livestate;
    kmTime		*_livekmtime;
    QSocket		*_livesocket;
    ProtocolState	_archivestate;
    QSocket		*_archivesocket;
    kmTime		*_archivekmtime;
    char		*_tzdata;
    int			_tzlength;
    int			_port;
    char		*_debug;
};

extern TimeControl *kmtime;

#endif	/* TIMECONTROL_H */
