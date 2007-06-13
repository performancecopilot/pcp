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

#include <qwt/qwt_slider.h>
#include <qmessagebox.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include "main.h"

void ShowBounds::init(Console *console,
			struct timeval *absStart, struct timeval *curStart,
			struct timeval *absEnd, struct timeval *curEnd)
{
    _console = console;
    _absoluteStart = absStart;
    _currentStart = curStart;
    _absoluteEnd = absEnd;
    _currentEnd = curEnd;
    reset();
}

void ShowBounds::reset()
{
    _localAbsoluteStart = secondsFromTV(_absoluteStart);
    _localCurrentStart = secondsFromTV(_currentStart);
    _localAbsoluteEnd = secondsFromTV(_absoluteEnd);
    _localCurrentEnd = secondsFromTV(_currentEnd);

    displayStartSlider();
    displayEndSlider();
    displayStartText();
    displayEndText();
}

void ShowBounds::displayStartSlider()
{
    sliderStart->blockSignals(TRUE);
    sliderStart->setRange(_localAbsoluteStart, _localAbsoluteEnd);
    sliderStart->setValue(_localCurrentStart);
    sliderStart->blockSignals(FALSE);
}

void ShowBounds::displayEndSlider()
{
    sliderEnd->blockSignals(TRUE);
    sliderEnd->setRange(_localAbsoluteStart, _localAbsoluteEnd);
    sliderEnd->setValue(_localCurrentEnd);
    sliderEnd->blockSignals(FALSE);
}

void ShowBounds::displayStartText()
{
    time_t	clock = (time_t)_localCurrentStart;
    char	ctimebuf[32];

    pmCtime(&clock, ctimebuf);
    lineEditStart->setText(tr(ctimebuf).stripWhiteSpace());
}

void ShowBounds::displayEndText()
{
    time_t	clock = (time_t)_localCurrentEnd;
    char	ctimebuf[32];

    pmCtime(&clock, ctimebuf);
    lineEditEnd->setText(tr(ctimebuf).stripWhiteSpace());
}

void ShowBounds::changedStart(double value)
{
    if (value != _localCurrentStart) {
	_localCurrentStart = value;
	displayStartSlider();
	displayStartText();
	if (_localCurrentStart > _localCurrentEnd)
	    sliderEnd->setValue(value);
    }
}

void ShowBounds::changedEnd(double value)
{
    if (value != _localCurrentEnd) {
	_localCurrentEnd = value;
	displayEndSlider();
	displayEndText();
	if (_localCurrentStart > _localCurrentEnd)
	    sliderStart->setValue(value);
    }
}

//
// This routine just verifies that input from the user is connect,
// before allowing the window to be closed.  If theres invalid date
// strings in start/end text boxes, we must deny the "OK" press.
// Actual work of updating kmtime is done in flush().
//
void ShowBounds::accept()
{
    struct timeval current, start, end;
    QString error, input;
    char *msg;

    _console->post(DBG_APP, "%s: Bounds OK pressed", __func__);

    secondsToTV(_localAbsoluteStart, &start);
    secondsToTV(_localAbsoluteEnd, &end);

    if (lineEditStart->isModified()) {
	input = lineEditStart->text().stripWhiteSpace();
	if (input.length() == 0) {
	    error.sprintf("Start time has not been set.\n");
	    QMessageBox::warning(0, tr("Warning"), error, tr("Quit"));
	    return;
	}
	if (input[0] != '@')
	    input.prepend("@");
	if (__pmParseTime(input.ascii(), &start, &end, &current, &msg) < 0) {
	    error.sprintf("Invalid start date/time:\n\n%s\n", msg);
	    QMessageBox::warning(0, tr("Warning"), error, tr("Quit"));
	    free(msg);
	    return;
	} else if (tcmp(&current, &start) < 0 || tcmp(&current, &end) > 0) {
	    error.sprintf("Start time is outside archive boundaries\n");
	    QMessageBox::warning(0, tr("Warning"), error, tr("Quit"));
	    return;
	}
	_localCurrentStart = secondsFromTV(&current);
	_console->post(DBG_APP, "%s start=%.2f (abs=%.2f-%.2f)", __func__,
		_localCurrentStart, _localAbsoluteStart, _localAbsoluteEnd);
    }

    if (lineEditEnd->isModified()) {
	input = lineEditEnd->text().stripWhiteSpace();
	if (input.length() == 0) {
	    error.sprintf("End time has not been set.\n");
	    QMessageBox::warning(0, tr("Warning"), error, tr("Quit"));
	    return;
	}
	if (input[0] != '@')
	    input.prepend("@");
	if (__pmParseTime(input.ascii(), &start, &end, &current, &msg) < 0) {
	    error.sprintf("Invalid end date/time:\n%s\n\n", msg);
	    QMessageBox::warning(0, tr("Warning"), error, tr("Quit"));
	    free(msg);
	    return;
	} else if (tcmp(&current, &start) < 0 || tcmp(&current, &end) > 0) {
	    error.sprintf("End time is outside the archive boundaries\n");
	    QMessageBox::warning(0, tr("Warning"), error, tr("Quit"));
	    return;
	}
	_localCurrentEnd = secondsFromTV(&current);
	_console->post(DBG_APP, "%s end=%.2f (abs=%.2f-%.2f)", __func__,
		_localCurrentEnd, _localAbsoluteStart, _localAbsoluteEnd);
    }

    if (_localCurrentStart > _localCurrentEnd) {
	error.sprintf("Start time must be less than end time.\n");
	QMessageBox::warning(0, tr("Warning"), error, tr("Quit"));
	return;
    }

    emit boundsChanged();

    done(0);
}

//
// Inform parent kmtime window of accepted changes (start/end),
// via the pointers-to-struct-timevals we were initially given.
//
void ShowBounds::flush()
{
    struct timeval start, end;

    _console->post(DBG_APP, "%s: Bounds flush requested", __func__);

    secondsToTV(_localCurrentStart, &start);
    secondsToTV(_localCurrentEnd, &end);

    _console->post(DBG_APP, "%s updating bounds to %.2f->%.2f",
		   __func__, secondsFromTV(&start), secondsFromTV(&end));

    if (tcmp(&start, _currentStart) != 0)
	*_currentStart = start;
    if (tcmp(&end, _currentEnd) != 0)
	*_currentEnd = end;
}

void ShowBounds::reject()
{
    done(1);
}
