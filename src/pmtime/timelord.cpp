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
    my.source = PmTime::NoSource;
    memset(&my.acktime, 0, sizeof(my.acktime));
    console->post(PmTime::DebugProtocol, "TimeClient initialised");
    connect(my.socket, SIGNAL(readyRead()), SLOT(readClient()));
    connect(my.socket, SIGNAL(disconnected()), SLOT(disconnectClient()));
}

TimeClient::~TimeClient()
{
    console->post(PmTime::DebugProtocol, "Destroying client %p", this);
    my.state = TimeClient::Disconnected;
    my.source = PmTime::NoSource;
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
    console->post(PmTime::DebugProtocol, "TimeClient::disconnectClient");
    delete this;
}

bool TimeClient::writeClient(PmTime::Packet *packet,
				char *tz, int tzlen, char *label, int llen)
{
    if (packet->source != my.source)
	return true;

    switch (my.state) {
    case TimeClient::Disconnected:
	return true;
    case TimeClient::ClientConnectSET:
	if (packet->command == PmTime::ACK) {
	    my.state = TimeClient::ServerConnectACK;
	    break;
	}
	return true;
    case TimeClient::ServerConnectACK:
	break;
    case TimeClient::ClientReady:
	if (packet->command == PmTime::Step) {
	    my.state = TimeClient::ServerNeedACK;
	    my.acktime = packet->position;
	}
	else {
	    my.state = TimeClient::ClientReady;
	    memset(&my.acktime, 0, sizeof(my.acktime));
	}
	break;
    case TimeClient::ServerNeedACK:
	if (packet->command != PmTime::Step) {
	    my.state = TimeClient::ClientReady;	// clear NEED_ACK
	    memset(&my.acktime, 0, sizeof(my.acktime));
	    break;
	}
	console->post(PmTime::DebugProtocol, "TimeClient::writeClient "
			"SKIP STEP to pos=%u.%u when client %p in NEED_ACK",
			packet->position.tv_sec,packet->position.tv_usec, this);
	return false;
    }

    int len = my.socket->write((const char *)packet, sizeof(PmTime::Packet));
    if (len != sizeof(PmTime::Packet)) {
	console->post(PmTime::DebugProtocol, "TimeCient::writeClient "
			"wrote %d bytes not %d (%x command)",
			len, packet->length, packet->command);
	my.state = TimeClient::Disconnected;
	endConnect(this);
    } else {
	console->post(PmTime::DebugProtocol, "TimeClient::writeClient "
			"wrote %d bytes command=%x state=%u",
			len, packet->command, my.state);
    }
    if (tzlen > 0 && len > 0 &&
	(len = my.socket->write(tz, tzlen)) != tzlen) {
	console->post(PmTime::DebugProtocol, "TimeClient::writeClient "
			"wrote %d bytes not %d (timezone)", len, tzlen);
	my.state = TimeClient::Disconnected;
	endConnect(this);
    } else if (tzlen) {
	console->post(PmTime::DebugProtocol, "TimeClient::writeClient "
			"wrote %d bytes of timezone successfully", len);
    }
    if (llen > 0 && len > 0 &&
	(len = my.socket->write(label, llen)) != llen) {
	console->post(PmTime::DebugProtocol, "TimeClient::writeClient "
			"wrote %d bytes not %d (tz label)", len, llen);
	my.state = TimeClient::Disconnected;
	endConnect(this);
    } else if (llen) {
	console->post(PmTime::DebugProtocol, "TimeClient::writeClient "
			"wrote %d bytes of tz label successfully", len);
    }
    return true;
}

void TimeClient::readClient(void)
{
    PmTime::Packet packet;
    char *payload = NULL;
    int bad = 0, len, sz;

    console->post(PmTime::DebugProtocol, "Reading data from client %p", this);

    len = my.socket->read((char *)&packet, sizeof(PmTime::Packet));
    if (len < 0) {
	console->post(PmTime::DebugProtocol, "Read error on client %p", this);
	bad = 1;
    } else if (packet.magic != PmTime::Magic) {
	console->post(PmTime::DebugProtocol, "Bad magic (%x) from client %p",
			packet.magic, this);
	bad = 1;
    } else if (len != sizeof(PmTime::Packet)) {
	console->post(PmTime::DebugProtocol,
			"Bad 1st read (want %d, got %d) on client %p",
			len, sizeof(PmTime::Packet), this);
	bad = 1;
    } else if (packet.length > sizeof(PmTime::Packet)) {
	sz = packet.length - sizeof(PmTime::Packet);
	payload = (char *)malloc(sz);
	if (payload == NULL) {
	    console->post(PmTime::DebugProtocol,
				"No memory (%d) for second read on client %p",
				sz, len, this);
	    bad = 1;
	} else if ((len = my.socket->read(payload, sz)) != sz) {
	    console->post(PmTime::DebugProtocol,
				"Bad 2nd read (want %d, got %d) on client %p",
				sz, len, this);
	    bad = 1;
	}
	console->post(PmTime::DebugProtocol, "+%d message from client %p",
				sz, this);
    } else {
	console->post(PmTime::DebugProtocol, "good message from client %p",
				this);
    }

    if (!bad) {
	console->post(PmTime::DebugProtocol, "state %s message %d",
				stateString(my.state), packet.command);
	switch(my.state) {
	case TimeClient::Disconnected:
	    if (packet.command == PmTime::Set)
		console->post(PmTime::DebugProtocol,
				"%s got new SET from client %p",
				__func__, this);
	    if (packet.source == PmTime::HostSource) {
		my.source = PmTime::HostSource;
		my.hc->setTime(&packet, payload);
	    } else {
		my.source = PmTime::ArchiveSource;
		my.ac->setTime(&packet, payload);
	    }
	    my.state = TimeClient::ClientConnectSET;
	    packet.command = PmTime::ACK;
	    packet.length = sizeof(PmTime::Packet);
	    writeClient(&packet);
	    return;

	case TimeClient::ClientConnectSET:
	    console->post(PmTime::DebugProtocol, "TimeClient::readClient "
				"bad client %p command %d in ConnectSET state",
				this, packet.command);
	    break;

	case TimeClient::ServerConnectACK:
	    if (packet.command == PmTime::ACK)
		my.state = TimeClient::ClientReady;
	    break;

	case TimeClient::ServerNeedACK:
	    if (packet.command != PmTime::ACK)
		break;
	    if (packet.position.tv_sec == my.acktime.tv_sec &&
		packet.position.tv_usec == my.acktime.tv_usec) {
		console->post(PmTime::DebugProtocol, "TimeClient::readClient "
				"good ACK client=%p (%u.%u)", this,
				my.acktime.tv_sec, my.acktime.tv_usec);
		my.state = TimeClient::ClientReady;
		break;
	    }
	    console->post(PmTime::DebugProtocol, "TimeClient::readClient "
				"BAD ACK client=%p (got %u.%u vs %u.%u)", this,
				packet.position.tv_sec, packet.position.tv_usec,
				my.acktime.tv_sec, my.acktime.tv_usec);
	    bad = 1;
	    break;

	case TimeClient::ClientReady:
	    if (packet.command == PmTime::ACK) {
		console->post(PmTime::DebugProtocol, "TimeClient:: readClient "
			      "unexpected client %p ACK in Ready state", this);
	    }
	    break;
	}

	switch(packet.command) {
	case PmTime::GUIHide:
	case PmTime::GUIShow:
	    console->post(PmTime::DebugProtocol, "TimeClient::readClient "
				"HIDE/SHOW from client %p", this);
	    if (my.source == PmTime::HostSource)
		my.hc->popup(packet.command == PmTime::GUIShow);
	    if (my.source == PmTime::ArchiveSource)
		my.ac->popup(packet.command == PmTime::GUIShow);
	    break;
	case PmTime::Bounds:
	    console->post(PmTime::DebugProtocol, "TimeClient::readClient "
				"BOUNDS from client %p", this);
	    my.ac->addBound(&packet, payload);
	    break;
	case PmTime::ACK:
	    break;
	default:
	    console->post(PmTime::DebugProtocol, "TimeClient::readClient "
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
    console->post(PmTime::DebugProtocol, "TimeClient::reset");
#if 0
    if (my.source == PmTime::HostSource)
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
    console->post(PmTime::DebugProtocol, "TimeLord initialised");
}

void TimeLord::quit()
{
    if (my.ac)
	my.ac->quit();
    if (my.hc)
	my.hc->quit();
}

void TimeLord::setContext(PmTimeLive *live, PmTimeArch *arch)
{
    my.hc = live;
    connect(live, SIGNAL(timePulse(PmTime::Packet *)),
		    SLOT(timePulse(PmTime::Packet *)));
    connect(live, SIGNAL(vcrModePulse(PmTime::Packet *, int)),
		    SLOT(vcrModePulse(PmTime::Packet *, int)));
    connect(live, SIGNAL(tzPulse(PmTime::Packet *, char *, int, char *, int)),
		    SLOT(tzPulse(PmTime::Packet *, char *, int, char *, int)));
    my.ac = arch;
    connect(arch, SIGNAL(timePulse(PmTime::Packet *)),
		    SLOT(timePulse(PmTime::Packet *)));
    connect(arch, SIGNAL(boundsPulse(PmTime::Packet *)),
		    SLOT(boundsPulse(PmTime::Packet *)));
    connect(arch, SIGNAL(vcrModePulse(PmTime::Packet *, int)),
		    SLOT(vcrModePulse(PmTime::Packet *, int)));
    connect(arch, SIGNAL(tzPulse(PmTime::Packet *, char *, int, char *, int)),
		    SLOT(tzPulse(PmTime::Packet *, char *, int, char *, int)));
}

void TimeLord::newConnection(void)
{
    TimeClient *c = new TimeClient(nextPendingConnection(), this);

    console->post(PmTime::DebugProtocol, "Adding new client %p", c);
    c->setContext(my.ac, my.hc);
    connect(c, SIGNAL(endConnect(TimeClient *)),
		 SLOT(endConnect(TimeClient *)));
    my.clientlist.append(c);
}

void TimeLord::endConnect(TimeClient *client)
{
    console->post(PmTime::DebugProtocol, "Removing client %p", client);
    my.clientlist.removeAll(client);
    if (my.clientlist.isEmpty()) {
	console->post(PmTime::DebugProtocol, "No clients remain, exiting");
	emit lastClientExit();
    }
}

void TimeLord::timePulse(PmTime::Packet *packet)
{
    QList<TimeClient*> overrunClients;

#if DESPERATE
    static int sequence;
    int localSequence = sequence++;
    console->post(PmTime::DebugProtocol, "TimeLord::timePulse %d (%d clients)",
					 localSequence, my.clientlist.count());
#endif

    packet->magic = PmTime::Magic;
    packet->length = sizeof(PmTime::Packet);
    packet->command = PmTime::Step;
    for (int i = 0; i < my.clientlist.size(); i++)
	if (my.clientlist.at(i)->writeClient(packet) == false)
	    overrunClients.append(my.clientlist.at(i));
    for (int i = 0; i < overrunClients.size(); i++)
	overrunClients.at(i)->reset();

#if DESPERATE
    console->post(PmTime::DebugProtocol, "TimeLord::timePulse ended %d (%d)",
					 localSequence, overrunClients.size());
#endif
}

void TimeLord::boundsPulse(PmTime::Packet *packet)
{
    console->post(PmTime::DebugProtocol, "TimeLord::boundsPulse (%d clients)",
					 my.clientlist.count());
    packet->magic = PmTime::Magic;
    packet->length = sizeof(PmTime::Packet);
    packet->command = PmTime::Bounds;
    for (int i = 0; i < my.clientlist.size(); i++)
	my.clientlist.at(i)->writeClient(packet);
}

void TimeLord::vcrModePulse(PmTime::Packet *packet, int drag)
{
    console->post(PmTime::DebugProtocol, "TimeLord::vcrModePulse (%d clients)"
		" %d", my.clientlist.count(), drag);
    packet->magic = PmTime::Magic;
    packet->length = sizeof(PmTime::Packet);
    packet->command = drag ? PmTime::VCRModeDrag : PmTime::VCRMode;
    for (int i = 0; i < my.clientlist.size(); i++)
	my.clientlist.at(i)->writeClient(packet);
}

void TimeLord::tzPulse(PmTime::Packet *packet,
				char *tz, int tzlen, char *l, int llen)
{
    console->post(PmTime::DebugProtocol, "TimeLord::tzPulse (%d clients)"
		" - %s/%d/%d", my.clientlist.count(), tz, tzlen, llen);
    packet->magic = PmTime::Magic;
    packet->length = sizeof(PmTime::Packet) + tzlen + llen;
    packet->command = PmTime::TZ;
    for (int i = 0; i < my.clientlist.size(); i++)
	my.clientlist.at(i)->writeClient(packet, tz, tzlen, l, llen);
}
