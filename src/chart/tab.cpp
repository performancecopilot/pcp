/*
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
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
#include "main.h"
#include <QtCore/QTimer>
#include <QtGui/QLayout>
#include <QtGui/QMessageBox>
#include <QtGui/QPalette>
#include <qwt_plot.h>
#include <qwt_plot_layout.h>
#include <qwt_scale_draw.h>
#include <qwt_scale_widget.h>
#include <qwt_text.h>
#include <qwt_text_label.h>
#include "openviewdialog.h"
#include "recorddialog.h"

#define DESPERATE 0

Tab::Tab(): QWidget(NULL)
{
    my.count = 0;
    my.splitter = NULL;
    my.current = -1;
    my.charts = NULL;
    my.group = NULL;
    my.samples = globalSettings.sampleHistory;
    my.visible = globalSettings.visibleHistory;
    my.realDelta = 0;
    my.realPosition = 0;
    my.timeData = NULL;
    my.recording = false;
    my.timeState = Tab::StartState;
    my.buttonState = TimeButton::Timeless;
    my.kmtimeState = KmTime::StoppedState;
    memset(&my.delta, 0, sizeof(struct timeval));
    memset(&my.position, 0, sizeof(struct timeval));
}

void Tab::init(QTabWidget *tab, int samples, int visible,
		QmcGroup *group, KmTime::Source source, QString label,
		struct timeval *interval, struct timeval *position)
{
    my.splitter = new QSplitter(tab);
    my.splitter->setOrientation(Qt::Vertical);
    my.splitter->setSizePolicy(QSizePolicy::MinimumExpanding,
				QSizePolicy::MinimumExpanding);
    tab->addTab(my.splitter, label);
    my.group = group;

    my.kmtimeSource = source;
    if (my.kmtimeSource == KmTime::HostSource) {
	my.kmtimeState = KmTime::ForwardState;
	my.buttonState = TimeButton::ForwardLive;
    }
    else {
	my.kmtimeState = KmTime::StoppedState;
	my.buttonState = TimeButton::StoppedArchive;
    }

    my.samples = samples;
    my.visible = visible;
    my.delta = *interval;
    my.position = *position;
    my.realDelta = tosec(*interval);
    my.realPosition = tosec(*position);

    my.timeData = (double *)malloc(samples * sizeof(double));
    for (int i = 0; i < samples; i++)
	my.timeData[i] = my.realPosition - (i * my.realDelta);

    kmchart->timeAxis()->setAxisScale(QwtPlot::xBottom,
		my.timeData[visible-1], my.timeData[0],
		kmchart->timeAxis()->scaleValue(my.realDelta, my.visible));
    kmchart->setButtonState(my.buttonState);
}

Chart *Tab::chart(int i)
{
    if (i >= 0 && i < my.count)
	return my.charts[i];
    return NULL;
}

Chart *Tab::addChart(void)
{
    if (my.count)
	kmchart->updateHeight(KmChart::minimumChartHeight());
    Chart *cp = new Chart(this, my.splitter);
    if (!cp)
	nomem();
    my.count++;
    my.charts = (Chart **)realloc(my.charts, my.count * sizeof(Chart *));
    my.charts[my.count-1] = cp;
    setCurrent(cp);
    my.current = my.count - 1;
    console->post("Tab::addChart: [%d]->Chart %p", my.current, cp);
    return cp;
}

int Tab::deleteCurrent(void)
{
    return deleteChart(my.current);
}

int Tab::deleteChart(Chart *cp)
{
    for (int i = 0; i < my.count - 1; i++)
	if (my.charts[i] == cp)
	    return deleteChart(i);
    return 0;
}

int Tab::deleteChart(int idx)
{
    Chart *cp = my.charts[idx];
    int newCurrent = my.current;

    if (my.count > 1)
	kmchart->updateHeight(-(cp->height()));
    delete cp;

    // shuffle left, don't bother with the realloc()
    for (int i = idx; i < my.count - 1; i++)
	my.charts[i] = my.charts[i+1];
    if (idx < newCurrent || idx == my.count - 1) {
	// old current slot no longer available, choose previous slot
	newCurrent--;
    }
    my.count--;
    my.current = -1;		// force re-assignment and re-highlighting
    setCurrent(my.charts[newCurrent]);
    return my.current;
}

int Tab::numChart(void)
{
    return my.count;
}

bool Tab::isArchiveSource(void)
{
    return my.kmtimeSource == KmTime::ArchiveSource;
}

Chart *Tab::currentChart(void)
{
    return my.charts[my.current];
}

void Tab::setCurrent(Chart *cp)
{
    QwtScaleWidget *sp;
    QwtText t;
    QPalette palette;
    int i;

    for (i = 0; i < my.count; i++)
	if (my.charts[i] == cp)
	    break;
    if (i == my.count || i == my.current)
	return;

    console->post("Tab::setCurrentChart(%p) -> %d", cp, i);
    if (my.current != -1) {
	// reset highlight for old current
	t = my.charts[my.current]->titleLabel()->text();
	t.setColor("black");
	my.charts[my.current]->setTitle(t);
	palette = my.charts[my.current]->titleLabel()->palette();
	palette.setColor(QPalette::Active, QPalette::Text, QColor("black"));
        my.charts[my.current]->titleLabel()->setPalette(palette);
	sp = my.charts[my.current]->axisWidget(QwtPlot::yLeft);
	t = sp->title();
	t.setColor("black");
	sp->setTitle(t);
    }
    my.current = i;
    // set title and y-axis highlight for new current
    // for title, have to set both QwtText and QwtTextLabel because of
    // the way attributes are cached and restored when printing charts
    //
    t = cp->titleLabel()->text();
    t.setColor(globalSettings.chartHighlight);
    cp->setTitle(t);
    my.charts[my.current]->setTitle(t);
    palette = my.charts[my.current]->titleLabel()->palette();
    palette.setColor(QPalette::Active, QPalette::Text, globalSettings.chartHighlight);
    my.charts[my.current]->titleLabel()->setPalette(palette);
    sp = cp->axisWidget(QwtPlot::yLeft);
    t = sp->title();
    t.setColor(globalSettings.chartHighlight);
    sp->setTitle(t);
}

QmcGroup *Tab::group()
{
    return my.group;
}

void Tab::updateTimeAxis(void)
{
    QString tz, otz, unused;

    if (my.group->numContexts() > 0 || isArchiveSource() == false) {
	if (my.group->numContexts() > 0)
	    my.group->defaultTZ(unused, otz);
	else
	    otz = QmcSource::localHost;
	tz = otz;
	kmchart->timeAxis()->setAxisScale(QwtPlot::xBottom,
		my.timeData[my.visible - 1], my.timeData[0],
		kmchart->timeAxis()->scaleValue(my.realDelta, my.visible));
	kmchart->setDateLabel(my.position.tv_sec, tz);
	kmchart->timeAxis()->replot();
    } else {
	kmchart->timeAxis()->noArchiveSources();
	kmchart->setDateLabel(tr("[No open archives]"));
    }

    if (console->logLevel(KmChart::DebugProtocol)) {
	int i = my.visible - 1;
	console->post(KmChart::DebugProtocol,
			"Tab::updateTimeAxis: tz=%s; visible points=%d",
			(const char *)tz.toAscii(), i);
	console->post(KmChart::DebugProtocol,
			"Tab::updateTimeAxis: first time is %.3f (%s)",
			my.timeData[i], timeString(my.timeData[i]));
	console->post(KmChart::DebugProtocol,
			"Tab::updateTimeAxis: final time is %.3f (%s)",
			my.timeData[0], timeString(my.timeData[0]));
    }
}

void Tab::updateTimeButton(void)
{
    kmchart->setButtonState(my.buttonState);
}

KmTime::State Tab::kmtimeState(void)
{
    return my.kmtimeState;
}

char *Tab::timeState()
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

//
// Drive all updates into each chart (refresh the display)
//
void Tab::refreshCharts(void)
{
#if DESPERATE
    for (int s = 0; s < me.samples; s++)
	console->post(KmChart::DebugProtocol,
			"Tab::refreshCharts: timeData[%2d] is %.2f (%s)",
			s, my.timeData[s], timeString(my.timeData[s]));
    console->post(KmChart::DebugProtocol,
			"Tab::refreshCharts: state=%s", timeState());
#endif

    for (int i = 0; i < my.count; i++) {
	my.charts[i]->setAxisScale(QwtPlot::xBottom, 
		my.timeData[my.visible - 1], my.timeData[0],
		kmchart->timeAxis()->scaleValue(my.realDelta, my.visible));
	my.charts[i]->update(my.timeState != Tab::BackwardState, true);
	my.charts[i]->fixLegendPen();
    }

    if (this == kmchart->activeTab()) {
	updateTimeButton();
	updateTimeAxis();
    }
}

//
// Create the initial scene on opening a view, and show it.
// Most of the work is in archive mode, in live mode we've
// got no historical data that we can display yet.
//
void Tab::setupWorldView(void)
{
    if (isArchiveSource()) {
	KmTime::Packet packet;
	packet.source = KmTime::ArchiveSource;
	packet.state = KmTime::ForwardState;
	packet.mode = KmTime::NormalMode;
	memcpy(&packet.delta, kmtime->archiveInterval(), sizeof(packet.delta));
	memcpy(&packet.position, kmtime->archivePosition(),
						sizeof(packet.position));
	memcpy(&packet.start, kmtime->archiveStart(), sizeof(packet.start));
	memcpy(&packet.end, kmtime->archiveEnd(), sizeof(packet.end));
	adjustWorldView(&packet, true);
    }
    for (int m = 0; m < my.count; m++)
	my.charts[m]->show();
}

//
// Received a Set or a VCRMode requiring us to adjust our state
// and possibly rethink everything.  This can result from a time
// control position change, delta change, direction change, etc.
//
void Tab::adjustWorldView(KmTime::Packet *packet, bool vcrMode)
{
    my.delta = packet->delta;
    my.position = packet->position;
    my.realDelta = tosec(packet->delta);
    my.realPosition = tosec(packet->position);

    console->post("Tab::adjustWorldView: "
		  "sh=%d vh=%d delta=%.2f position=%.2f (%s) state=%s",
		my.samples, my.visible, my.realDelta, my.realPosition,
		timeString(my.realPosition), timeState());

    if (isArchiveSource())
	adjustArchiveWorldView(packet, vcrMode);
    else
	adjustLiveWorldView(packet);
}

void Tab::adjustArchiveWorldView(KmTime::Packet *packet, bool needFetch)
{
    if (packet->state == KmTime::ForwardState)
	adjustArchiveWorldViewForward(packet, needFetch);
    else if (packet->state == KmTime::BackwardState)
	adjustArchiveWorldViewBackward(packet, needFetch);
    else
	adjustArchiveWorldViewStop(packet, needFetch);
}

static bool fuzzyTimeMatch(double a, double b, double tolerance)
{
    // a matches b if the difference is within 1% of the delta (tolerance)
    return (b > a && a + tolerance > b) || (b < a && a - tolerance < b);
}

void Tab::adjustLiveWorldView(KmTime::Packet *packet)
{
    console->post("Tab::adjustLiveWorldView");

    //
    // X-Axis _max_ becomes packet->position.
    // Rest of (preceeding) time window filled in using packet->delta.
    // In live mode, we can only fetch current data.  However, we make
    // an effort to keep old data that happens to align with the delta
    // time points that we are now interested in.
    //
    int last = my.samples - 1;
    double tolerance = my.realDelta / 20.0;	// 5% of the sample interval
    double position = my.realPosition - (my.realDelta * last);

    for (int i = last; i >= 0; i--, position += my.realDelta) {
	if (fuzzyTimeMatch(my.timeData[i], position, tolerance)) {
	    console->post("Tab::adjustLiveWorldView: "
			  "skipped fetch, position[%d] matches existing time "
			  "(%s)", i, timeString(position));
	    continue;
	}

	my.timeData[i] = position;

	if (i == 0) {	// refreshCharts() finishes up last one
	    console->post("Fetching data[%d] at %s", i, timeString(position));
	    my.group->fetch();
	}
	else {
	    console->post("No live data for %s", timeString(position));
	    for (int j = 0; j < my.count; j++)
		my.charts[j]->updateNoLiveData(i);
	}
    }
    my.timeState = (packet->state == KmTime::StoppedState) ?
			Tab::StandbyState : Tab::ForwardState;
    newButtonState(packet->state, packet->mode, my.group->mode(), my.recording);
    refreshCharts();
}

void Tab::adjustArchiveWorldViewForward(KmTime::Packet *packet, bool setup)
{
    console->post("Tab::adjustArchiveWorldViewForward");
    my.timeState = Tab::ForwardState;

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

    for (int i = last; i >= 0; i--, position += my.realDelta) {
	if (setup == false &&
	    fuzzyTimeMatch(my.timeData[i], position, tolerance) == true) {
	    continue;
	}

	my.timeData[i] = position;

	struct timeval timeval;
	fromsec(position, &timeval);
	my.group->setArchiveMode(setmode, &timeval, delta);
	console->post("Fetching data[%d] at %s", i, timeString(position));
	my.group->fetch();
	if (i == 0)		// refreshCharts() finishes up last one
	    break;
	console->post("Tab::adjustArchiveWorldViewForward: "
		      "setting time position[%d]=%.2f[%s] state=%s count=%d",
			i, position, timeString(position),
			timeState(), my.count);
	for (int j = 0; j < my.count; j++)
	    my.charts[j]->update(true, false);
    }

    if (setup)
	packet->state = KmTime::StoppedState;
    newButtonState(packet->state, packet->mode, my.group->mode(), my.recording);
    refreshCharts();
}

void Tab::adjustArchiveWorldViewBackward(KmTime::Packet *packet, bool setup)
{
    console->post("Tab::adjustArchiveWorldViewBackward");
    my.timeState = Tab::BackwardState;

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

    for (int i = 0; i <= last; i++, position -= my.realDelta) {
	if (setup == false &&
	    fuzzyTimeMatch(my.timeData[i], position, tolerance) == true) {
	    continue;
	}

	my.timeData[i] = position;

	struct timeval timeval;
	fromsec(position, &timeval);
	my.group->setArchiveMode(setmode, &timeval, -delta);
	console->post("Fetching data[%d] at %s", i, timeString(position));
	my.group->fetch();
	if (i == last)		// refreshCharts() finishes up last one
	    break;
	console->post("Tab::adjustArchiveWorldViewBackward: "
		      "setting time position[%d]=%.2f[%s] state=%s count=%d",
			i, position, timeString(position),
			timeState(), my.count);
	for (int j = 0; j < my.count; j++)
	    my.charts[j]->update(false, false);
    }

    if (setup)
	packet->state = KmTime::StoppedState;
    newButtonState(packet->state, packet->mode, my.group->mode(), my.recording);
    refreshCharts();
}

void Tab::adjustArchiveWorldViewStop(KmTime::Packet *packet, bool needFetch)
{
    if (needFetch) {	// stopped, but VCR reposition event occurred
	adjustArchiveWorldViewForward(packet, needFetch);
	return;
    }
    my.timeState = Tab::StandbyState;
    packet->state = KmTime::StoppedState;
    newButtonState(packet->state, packet->mode, my.group->mode(), my.recording);
    updateTimeButton();
}

//
// Catch the situation where we get a larger than expected increase
// in position.  This happens when we restart after a stop in live
// mode (both with and without a change in the delta).
//
static bool sideStep(struct timeval, struct timeval, struct timeval)
{
    // thinks needs deeper thought / a rework, triggering too often atm
    // and adjustLiveWorldView isn't correct yet either, making it worse
    return false;
#if 0
    double interval = tosec(delta);
    double tolerance = interval / 20.0;	// 5% of the sample interval
    double newExpected = tosec(o) + interval;
    double newPosition = tosec(n);

    return fuzzyTimeMatch(newExpected, newPosition, tolerance) == false;
#endif
}

//
// Fetch all metric values across all plots and all charts,
// and also update the single time scale across the bottom.
//
void Tab::step(KmTime::Packet *packet)
{
    console->post(KmChart::DebugProtocol,
		  "Tab::step: stepping to time %.2f, delta=%.2f, state=%s",
		  tosec(packet->position), tosec(packet->delta), timeState());

    if ((packet->source == KmTime::ArchiveSource &&
	((packet->state == KmTime::ForwardState &&
		my.timeState != Tab::ForwardState) ||
	 (packet->state == KmTime::BackwardState &&
		my.timeState != Tab::BackwardState))) ||
	 sideStep(packet->position, my.position, my.delta))
	return adjustWorldView(packet, false);

    int last = my.samples - 1;
    my.kmtimeState = packet->state;
    my.realPosition = tosec(packet->position);

    if (packet->state == KmTime::ForwardState) { // left-to-right (all but 1st)
	if (my.samples > 1)
	    memmove(&my.timeData[1], &my.timeData[0], sizeof(double) * last);
	my.timeData[0] = my.realPosition;
    }
    else if (packet->state == KmTime::BackwardState) { // right-to-left
	if (my.samples > 1)
	    memmove(&my.timeData[0], &my.timeData[1], sizeof(double) * last);
	my.timeData[last] = my.realPosition - torange(my.delta, last);
    }

    my.group->fetch();
    newButtonState(packet->state, packet->mode, my.group->mode(), my.recording);
    refreshCharts();
}

void Tab::VCRMode(KmTime::Packet *packet, bool dragMode)
{
    if (!dragMode)
	adjustWorldView(packet, true);
}

void Tab::setTimezone(char *tz)
{
    console->post(KmChart::DebugProtocol, "Tab::setTimezone - %s", tz);
    my.group->useTZ(QString(tz));
    updateTimeAxis();
}

void Tab::setSampleHistory(int v)
{
    console->post("Tab::setSampleHistory (%d -> %d)", my.samples, v);
    if (my.samples != v) {
	my.samples = v;
	for (int i = 0; i < my.count; i++)
	    for (int m = 0; m < my.charts[i]->numPlot(); m++)
		my.charts[i]->resetDataArrays(m, my.samples);
	my.timeData = (double *)malloc(my.samples * sizeof(my.timeData[0]));
	if (my.timeData == NULL)
	    nomem();
	for (int i = 0; i < my.samples; i++)
	    my.timeData[i] = my.realPosition - (i * my.realDelta);
    }
}

int Tab::sampleHistory(void)
{
    return my.samples;
}

void Tab::setVisibleHistory(int v)
{
    console->post("Tab::setVisibleHistory (%d -> %d)", my.visible, v);
    if (my.visible != v) {
	my.visible = v;
	for (int i = 0; i < my.count; i++)
	    my.charts[i]->replot();
	kmchart->timeAxis()->replot();
    }
}

int Tab::visibleHistory(void)
{
    return my.visible;
}

double *Tab::timeAxisData(void)
{
    return my.timeData;
}

bool Tab::isRecording(void)
{
    return my.recording;
}

bool Tab::startRecording(void)
{
    RecordDialog record(this);

    console->post("Tab::startRecording");
    record.init(this);
    if (record.exec() != QDialog::Accepted)
	my.recording = false;
    else {	// write pmlogger/kmchart/pmafm configs and start up loggers.
	console->post("Tab::startRecording starting loggers");
	record.startLoggers();
	my.recording = true;
    }
    return my.recording;
}

void Tab::stopRecording(void)
{
    QString msg = "Q\n";
    int count = my.loggerList.size();

    console->post("Tab::stopRecording stopping %d logger(s)", count);
    for (int i = 0; i < count; i++) {
	my.loggerList.at(i)->write(msg.toAscii());
	my.loggerList.at(i)->terminate();
    }

#if 0
    int i, sts;
    for (i = 0; i < my.archiveList.size(); i++) {
	QString archive = my.archiveList.at(i);
	if ((sts = archiveGroup->use(PM_CONTEXT_ARCHIVE, archive)) < 0) {
	    archive.prepend(tr("Cannot open PCP archive: "));
	    archive.append(tr("\n"));
	    archive.append(tr(pmErrStr(sts)));
	    QMessageBox::warning(this, pmProgname, archive,
		    QMessageBox::Ok|QMessageBox::Default|QMessageBox::Escape,
		    QMessageBox::NoButton, QMessageBox::NoButton);
	    break;
	}
	archiveGroup->updateBounds();
	QmcSource source = archiveGroup->context()->source();
	kmtime->addArchive(source.start(), source.end(),
			   source.timezone(), source.host());
    }

    // If all is well, we can now create the new Tab
    if (i == my.archiveList.size()) {
	QString label = tr("Record"); // QFileInfo(my.folio).completeBaseName()
	Tab *tab = new Tab;

	tab->init(my.visible, my.samples, archiveGroup, KmTime::ArchiveSource,
		  kmtime->archiveInterval(), kmtime->archivePosition());
	kmchart->addTab(tab, label);
	kmchart->setActiveTab(tabs.size() - 1, false);
	OpenViewDialog::openView((const char *)my.view.toAscii());
    }
#endif

    cleanupRecording();
}

void Tab::cleanupRecording(void)
{
    my.recording = false;
    my.loggerList.clear();
    my.archiveList.clear();
    my.view = QString::null;
    my.folio = QString::null;
    kmchart->setRecordState(this, false);
}

void Tab::queryRecording(void)
{
    QString msg = "?\n";
    int count = my.loggerList.size();

    console->post("Tab::stopRecording querying %d logger(s)", count);
    for (int i = 0; i < count; i++)
	my.loggerList.at(i)->write(msg.toAscii());
}

void Tab::detachLoggers(void)
{
    QString msg = "D\n";
    int count = my.loggerList.size();

    console->post("Tab::detachLoggers detaching %d logger(s)", count);
    for (int i = 0; i < count; i++)
	my.loggerList.at(i)->write(msg.toAscii());
    cleanupRecording();
}

void Tab::addFolio(QString folio, QString view)
{
    my.view = view;
    my.folio = folio;
}

void Tab::addLogger(PmLogger *pmlogger, QString archive)
{
    my.loggerList.append(pmlogger);
    my.archiveList.append(archive);
}

TimeButton::State Tab::buttonState(void)
{
    return my.buttonState;
}

void Tab::newButtonState(KmTime::State s, KmTime::Mode m, int src, bool record)
{
    if (src != PM_CONTEXT_ARCHIVE) {
	if (s == KmTime::StoppedState)
	    my.buttonState = record ?
			TimeButton::StoppedRecord : TimeButton::StoppedLive;
	else
	    my.buttonState = record ?
			TimeButton::ForwardRecord : TimeButton::ForwardLive;
    }
    else if (m == KmTime::StepMode) {
	if (s == KmTime::ForwardState)
	    my.buttonState = TimeButton::StepForwardArchive;
	else if (s == KmTime::BackwardState)
	    my.buttonState = TimeButton::StepBackwardArchive;
	else
	    my.buttonState = TimeButton::StoppedArchive;
    }
    else if (m == KmTime::FastMode) {
	if (s == KmTime::ForwardState)
	    my.buttonState = TimeButton::FastForwardArchive;
	else if (s == KmTime::BackwardState)
	    my.buttonState = TimeButton::FastBackwardArchive;
	else
	    my.buttonState = TimeButton::StoppedArchive;
    }
    else if (s == KmTime::ForwardState)
	my.buttonState = TimeButton::ForwardArchive;
    else if (s == KmTime::BackwardState)
	my.buttonState = TimeButton::BackwardArchive;
    else
	my.buttonState = TimeButton::StoppedArchive;
}
