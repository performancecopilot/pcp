/****************************************************************************
** ui.h extension file, included from the uic-generated form implementation.
**
** If you want to add, delete, or rename functions or slots, use
** Qt Designer to update this file, preserving your code.
**
** You should not define a constructor or destructor in this file.
** Instead, write your code in functions called init() and destroy().
** These will automatically be called by the form's constructor and
** destructor.
*****************************************************************************/

#include <qdir.h>
#include <qtimer.h>
#include <qstatusbar.h>
#include <qvalidator.h>
#include <qmessagebox.h>
#include <qwt/qwt_wheel.h>
#include <qwt/qwt_slider.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include "main.h"
#include "aboutdialog.h"
#include "aboutpcpdialog.h"
#include "../../images/stop_on.xpm"
#include "../../images/stop_off.xpm"
#include "../../images/play_on.xpm"
#include "../../images/play_off.xpm"
#include "../../images/back_on.xpm"
#include "../../images/back_off.xpm"
#include "../../images/fastfwd_on.xpm"
#include "../../images/fastfwd_off.xpm"
#include "../../images/fastback_on.xpm"
#include "../../images/fastback_off.xpm"
#include "../../images/stepfwd_on.xpm"
#include "../../images/stepfwd_off.xpm"
#include "../../images/stepback_on.xpm"
#include "../../images/stepback_off.xpm"

typedef struct {
    km_tctl_state	state;
    km_tctl_mode	mode;
    const char		**back_xpm;
    QPixmap 		*back_pixmap;
    const char		**stop_xpm;
    QPixmap 		*stop_pixmap;
    const char		**play_xpm;
    QPixmap 		*play_pixmap;
} tctl_h;

static tctl_h tctl[] = {
    { KM_STATE_STOP, KM_MODE_NORMAL,
	back_off_xpm, NULL, stop_on_xpm, NULL, play_off_xpm, NULL },
    { KM_STATE_FORWARD, KM_MODE_NORMAL,
	back_off_xpm, NULL, stop_off_xpm, NULL, play_on_xpm, NULL },
    { KM_STATE_BACKWARD, KM_MODE_NORMAL,
	back_on_xpm, NULL, stop_off_xpm, NULL, play_off_xpm, NULL },
    { KM_STATE_STOP, KM_MODE_FAST,
	fastback_off_xpm, NULL, stop_on_xpm, NULL, fastfwd_off_xpm, NULL },
    { KM_STATE_FORWARD, KM_MODE_FAST,
	fastback_off_xpm, NULL, stop_off_xpm, NULL, fastfwd_on_xpm, NULL },
    { KM_STATE_BACKWARD, KM_MODE_FAST,
	fastback_on_xpm, NULL, stop_off_xpm, NULL, fastfwd_off_xpm, NULL },
    { KM_STATE_STOP, KM_MODE_STEP,
	stepback_off_xpm, NULL, stop_on_xpm, NULL, stepfwd_off_xpm, NULL },
    { KM_STATE_FORWARD, KM_MODE_STEP,
	stepback_off_xpm, NULL, stop_off_xpm, NULL, stepfwd_on_xpm, NULL },
    { KM_STATE_BACKWARD, KM_MODE_STEP,
	stepback_on_xpm, NULL, stop_off_xpm, NULL, stepfwd_off_xpm, NULL },
};

static int nctl = sizeof(tctl) / sizeof(tctl[0]);

void KmTimeArch::setControl(km_tctl_state newstate, km_tctl_mode newmode)
{
    if (_kmtime.state != newstate || _kmtime.mode != newmode) {
	for (int i = 0; i < nctl; i++) {
	    if (tctl[i].state == newstate && tctl[i].mode == newmode) {
		if (tctl[i].back_pixmap == NULL)
		    tctl[i].back_pixmap = new QPixmap(tctl[i].back_xpm);
		pushButtonBack->setPixmap(*tctl[i].back_pixmap);
		if (tctl[i].stop_pixmap == NULL)
		    tctl[i].stop_pixmap = new QPixmap(tctl[i].stop_xpm);
		pushButtonStop->setPixmap(*tctl[i].stop_pixmap);
		if (tctl[i].play_pixmap == NULL)
		    tctl[i].play_pixmap = new QPixmap(tctl[i].play_xpm);
		pushButtonPlay->setPixmap(*tctl[i].play_pixmap);
		
	    }
	}
	_kmtime.state = newstate;
	_kmtime.mode = newmode;
	if (_kmtime.mode == KM_MODE_NORMAL) {
	    pushButtonSpeed->setEnabled(TRUE);
	    textLabelSpeed->setEnabled(TRUE);
	    lineEditSpeed->setEnabled(TRUE);
	    wheelSpeed->setEnabled(TRUE);
	}
	else {
	    pushButtonSpeed->setEnabled(FALSE);
	    textLabelSpeed->setEnabled(FALSE);
	    lineEditSpeed->setEnabled(FALSE);
	    wheelSpeed->setEnabled(FALSE);
	}
    }
}

void KmTimeArch::init(Console *cp)
{
    char utc[] = "UTC\0Universal Coordinated Time";

    _console = cp;
    _console->post(DBG_APP, "Starting Archive Time Control...");

    _units = Sec;
    _first = true;
    _tzActions = NULL;

    memset(&absoluteStart, 0, sizeof(struct timeval));
    memset(&absoluteEnd, 0, sizeof(struct timeval));
    memset(&_kmtime, 0, sizeof(_kmtime));
    _kmtime.source = KM_SOURCE_ARCHIVE;
    _kmtime.delta.tv_sec = KM_DEFAULT_DELTA;

    _show_msec = FALSE;
    optionsDetailShow_MillisecondsAction->setOn(_show_msec);
    _show_year = FALSE;
    optionsDetailShow_YearAction->setOn(_show_year);

    _timer = new QTimer(this);
    _timer->changeInterval((_kmtime.delta.tv_sec * 1000) +
			   (_kmtime.delta.tv_usec / 1000));
    _timer->stop();
    connect(_timer, SIGNAL(timeout()), SLOT(timerTick()));

    addTimezone(utc);
    displayDeltaText();
    setControl(KM_STATE_STOP, KM_MODE_NORMAL);

    double delta = secondsFromTV(&_kmtime.delta);
    changeSpeed(KM_DEFAULT_SPEED(delta));
    wheelSpeed->setRange(KM_MINIMUM_SPEED(delta), KM_MAXIMUM_SPEED(delta), 0.1);
    wheelSpeed->setValue(KM_DEFAULT_SPEED(delta));
    lineEditDelta->setAlignment(Qt::AlignRight);
    lineEditDelta->setValidator(
		new QDoubleValidator(0.001, ULONG_MAX, 3, lineEditDelta));
    lineEditSpeed->setAlignment(Qt::AlignRight);
    lineEditSpeed->setValidator(
		new QDoubleValidator(0.001, UINT_MAX, 1, lineEditSpeed));

    _bounds = new ShowBounds(this);
    _bounds->init(_console, &absoluteStart, &_kmtime.start,
			    &absoluteEnd, &_kmtime.end);
    connect(_bounds, SIGNAL(boundsChanged()), this, SLOT(doneBounds()));

    _console->post(DBG_APP, "%s: absS=%p S=%p absE=%p E=%p\n", __func__,
		   &absoluteStart, &_kmtime.start, &absoluteEnd, &_kmtime.end);

    _assistant = new QAssistantClient(tr(""), this);
}

void KmTimeArch::helpAbout()
{
    About about;
    about.exec();
}

void KmTimeArch::helpAboutPCP()
{
    AboutPCP about;
    about.exec();
}

void KmTimeArch::whatsThis()
{
    QMainWindow::whatsThis();
}

void KmTimeArch::playClicked()
{
    if (lineEditCtime->isModified())
	lineEditCtime_validate();
    if (lineEditDelta->isModified())
	lineEditDelta_validate();
    if (_kmtime.state != KM_STATE_FORWARD || _kmtime.mode == KM_MODE_STEP)
	play();
}

void KmTimeArch::play()
{
    if (addDelta()) {
	setControl(KM_STATE_FORWARD, _kmtime.mode);
	updateTime();
	if (_kmtime.mode == KM_MODE_NORMAL)
	    _timer->start((int)(((_kmtime.delta.tv_sec * 1000) +
				 (_kmtime.delta.tv_usec / 1000)) / _speed));
	else if (_kmtime.mode == KM_MODE_FAST)
	    _timer->start(KM_FASTMODE_DELAY);
	_console->post(DBG_APP, "%s moved time forward", __func__);
    } else {
	_console->post(DBG_APP, "%s reached archive end", __func__);
	emit boundsPulse(&_kmtime);
	stop();
    }
}

void KmTimeArch::backClicked()
{
    if (lineEditCtime->isModified())
	lineEditCtime_validate();
    if (lineEditDelta->isModified())
	lineEditDelta_validate();
    if (_kmtime.state != KM_STATE_BACKWARD || _kmtime.mode == KM_MODE_STEP)
	back();
}
    
void KmTimeArch::back()
{
    if (subDelta()) {
	setControl(KM_STATE_BACKWARD, _kmtime.mode);
	updateTime();
	if (_kmtime.mode == KM_MODE_NORMAL)
	    _timer->start((int)(((_kmtime.delta.tv_sec * 1000) +
				 (_kmtime.delta.tv_usec / 1000)) / _speed));
	else if (_kmtime.mode == KM_MODE_FAST)
	    _timer->start(KM_FASTMODE_DELAY);
	_console->post(DBG_APP, "%s moved time backward", __func__);
    } else {
	_console->post(DBG_APP, "%s reached archive end", __func__);
	emit boundsPulse(&_kmtime);
	stop();
    }
}

void KmTimeArch::stopClicked()
{
    if (_kmtime.state != KM_STATE_STOP)
	stop();
}

void KmTimeArch::stop()
{
    setControl(KM_STATE_STOP, _kmtime.mode);
    _timer->stop();
    emit vcrModePulse(&_kmtime, 0);
    _console->post(DBG_APP, "%s halted progression of time", __func__);
}

void KmTimeArch::timerTick()
{
    if (_kmtime.state == KM_STATE_FORWARD)
	play();
    else if (_kmtime.state == KM_STATE_BACKWARD)
	back();
    else
	_console->post(DBG_APP, "%s -- hmm?  bad state?", __func__);
}

int KmTimeArch::addDelta()
{
    struct timeval current = _kmtime.position;

#ifdef DESPERATE
    _console->post(DBG_PROTO,
	"%s: now=%u.%u end=%u.%u start=%u.%u delta=%u.%u speed=%.3e",
    	__func__, _kmtime.position.tv_sec, _kmtime.position.tv_usec,
    	_kmtime.end.tv_sec, _kmtime.end.tv_usec, _kmtime.start.tv_sec,
	_kmtime.start.tv_usec, _kmtime.delta.tv_sec, _kmtime.delta.tv_usec,
	_speed);
#endif

    tadd(&current, &_kmtime.delta);
    if (tcmp(&current, &_kmtime.end) > 0 || tcmp(&current, &_kmtime.start) < 0)
	return 0;
    _kmtime.position = current;
    return 1;
}

int KmTimeArch::subDelta()
{
    struct timeval current = _kmtime.position;

    tsub(&current, &_kmtime.delta);
    if (tcmp(&current, &_kmtime.end) > 0 || tcmp(&current, &_kmtime.start) < 0)
	return 0;
    _kmtime.position = current;
    return 1;
}

void KmTimeArch::changeDelta(int value)
{
    _units = (delta_units)value;
    displayDeltaText();
}

void KmTimeArch::changeControl(int value)
{
    setControl(KM_STATE_STOP, (km_tctl_mode)value);
}

void KmTimeArch::updateTime()
{
    emit timePulse(&_kmtime);
    displayPositionText();
    displayPositionSlide();
}

void KmTimeArch::displayDeltaText()
{
    QString 	text;
    double	delta = secondsFromTV(&_kmtime.delta);

    delta = secondsToUnits(delta, _units);
    if ((double)(int)delta == delta)
	text.sprintf("%.2f", delta);
    else
	text.sprintf("%.6f", delta);
    lineEditDelta->setText(text);
}

void KmTimeArch::displayPositionText()
{
    QString 	text;
    char	ctimebuf[32], msecbuf[5];

    pmCtime(&_kmtime.position.tv_sec, ctimebuf);
    text = tr(ctimebuf);
    if (_show_year == FALSE)
	text.remove(19, 5);
    if (_show_msec == TRUE) {
	sprintf(msecbuf, ".%03u", (uint)_kmtime.position.tv_usec / 1000);
	text.insert(19, msecbuf);
    }
    lineEditCtime->setText(text.stripWhiteSpace());
}

void KmTimeArch::displayPositionSlide(void)
{
    sliderPosition->setValue(secondsFromTV(&_kmtime.position));
}

void KmTimeArch::setPositionSlideRange(void)
{
    sliderPosition->setRange(secondsFromTV(&_kmtime.start),
			     secondsFromTV(&_kmtime.end));
}

void KmTimeArch::setPositionSlideDelta(void)
{
    sliderPosition->setStep(secondsFromTV(&_kmtime.delta));
}

void KmTimeArch::pressedPosition()
{
    emit vcrModePulse(&_kmtime, 1);
}

void KmTimeArch::releasedPosition()
{
    emit vcrModePulse(&_kmtime, 0);
}

void KmTimeArch::changedPosition(double value)
{
#define DESPERATE 1
#ifdef DESPERATE
    _console->post(DBG_APP, "%s changing pos from %d.%d", __func__,
    			_kmtime.position.tv_sec, _kmtime.position.tv_usec);
#endif

    secondsToTV(value, &_kmtime.position);
    displayPositionText();

#ifdef DESPERATE
    _console->post(DBG_APP, "%s changed pos to %d.%d", __func__,
    			_kmtime.position.tv_sec, _kmtime.position.tv_usec);
#endif
}

void KmTimeArch::clickShowMsec()
{
    if (_show_msec == TRUE) _show_msec = FALSE;
    else _show_msec = TRUE;
    optionsDetailShow_MillisecondsAction->setOn(_show_msec);
    displayPositionText();
}

void KmTimeArch::clickShowYear()
{
    if (_show_year == TRUE) _show_year = FALSE;
    else _show_year = TRUE;
    optionsDetailShow_YearAction->setOn(_show_year);
    displayPositionText();
}

void KmTimeArch::resetSpeed()
{
    changeSpeed(KM_DEFAULT_SPEED(secondsFromTV(&_kmtime.delta)));
}

void KmTimeArch::changeSpeed(double value)
{
    QString 	text;
    int		reset = _timer->isActive();
    double	upper, lower, delta = secondsFromTV(&_kmtime.delta);

    _timer->stop();

    upper = KM_MAXIMUM_SPEED(delta);
    lower = KM_MINIMUM_SPEED(delta);
    if (value > upper)
	value = upper;
    else if (value < lower)
	value = lower;
    text.sprintf("%.1f", value);
    lineEditSpeed->setText(text);
    if (wheelSpeed->value() != value)
	wheelSpeed->setValue(value);

    _speed = value;
    if (reset)
	_timer->start((int)(((_kmtime.delta.tv_sec * 1000) +
			     (_kmtime.delta.tv_usec / 1000)) / _speed));

    _console->post(DBG_APP, "%s changed delta to %d.%d (%.2fs)", __func__,
			_kmtime.delta.tv_sec, _kmtime.delta.tv_usec, value);
}

void KmTimeArch::showBounds()
{
    _bounds->reset();
    _console->post(DBG_APP, "%s: absS=%p S=%p absE=%p E=%p\n", __func__,
		   &absoluteStart, &_kmtime.start, &absoluteEnd, &_kmtime.end);
    _bounds->show();
}

void KmTimeArch::doneBounds(void)
{
    int tellclients = 0;

    _console->post(DBG_APP, "%s: signal received\n", __func__);

    _bounds->flush();
    if (tcmp(&_kmtime.position, &_kmtime.start) < 0) {
	_kmtime.position = _kmtime.start;
	tellclients = 1;
    }
    if (tcmp(&_kmtime.position, &_kmtime.end) > 0) {
	_kmtime.position = _kmtime.end;
	tellclients = 1;
    }
    setPositionSlideRange();
    if (tellclients)
	emit vcrModePulse(&_kmtime, 0);
}

void KmTimeArch::showConsole()
{
    _console->show();
}

void KmTimeArch::disableConsole()
{
    optionsShowConsoleAction->setVisible(FALSE);
}

void KmTimeArch::hideWindow()
{
    if (isVisible()) hide();
    else show();
}

void KmTimeArch::popup(int hello_popetts)
{
    if (hello_popetts) show();
    else hide();
}

void KmTimeArch::closeEvent(QCloseEvent *ce)
{
    hide();
    ce->ignore();
}

void KmTimeArch::lineEditDelta_changed(const QString &s)
{
    (void)s;
    if (lineEditDelta->isModified())
	stopClicked();
}

void KmTimeArch::lineEditCtime_changed(const QString &s)
{
    (void)s;
    if (lineEditCtime->isModified())
	stopClicked();
}

void KmTimeArch::lineEditDelta_validate()
{
    double delta;
    bool ok, reset = _timer->isActive();

    delta = lineEditDelta->text().toDouble(&ok);
    if (!ok || delta <= 0) {
	displayDeltaText();	// reset to previous, known-good delta
    } else {
	_timer->stop();
	secondsToTV(unitsToSeconds(delta, _units), &_kmtime.delta);
	emit vcrModePulse(&_kmtime, 0);
	if (reset)
	    _timer->start((int)(((_kmtime.delta.tv_sec * 1000) +
			     (_kmtime.delta.tv_usec / 1000)) / _speed));
    }
}

void KmTimeArch::lineEditCtime_validate()
{
    struct timeval current;
    QString input, error;
    char *msg;

    input = lineEditCtime->text().stripWhiteSpace();
    if (input.length() == 0) {
	error.sprintf("Position time has not been set.\n");
	QMessageBox::warning(0, tr("Warning"), error, tr("Quit"));
	return;
    }
    if (input[0] != '@')
	input.prepend("@");
    if (__pmParseTime(input.ascii(),
			&_kmtime.start, &_kmtime.end, &current, &msg) < 0) {
	error.sprintf("Invalid position date/time:\n\n%s\n", msg);
	QMessageBox::warning(0, tr("Warning"), error, tr("Quit"));
	displayPositionText();	// reset to previous, known-good position
	free(msg);
    } else {
	_kmtime.position = current;
	displayPositionText();
	displayPositionSlide();
	emit vcrModePulse(&_kmtime, 0);
    }
}

void KmTimeArch::lineEditSpeed_validate()
{
    double speed, upper, lower, delta = secondsFromTV(&_kmtime.delta);
    bool ok, reset = _timer->isActive();

    lower = KM_MINIMUM_SPEED(delta);
    upper = KM_MAXIMUM_SPEED(delta);
    speed = lineEditSpeed->text().toDouble(&ok);
    if (!ok || speed < lower || speed > upper) {
	wheelSpeed->setValue(_speed);	// reset to previous, known-good speed
    } else {
	_speed = speed;
	wheelSpeed->setValue(_speed);
	if (reset) {
	    _timer->stop();
	    _timer->start((int)(((_kmtime.delta.tv_sec * 1000) +
				 (_kmtime.delta.tv_usec / 1000)) / _speed));
	}
    }
}

void KmTimeArch::setTimezone(QAction *action)
{
    TimeZone *tz;

    for (tz = _tzlist.first(); tz; tz = _tzlist.next()) {
	if (tz->action() == action) {
	    _first = true;	// resetting time completely
	    pmUseZone(tz->handle());
	    emit timeZonePulse(&_kmtime, tz->tz(), strlen(tz->tz()) + 1,
				tz->tzlabel(), strlen(tz->tzlabel()) + 1);
	    _console->post(DBG_APP, "%s sent timezone %s (%s) to clients",
			__func__, tz->tz(), tz->tzlabel());
	    setTime(&_kmtime, NULL);	// re-display the time, no messages
	    break;
	}
    }
}

void KmTimeArch::addTimezone(char *string)
{
    TimeZone *tmp, *tzp;
    QAction *tzAction;
    char *label, *tz;
    int handle;

    if ((handle = pmNewZone(string)) < 0)
	return;

    if ((tz = strdup(string)) == NULL)
	return;

    if ((label = strdup(string + strlen(string) + 1)) == NULL) {
	free(tz);
	return;
    }

    for (tmp = _tzlist.first(); tmp; tmp = _tzlist.next()) {
	if (strcmp(tmp->tzlabel(), label) == 0) {
	    free(label);
	    free(tz);
	    return;
	}
    }

    tzAction = new QAction(this);
    tzAction->setToggleAction(TRUE);
    tzAction->setToolTip(tz);
    tzAction->setText(label);
    tzAction->setMenuText(label);

    tzp = new TimeZone(tz, label, tzAction, handle);
    _tzlist.append(tzp);

    if (!_tzActions) {
	_tzActions = new QActionGroup(this);
	connect(_tzActions, SIGNAL(selected(QAction *)) , this,
		SLOT(setTimezone(QAction *)));
	_tzActions->setUsesDropDown(FALSE);
	_tzActions->setExclusive(TRUE);
    } else {
	_tzActions->removeFrom(Timezone);
    }
    _tzActions->add(tzAction);
    _tzActions->addTo(Timezone);
    _console->post(DBG_APP, "%s added tz=%s label=%s", __func__, tz, label);
}

void KmTimeArch::setTime(kmTime *k, char *tzdata)
{
#ifdef DESPERATE
    _console->post(DBG_PROTO,
	"%s START: 1st=%d now=%u.%u end=%u.%u start=%u.%u delta=%u.%u",
    	__func__, _first, _kmtime.position.tv_sec, _kmtime.position.tv_usec,
    	_kmtime.end.tv_sec, _kmtime.end.tv_usec, _kmtime.start.tv_sec,
	_kmtime.start.tv_usec, _kmtime.delta.tv_sec, _kmtime.delta.tv_usec);
#endif

    if (_first == true) {
	_first = false;
	if (tzdata != NULL)
	    addTimezone(tzdata);
	absoluteStart = _kmtime.start = k->start;
	absoluteEnd = _kmtime.end = k->end;
	_kmtime.position = k->position;
	_kmtime.delta = k->delta;
	sliderPosition->blockSignals(TRUE);
	setPositionSlideRange();
	setPositionSlideDelta();
	sliderPosition->blockSignals(FALSE);
	displayDeltaText();
	displayPositionText();
	displayPositionSlide();
	_bounds->reset();
	changeSpeed(KM_DEFAULT_SPEED(secondsFromTV(&k->delta)));
    } else {
	addBound(k, tzdata);
    }

#ifdef DESPERATE
    _console->post(DBG_PROTO,
	"%s ENDED: 1st=%d now=%u.%u end=%u.%u start=%u.%u delta=%u.%u",
	__func__, _first, _kmtime.position.tv_sec, _kmtime.position.tv_usec,
	_kmtime.end.tv_sec, _kmtime.end.tv_usec, _kmtime.start.tv_sec,
	_kmtime.start.tv_usec, _kmtime.delta.tv_sec, _kmtime.delta.tv_usec);
#endif
}

void KmTimeArch::addBound(kmTime *k, char *tzdata)
{
    // Note: kmchart can start kmtime up without an archive
    // so, we need to explicitly initialise some fields now
    // that one might otherwise have expected to be setup.
    bool need_pulse = tnonzero(&_kmtime.position);

    _console->post(DBG_PROTO, "%s START: p?=%d now=%u.%u end=%u.%u start=%u.%u",
	__func__, need_pulse,
	_kmtime.position.tv_sec, _kmtime.position.tv_usec, _kmtime.end.tv_sec,
	_kmtime.end.tv_usec, _kmtime.start.tv_sec, _kmtime.start.tv_usec);

    if (tzdata != NULL)
	addTimezone(tzdata);

    if (tcmp(&k->start, &absoluteStart) < 0 || need_pulse)
	absoluteStart = k->start;
    if (tcmp(&k->end, &absoluteEnd) > 0 || need_pulse)
	absoluteEnd = k->end;
    if (!need_pulse) {	// first-time archive initialisation
	_kmtime.position = k->position;
	_kmtime.start = k->start;
	_kmtime.end = k->end;
    }

    setPositionSlideRange();
    _bounds->reset();

    if (need_pulse)
	emit vcrModePulse(&_kmtime, 0);

    _console->post(DBG_PROTO, "%s START: p?=%d now=%u.%u end=%u.%u start=%u.%u",
	__func__, need_pulse,
	_kmtime.position.tv_sec, _kmtime.position.tv_usec, _kmtime.end.tv_sec,
	_kmtime.end.tv_usec, _kmtime.start.tv_sec, _kmtime.start.tv_usec);
}

void KmTimeArch::setupAssistant()
{
    static char *paths[] = { "/usr/share/doc/kmchart/html", "../../man/html" };

    if (!_assistant->isOpen()) {
	QStringList args;
	QString profile;
	uint i;

	for (i = 0; i < sizeof(paths)/sizeof(paths[0]); i++) {
	    QDir path(tr(paths[i]));
	    if (!path.exists())
		continue;
	    profile = path.absPath();
	    break;
	}

	args << tr("-profile");
	profile.append("/kmtime.adp");
	args << profile;

	_assistant->setArguments(args);
	_assistant->openAssistant();
    }
}

void KmTimeArch::helpManual()
{
    setupAssistant();
    _assistant->showPage(tr("contents.html"));
}

void KmTimeArch::helpContents()
{
    setupAssistant();
    _assistant->showPage(tr("manual.html"));
}
