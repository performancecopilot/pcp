/*
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
 * Copyright (c) 2007, Nathan Scott.  All Rights Reserved.
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

#include "main.h"
#include <qtimer.h>
#include <qlayout.h>
#include <qwt/qwt_plot.h>
#include <qwt/qwt_plot_layout.h>
#include <qwt/qwt_scale_draw.h>
#include <qwt/qwt_scale_widget.h>
#include <qwt/qwt_text.h>
#include <qwt/qwt_text_label.h>
#include "recorddialog.h"

#define DESPERATE 0
#define REALLY_DESPERATE 0

#if REALLY_DESPERATE
void
dumpGeom(Tab *tab)
{
    QwtPlotLayout	*plp;
    QRect		r;
    int			i;

    for (i = 0; i < tab->numChart(); i++) {
	plp = tab->chart(i)->plotLayout();
	r = plp->titleRect();
	fprintf(stderr, "dumpGeom: chart #%d\n", i);
	fprintf(stderr, "  title pos: (%d,%d) %dx%d\n", r.left(), r.top(),
	r.width(), r.height());
	r = plp->legendRect();
	fprintf(stderr, "  legend pos: (%d,%d) %dx%d\n", r.left(), r.top(),
	r.width(), r.height());
	r = plp->scaleRect(QwtPlot::yLeft);
	fprintf(stderr, "  y-axis pos: (%d,%d) %dx%d\n", r.left(), r.top(),
	r.width(), r.height());
	r = plp->scaleRect(QwtPlot::xBottom);
	fprintf(stderr, "  x-axis pos: (%d,%d) %dx%d\n", r.left(), r.top(),
	r.width(), r.height());
	r = plp->canvasRect();
	fprintf(stderr, "  canvas pos: (%d,%d) %dx%d\n", r.left(), r.top(),
	r.width(), r.height());
    }
}
#endif

//#if DESPERATE
static char *timestate(int state)
{
    static char buf[16];

    switch (state) {
    case START_STATE:
	strcpy(buf, "START");
	break;
    case FORWARD_STATE:
	strcpy(buf, "FORWARD");
	break;
    case BACKWARD_STATE:
	strcpy(buf, "BACKWARD");
	break;
    case ENDLOG_STATE:
	strcpy(buf, "ENDLOG");
	break;
    case STANDBY_STATE:
	strcpy(buf, "STANDBY");
	break;
    }
    return buf;
}
//#endif

Tab::Tab(QWidget *parent): QWidget(parent)
{
    _num = 0;
    _splitter = NULL;
    _current = -1;
    _charts = NULL;
    _group = NULL;
    _samples = settings.sampleHistory;
    _visible = settings.visibleHistory;
    _interval = 0;
    _recording = false;
    _showdate = true;
    _timeData = NULL;
    _timestate = START_STATE;
    _buttonstate = BUTTON_TIMELESS;
    _lastkmstate = KM_STATE_STOP;
    memset(&_lastkmdelta, 0, sizeof(_lastkmdelta));
    memset(&_lastkmposition, 0, sizeof(_lastkmposition));
}

void Tab::showTimeControl(void)
{
    if (_mode == KM_SOURCE_HOST)
	kmtime->showLiveTimeControl();
    else
	kmtime->showArchiveTimeControl();
}

void Tab::init(QTabWidget *chartTab, int samples, int visible,
		PMC_Group *group, km_tctl_source mode, const char *label,
		struct timeval *ip, struct timeval *sp)
{
    int i;

    _splitter = new QSplitter(chartTab);
    _splitter->setOrientation(QSplitter::Vertical);
    _splitter->setMinimumSize(QSize(80, 80));
    _splitter->setMaximumSize(QSize(32767, 32767));
    _splitter->setSizePolicy(QSizePolicy::MinimumExpanding,
				QSizePolicy::MinimumExpanding);
    chartTab->insertTab(_splitter, tr(label));	// TODO: why not just addTab()?

    _loglist.setAutoDelete(TRUE);

    _group = group;
    _mode = mode;
    _buttonstate = mode==KM_SOURCE_HOST ? BUTTON_PLAYLIVE : BUTTON_STOPARCHIVE;

    _samples = samples;
    _visible = visible;
    _interval = tosec(*ip);
    _lastkmdelta = *ip;
    _lastkmposition = *sp;

    double position = tosec(_lastkmposition);
    _timeData = (double *)malloc(_samples * sizeof(_timeData[0]));
    for (i = 0; i < _samples; i++)
	_timeData[i] = position - (i * _interval);

    kmchart->timeAxis()->setAxisScale(QwtPlot::xBottom,
			_timeData[_visible-1], _timeData[0],
			kmchart->timeAxis()->scaleValue(_interval, _visible));
    kmchart->setButtonState(_buttonstate);
}

Chart *Tab::chart(int i)
{
    if (i >= 0 && i < _num)
	return _charts[i].cp;
    return NULL;
}

Chart *Tab::addChart(void)
{
    Chart *cp;

    _num++;
    _charts = (chart_t *)realloc(_charts, _num * sizeof(_charts[0]));
    cp = new Chart(this, activeTab->splitter());
    _charts[_num-1].cp = cp;
    setCurrent(cp);
    _current = _num-1;
#if DESPERATE
    fprintf(stderr, "Tab::addChart() [%d]->Chart %p\n", _current, cp);
#endif
    return cp;
}

int Tab::deleteCurrent(void)
{
    return deleteChart(_current);
}

int Tab::deleteChart(Chart *cp)
{
    for (int j = 0; j < _num-1; j++)
	if (_charts[j].cp == cp)
	    return deleteChart(j);
    return 0;
}

int Tab::deleteChart(int idx)
{
    int	new_current = _current;

    _charts[idx].cp->~Chart();
    // shuffle left, don't bother with the realloc()
    for (int j = idx; j < _num-1; j++)
	_charts[j] = _charts[j+1];
    if (idx < new_current || idx == _num-1) {
	// old current slot no longer available, choose previous slot
	new_current--;
    }
    _num--;
    _current = -1;		// force re-assignment and re-highlighting
    setCurrent(_charts[new_current].cp);
    // TODO - need to resize top-level (?) window to be smaller in
    // height now ... something like scaling by _num/(_num+1)
    return _current;
}

int Tab::numChart(void)
{
    return _num;
}

bool Tab::isArchiveMode(void)
{
    return _mode == KM_SOURCE_ARCHIVE;
}

Chart *Tab::currentChart(void)
{
    return _charts[_current].cp;
}

int Tab::setCurrent(Chart *cp)
{
    int		i;

    for (i = 0; i < _num; i++) {
	if (_charts[i].cp == cp) {
	    QwtScaleWidget	*sp;
	    QwtText		t;
#if DESPERATE
	    fprintf(stderr, "Tab::setCurrentChart(%p) -> %d\n", cp, i);
#endif
	    if (_current == i)
		return _current;
	    if (_current != -1) {
		// reset highlight for old current
		t = _charts[_current].cp->titleLabel()->text();
		t.setColor("black");
		_charts[_current].cp->setTitle(t);
		sp = _charts[_current].cp->axisWidget(QwtPlot::yLeft);
		t = sp->title();
		t.setColor("black");
		sp->setTitle(t);
		sp->setPaletteForegroundColor("black");	// TODO -- not working
		sp = _charts[_current].cp->axisWidget(QwtPlot::xBottom);
		sp->setPaletteForegroundColor("black");	// TODO -- not working
	    }
	    _current = i;
	    // set highlight for new current
	    t = cp->titleLabel()->text();
	    t.setColor(settings.chartHighlight);
	    cp->setTitle(t);
	    sp = cp->axisWidget(QwtPlot::yLeft);
	    t = sp->title();
	    t.setColor(settings.chartHighlight);
	    sp->setTitle(t);
	    sp = cp->axisWidget(QwtPlot::xBottom);
	    return _current;
	}
    }
    // TODO -- error code?
    return -1;
}

PMC_Group *Tab::group()
{
    return _group;
}

void Tab::updateTimeAxis(void)
{
    PMC_String label, tz;

    if (_group->numContexts() > 0) {
	_group->useTZ();
	_group->defaultTZ(label, tz);
	kmchart->timeAxis()->setAxisScale(QwtPlot::xBottom,
			_timeData[_visible-1], _timeData[0],
			kmchart->timeAxis()->scaleValue(_interval, _visible));
    } else {
	tz = tr("UTC");
	kmchart->timeAxis()->setAxisScale(QwtPlot::xBottom, 0, 0,
			kmchart->timeAxis()->scaleValue(_interval, _visible));
    }
    kmchart->timeAxis()->replot();
    kmchart->setDateLabel(_lastkmposition.tv_sec, tz.ptr());

#if DESPERATE
    fprintf(stderr, "%s: used %s TZ (%s), final time is %.3f (%s)\n", __func__,
		tz.ptr(), label.ptr(), _timeData[0], timestring(_timeData[0]));
#endif
}

void Tab::updateTimeButton(void)
{
    kmchart->setButtonState(_buttonstate);
}

//
// Drive all updates into each chart (refresh the display)
//
void Tab::refresh_charts(void)
{
    int i;

#if DESPERATE
    for (i = 0; i < _samples; i++)
	fprintf(stderr, "%s: timeData[%2d] is %.2f (%s)\n",
		__FUNCTION__, i, _timeData[i], timestring(_timeData[i]));
    fprintf(stderr, "%s: state=%s\n", __FUNCTION__, timestate(_timestate));
#endif

    for (i = 0; i < _num; i++) {
	_charts[i].cp->setAxisScale(QwtPlot::xBottom, 
			_timeData[_visible-1], _timeData[0],
			kmchart->timeAxis()->scaleValue(_interval, _visible));
	_charts[i].cp->update(_timestate != BACKWARD_STATE, true);
	_charts[i].cp->fixLegendPen();
    }

#if REALLY_DESPERATE
    dumpGeom(this);
#endif

    if (this == activeTab) {
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
    if (isArchiveMode()) {
	kmTime k;
	k.source = KM_SOURCE_ARCHIVE;
	k.state = KM_STATE_FORWARD;
	k.mode = KM_MODE_NORMAL;
	memcpy(&k.delta, kmtime->archiveInterval(), sizeof(k.delta));
	memcpy(&k.position, kmtime->archivePosition(), sizeof(k.position));
	memcpy(&k.start, kmtime->archiveStart(), sizeof(k.start));
	memcpy(&k.end, kmtime->archiveEnd(), sizeof(k.end));
	adjustArchiveWorldView(&k, TRUE);
    }
    for (int m = 0; m < _num; m++)
	_charts[m].cp->show();
}

//
// Received a set or a vcrmode requiring us to adjust our state
// and possibly rethink everything.  This can result from a time
// control position change, delta change, direction change, etc.
//
void Tab::adjustWorldView(kmTime *kmtime)
{
    if (isArchiveMode())
	adjustArchiveWorldView(kmtime, FALSE);
    else
	adjustLiveWorldView(kmtime);
}

void Tab::adjustArchiveWorldView(kmTime *kmtime, bool need_fetch)
{
    if (kmtime->state == KM_STATE_FORWARD)
	adjustArchiveWorldViewForward(kmtime, need_fetch);
    else if (kmtime->state == KM_STATE_BACKWARD)
	adjustArchiveWorldViewBackward(kmtime, need_fetch);
    else
	adjustArchiveWorldViewStop(kmtime);
}

void Tab::adjustLiveWorldView(kmTime *kmtime)
{
    int i, j, delta, setmode;
    double interval, position;
    int last = _samples - 1;

    delta = kmtime->delta.tv_sec;
    setmode = PM_MODE_LIVE;
    if (kmtime->delta.tv_usec == 0) {
	setmode |= PM_XTB_SET(PM_TIME_SEC);
    } else {
	delta = delta * 1000 + kmtime->delta.tv_usec / 1000;
	setmode |= PM_XTB_SET(PM_TIME_MSEC);
    }

    _lastkmdelta = kmtime->delta;
    interval = tosec(kmtime->delta);
    _lastkmposition = kmtime->position;
    position = tosec(kmtime->position);

    //
    // X-Axis _max_ becomes kmtime->position.
    // Rest of (preceeding) time window filled in using kmtime->delta.
    // In live mode, we can only fetch current data.  However, we make
    // an effort to keep old data that happens to align with the delta
    // time points that we are now interested in.
    //
    position -= (interval * last);
    for (i = last; i >= 0; i--, position += interval) {
	if (_timeData[i] == position) {
fprintf(stderr, "%s: skip fetch, position[%d] matches existing\n", __func__, i);
	    continue;
	}
	_timeData[i] = position;
	if (i == 0) {	// refresh_charts() finishes up last one
	    _group->fetch();
	    break;
	} else {
	    // TODO: nuke old data that we're now overwriting? (new start/delta)
fprintf(stderr, "%s: live mode TODO case, position[%d]\n", __func__, i);
	}
	for (j = 0; j < _num; j++)
	    _charts[j].cp->update(_timestate != BACKWARD_STATE, false);
    }
    _timestate = kmtime->state == KM_STATE_STOP? STANDBY_STATE : FORWARD_STATE;
    newButtonState(kmtime->state, kmtime->mode, _group->mode(), _recording);
    refresh_charts();
}

void Tab::adjustArchiveWorldViewForward(kmTime *kmtime, bool setup)
{
    struct timeval timeval;
    double interval, position;
    int i, j, delta, setmode;
    int last = _samples - 1;

    _timestate = FORWARD_STATE;

    setmode = PM_MODE_FORW;
    delta = kmtime->delta.tv_sec;
    if (kmtime->delta.tv_usec == 0) {
	setmode |= PM_XTB_SET(PM_TIME_SEC);
    } else {
	delta = delta * 1000 + kmtime->delta.tv_usec / 1000;
	setmode |= PM_XTB_SET(PM_TIME_MSEC);
    }

    _lastkmdelta = kmtime->delta;
    _lastkmposition = kmtime->position;
    interval = tosec(kmtime->delta);
    position = tosec(kmtime->position);

//#if DESPERATE
    fprintf(stderr, "%s: sh=%d vh=%d delta=%.2f position=%.2f (%s) state=%s\n",
		__func__, _samples, _visible, interval, position,
		timestring(position), timestate(_timestate));
//#endif

    //
    // X-Axis _max_ becomes kmtime->position.
    // Rest of (preceeding) time window filled in using kmtime->delta.
    //
    position -= (interval * last);
    for (i = last; i >= 0; i--, position += interval) {
	// TODO: fuzzy position match needed here
	if (setup == FALSE && _timeData[i] == position) {
fprintf(stderr, "%s: skip fetch, position[%d] matches existing\n", __func__, i);
	    continue;
	}
	_timeData[i] = position;
	fromsec(position, &timeval);
	if (_group->setArchiveMode(setmode, &timeval, delta) < 0) {
fprintf(stderr, "%s: eh? bailed on setArchiveMode?\n", __func__);
	    continue;
	}
	_group->fetch();
	if (i == 0)		// refresh_charts() finishes up last one
	    break;
fprintf(stderr, "%s: setting a time position[%d]=%.2f (%s), state=%s num=%d\n", __func__, i, position, timestring(position), timestate(_timestate), _num);
	for (j = 0; j < _num; j++) {
	    _charts[j].cp->update(_timestate != BACKWARD_STATE, false);
	}
    }

    if (setup)
	kmtime->state = KM_STATE_STOP;
    newButtonState(kmtime->state, kmtime->mode, _group->mode(), _recording);
    refresh_charts();
}

void Tab::adjustArchiveWorldViewBackward(kmTime *kmtime, bool setup)
{
    struct timeval timeval;
    double interval, position;
    int i, j, delta, setmode;
    int last = _samples - 1;

    _timestate = BACKWARD_STATE;

    setmode = PM_MODE_BACK;
    delta = kmtime->delta.tv_sec;
    if (kmtime->delta.tv_usec == 0) {
	setmode |= PM_XTB_SET(PM_TIME_SEC);
    } else {
	delta = delta * 1000 + kmtime->delta.tv_usec / 1000;
	setmode |= PM_XTB_SET(PM_TIME_MSEC);
    }

    _lastkmdelta = kmtime->delta;
    _lastkmposition = kmtime->position;
    interval = tosec(kmtime->delta);
    position = tosec(kmtime->position);

//#if DESPERATE
    fprintf(stderr, "%s: sh=%d vh=%d delta=%.2f position=%.2f (%s) state=%s\n",
		__func__, _samples, _visible, interval, position,
		timestring(position), timestate(_timestate));
//#endif

    //
    // X-Axis _min_ becomes kmtime->position.
    // Rest of (following) time window filled in using kmtime->delta.
    //
    for (i = 0; i <= last; i++, position -= interval) {
	// TODO: fuzzy position match needed here
	if (setup == FALSE && _timeData[i] == position) {
fprintf(stderr, "%s: skip fetch, position[%d] matches existing\n", __func__, i);
	    continue;
	}
	_timeData[i] = position;
	fromsec(position, &timeval);
fprintf(stderr, "%s: fetching for position[%d] at %s\n", __func__, i, timestring(position));
	if (_group->setArchiveMode(setmode, &timeval, delta) < 0)
	    continue;
	_group->fetch();
	if (i == last)		// refresh_charts() finishes up last one
	    break;
fprintf(stderr, "%s: setting a time position[%d]=%.2f (%s), state=%s num=%d\n", __func__, i, position, timestring(position), timestate(_timestate), _num);
	for (j = 0; j < _num; j++) {
	    _charts[j].cp->update(_timestate != BACKWARD_STATE, false);
	}
    }

    if (setup)
	kmtime->state = KM_STATE_STOP;
    newButtonState(kmtime->state, kmtime->mode, _group->mode(), _recording);
    refresh_charts();
}

void Tab::adjustArchiveWorldViewStop(kmTime *kmtime)
{
    _timestate = STANDBY_STATE;
    newButtonState(kmtime->state, kmtime->mode, _group->mode(), _recording);
    refresh_charts();
}

//
// Fetch all metric values across all plots and all charts,
// and also update the single time scale across the bottom.
//
void Tab::step(kmTime *kmtime)
{
    int last;

    if (pmDebug & DBG_TRACE_TIMECONTROL) {
	fprintf(stderr, "%s: stepping to time %.2f, delta=%.2f, state=%d\n",
		__FUNCTION__, tosec(kmtime->position), tosec(kmtime->delta),
		_timestate);
    }

    if (kmtime->source == KM_SOURCE_ARCHIVE &&
	((kmtime->state == KM_STATE_FORWARD && _timestate != FORWARD_STATE) ||
	 (kmtime->state == KM_STATE_BACKWARD && _timestate != BACKWARD_STATE)))
	return adjustWorldView(kmtime);

    last = _samples - 1;
    _lastkmstate = kmtime->state;
    _lastkmdelta = kmtime->delta;
    _lastkmposition = kmtime->position;

    if (kmtime->state == KM_STATE_FORWARD) {	// left-to-right (all but 1st)
	if (_samples > 1)
	    memmove(&_timeData[1], &_timeData[0], sizeof(double) * last);
	_timeData[0] = tosec(kmtime->position);
    }
    else if (kmtime->state == KM_STATE_BACKWARD) {	// right-to-left
	if (_samples > 1)
	    memmove(&_timeData[0], &_timeData[1], sizeof(double) * last);
	_timeData[last] = tosec(_lastkmposition) - torange(_lastkmdelta, last);
    }

    _group->fetch();
    refresh_charts();
}

void Tab::vcrmode(kmTime *kmtime, bool dragmode)
{
    if (dragmode)	// TODO
	return;
    adjustWorldView(kmtime);
}

void Tab::setTimezone(char *tz)
{
#if DESPERATE
    fprintf(stderr, "%s (%s)\n", __func__, tz);
#endif
    _group->useTZ(PMC_String(tz));
}

void Tab::setSampleHistory(int v)
{
#if DESPERATE
    fprintf(stderr, "%s (%d->%d)\n", __func__, _samples, v);
#endif
    if (_samples != v) {
	_samples = v;
	for (int i = 0; i < _num; i++)
	    for (int m = 0; m < _charts[i].cp->numPlot(); m++)
		_charts[i].cp->resetDataArrays(m, _samples);
	_timeData = (double *)malloc(_samples * sizeof(_timeData[0]));
	if (_timeData == NULL)
	    nomem();
	double position = tosec(_lastkmposition);
	for (int i = 0; i < _samples; i++)
	    _timeData[i] = position - (i * _interval);
    }
}

int Tab::sampleHistory(void)
{
    return _samples;
}

void Tab::setVisibleHistory(int v)
{
#if DESPERATE
    fprintf(stderr, "%s (%d->%d)\n", __func__, _visible, v);
#endif
    if (_visible != v) {
	_visible = v;
	for (int i = 0; i < _num; i++)
	    _charts[i].cp->replot();
    }
}

int Tab::visibleHistory(void)
{
    return _visible;
}

double *Tab::timeData(void)
{
    return _timeData;
}

bool Tab::isRecording(void)
{
    return _recording;
}

int Tab::startRecording(void)
{
    RecordDialog record;

    record.init();
    if (record.exec() != QDialog::Accepted) {
	_recording = false;
	return 1;
    }

    // write pmlogger, kmchart and pmafm configs, then start up the loggers.
    record.startLoggers();

    _recording = true;
    return 0;
}

void Tab::stopRecording(void)
{
    PmLogger *log;

    for (log = _loglist.first(); log; log = _loglist.next()) {
	log->setTerminating();
	log->tryTerminate();
    }

    QString msg = tr("Recording process(es) complete.  To replay, run:\n");
    msg.append("  $ pmafm ");
    msg.append(_folio);
    msg.append(" replay\n");
    QMessageBox::information(kmchart, pmProgname, msg);

    _loglist.clear();
    _folio = QString::null;
    _recording = false;
}

void Tab::setFolio(QString folio)
{
    _folio = folio;
}

void Tab::addLogger(PmLogger *pmlogger)
{
    fprintf(stderr, "%s: log=0x%p folio=%s\n", __func__, pmlogger, _folio.ascii());
    _loglist.append(pmlogger);
}

KmButtonState Tab::buttonState(void)
{
    return _buttonstate;
}

km_tctl_state Tab::kmtimeState(void)
{
    return _lastkmstate;
}

void Tab::newButtonState(km_tctl_state s, km_tctl_mode m, int mode, bool record)
{
    if (mode != PM_CONTEXT_ARCHIVE) {
	if (s == KM_STATE_STOP)
	    _buttonstate = record ? BUTTON_STOPRECORD : BUTTON_STOPLIVE;
	else
	    _buttonstate = record ? BUTTON_PLAYRECORD : BUTTON_PLAYLIVE;
    }
    else if (m == KM_MODE_STEP) {
	if (s == KM_STATE_FORWARD)
	    _buttonstate = BUTTON_STEPFWDARCHIVE;
	else if (s == KM_STATE_BACKWARD)
	    _buttonstate = BUTTON_STEPBACKARCHIVE;
	else
	    _buttonstate = BUTTON_STOPARCHIVE;
    }
    else if (m == KM_MODE_FAST) {
	if (s == KM_STATE_FORWARD)
	    _buttonstate = BUTTON_FASTFWDARCHIVE;
	else if (s == KM_STATE_BACKWARD)
	    _buttonstate = BUTTON_FASTBACKARCHIVE;
	else
	    _buttonstate = BUTTON_STOPARCHIVE;
    }
    else if (s == KM_STATE_FORWARD)
	_buttonstate = BUTTON_PLAYARCHIVE;
    else if (s == KM_STATE_BACKWARD)
	_buttonstate = BUTTON_BACKARCHIVE;
    else
	_buttonstate = BUTTON_STOPARCHIVE;
}

