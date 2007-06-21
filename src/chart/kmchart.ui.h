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

#include <qwt/qwt_plot.h>
#include <qwt/qwt_plot_layout.h>
#include <qwt/qwt_plot_canvas.h>
#include <qwt/qwt_plot_curve.h>
#include <qwt/qwt_plot_picker.h>
#include <qwt/qwt_double_rect.h>
#include <qwt/qwt_legend.h>
#include <qwt/qwt_legend_item.h>
#include <qwt/qwt_counter.h>
#include "aboutdialog.h"
#include "aboutpcpdialog.h"

#include "../../images/play_live.xpm"
#include "../../images/stop_live.xpm"
#include "../../images/play_record.xpm"
#include "../../images/stop_record.xpm"
#include "../../images/play_archive.xpm"
#include "../../images/stop_archive.xpm"
#include "../../images/back_archive.xpm"
#include "../../images/stepback_archive.xpm"
#include "../../images/stepfwd_archive.xpm"
#include "../../images/fastback_archive.xpm"
#include "../../images/fastfwd_archive.xpm"

#define DESPERATE 0

#if DESPERATE
char *_style[] = { "None", "Line", "Bar", "Stack", "Area", "Util" };
#define stylestr(x) _style[(int)x]
#endif

void KmChart::init(void)
{
    timeAxisPlot->init();

    _timeplaylive_pixmap = new QPixmap(play_live_xpm);
    _timestoplive_pixmap = new QPixmap(stop_live_xpm);
    _timeplayrecord_pixmap = new QPixmap(play_record_xpm);
    _timestoprecord_pixmap = new QPixmap(stop_record_xpm);
    _timeplayarchive_pixmap = new QPixmap(play_archive_xpm);
    _timestoparchive_pixmap = new QPixmap(stop_archive_xpm);
    _timebackarchive_pixmap = new QPixmap(back_archive_xpm);
    _timestepfwdarchive_pixmap = new QPixmap(stepfwd_archive_xpm);
    _timefastfwdarchive_pixmap = new QPixmap(fastfwd_archive_xpm);
    _timestepbackarchive_pixmap = new QPixmap(stepback_archive_xpm);
    _timefastbackarchive_pixmap = new QPixmap(fastback_archive_xpm);

    _info = new InfoDialog(this);
    _newtab = new TabDialog(this);
    _edittab = new TabDialog(this);
    _newchart = new ChartDialog(this);
    _editchart = new ChartDialog(this);
    _openview = new OpenViewDialog(this);
    _saveview = new SaveViewDialog(this);
    _settings = new SettingsDialog(this);

    _assistant = new QAssistantClient(tr(""), this);

    _newchart->init();
    _editchart->init();

    connect(_newtab->buttonOk, SIGNAL(clicked()), this, SLOT(acceptNewTab()));
    connect(_edittab->buttonOk, SIGNAL(clicked()), this, SLOT(acceptEditTab()));
    connect(_newchart->buttonOk, SIGNAL(clicked()), this,
					SLOT(acceptNewChart()));
    connect(_editchart->buttonOk, SIGNAL(clicked()), this,
					SLOT(acceptEditChart()));
    connect(_settings->buttonOk, SIGNAL(clicked()), this,
					SLOT(acceptSettings()));
    connect(_settings->buttonCancel, SIGNAL(clicked()), this,
					SLOT(revertSettings()));
    connect(_assistant, SIGNAL(error(const QString &)), this,
					SLOT(assistantError(const QString &)));
}

void KmChart::destroy(void)
{
}

TimeAxis *KmChart::timeAxis(void)
{
    return timeAxisPlot;
}

void KmChart::enableUI(void)
{
    bool	haveTabs = (ntabs > 1);
    bool	haveCharts = (activeTab->numChart() > 0);
    bool	haveLiveHosts = (!activeTab->isArchiveMode());

    closeTabAction->setEnabled(haveTabs);
    deleteTabAction->setEnabled(haveTabs);
    fileSaveViewAction->setEnabled(haveCharts);
    fileRecordAction->setEnabled(haveCharts && haveLiveHosts);
    filePrintAction->setEnabled(haveCharts);
    editChartAction->setEnabled(haveCharts);
    deleteChartAction->setEnabled(haveCharts);
}

void KmChart::setButtonState(enum KmButtonState newstate)
{
    switch(newstate) {
    case BUTTON_PLAYLIVE:
	timePushButton->setPixmap(*_timeplaylive_pixmap);
	break;
    case BUTTON_STOPLIVE:
	timePushButton->setPixmap(*_timestoplive_pixmap);
	break;
    case BUTTON_PLAYRECORD:
	timePushButton->setPixmap(*_timeplayrecord_pixmap);
	break;
    case BUTTON_STOPRECORD:
	timePushButton->setPixmap(*_timestoprecord_pixmap);
	break;
    case BUTTON_PLAYARCHIVE:
	timePushButton->setPixmap(*_timeplayarchive_pixmap);
	break;
    case BUTTON_STOPARCHIVE:
	timePushButton->setPixmap(*_timestoparchive_pixmap);
	break;
    case BUTTON_BACKARCHIVE:
	timePushButton->setPixmap(*_timebackarchive_pixmap);
	break;
    case BUTTON_STEPFWDARCHIVE:
	timePushButton->setPixmap(*_timestepfwdarchive_pixmap);
	break;
    case BUTTON_STEPBACKARCHIVE:
	timePushButton->setPixmap(*_timestepbackarchive_pixmap);
	break;
    case BUTTON_FASTFWDARCHIVE:
	timePushButton->setPixmap(*_timefastfwdarchive_pixmap);
	break;
    case BUTTON_FASTBACKARCHIVE:
	timePushButton->setPixmap(*_timefastbackarchive_pixmap);
	break;
    default:
	abort();
    }
}

void KmChart::step(bool livemode, kmTime *kmtime)
{
    int	i;

    for (i = 0; i < ntabs; i++) {
	if (tabs[i]->isArchiveMode()) {
	    if (!livemode)
		tabs[i]->step(kmtime);
	}
	else if (livemode)
	    tabs[i]->step(kmtime);
    }
}

void KmChart::vcrmode(bool livemode, kmTime *kmtime, bool drag)
{
    int	i;

    for (i = 0; i < ntabs; i++) {
	if (tabs[i]->isArchiveMode()) {
	    if (!livemode)
		tabs[i]->vcrmode(kmtime, drag);
	} else if (livemode)
	    tabs[i]->vcrmode(kmtime, drag);
    }
}

void KmChart::timezone(bool livemode, char *tzdata)
{
    int	i;

    for (i = 0; i < ntabs; i++) {
	if (tabs[i]->isArchiveMode()) {
	    if (!livemode)
		tabs[i]->setTimezone(tzdata);
	}
	else if (livemode)
	    tabs[i]->setTimezone(tzdata);
    }
}

void KmChart::setStyle(char *style)
{
    settings.style = QApplication::setStyle(tr(style));
}

void KmChart::showTimeControl()
{
    activeTab->showTimeControl();
}

void KmChart::fileOpenView()
{
    _openview->reset();
    _openview->show();
}

void KmChart::fileSaveView()
{
    _saveview->reset();
    _saveview->show();
}

void KmChart::fileRecord()
{
    if (!activeTab->isRecording()) {
	if (activeTab->startRecording() == 0) {
	    fileRecordAction->setText(tr("Stop Recording ..."));
	    fileRecordAction->setMenuText(tr("Stop &Recording ..."));
	    activeTab->newButtonState(activeTab->kmtimeState(),
			KM_MODE_NORMAL, PM_CONTEXT_HOST, true);
	    setButtonState(activeTab->buttonState());
	}
    } else {
	activeTab->stopRecording();
	fileRecordAction->setText(tr("Record ..."));
	fileRecordAction->setMenuText(tr("&Record ..."));
	activeTab->newButtonState(activeTab->kmtimeState(),
			KM_MODE_NORMAL, PM_CONTEXT_HOST, false);
	setButtonState(activeTab->buttonState());
    }
}

void KmChart::filePrint()
{
    // TODO - this only prints the current chart
    QPrinter	printer;
    QString	creator = QString("kmchart Version ");
    creator.append(VERSION);
    printer.setCreator(creator);
    printer.setOrientation(QPrinter::Landscape);
    printer.setDocName("foo");
    if (printer.setup()) {
	Chart 	*cp = activeTab->currentChart();
	QwtPlotPrintFilter	filter;
	// if (printer.colorMode() == QPrinter::GrayScale)
	    // background and grid not helpful for monochrome printers
	    filter.setOptions(QwtPlotPrintFilter::PrintAll &
		~QwtPlotPrintFilter::PrintCanvasBackground &
		~QwtPlotPrintFilter::PrintWidgetBackground &
		~QwtPlotPrintFilter::PrintGrid);
	cp->print(printer, filter);
    }
}

void KmChart::fileQuit()
{
    QApplication::exit(0);
}

void KmChart::assistantError(const QString &msg)
{
    QMessageBox::warning(this, pmProgname, msg);
}

void KmChart::setupAssistant()
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
	profile.append("/kmchart.adp");
	args << profile;

	_assistant->setArguments(args);
	_assistant->openAssistant();
    }
}

void KmChart::helpManual()
{
    setupAssistant();
    _assistant->showPage(tr("manual.html"));
}

void KmChart::helpContents()
{
    setupAssistant();
    _assistant->showPage(tr("contents.html"));
}

void KmChart::helpAbout()
{
    About about;
    about.exec();
}

void KmChart::helpAboutPCP()
{
    AboutPCP about;
    about.exec();
}

void KmChart::whatsThis()
{
    QMainWindow::whatsThis();
}

void KmChart::optionsShowTimeControl()
{
    if (activeTab->isArchiveMode())
	kmtime->showArchiveTimeControl();
    else
	kmtime->showLiveTimeControl();
}

void KmChart::optionsHideTimeControl()
{
    if (activeTab->isArchiveMode())
	kmtime->hideArchiveTimeControl();
    else
	kmtime->hideLiveTimeControl();
}

void KmChart::optionsLaunchNewKmchart()
{
    QProcess *buddy = new QProcess(this);
    QString port;

    port.setNum(kmtime->port());
    buddy->addArgument("kmchart");
    buddy->addArgument("-p");
    buddy->addArgument(port);

    if (!buddy->start()) {
	port.prepend(tr("Cannot start kmchart via:\nkmchart -p "));
	QMessageBox::warning(this, pmProgname, port);
    }
}

void KmChart::createNewChart(chartStyle style)
{
    _newchart->reset(NULL, (int)style - 1);
    _newchart->show();
}

void KmChart::acceptNewChart()
{
    // TODO: check type of New Chart dialog matches current activeTab
    // If not, create new Tab?  (can't really just toss it!)
    // TODO: check up front if feasible to create chart...

    Chart *cp = activeTab->addChart();
    cp->setStyle((chartStyle)(_newchart->style() + 1));
    QString newtitle = _newchart->title().stripWhiteSpace();
    if (newtitle.isEmpty() == FALSE)
	cp->changeTitle((char *)newtitle.ascii(), TRUE);
    cp->setLegendVisible(_newchart->legend());
    if (_newchart->setupChartPlotsShortcut(cp) == FALSE)
	_newchart->setupChartPlots(cp);
    cp->show();

    // TODO: Y-Axis scaling
    // TODO: teardown ListViews and free up memory (both?  chartList only?)
    // TODO: might be an idea to keep available once its built, to optimise
    // subsequent accesses to the metric selection process...

    enableUI();
}

void KmChart::fileNewLinePlot()
{
    createNewChart(Line);
}

void KmChart::fileNewBarPlot()
{
    createNewChart(Bar);
}

void KmChart::fileNewAreaPlot()
{
    createNewChart(Area);
}

void KmChart::fileNewStackedBar()
{
    createNewChart(Stack);
}

void KmChart::fileNewUtilization()
{
    createNewChart(Util);
}

void KmChart::editChart()
{
    bool yautoscale;
    double ymin, ymax;
    Chart *cp = activeTab->currentChart();

    _editchart->reset(cp, (int)activeTab->currentChart()->style() - 1);
    _editchart->titleLineEdit->setText(cp->title());
    _editchart->legendOn->setChecked(cp->legendVisible());
    _editchart->legendOff->setChecked(!cp->legendVisible());
    cp->scale(&yautoscale, &ymin, &ymax);
    _editchart->setScale(yautoscale, ymin, ymax);

    _editchart->show();
}

void KmChart::acceptEditChart()
{
    bool yautoscale;
    double ymin, ymax;
    Chart *cp = _editchart->chart();

    if (_editchart->style() + 1 != cp->style())
	cp->setStyle((chartStyle)(_editchart->style() + 1));
    QString edittitle = _editchart->title().stripWhiteSpace();
    if (edittitle.isEmpty() == FALSE && edittitle != cp->title())
	cp->changeTitle((char *)edittitle.ascii(), TRUE);
    cp->setLegendVisible(_editchart->legend());
    _editchart->scale(&yautoscale, &ymin, &ymax);
    cp->setScale(yautoscale, ymin, ymax);
    _editchart->setupChartPlots(cp);
    if (cp->numPlot() > 0)
	cp->show();
    else
	activeTab->deleteChart(cp);

    // TODO: need a flag on "cp" that says "currently being edited" so
    // the Chart Delete menu option doesn't shoot it down before we're
    // done here.  Both Cancel and OK will need to clear that flag.

    // TODO: teardown ListViews and free up memory (both?  chartList only?)
    // TODO: might be an idea to keep available once its built, to optimise
    // subsequent accesses to the metric selection process...

    enableUI();
}

void KmChart::deleteChart()
{
    activeTab->deleteCurrent();
    enableUI();
}

void KmChart::metricInfo(QString src, QString m, QString inst, bool archive)
{
    _info->reset(src, m, inst, archive);
    _info->show();
}

void KmChart::editTab()
{
    QWidget *which = chartTab->currentPage();
    _edittab->reset(chartTab->tabLabel(which), activeTab->isArchiveMode(),
		    activeTab->sampleHistory(), activeTab->visibleHistory());
    _edittab->show();
}

void KmChart::acceptEditTab()
{
    chartTab->changeTab(activeTab->splitter(), _edittab->labelLineEdit->text());
    activeTab->setSampleHistory((int)_edittab->samplePointsCounter->value());
    activeTab->setVisibleHistory((int)_edittab->visiblePointsCounter->value());
}

void KmChart::createNewTab(bool liveMode)
{
    _newtab->reset(QString::null, liveMode,
		    settings.sampleHistory, settings.visibleHistory);
    _newtab->show();
}

void KmChart::acceptNewTab()
{
    tabs = (Tab **)realloc(tabs, (ntabs+1) * sizeof(Tab *));	// TODO: NULL
    tabs[ntabs] = new Tab();
    if (_newtab->isArchiveMode())
	tabs[ntabs]->init(kmchart->tabWidget(),
		(int)_newtab->samplePointsCounter->value(),
		(int)_newtab->visiblePointsCounter->value(),
		archiveGroup, KM_SOURCE_ARCHIVE,
		_newtab->labelLineEdit->text().stripWhiteSpace().ascii(),
		kmtime->archiveInterval(), kmtime->archivePosition());
    else
	tabs[ntabs]->init(kmchart->tabWidget(),
		(int)_newtab->samplePointsCounter->value(),
		(int)_newtab->visiblePointsCounter->value(),
		 liveGroup, KM_SOURCE_HOST,
		_newtab->labelLineEdit->text().stripWhiteSpace().ascii(),
		kmtime->liveInterval(), kmtime->livePosition());
    ntabs++;
    enableUI();
}

void KmChart::addLiveTab()
{
    createNewTab(TRUE);
}

void KmChart::addArchiveTab()
{
    createNewTab(FALSE);
}

void KmChart::closeTab()
{
    int	index = chartTab->currentPageIndex();

    chartTab->removePage(chartTab->currentPage());
    if (index < ntabs - 1)
	memmove(&tabs[index], &tabs[index+1], sizeof(Tab *));
    ntabs--;
    setActiveTab(chartTab->currentPageIndex(), false);
    enableUI();
}

QTabWidget *KmChart::tabWidget()
{
    return chartTab;
}

void KmChart::setActiveTab(int index, bool redisplay)
{
    activeTab = tabs[index];
    if (tabs[index]->isArchiveMode()) {
	activeGroup = archiveGroup;
	activeSources = archiveSources;
    } else {
	activeGroup = liveGroup;
	activeSources = liveSources;
    }
    activeTab->updateTimeButton();
    activeTab->updateTimeAxis();

    if (redisplay)
	chartTab->setCurrentPage(index);
}

void KmChart::activeTabChanged(QWidget *)
{
    int index = chartTab->currentPageIndex();

    if (index < ntabs)
	setActiveTab(index, false);
    enableUI();
}

void KmChart::editSettings()
{
    _settings->reset();
    _settings->show();
}

void KmChart::acceptSettings()
{
    _settings->flush();
    writeSettings();
}

void KmChart::revertSettings()
{
    _settings->revert();
}

void KmChart::setDateLabel(time_t seconds, const char *tz)
{
    char datestring[32];
    QString label;

    if (seconds) {
	pmCtime(&seconds, datestring);
	label = tr(datestring);
	label.remove(10, 9);
	label.replace(15, 1, " ");
	label.append(tz);
    }
    else {
	label = tr("");
    }
    dateLabel->setText(label);
}
