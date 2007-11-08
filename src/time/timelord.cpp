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
#include "timelord.h"
#include <stdlib.h>

TimeClient::TimeClient(QTcpSocket *s, QObject *p) : QObject(p)
{
    my.ac = NULL;
    my.hc = NULL;
    my.socket = s;
    my.state = TimeClient::Disconnected;
    my.source = KmTime::NoSource;
    memset(&my.acktime, 0, sizeof(my.acktime));
    console->post(KmTime::DebugProtocol, "TimeClient initialised");
    connect(my.socket, SIGNAL(readyRead()), SLOT(readClient()));
    connect(my.socket, SIGNAL(disconnected()), SLOT(disconnectClient()));
}

TimeClient::~TimeClient()
{
    console->post(KmTime::DebugProtocol, "Destroying client %p", this);
    my.state = TimeClient::Disconnected;
    my.source = KmTime::NoSource;
    emit endConnect(this);
}

static char *stateString(int state)
{
    static char buffer[32];

    if (state == TimeClient::Disconnected)
	strcpy(buffer, "Disconnected State");
    else if (state == TimeClient::ClientConnectSET)
	strcpy(buffer, "ClientConnectSET State");
    else if (state == TimeClient::ServerConnectACK)
	strcpy(buffer, "ServerConnectACK State");
    else if (state == TimeClient::ServerNeedACK)
	strcpy(buffer, "ServerNeedACK State");
    else if (state == TimeClient::ClientReady)
	strcpy(buffer, "ClientReady State");
    else
	strcpy(buffer, "Unknown State");
    return buffer;
}

void TimeClient::disconnectClient()
{
    console->post(KmTime::DebugProtocol, "TimeClient::disconnectClient");
    delete this;
}

bool TimeClient::writeClient(KmTime::Packet *packet,
				char *tz, int tzlen, char *label, int llen)
{
    if (packet->source != my.source)
	return true;

    switch (my.state) {
    case TimeClient::Disconnected:
	return true;
    case TimeClient::ClientConnectSET:
	if (packet->command == KmTime::ACK) {
	    my.state = TimeClient::ServerConnectACK;
	    break;
	}
	return true;
    case TimeClient::ServerConnectACK:
	break;
    case TimeClient::ClientReady:
	if (packet->command == KmTime::Step) {
	    my.state = TimeClient::ServerNeedACK;
	    my.acktime = packet->position;
	}
	else {
	    my.state = TimeClient::ClientReady;
	    memset(&my.acktime, 0, sizeof(my.acktime));
	}
	break;
    case TimeClient::ServerNeedACK:
	if (packet->command != KmTime::Step) {
	    my.state = TimeClient::ClientReady;	// clear NEED_ACK
	    memset(&my.acktime, 0, sizeof(my.acktime));
	    break;
	}
	console->post(KmTime::DebugProtocol, "TimeClient::writeClient "
			"SKIP STEP to pos=%u.%u when client %p in NEED_ACK",
			packet->position.tv_sec,packet->position.tv_usec, this);
	return false;
    }

    int len = my.socket->write((const char *)packet, sizeof(KmTime::Packet));
    if (len != sizeof(KmTime::Packet)) {
	console->post(KmTime::DebugProtocol, "TimeCient::writeClient "
			"wrote %d bytes not %d (%x command)",
			len, packet->length, packet->command);
	my.state = TimeClient::Disconnected;
	endConnect(this);
    } else {
	console->post(KmTime::DebugProtocol, "TimeClient::writeClient "
			"wrote %d bytes command=%x state=%u",
			len, packet->command, my.state);
    }
    if (tzlen > 0 && len > 0 &&
	(len = my.socket->write(tz, tzlen)) != tzlen) {
	console->post(KmTime::DebugProtocol, "TimeClient::writeClient "
			"wrote %d bytes not %d (timezone)", len, tzlen);
	my.state = TimeClient::Disconnected;
	endConnect(this);
    } else if (tzlen) {
	console->post(KmTime::DebugProtocol, "TimeClient::writeClient "
			"wrote %d bytes of timezone successfully", len);
    }
    if (llen > 0 && len > 0 &&
	(len = my.socket->write(label, llen)) != llen) {
	console->post(KmTime::DebugProtocol, "TimeClient::writeClient "
			"wrote %d bytes not %d (tz label)", len, llen);
	my.state = TimeClient::Disconnected;
	endConnect(this);
    } else if (llen) {
	console->post(KmTime::DebugProtocol, "TimeClient::writeClient "
			"wrote %d bytes of tz label successfully", len);
    }
    return true;
}

void TimeClient::readClient(void)
{
    KmTime::Packet packet;
    char *payload = NULL;
    int bad = 0, len, sz;

    console->post(KmTime::DebugProtocol, "Reading data from client %p", this);

    len = my.socket->read((char *)&packet, sizeof(KmTime::Packet));
    if (len < 0) {
	console->post(KmTime::DebugProtocol, "Read error on client %p", this);
	bad = 1;
    } else if (packet.magic != KmTime::Magic) {
	console->post(KmTime::DebugProtocol, "Bad magic (%x) from client %p",
			packet.magic, this);
	bad = 1;
    } else if (len != sizeof(KmTime::Packet)) {
	console->post(KmTime::DebugProtocol,
			"Bad 1st read (want %d, got %d) on client %p",
			len, sizeof(KmTime::Packet), this);
	bad = 1;
    } else if (packet.length > sizeof(KmTime::Packet)) {
	sz = packet.length - sizeof(KmTime::Packet);
	payload = (char *)malloc(sz);
	if (payload == NULL) {
	    console->post(KmTime::DebugProtocol,
				"No memory (%d) for second read on client %p",
				sz, len, this);
	    bad = 1;
	} else if ((len = my.socket->read(payload, sz)) != sz) {
	    console->post(KmTime::DebugProtocol,
				"Bad 2nd read (want %d, got %d) on client %p",
				sz, len, this);
	    bad = 1;
	}
	console->post(KmTime::DebugProtocol, "+%d message from client %p",
				sz, this);
    } else {
	console->post(KmTime::DebugProtocol, "good message from client %p",
				this);
    }

    if (!bad) {
	console->post(KmTime::DebugProtocol, "state %s message %d",
				stateString(my.state), packet.command);
	switch(my.state) {
	case TimeClient::Disconnected:
	    if (packet.command == KmTime::Set)
		console->post(KmTime::DebugProtocol,
				"%s got new SET from client %p",
				__func__, this);
	    if (packet.source == KmTime::HostSource) {
		my.source = KmTime::HostSource;
		my.hc->setTime(&packet, payload);
	    } else {
		my.source = KmTime::ArchiveSource;
		my.ac->setTime(&packet, payload);
	    }
	    my.state = TimeClient::ClientConnectSET;
	    packet.command = KmTime::ACK;
	    packet.length = sizeof(KmTime::Packet);
	    writeClient(&packet);
	    return;

	case TimeClient::ClientConnectSET:
	    console->post(KmTime::DebugProtocol, "TimeClient::readClient "
				"bad client %p command %d in ConnectSET state",
				this, packet.command);
	    break;

	case TimeClient::ServerConnectACK:
	    if (packet.command == KmTime::ACK)
		my.state = TimeClient::ClientReady;
	    break;

	case TimeClient::ServerNeedACK:
	    if (packet.command != KmTime::ACK)
		break;
	    if (packet.position.tv_sec == my.acktime.tv_sec &&
		packet.position.tv_usec == my.acktime.tv_usec) {
		console->post(KmTime::DebugProtocol, "TimeClient::readClient "
				"good ACK client=%p (%u.%u)", this,
				my.acktime.tv_sec, my.acktime.tv_usec);
		my.state = TimeClient::ClientReady;
		break;
	    }
	    console->post(KmTime::DebugProtocol, "TimeClient::readClient "
				"BAD ACK client=%p (got %u.%u vs %u.%u)", this,
				packet.position.tv_sec, packet.position.tv_usec,
				my.acktime.tv_sec, my.acktime.tv_usec);
	    bad = 1;
	    break;

	case TimeClient::ClientReady:
	    if (packet.command == KmTime::ACK) {
		console->post(KmTime::DebugProtocol, "TimeClient:: readClient "
			      "unexpected client %p ACK in Ready state", this);
	    }
	    break;
	}

	switch(packet.command) {
	case KmTime::GUIHide:
	case KmTime::GUIShow:
	    console->post(KmTime::DebugProtocol, "TimeClient::readClient "
				"HIDE/SHOW from client %p", this);
	    if (my.source == KmTime::HostSource)
		my.hc->popup(packet.command == KmTime::GUIShow);
	    if (my.source == KmTime::ArchiveSource)
		my.ac->popup(packet.command == KmTime::GUIShow);
	    break;
	case KmTime::Bounds:
	    console->post(KmTime::DebugProtocol, "TimeClient::readClient "
				"BOUNDS from client %p", this);
	    my.ac->addBound(&packet, payload);
	    break;
	case KmTime::ACK:
	    break;
	case KmTime::GUIStyle:
	    console->post(KmTime::DebugProtocol, "TimeClient::readClient "
				"STYLE from client %p (msg=%s)", this, payload);
	    QApplication::setStyle(tr(payload));
	    my.hc->style(payload, this);
	    break;
	default:
	    console->post(KmTime::DebugProtocol, "TimeClient::readClient "
				"unknown command %d from client %p",
				packet.command, this);
	    bad = 1;
	}
    }

    if (bad)
	reset();
}

void TimeClient::reset()
{
    console->post(KmTime::DebugProtocol, "TimeClient::reset");
#if 0
    if (my.source == KmTime::HostSource)
	my.hc->stop();
    else
	my.ac->stop();
#endif
}

TimeLord::TimeLord(QApplication *app)
{
    my.ac = NULL;
    my.hc = NULL;
    connect(this, SIGNAL(lastClientExit()), app, SLOT(quit()));
    connect(this, SIGNAL(lastClientExit()), this, SLOT(quit()));
    connect(this, SIGNAL(newConnection()), SLOT(newConnection()));
    console->post(KmTime::DebugProtocol, "TimeLord initialised");
}

void TimeLord::quit()
{
    if (my.ac)
	my.ac->quit();
    if (my.hc)
	my.hc->quit();
}

void TimeLord::setContext(KmTimeLive *live, KmTimeArch *arch)
{
    my.hc = live;
    connect(live, SIGNAL(timePulse(KmTime::Packet *)),
		    SLOT(timePulse(KmTime::Packet *)));
    connect(live, SIGNAL(stylePulse(KmTime::Packet *, char *, int, void *)),
		    SLOT(stylePulse(KmTime::Packet *, char *, int, void *)));
    connect(live, SIGNAL(vcrModePulse(KmTime::Packet *, int)),
		    SLOT(vcrModePulse(KmTime::Packet *, int)));
    connect(live, SIGNAL(tzPulse(KmTime::Packet *, char *, int, char *, int)),
		    SLOT(tzPulse(KmTime::Packet *, char *, int, char *, int)));
    my.ac = arch;
    connect(arch, SIGNAL(timePulse(KmTime::Packet *)),
		    SLOT(timePulse(KmTime::Packet *)));
    connect(arch, SIGNAL(boundsPulse(KmTime::Packet *)),
		    SLOT(boundsPulse(KmTime::Packet *)));
    connect(arch, SIGNAL(vcrModePulse(KmTime::Packet *, int)),
		    SLOT(vcrModePulse(KmTime::Packet *, int)));
    connect(arch, SIGNAL(tzPulse(KmTime::Packet *, char *, int, char *, int)),
		    SLOT(tzPulse(KmTime::Packet *, char *, int, char *, int)));
}

void TimeLord::newConnection(void)
{
    TimeClient *c = new TimeClient(nextPendingConnection(), this);

    console->post(KmTime::DebugProtocol, "Adding new client %p", c);
    c->setContext(my.ac, my.hc);
    connect(c, SIGNAL(endConnect(TimeClient *)),
		 SLOT(endConnect(TimeClient *)));
    my.clientlist.append(c);
}

void TimeLord::endConnect(TimeClient *client)
{
    console->post(KmTime::DebugProtocol, "Removing client %p", client);
    my.clientlist.removeAll(client);
    if (my.clientlist.isEmpty()) {
	console->post(KmTime::DebugProtocol, "No clients remain, exiting");
	emit lastClientExit();
    }
}

void TimeLord::timePulse(KmTime::Packet *packet)
{
    TimeClient *overrun = NULL;

#if DESPERATE
    static int sequence;
    int localSequence = sequence++;
    console->post(KmTime::DebugProtocol, "TimeLord::timePulse %d (%d clients)",
					 localSequence, my.clientlist.count());
#endif

    packet->magic = KmTime::Magic;
    packet->length = sizeof(KmTime::Packet);
    packet->command = KmTime::Step;
    for (int i = 0; i < my.clientlist.size(); i++)
	if (my.clientlist.at(i)->writeClient(packet) == false)
	    overrun = my.clientlist.at(i);
    if (overrun == NULL)
	overrun->reset();

#if DESPERATE
    console->post(KmTime::DebugProtocol, "TimeLord::timePulse ended %d (%d)",
					 localSequence, overrun == NULL);
#endif
}

void TimeLord::boundsPulse(KmTime::Packet *packet)
{
    console->post(KmTime::DebugProtocol, "TimeLord::boundsPulse (%d clients)",
					 my.clientlist.count());
    packet->magic = KmTime::Magic;
    packet->length = sizeof(KmTime::Packet);
    packet->command = KmTime::Bounds;
    for (int i = 0; i < my.clientlist.size(); i++)
	my.clientlist.at(i)->writeClient(packet);
}

void TimeLord::vcrModePulse(KmTime::Packet *packet, int drag)
{
    console->post(KmTime::DebugProtocol, "TimeLord::vcrModePulse (%d clients)"
		" %d", my.clientlist.count(), drag);
    packet->magic = KmTime::Magic;
    packet->length = sizeof(KmTime::Packet);
    packet->command = drag ? KmTime::VCRModeDrag : KmTime::VCRMode;
    for (int i = 0; i < my.clientlist.size(); i++)
	my.clientlist.at(i)->writeClient(packet);
}

void TimeLord::tzPulse(KmTime::Packet *packet,
				char *tz, int tzlen, char *l, int llen)
{
    console->post(KmTime::DebugProtocol, "TimeLord::tzPulse (%d clients)"
		" - %s/%d/%d", my.clientlist.count(), tz, tzlen, llen);
    packet->magic = KmTime::Magic;
    packet->length = sizeof(KmTime::Packet) + tzlen + llen;
    packet->command = KmTime::TZ;
    for (int i = 0; i < my.clientlist.size(); i++)
	my.clientlist.at(i)->writeClient(packet, tz, tzlen, l, llen);
}

void TimeLord::stylePulse(KmTime::Packet *packet,
				char *style, int len, void *source)
{
    console->post(KmTime::DebugProtocol, "TimeLord::stylePulse (%d-1 clients)"
		" - %s", my.clientlist.count(), style, len);
    packet->magic = KmTime::Magic;
    packet->length = sizeof(KmTime::Packet) + len;
    packet->command = KmTime::GUIStyle;
    for (int i = 0; i < my.clientlist.size(); i++) {
	if (my.clientlist.at(i) != (TimeClient *)source)
	    my.clientlist.at(i)->writeClient(packet, style, len);
    }
}
