/*
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
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
 * Contact information: Ken McDonell, kenj At internode DoT on DoT net
 *                      Nathan Scott, nathans At debian DoT org
 */
#ifndef TIMELORD_H
#define TIMELORD_H

#include <qsocket.h>
#include <qserversocket.h>
#include <qapplication.h>
#include <qtextstream.h>
#include <stdlib.h>
#include "main.h"
#include "console.h"
#include "kmtimelive.h"
#include "kmtimearch.h"

/*
   State transitions for a kmtime client.  Basic SET/ACK protocol is:
   - client connects and sends initial (global) SET
   - server responds with ACK (beware live mode, with timers SETs here)
   - server now sends SETs with optional ACKs, until first ACK recv'd
   - after first ACK recv'd by server, all subsequent SETs must be ACK'd.
   The other messages can be sent/recv'd any time after the server has
   ACK'd the initial connection.
 */
typedef enum {
    DISCONNECTED_STATE = 1,
    CLIENT_CONN_SET_STATE,
    SERVER_CONN_ACK_STATE,
    SERVER_NEED_ACK_STATE,
    CLIENT_READY_STATE,
} client_state;

/*
  The TimeClient class provides a socket that is connected with a client.
  For every client that connects to the server, the server creates a new
  instance of this class.
*/
class TimeClient : public QSocket
{
    Q_OBJECT
public:
    TimeClient(int sock, Console *c, QObject *parent=0, const char *name=0):
	QSocket(parent, name)
    {
	_ac = NULL;
	_hc = NULL;
	_state = DISCONNECTED_STATE;
	_source = KM_SOURCE_NONE;
	memset(&_acktime, 0, sizeof(_acktime));
	_console = c;
	_console->post(DBG_PROTO, "TimeClient initialised");
	connect(this, SIGNAL(readyRead()), SLOT(readClient()));
	connect(this, SIGNAL(connectionClosed()), SLOT(deleteLater()));
	setSocket(sock);
    }

    ~TimeClient()
    {
	_console->post(DBG_PROTO, "destroying client %p", this);
	_state = DISCONNECTED_STATE;
	_source = KM_SOURCE_NONE;
	emit endConnect(this);
    }

    char *stateString(void)
    {
	static char buffer[32];

	if (_state == DISCONNECTED_STATE)
	    strcpy(buffer, "DISCONNECTED_STATE");
	else if (_state == CLIENT_CONN_SET_STATE)
	    strcpy(buffer, "CLIENT_CONN_SET_STATE");
	else if (_state == SERVER_CONN_ACK_STATE)
	    strcpy(buffer, "SERVER_CONN_ACK_STATE");
	else if (_state == SERVER_NEED_ACK_STATE)
	    strcpy(buffer, "SERVER_NEED_ACK_STATE");
	else if (_state == CLIENT_READY_STATE)
	    strcpy(buffer, "CLIENT_READY_STATE");
	return buffer;
    }

    void setContext(KmTimeArch *ac, KmTimeLive *hc) { _ac = ac; _hc = hc; }
    void writeClient(kmTime *k, char *tz = NULL, int tzlen = 0,
				char *label = NULL, int llen = 0)
    {
	client_state newstate = _state;
	int len;

	if (k->source != _source)
	    return;

	switch (_state) {
	case DISCONNECTED_STATE:
	    return;
	case CLIENT_CONN_SET_STATE:
	    if (k->command == KM_TCTL_ACK) {
		newstate = SERVER_CONN_ACK_STATE;
		break;
	    }
	    return;
	case SERVER_CONN_ACK_STATE:
	    break;
	case CLIENT_READY_STATE:
	    if (k->command == KM_TCTL_STEP)
		newstate = SERVER_NEED_ACK_STATE;
	    break;
	case SERVER_NEED_ACK_STATE:
	    if (k->command != KM_TCTL_STEP)
		break;
	    _console->post(DBG_PROTO,
			   "%s skipped STEP pos=%u.%u, client %p in NEED_ACK",
			   __func__,
			   k->position.tv_sec, k->position.tv_usec, this);
	    return;
	}
	len = writeBlock((const char *)k, sizeof(kmTime));
	if (len != sizeof(kmTime)) {
	    _console->post(DBG_PROTO, "%s wrote %d bytes not %d (%x command)",
			   __func__, len, k->length, k->command);
	    _state = DISCONNECTED_STATE;
	    endConnect(this);
	} else {
	    _console->post(DBG_PROTO, "%s wrote %d bytes command=%x state=%u",
			   __func__, len, k->command, _state);
	}
	if (tzlen > 0 && len > 0 &&
	    (len = writeBlock(tz, tzlen)) != tzlen) {
	    _console->post(DBG_PROTO, "%s wrote %d bytes not %d (timezone)",
			   __func__, len, tzlen);
	    _state = DISCONNECTED_STATE;
	    endConnect(this);
	} else if (tzlen) {
	    _console->post(DBG_PROTO, "%s wrote %d bytes of timezone",
			   __func__, len);
	}
	if (llen > 0 && len > 0 &&
	    (len = writeBlock(label, llen)) != llen) {
	    _console->post(DBG_PROTO, "%s wrote %d bytes not %d (tz label)",
			   __func__, len, llen);
	    _state = DISCONNECTED_STATE;
	    endConnect(this);
	} else if (llen) {
	    _console->post(DBG_PROTO, "%s wrote %d bytes of tz label",
			   __func__, len);
	}
	if (newstate == SERVER_NEED_ACK_STATE)
	    _acktime = k->position;
	_state = newstate;
    }

signals:
    void endConnect(TimeClient *);

private slots:
    void readClient(void)
    {
	kmTime packet;
	char *tzdata = NULL;
	int bad = 0, len, sz;

	len = readBlock((char *)&packet, sizeof(kmTime));
	if (len < 0) {
	    _console->post(DBG_PROTO, "Read error on client %p", this);
	    bad = 1;
	} else if (packet.magic != KMTIME_MAGIC) {
	    _console->post(DBG_PROTO, "Bad magic (%x) from client %p",
				packet.magic, this);
	    bad = 1;
	} else if (len != sizeof(kmTime)) {
	    _console->post(DBG_PROTO,
				"Bad 1st read (want %d, got %d) on client %p",
				len, sizeof(kmTime), this);
	    bad = 1;
	} else if (packet.length > sizeof(kmTime)) {
	    sz = packet.length - sizeof(kmTime);
	    tzdata = (char *)malloc(sz);
	    if (tzdata == NULL) {
		_console->post(DBG_PROTO,
				"No memory (%d) for second read on client %p",
				sz, len, this);
		bad = 1;
	    } else if ((len = readBlock(tzdata, sz)) != sz) {
		_console->post(DBG_PROTO,
				"Bad 2nd read (want %d, got %d) on client %p",
				sz, len, this);
		bad = 1;
	    }
	    _console->post(DBG_PROTO, "+%d message from client %p", sz, this);
	} else
	    _console->post(DBG_PROTO, "good message from client %p", this);

	if (!bad) {
	    _console->post(DBG_PROTO, "state %s message %d",
			    stateString(), packet.command);
	    switch(_state) {
	    case DISCONNECTED_STATE:
		if (packet.command == KM_TCTL_SET)
		    _console->post(DBG_PROTO, "%s got new SET from client %p",
				    __func__, this);
		if (packet.source == KM_SOURCE_HOST) {
		    _source = KM_SOURCE_HOST;
		    _hc->setTime(&packet, tzdata);
		} else {
		    _source = KM_SOURCE_ARCHIVE;
		    _ac->setTime(&packet, tzdata);
		}
		_state = CLIENT_CONN_SET_STATE;
		packet.command = KM_TCTL_ACK;
		packet.length = sizeof(kmTime);
		writeClient(&packet);
		return;

	    case CLIENT_CONN_SET_STATE:
		_console->post(DBG_PROTO,
				"%s bad client %p command %d in CONN_SET",
				__func__, this, packet.command);
		break;

	    case SERVER_CONN_ACK_STATE:
		if (packet.command == KM_TCTL_ACK)
		    _state = CLIENT_READY_STATE;
		break;

	    case SERVER_NEED_ACK_STATE:
		if (packet.command != KM_TCTL_ACK)
		    break;
		if (packet.position.tv_sec == _acktime.tv_sec &&
		    packet.position.tv_usec == _acktime.tv_usec) {
		    _console->post(DBG_PROTO, "%s good ACK client %p (%u.%u)",
				   __func__, this,
				   _acktime.tv_sec, _acktime.tv_usec);
		    _state = CLIENT_READY_STATE;
		    break;
		}
		_console->post(DBG_PROTO,
				"%s BAD ACK client %p (got %u.%u vs %u.%u)",
				__func__, this, packet.position.tv_sec,
				packet.position.tv_usec, _acktime.tv_sec,
				_acktime.tv_usec);
		endConnect(this);
		return;

	    case CLIENT_READY_STATE:
		if (packet.command == KM_TCTL_ACK)
		    _console->post(DBG_PROTO,
				   "%s unexpected client %p ACK in READY",
				   __func__, this);
		break;
	    }

	    switch(packet.command) {
	    case KM_TCTL_GUIHIDE:
	    case KM_TCTL_GUISHOW:
		_console->post(DBG_PROTO, "%s: HIDE/SHOW from client %p",
					__func__, this);
		if (_source == KM_SOURCE_HOST)
		    _hc->popup(packet.command == KM_TCTL_GUISHOW);
		if (_source == KM_SOURCE_ARCHIVE)
		    _ac->popup(packet.command == KM_TCTL_GUISHOW);
		break;
	    case KM_TCTL_BOUNDS:
		_console->post(DBG_PROTO, "%s: BOUNDS update from client %p",
					__func__, this);
		_ac->addBound(&packet, tzdata);
		break;
	    case KM_TCTL_ACK:
		break;
	    case KM_TCTL_GUISTYLE:
		_console->post(DBG_PROTO, "%s: set STYLE from client %p",
					__func__, this);
		_console->post(DBG_PROTO, "%s: data=%s", __func__, tzdata);
		QApplication::setStyle(tr(tzdata));
		_hc->style(tzdata, this);
		break;
	    default:
		_console->post(DBG_PROTO, "%s: bad command %d from client %p",
				__func__, packet.command, this);
		endConnect(this);
	    }
	} else {
	    endConnect(this);
	}
    }

private:
    Console		*_console;
    client_state	_state;
    struct timeval	_acktime;	// time position @ last STEP
    km_tctl_source	_source;
    KmTimeLive		*_hc;
    KmTimeArch		*_ac;
};

/*
  The TimeServer class handles new connections to the kmtime server. For every
  client that connects, it creates a new TimeClient (all maintained in a linked
  list; the new instance is now responsible for communication with that client.
*/
class TimeServer : public QServerSocket
{
    Q_OBJECT
public:
    TimeServer(int port, Console *c, QObject* parent=0) :
	QServerSocket(port, 1, parent)
    {
	_ac = NULL;
	_hc = NULL;
	_console = c;
	_console->post(DBG_PROTO, "TimeServer initialised (ok=%d)", ok());
    }

    ~TimeServer() { }

    void setContext(KmTimeLive *h, KmTimeArch *a) { _hc = h; _ac = a; }

    void newConnection(int socket)
    {
	TimeClient *c = new TimeClient(socket, _console, this);
	c->setContext(_ac, _hc);
	emit newConnect(c);
    }

signals:
    void newConnect(TimeClient *);

private:
    Console	*_console;
    KmTimeLive	*_hc;
    KmTimeArch	*_ac;
};

/*
  The TimeLord class simply instantiates a TimeServer and then randomly
  travels the space time continuim, servicing and connecting up clients
  with the server; it also hooks up logging of the action to the big-
  screen console in the Tardis.
 */
class TimeLord : public QObject
{
    Q_OBJECT
public:
    TimeLord(int port, Console *console, QApplication *app)
    {
	_console = console;
	connect(this, SIGNAL(lastClientExit()), app, SLOT(quit()));
	_server = new TimeServer(port, console, this);
	connect(_server,
		SIGNAL(newConnect(TimeClient*)), SLOT(newConnect(TimeClient*)));
    }

    ~TimeLord() { }

    int ok(void) { return _server->ok(); }

    void setContext(KmTimeLive *live, KmTimeArch *archive)
    {
	_server->setContext(live, archive);
	connect(live, SIGNAL(timePulse(kmTime *)),
		SLOT(timePulse(kmTime *)));
	connect(live, SIGNAL(stylePulse(kmTime *, char *, int, void *)),
		SLOT(stylePulse(kmTime *, char *, int, void *)));
	connect(live, SIGNAL(vcrModePulse(kmTime *, int)),
		SLOT(vcrModePulse(kmTime *, int)));
	connect(live, SIGNAL(timeZonePulse(kmTime *, char *, int, char *, int)),
		SLOT(timeZonePulse(kmTime *, char *, int, char *, int)));
	connect(archive, SIGNAL(timePulse(kmTime *)),
		SLOT(timePulse(kmTime *)));
	connect(archive, SIGNAL(boundsPulse(kmTime *)),
		SLOT(boundsPulse(kmTime *)));
	connect(archive, SIGNAL(vcrModePulse(kmTime *, int)),
		SLOT(vcrModePulse(kmTime *, int)));
	connect(archive, SIGNAL(timeZonePulse(kmTime *, char *, int, char *, int)),
		SLOT(timeZonePulse(kmTime *, char *, int, char *, int)));
    }

signals:
    void lastClientExit(void);

private slots:
    void newConnect(TimeClient *client)
    {
	_console->post(DBG_PROTO, "Adding new client %p to server %p",
			client, _server);
	connect(client,
		SIGNAL(endConnect(TimeClient*)), SLOT(endConnect(TimeClient*)));
	_clientlist.append(client);
    }

    void endConnect(TimeClient *client)
    {
	_console->post(DBG_PROTO, "Removing client %p from server %p",
			client, _server);
	_clientlist.remove(client);
	if (_clientlist.isEmpty()) {
	    _console->post(DBG_PROTO, "No clients remain, exiting");
	    emit lastClientExit();
	}
    }

    void timePulse(kmTime *kmtime)
    {
	TimeClient *c;
	_console->post(DBG_PROTO, "%s (%d clients)", __func__,
			_clientlist.count());
	kmtime->magic = KMTIME_MAGIC;
	kmtime->length = sizeof(kmTime);
	kmtime->command = KM_TCTL_STEP;
	for (c = _clientlist.first(); c; c = _clientlist.next())
	    c->writeClient(kmtime);
    }

    void boundsPulse(kmTime *kmtime)
    {
	TimeClient *c;
	_console->post(DBG_PROTO, "%s (%d clients)", __func__,
			_clientlist.count());
	kmtime->magic = KMTIME_MAGIC;
	kmtime->length = sizeof(kmTime);
	kmtime->command = KM_TCTL_BOUNDS;
	for (c = _clientlist.first(); c; c = _clientlist.next())
	    c->writeClient(kmtime);
    }

    void vcrModePulse(kmTime *kmtime, int drag)
    {
	TimeClient *c;
	_console->post(DBG_PROTO, "%s - %d (%d clients)", __func__,
			drag, _clientlist.count());
	kmtime->magic = KMTIME_MAGIC;
	kmtime->length = sizeof(kmTime);
	kmtime->command = drag ? KM_TCTL_VCRMODE_DRAG : KM_TCTL_VCRMODE;
	for (c = _clientlist.first(); c; c = _clientlist.next())
	    c->writeClient(kmtime);
    }

    void timeZonePulse(kmTime *kmtime, char *tz, int tzlen, char *l, int llen)
    {
	TimeClient *c;

	_console->post(DBG_PROTO, "%s - %s/%d/%d (%d clients)", __func__,
			tz, tzlen, llen, _clientlist.count());
	kmtime->magic = KMTIME_MAGIC;
	kmtime->length = sizeof(kmTime) + tzlen + llen;
	kmtime->command = KM_TCTL_TZ;
	for (c = _clientlist.first(); c; c = _clientlist.next())
	    c->writeClient(kmtime, tz, tzlen, l, llen);
    }

    void stylePulse(kmTime *kmtime, char *style, int len, void *source)
    {
	TimeClient *c;

	_console->post(DBG_PROTO, "%s - %s (%d clients)", __func__,
			style, len, _clientlist.count() - 1);
	kmtime->magic = KMTIME_MAGIC;
	kmtime->length = sizeof(kmTime) + len;
	kmtime->command = KM_TCTL_GUISTYLE;
	for (c = _clientlist.first(); c; c = _clientlist.next())
	    if (c != (TimeClient *)source)
		c->writeClient(kmtime, style, len);
    }

private:
    Console		*_console;
    TimeServer		*_server;
    QPtrList<TimeClient> _clientlist;
};

#endif
