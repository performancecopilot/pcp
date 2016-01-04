/*
 * Copyright (c) 2012, Red Hat.
 * Copyright (c) 2007-2008, Aconex.  All Rights Reserved.
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
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
#include "groupcontrol.h"

#define DESPERATE 0

GroupControl::GroupControl()
{
    my.samples = 0;
    my.visible = 0;
    my.realDelta = 0;
    my.realPosition = 0;
    my.timeData = NULL;
    my.timeState = StartState;
    my.buttonState = QedTimeButton::Timeless;
    my.pmtimeState = QmcTime::StoppedState;
    memset(&my.delta, 0, sizeof(struct timeval));
    memset(&my.position, 0, sizeof(struct timeval));
}

void
GroupControl::init(int samples, int visible,
			struct timeval *interval, struct timeval *position)
{
    my.samples = samples;
    my.visible = visible;

    if (isArchiveSource()) {
	my.pmtimeState = QmcTime::StoppedState;
	my.buttonState = QedTimeButton::StoppedArchive;
    }
    else {
	my.pmtimeState = QmcTime::ForwardState;
	my.buttonState = QedTimeButton::ForwardLive;
    }
    my.delta = *interval;
    my.position = *position;
    my.realDelta = __pmtimevalToReal(interval);
    my.realPosition = __pmtimevalToReal(position);

    my.timeData = (double *)malloc(samples * sizeof(double));
    for (int i = 0; i < samples; i++)
	my.timeData[i] = my.realPosition - (i * my.realDelta);
}

bool
GroupControl::isArchiveSource(void)
{
    // Note: We purposefully are not using QmcGroup::mode() here, as we
    // may not have initialised any contexts yet.  In such a case, live
    // mode is always returned (default, from the QmcGroup constructor).

    return this == archiveGroup;
}

bool
GroupControl::isActive(QmcTime::Packet *packet)
{
    return (((activeGroup == archiveGroup) &&
	     (packet->source == QmcTime::ArchiveSource)) ||
	    ((activeGroup == liveGroup) && 
	     (packet->source == QmcTime::HostSource)));
}

void
GroupControl::addGadget(Gadget *gadget)
{
    my.gadgetsList.append(gadget);
}

void
GroupControl::deleteGadget(Gadget *gadget)
{
    for (int i = 0; i < gadgetCount(); i++)
	if (my.gadgetsList.at(i) == gadget)
	    my.gadgetsList.removeAt(i);
}

int
GroupControl::gadgetCount() const
{
    return my.gadgetsList.size();
}

void
GroupControl::updateBackground(void)
{
    for (int i = 0; i < gadgetCount(); i++)
	my.gadgetsList.at(i)->updateBackground(globalSettings.chartBackground);
}

void
GroupControl::updateTimeAxis(void)
{
    QString tz, otz, unused;

    if (numContexts() > 0 || isArchiveSource() == false) {
	if (numContexts() > 0)
	    defaultTZ(unused, otz);
	else
	    otz = QmcSource::localHost;
	tz = otz;
	pmchart->timeAxis()->setAxisScale(QwtPlot::xBottom,
		my.timeData[my.visible - 1], my.timeData[0],
		pmchart->timeAxis()->scaleValue(my.realDelta, my.visible));
	pmchart->setDateLabel(my.position.tv_sec, tz);
	pmchart->timeAxis()->replot();
    } else {
	pmchart->timeAxis()->noArchiveSources();
	pmchart->setDateLabel(tr("[No open archives]"));
    }

    if (console->logLevel(PmChart::DebugProtocol)) {
	int i = my.visible - 1;
	console->post(PmChart::DebugProtocol,
		"GroupControl::updateTimeAxis: tz=%s; visible points=%d",
		(const char *)tz.toLatin1(), i);
	console->post(PmChart::DebugProtocol,
		"GroupControl::updateTimeAxis: first time is %.3f (%s)",
		my.timeData[i], timeString(my.timeData[i]));
	console->post(PmChart::DebugProtocol,
		"GroupControl::updateTimeAxis: final time is %.3f (%s)",
		my.timeData[0], timeString(my.timeData[0]));
    }
}

void
GroupControl::updateTimeButton(void)
{
    pmchart->setButtonState(my.buttonState);
}

QmcTime::State
GroupControl::pmtimeState(void)
{
    return my.pmtimeState;
}

char *
GroupControl::timeState()
{
    static char buf[16];

    switch (my.timeState) {
    case StartState:	strcpy(buf, "Start"); break;
    case ForwardState:	strcpy(buf, "Forward"); break;
    case BackwardState:	strcpy(buf, "Backward"); break;
    case EndLogState:	strcpy(buf, "EndLog"); break;
    case StandbyState:	strcpy(buf, "Standby"); break;
    default:		strcpy(buf, "Dodgey"); break;
    }
    return buf;
}

void
GroupControl::timeSelectionActive(Gadget *source, int timestamp)
{
    QPoint point(timestamp, -1);
    QMouseEvent pressed(QEvent::MouseButtonPress, point,
			Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    for (int i = 0; i < gadgetCount(); i++) {
	Gadget *gadget = my.gadgetsList.at(i);
	if (source != gadget)
	    gadget->activateTime(&pressed);
    }
}

void
GroupControl::timeSelectionReactive(Gadget *source, int timestamp)
{
    QPoint point(timestamp, -1);
    QMouseEvent moved(QEvent::MouseMove, point,
			Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    for (int i = 0; i < gadgetCount(); i++) {
	Gadget *gadget = my.gadgetsList.at(i);
	if (source != gadget)
	    gadget->reactivateTime(&moved);
    }
}

void
GroupControl::timeSelectionInactive(Gadget *source)
{
    QPoint point(-1, -1);
    QMouseEvent release(QEvent::MouseButtonRelease, point,
			Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    for (int i = 0; i < gadgetCount(); i++) {
	Gadget *gadget = my.gadgetsList.at(i);
	if (source != gadget)
	    gadget->deactivateTime(&release);
    }
}

//
// Drive all updates into each gadget (refresh the display)
//
void
GroupControl::refreshGadgets(bool active)
{
#if DESPERATE
    for (int s = 0; s < my.samples; s++)
	console->post(PmChart::DebugProtocol,
		"GroupControl::refreshGadgets: timeData[%2d] is %.2f (%s)",
		s, my.timeData[s], timeString(my.timeData[s]));
    console->post(PmChart::DebugProtocol,
		"GroupControl::refreshGadgets: state=%s", timeState());
#endif

    for (int i = 0; i < gadgetCount(); i++) {
	my.gadgetsList.at(i)->updateValues(my.timeState != BackwardState,
					active, my.samples, my.visible,
					my.timeData[my.visible - 1],
					my.timeData[0],
					my.realDelta);
    }
    if (active) {
	updateTimeButton();
	updateTimeAxis();
    }
}

//
// Setup the initial data needed after opening a view.
// All of the work is in archive mode; in live mode we have
// not yet got any historical data that we can display...
//
void
GroupControl::setupWorldView(void)
{
    if (isArchiveSource() == false)
	return;

    QmcTime::Packet packet;
    packet.source = QmcTime::ArchiveSource;
    packet.state = QmcTime::ForwardState;
    packet.mode = QmcTime::NormalMode;
    memcpy(&packet.delta, pmtime->archiveInterval(), sizeof(packet.delta));
    memcpy(&packet.position, pmtime->archivePosition(), sizeof(packet.position));
    memcpy(&packet.start, pmtime->archiveStart(), sizeof(packet.start));
    memcpy(&packet.end, pmtime->archiveEnd(), sizeof(packet.end));
    adjustWorldView(&packet, true);
}

//
// Received a Set or a VCRMode requiring us to adjust our state
// and possibly rethink everything.  This can result from a time
// control position change, delta change, direction change, etc.
//
void
GroupControl::adjustWorldView(QmcTime::Packet *packet, bool vcrMode)
{
    my.delta = packet->delta;
    my.position = packet->position;
    my.realDelta = __pmtimevalToReal(&packet->delta);
    my.realPosition = __pmtimevalToReal(&packet->position);

    console->post("GroupControl::adjustWorldView: "
		  "sh=%d vh=%d delta=%.2f position=%.2f (%s) state=%s",
		my.samples, my.visible, my.realDelta, my.realPosition,
		timeString(my.realPosition), timeState());

    QmcTime::State state = packet->state;
    if (isArchiveSource()) {
	if (packet->state == QmcTime::ForwardState)
	    adjustArchiveWorldViewForward(packet, vcrMode);
	else if (packet->state == QmcTime::BackwardState)
	    adjustArchiveWorldViewBackward(packet, vcrMode);
	else
	    adjustArchiveWorldViewStopped(packet, vcrMode);
    }
    else if (state != QmcTime::StoppedState)
	adjustLiveWorldViewForward(packet);
    else
	adjustLiveWorldViewStopped(packet);
}

void
GroupControl::adjustLiveWorldViewStopped(QmcTime::Packet *packet)
{
    if (isActive(packet)) {
	newButtonState(packet->state, packet->mode, pmchart->isTabRecording());
	updateTimeButton();
    }
}

static bool
fuzzyTimeMatch(double a, double b, double tolerance)
{
    // a matches b if the difference is within 1% of the delta (tolerance)
    return (a == b ||
	    (b > a && a + tolerance > b) ||
	    (b < a && a - tolerance < b));
}

void
GroupControl::adjustLiveWorldViewForward(QmcTime::Packet *packet)
{
    //
    // X-Axis _max_ becomes packet->position.
    // Rest of (preceeding) time window filled in using packet->delta.
    // In live mode, we can only fetch current data.  However, we make
    // an effort to keep old data that happens to align with the delta
    // time points (or near enough) that we are now interested in.  So,
    // "oi" is the old index, whereas "i" is the new timeData[] index.
    // First we try to find a fuzzy match on current old index, if it
    // doesn't exit, we continue moving "oi" until it points at a time
    // larger than the one we're after, and then see how that fares on
    // the next iteration.
    //
    int i, oi, last = my.samples - 1;
    bool preserve = false;
    double tolerance = my.realDelta;
    double position = my.realPosition - (my.realDelta * my.samples);

    for (i = oi = last; i >= 0; i--, position += my.realDelta) {
	while (i > 0 && my.timeData[oi] < position + my.realDelta && oi > 0) {
	    if (fuzzyTimeMatch(my.timeData[oi], position, tolerance) == false) {
#if DESPERATE
		console->post("NO fuzzyTimeMatch %.3f to %.3f (%s)",
			my.timeData[oi], position, timeString(position));
#endif
		if (my.timeData[oi] > position)
		    break;
		oi--;
		continue;
	    }
	    console->post("Saved live data (oi=%d/i=%d) for %s", oi, i,
						timeString(position));
	    for (int j = 0; j < gadgetCount(); j++)
		my.gadgetsList.at(j)->preserveSample(i, oi);
	    my.timeData[i] = my.timeData[oi];
	    preserve = true;
	    oi--;
	    break;
	}

	if (i == 0) {	// refreshGadgets() finishes up last one
	    console->post("Fetching data[%d] at %s", i, timeString(position));
	    my.timeData[i] = position;
	    fetch();
	}
	else if (preserve == false) {
#if DESPERATE
	    console->post("No live data for %s", timeString(position));
#endif
	    my.timeData[i] = position;
	    for (int j = 0; j < gadgetCount(); j++)
		my.gadgetsList.at(j)->punchoutSample(i);
	}
	else
	    preserve = false;
    }
    // One (per-gadget) recalculation & refresh at the end, after all data moved
    for (int j = 0; j < gadgetCount(); j++)
	my.gadgetsList.at(j)->adjustValues();
    my.timeState = (packet->state == QmcTime::StoppedState) ?
			StandbyState : ForwardState;

    bool active = isActive(packet);
    if (active)
	newButtonState(packet->state, packet->mode, pmchart->isTabRecording());
    refreshGadgets(active);
}

void
GroupControl::adjustArchiveWorldViewForward(QmcTime::Packet *packet, bool setup)
{
    console->post("GroupControl::adjustArchiveWorldViewForward");
    my.timeState = ForwardState;

    int setmode = PM_MODE_INTERP;
    int delta = packet->delta.tv_sec;
    if (packet->delta.tv_usec == 0) {
	setmode |= PM_XTB_SET(PM_TIME_SEC);
    } else {
	delta = delta * 1000 + packet->delta.tv_usec / 1000;
	setmode |= PM_XTB_SET(PM_TIME_MSEC);
    }

    //
    // X-Axis _max_ becomes packet->position.
    // Rest of (preceeding) time window filled in using packet->delta.
    //
    int last = my.samples - 1;
    double tolerance = my.realDelta / 20.0;	// 5% of the sample interval
    double position = my.realPosition - (my.realDelta * last);

    double left = position;
    double right = my.realPosition;
    double interval = pmchart->timeAxis()->scaleValue((double)delta, my.visible);

    for (int i = last; i >= 0; i--, position += my.realDelta) {
	if (setup == false &&
	    fuzzyTimeMatch(my.timeData[i], position, tolerance) == true) {
	    continue;
	}

	my.timeData[i] = position;

	struct timeval timeval;
	__pmtimevalFromReal(position, &timeval);
	setArchiveMode(setmode, &timeval, delta);
	console->post("Fetching data[%d] at %s", i, timeString(position));
	fetch();
	if (i == 0)		// refreshGadgets() finishes up last one
	    break;
	console->post("GroupControl::adjustArchiveWorldViewForward: "
		      "setting time position[%d]=%.2f[%s] state=%s count=%d",
			i, position, timeString(position),
			timeState(), gadgetCount());
	for (int j = 0; j < gadgetCount(); j++)
	    my.gadgetsList.at(j)->updateValues(true, false, my.samples, my.visible,
						left, right, interval);
    }

    bool active = isActive(packet);
    if (setup)
	packet->state = QmcTime::StoppedState;
    if (active)
	newButtonState(packet->state, packet->mode, pmchart->isTabRecording());
    pmtime->setArchivePosition(&packet->position);
    pmtime->setArchiveInterval(&packet->delta);
    refreshGadgets(active);
}

void
GroupControl::adjustArchiveWorldViewBackward(QmcTime::Packet *packet, bool setup)
{
    console->post("GroupControl::adjustArchiveWorldViewBackward");
    my.timeState = BackwardState;

    int setmode = PM_MODE_INTERP;
    int delta = packet->delta.tv_sec;
    if (packet->delta.tv_usec == 0) {
	setmode |= PM_XTB_SET(PM_TIME_SEC);
    } else {
	delta = delta * 1000 + packet->delta.tv_usec / 1000;
	setmode |= PM_XTB_SET(PM_TIME_MSEC);
    }

    //
    // X-Axis _min_ becomes packet->position.
    // Rest of (following) time window filled in using packet->delta.
    //
    int last = my.samples - 1;
    double tolerance = my.realDelta / 20.0;	// 5% of the sample interval
    double position = my.realPosition;

    double left = position - (my.realDelta * last);
    double right = position;
    double interval = pmchart->timeAxis()->scaleValue((double)delta, my.visible);

    for (int i = 0; i <= last; i++, position -= my.realDelta) {
	if (setup == false &&
	    fuzzyTimeMatch(my.timeData[i], position, tolerance) == true) {
	    continue;
	}

	my.timeData[i] = position;

	struct timeval timeval;
	__pmtimevalFromReal(position, &timeval);
	setArchiveMode(setmode, &timeval, -delta);
	console->post("Fetching data[%d] at %s", i, timeString(position));
	fetch();
	if (i == last)		// refreshGadgets() finishes up last one
	    break;
	console->post("GroupControl::adjustArchiveWorldViewBackward: "
		      "setting time position[%d]=%.2f[%s] state=%s count=%d",
			i, position, timeString(position),
			timeState(), gadgetCount());
	for (int j = 0; j < gadgetCount(); j++)
	    my.gadgetsList.at(j)->updateValues(false, false, my.samples, my.visible,
						left, right, interval);
    }

    bool active = isActive(packet);
    if (setup)
	packet->state = QmcTime::StoppedState;
    if (active)
	newButtonState(packet->state, packet->mode, pmchart->isTabRecording());
    pmtime->setArchivePosition(&packet->position);
    pmtime->setArchiveInterval(&packet->delta);
    refreshGadgets(active);
}

void
GroupControl::adjustArchiveWorldViewStopped(QmcTime::Packet *packet, bool needFetch)
{
    if (needFetch) {	// stopped, but VCR reposition event occurred
	adjustArchiveWorldViewForward(packet, needFetch);
    } else {
	my.timeState = StandbyState;
	packet->state = QmcTime::StoppedState;
	newButtonState(packet->state, packet->mode, pmchart->isTabRecording());
	updateTimeButton();
    }
}

//
// Catch the situation where we get a larger than expected increase
// in position.  This happens when we restart after a stop in live
// mode (both with and without a change in the delta).
//
static bool
sideStep(double n, double o, double interval)
{
    // tolerance set to 5% of the sample interval:
    return fuzzyTimeMatch(o + interval, n, interval/20.0) == false;
}

//
// Fetch all metric values across all gadgets, and also update the
// unified time axis.
//
void
GroupControl::step(QmcTime::Packet *packet)
{
    double stepPosition = __pmtimevalToReal(&packet->position);

    console->post(PmChart::DebugProtocol,
	"GroupControl::step: stepping to time %.2f, delta=%.2f, state=%s",
	stepPosition, my.realDelta, timeState());

    if ((packet->source == QmcTime::ArchiveSource &&
	((packet->state == QmcTime::ForwardState &&
		my.timeState != ForwardState) ||
	 (packet->state == QmcTime::BackwardState &&
		my.timeState != BackwardState))) ||
	 sideStep(stepPosition, my.realPosition, my.realDelta))
	return adjustWorldView(packet, false);

    my.pmtimeState = packet->state;
    my.position = packet->position;
    my.realPosition = stepPosition;

    int last = my.samples - 1;
    if (packet->state == QmcTime::ForwardState) { // left-to-right (all but 1st)
	if (my.samples > 1)
	    memmove(&my.timeData[1], &my.timeData[0], sizeof(double) * last);
	my.timeData[0] = my.realPosition;
    }
    else if (packet->state == QmcTime::BackwardState) { // right-to-left
	if (my.samples > 1)
	    memmove(&my.timeData[0], &my.timeData[1], sizeof(double) * last);
	my.timeData[last] = my.realPosition - torange(my.delta, last);
    }

    fetch();

    bool active = isActive(packet);
    if (isActive(packet))
	newButtonState(packet->state, packet->mode, pmchart->isTabRecording());
    refreshGadgets(active);
}

void
GroupControl::VCRMode(QmcTime::Packet *packet, bool dragMode)
{
    if (!dragMode)
	adjustWorldView(packet, true);
}

void
GroupControl::setTimezone(QmcTime::Packet *packet, char *tz)
{
    console->post(PmChart::DebugProtocol, "GroupControl::setTimezone %s", tz);

    useTZ(QString(tz));

    if (isActive(packet))
	updateTimeAxis();
}

void
GroupControl::setSampleHistory(int v)
{
    console->post("GroupControl::setSampleHistory (%d -> %d)", my.samples, v);
    if (my.samples != v) {
	my.samples = v;

	my.timeData = (double *)malloc(my.samples * sizeof(my.timeData[0]));
	if (my.timeData == NULL)
	    nomem();

	double right = my.realPosition;
	for (v = 0; v < my.samples; v++)
	    my.timeData[v] = my.realPosition - (v * my.realDelta);
	double left = my.timeData[v-1];
	for (v = 0; v < gadgetCount(); v++)
	    my.gadgetsList.at(v)->resetValues(my.samples, left, right);
    }
}

int
GroupControl::sampleHistory(void)
{
    return my.samples;
}

void
GroupControl::setVisibleHistory(int v)
{
    console->post("GroupControl::setVisibleHistory (%d -> %d)", my.visible, v);

    if (my.visible != v)
	my.visible = v;
}

int
GroupControl::visibleHistory(void)
{
    return my.visible;
}

double *
GroupControl::timeAxisData(void)
{
    return my.timeData;
}

QedTimeButton::State
GroupControl::buttonState(void)
{
    return my.buttonState;
}

void
GroupControl::newButtonState(QmcTime::State s, QmcTime::Mode m, bool record)
{
    if (isArchiveSource() == false) {
	if (s == QmcTime::StoppedState)
	    my.buttonState = record ?
		QedTimeButton::StoppedRecord : QedTimeButton::StoppedLive;
	else
	    my.buttonState = record ?
		QedTimeButton::ForwardRecord : QedTimeButton::ForwardLive;
    }
    else if (m == QmcTime::StepMode) {
	if (s == QmcTime::ForwardState)
	    my.buttonState = QedTimeButton::StepForwardArchive;
	else if (s == QmcTime::BackwardState)
	    my.buttonState = QedTimeButton::StepBackwardArchive;
	else
	    my.buttonState = QedTimeButton::StoppedArchive;
    }
    else if (m == QmcTime::FastMode) {
	if (s == QmcTime::ForwardState)
	    my.buttonState = QedTimeButton::FastForwardArchive;
	else if (s == QmcTime::BackwardState)
	    my.buttonState = QedTimeButton::FastBackwardArchive;
	else
	    my.buttonState = QedTimeButton::StoppedArchive;
    }
    else if (s == QmcTime::ForwardState)
	my.buttonState = QedTimeButton::ForwardArchive;
    else if (s == QmcTime::BackwardState)
	my.buttonState = QedTimeButton::BackwardArchive;
    else
	my.buttonState = QedTimeButton::StoppedArchive;
}
