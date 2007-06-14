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
#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include "main.h"
#include "aboutdialog.h"
#include "aboutpcpdialog.h"
#include "../../images/stop_on.xpm"
#include "../../images/stop_off.xpm"
#include "../../images/play_on.xpm"
#include "../../images/play_off.xpm"

extern void QTimeFromTimeval(QTime *qt, struct timeval *tv);

typedef struct {
    km_tctl_state	state;
    const char		**stop_xpm;
    QPixmap 		*stop_pixmap;
    const char		**play_xpm;
    QPixmap 		*play_pixmap;
} tctl_h;

static tctl_h tctl[] = {
    { KM_STATE_STOP, stop_on_xpm, NULL, play_off_xpm, NULL },
    { KM_STATE_FORWARD, stop_off_xpm, NULL, play_on_xpm, NULL }
};

static int nctl = sizeof(tctl) / sizeof(tctl[0]);

void KmTimeLive::setControl(km_tctl_state newstate)
{
    if (_kmtime.state != newstate) {
	for (int i = 0; i < nctl; i++) {
	    if (tctl[i].state == newstate) {
		if (tctl[i].stop_pixmap == NULL)
		    tctl[i].stop_pixmap = new QPixmap(tctl[i].stop_xpm);
		pushButtonStop->setPixmap(*tctl[i].stop_pixmap);
		if (tctl[i].play_pixmap == NULL)
		    tctl[i].play_pixmap = new QPixmap(tctl[i].play_xpm);
		pushButtonPlay->setPixmap(*tctl[i].play_pixmap);
	    }
	}
	_kmtime.state = newstate;
    }
}

void KmTimeLive::init(Console *cp)
{
    char utc[] = "UTC\0Universal Coordinated Time";

    _console = cp;
    _console->post(DBG_APP, "Starting Live Time Control...");

    _units = Sec;
    _first = true;
    _tzActions = NULL;

    memset(&_kmtime, 0, sizeof(_kmtime));
    _kmtime.source = KM_SOURCE_HOST;
    _kmtime.delta.tv_sec = KM_DEFAULT_DELTA;

    _show_msec = FALSE;
    optionsDetailShow_MillisecondsAction->setOn(_show_msec);
    _show_year = FALSE;
    optionsDetailShow_YearAction->setOn(_show_year);

    addTimezone(utc);
    displayPosition();
    displayDeltaText();
    setControl(KM_STATE_FORWARD);

    _timer = new QTimer(this);
    connect(_timer, SIGNAL(timeout()), SLOT(updateTime()));
    _timer->start((_kmtime.delta.tv_sec * 1000) +
		  (_kmtime.delta.tv_usec / 1000));
    lineEditDelta->setAlignment(Qt::AlignRight);
    lineEditDelta->setValidator(new QDoubleValidator
		(0.001, ULONG_MAX, 3, lineEditDelta));

    _assistant = new QAssistantClient(tr(""), this);
}

void KmTimeLive::helpAbout()
{
    About about;
    about.exec();
}

void KmTimeLive::helpAboutPCP()
{
    AboutPCP about;
    about.exec();
}

void KmTimeLive::whatsThis()
{
    QMainWindow::whatsThis();
}

void KmTimeLive::playClicked()
{
    if (lineEditDelta->isModified())
	lineEditDelta_validate();
    if (_kmtime.state != KM_STATE_FORWARD)
	play();
}

void KmTimeLive::play()
{
    setControl(KM_STATE_FORWARD);
    updateTime();
    if (!_timer->isActive())
	_timer->start((_kmtime.delta.tv_sec * 1000) +
		      (_kmtime.delta.tv_usec / 1000));
    _console->post(DBG_APP, "%s pressed", __func__);
}

void KmTimeLive::stopClicked()
{
    if (_kmtime.state != KM_STATE_STOP)
	stop();
}

void KmTimeLive::stop()
{
    setControl(KM_STATE_STOP);
    _timer->stop();
    emit vcrModePulse(&_kmtime, 0);
    _console->post(DBG_APP, "%s halted progression of time", __func__);
}

void KmTimeLive::updateTime()
{
    gettimeofday(&_kmtime.position, NULL);
    displayPosition();
    emit timePulse(&_kmtime);
}

void KmTimeLive::displayPosition()
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

void KmTimeLive::clickShowMsec()
{
    if (_show_msec == TRUE) _show_msec = FALSE;
    else _show_msec = TRUE;
    optionsDetailShow_MillisecondsAction->setOn(_show_msec);
    displayPosition();
}

void KmTimeLive::clickShowYear()
{
    if (_show_year == TRUE) _show_year = FALSE;
    else _show_year = TRUE;
    optionsDetailShow_YearAction->setOn(_show_year);
    displayPosition();
}

void KmTimeLive::changeDelta(int value)
{
    _units = (delta_units)value;
    displayDeltaText();
}

void KmTimeLive::displayDeltaText()
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

void KmTimeLive::showConsole()
{
    _console->show();
}

void KmTimeLive::disableConsole()
{
    optionsShowConsoleAction->setVisible(FALSE);
}

void KmTimeLive::hideWindow()
{
    if (isVisible()) hide();
    else show();
}

void KmTimeLive::popup(int hello_popetts)
{
    if (hello_popetts) show();
    else hide();
}

void KmTimeLive::closeEvent(QCloseEvent *ce)
{
    hide();
    ce->ignore();
}

void KmTimeLive::lineEditDelta_changed(const QString &s)
{
    (void)s;
    if (lineEditDelta->isModified())
	stopClicked();
}

void KmTimeLive::lineEditDelta_validate()
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
	    _timer->start((_kmtime.delta.tv_sec * 1000) +
			  (_kmtime.delta.tv_usec / 1000));
    }
}

void KmTimeLive::setTimezone(QAction *action)
{
    TimeZone *tz;

    _console->post(DBG_APP, "%s entered (menu choice)\n", __func__);
    for (tz = _tzlist.first(); tz; tz = _tzlist.next()) {
	if (tz->action() == action) {
	    pmUseZone(tz->handle());
	    emit timeZonePulse(&_kmtime, tz->tz(), strlen(tz->tz()) + 1,
				tz->tzlabel(), strlen(tz->tzlabel()) + 1);
	    setTime(&_kmtime, NULL);	// re-display the time, no messages
	    _console->post(DBG_APP, "%s sent timezone %s (%s) to clients\n",
			__func__, tz->tz(), tz->tzlabel());
	    break;
	}
    }
}

void KmTimeLive::addTimezone(char *string)
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

void KmTimeLive::setTime(kmTime *k, char *tzdata)
{
    if (tzdata != NULL)
	addTimezone(tzdata);

    if (_first == true) {
	bool reset = _timer->isActive();

	_first = false;
	_kmtime.position = k->position;
	_kmtime.delta = k->delta;
	_timer->stop();
	if (reset)
	    _timer->start((_kmtime.delta.tv_sec * 1000) +
			  (_kmtime.delta.tv_usec / 1000));
	displayDeltaText();
	displayPosition();
    }
}

void KmTimeLive::style(char *style, void *source)
{
    emit stylePulse(&_kmtime, style, strlen(style) + 1, source);
}

void KmTimeLive::setupAssistant()
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

void KmTimeLive::helpManual()
{
    setupAssistant();
    _assistant->showPage(tr("contents.html"));
}

void KmTimeLive::helpContents()
{
    setupAssistant();
    _assistant->showPage(tr("manual.html"));
}
