/*
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
 * Copyright (c) 2006-2007, Aconex.  All Rights Reserved.
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
 */
#include <QtGui/QApplication>
#include <QtGui/QPrintDialog>
#include <QtGui/QMessageBox>
#include <QtGui/QWhatsThis>
#include <pcp/pmapi.h>

#include "main.h"
#include "kmchart.h"
#include "aboutdialog.h"
#include "chartdialog.h"
#include "exportdialog.h"
#include "infodialog.h"
#include "openviewdialog.h"
#include "recorddialog.h"
#include "saveviewdialog.h"
#include "seealsodialog.h"
#include "settingsdialog.h"
#include "tabdialog.h"
#include "timeaxis.h"
#include "timebutton.h"

#define DESPERATE 0

#if DESPERATE
char *_style[] = { "None", "Line", "Bar", "Stack", "Area", "Util" };
#define stylestr(x) _style[(int)x]
#endif

KmChart::KmChart() : QMainWindow(NULL)
{
    my.dialogsSetup = false;
    setupUi(this);

    toolBar->setAllowedAreas(Qt::RightToolBarArea | Qt::TopToolBarArea);
    updateToolbarLocation();
    setupEnabledActionsList();
    if (!globalSettings.initialToolbar)
	toolBar->hide();

    my.liveHidden = true;
    my.archiveHidden = true;
    timeControlAction->setChecked(false);
    toolbarAction->setChecked(true);
    my.toolbarHidden = !globalSettings.initialToolbar;
    my.consoleHidden = true;
    if (!pmDebug)
	consoleAction->setVisible(false);
    consoleAction->setChecked(false);

    setIconSize(QSize(22, 22));
    dateLabel->setFont(globalFont);
}

void KmChart::languageChange()
{
    retranslateUi(this);
}

void KmChart::init(void)
{
    timeAxisPlot->init();
}

void KmChart::setupDialogs(void)
{
    // In order to speed startup times, we delay creation of these
    // global dialogs until after the main window is displayed. We
    // do NOT delay until the very last minute, otherwise we start
    // to hit the same problem with the dialogs (if we create them
    // on-demand there's a noticable delay after action selected).

    if (my.dialogsSetup)
	return;

    my.info = new InfoDialog(this);
    my.newtab = new TabDialog(this);
    my.edittab = new TabDialog(this);
    my.exporter = new ExportDialog(this);
    my.newchart = new ChartDialog(this);
    my.editchart = new ChartDialog(this);
    my.openview = new OpenViewDialog(this);
    my.saveview = new SaveViewDialog(this);
    my.settings = new SettingsDialog(this);
    // my.assistant = new QAssistantClient("");

    connect(my.newtab->buttonOk, SIGNAL(clicked()),
				this, SLOT(acceptNewTab()));
    connect(my.edittab->buttonOk, SIGNAL(clicked()),
				this, SLOT(acceptEditTab()));
    connect(my.newchart->buttonOk, SIGNAL(clicked()),
				this, SLOT(acceptNewChart()));
    connect(my.editchart->buttonOk, SIGNAL(clicked()),
				this, SLOT(acceptEditChart()));
    connect(my.exporter->buttonOk, SIGNAL(clicked()),
				this, SLOT(acceptExport()));
    connect(my.settings->buttonOk, SIGNAL(clicked()),
				this, SLOT(acceptSettings()));
    // connect(my.assistant, SIGNAL(error(const QString &)),
    //				this, SLOT(assistantError(const QString &)));

    my.dialogsSetup = true;
}

void KmChart::quit()
{
    // End any processes we may have started
    if (my.dialogsSetup)
	my.info->quit();
    if (kmtime)
	kmtime->quit();
}

TimeAxis *KmChart::timeAxis(void)
{
    return timeAxisPlot;
}

void KmChart::enableUi(void)
{
    bool haveTabs = (tabs.size() > 1);
    bool haveCharts = (activeTab->numChart() > 0);
    bool haveLoggers = (activeTab->isRecording());
    bool haveLiveHosts = (!activeTab->isArchiveSource());

    closeTabAction->setEnabled(haveTabs);
    fileSaveViewAction->setEnabled(haveCharts);
    fileExportAction->setEnabled(haveCharts);
    filePrintAction->setEnabled(haveCharts);
    editChartAction->setEnabled(haveCharts);
    closeChartAction->setEnabled(haveCharts);
    recordStartAction->setEnabled(haveCharts && haveLiveHosts && !haveLoggers);
    recordQueryAction->setEnabled(haveLoggers);
    recordStopAction->setEnabled(haveLoggers);
    recordDetachAction->setEnabled(haveLoggers);

    zoomInAction->setEnabled(activeTab->visibleHistory() > minimumPoints());
    zoomOutAction->setEnabled(activeTab->visibleHistory() < maximumPoints());
}

const int KmChart::defaultFontSize()
{
#ifdef IS_DARWIN
    return 9;
#else
    return 7;
#endif
}

void KmChart::updateHeight(int adjustment)
{
    QSize newSize = size();
    newSize.setHeight(newSize.height() + adjustment);	// may be negative
    resize(newSize);
}

void KmChart::updateToolbarLocation()
{
    if (globalSettings.toolbarLocation)
	addToolBar(Qt::RightToolBarArea, toolBar);
    else
	addToolBar(Qt::TopToolBarArea, toolBar);
}

void KmChart::setButtonState(TimeButton::State state)
{
    timeButton->setButtonState(state);
}

void KmChart::step(bool live, KmTime::Packet *packet)
{
    for (int i = 0; i < tabs.size(); i++) {
	if (tabs.at(i)->isArchiveSource()) {
	    if (!live)
		tabs.at(i)->step(packet);
	}
	else if (live)
	    tabs.at(i)->step(packet);
    }
}

void KmChart::VCRMode(bool live, KmTime::Packet *packet, bool drag)
{
    for (int i = 0; i < tabs.size(); i++) {
	if (tabs.at(i)->isArchiveSource()) {
	    if (!live)
		tabs.at(i)->VCRMode(packet, drag);
	} else if (live)
	    tabs.at(i)->VCRMode(packet, drag);
    }
}

void KmChart::timeZone(bool live, char *tzdata)
{
    for (int i = 0; i < tabs.size(); i++) {
	if (tabs.at(i)->isArchiveSource()) {
	    if (!live)
		tabs.at(i)->setTimezone(tzdata);
	}
	else if (live)
	    tabs.at(i)->setTimezone(tzdata);
    }
}

void KmChart::setStyle(char *newlook)
{
    QApplication::setStyle(newlook);
}

void KmChart::fileOpenView()
{
    my.openview->reset();
    my.openview->show();
}

void KmChart::fileSaveView()
{
    my.saveview->reset();
    my.saveview->show();
}

void KmChart::fileExport()
{
    my.exporter->show();
}

void KmChart::acceptExport()
{
    my.exporter->flush();
}

void KmChart::filePrint()
{
    // TODO - this only prints the current chart
    QPrinter printer;
    QString creator = QString("kmchart Version ");

    creator.append(VERSION);
    printer.setCreator(creator);
    printer.setOrientation(QPrinter::Landscape);
    printer.setDocName("print.ps");	// TODO
    // TODO: pdf also now, in qt4

    QPrintDialog print(&printer, (QWidget *)this);
    if (print.exec()) {
	// TODO: needs to iterate over charts...
	Chart *cp = activeTab->currentChart();
	QwtPlotPrintFilter filter;
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
#if 0
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

	assistant->setArguments(args);
	assistant->openAssistant();
    }
#endif
}

void KmChart::helpManual()
{
    setupAssistant();
    my.assistant->showPage("manual.html");
}

void KmChart::helpTutorial()
{
    setupAssistant();
    my.assistant->showPage("tutorial.html");
}

void KmChart::helpAbout()
{
    AboutDialog about(this);
    about.exec();
}

void KmChart::helpSeeAlso()
{
    SeeAlsoDialog seealso(this);
    seealso.exec();
}

void KmChart::whatsThis()
{
    QWhatsThis::enterWhatsThisMode();
}

void KmChart::optionsTimeControl()
{
    if (activeTab->isArchiveSource()) {
	if (my.archiveHidden)
	    kmtime->showArchiveTimeControl();
	else
	    kmtime->hideArchiveTimeControl();
	my.archiveHidden = !my.archiveHidden;
	timeControlAction->setChecked(!my.archiveHidden);
    }
    else {
	if (my.liveHidden)
	    kmtime->showLiveTimeControl();
	else
	    kmtime->hideLiveTimeControl();
	my.liveHidden = !my.liveHidden;
	timeControlAction->setChecked(!my.liveHidden);
    }
}

void KmChart::optionsToolbar()
{
    if (my.toolbarHidden)
	toolBar->show();
    else
	toolBar->hide();
    my.toolbarHidden = !my.toolbarHidden;
}

void KmChart::optionsConsole()
{
    if (pmDebug) {
	if (my.consoleHidden)
	    console->show();
	else
	    console->hide();
	my.consoleHidden = !my.consoleHidden;
    }
}

void KmChart::optionsNewKmchart()
{
    QProcess *buddy = new QProcess(this);
    QStringList arguments;
    QString port;

    port.setNum(kmtime->port());
    arguments << "-p" << port;
    buddy->start("kmchart", arguments);
}

void KmChart::createNewChart(Chart::Style style)
{
    my.newchart->reset(NULL, (int)style - 1);
    my.newchart->show();
}

void KmChart::acceptNewChart()
{
    bool yAutoScale;
    double yMin, yMax;

    Chart *cp = activeTab->addChart();
    QString newTitle = my.newchart->title().trimmed();
    if (newTitle.isEmpty() == false)
	cp->changeTitle(newTitle, true);
    cp->setLegendVisible(my.newchart->legend());
    if (my.newchart->setupChartPlotsShortcut(cp) == false)
	my.newchart->setupChartPlots(cp);
    my.newchart->scale(&yAutoScale, &yMin, &yMax);
    cp->setScale(yAutoScale, yMin, yMax);
    cp->show();

    // TODO: teardown TreeViews and free up memory (both?  chartList only?)
    // TODO: might be an idea to keep available once its built, to optimise
    // subsequent accesses to the metric selection process...

    enableUi();
}

void KmChart::fileNewChart()
{
    createNewChart(Chart::LineStyle);
}

void KmChart::editChart()
{
    bool yAutoScale;
    double yMin, yMax;
    Chart *cp = activeTab->currentChart();

    my.editchart->reset(cp, (int)activeTab->currentChart()->style() - 1);
    my.editchart->titleLineEdit->setText(cp->title());
    my.editchart->legendOn->setChecked(cp->legendVisible());
    my.editchart->legendOff->setChecked(!cp->legendVisible());
    cp->scale(&yAutoScale, &yMin, &yMax);
    my.editchart->setScale(yAutoScale, yMin, yMax);
    my.editchart->show();
}

void KmChart::acceptEditChart()
{
    bool yAutoScale;
    double yMin, yMax;
    Chart *cp = my.editchart->chart();

    QString editTitle = my.editchart->title().trimmed();
    if (editTitle.isEmpty() == false && editTitle != cp->title())
	cp->changeTitle(editTitle, true);
    cp->setLegendVisible(my.editchart->legend());
    my.editchart->scale(&yAutoScale, &yMin, &yMax);
    cp->setScale(yAutoScale, yMin, yMax);
    my.editchart->setupChartPlots(cp);
    if (cp->numPlot() > 0)
	cp->show();
    else
	activeTab->deleteChart(cp);

    // TODO: need a flag on "cp" that says "currently being edited" so
    // the Chart Delete menu option doesn't shoot it down before we're
    // done here.  Both Cancel and OK will need to clear that flag.

    // TODO: teardown TreeViews and free up memory (both?  chartList only?)
    // TODO: might be an idea to keep available once its built, to optimise
    // subsequent accesses to the metric selection process...

    enableUi();
}

void KmChart::closeChart()
{
    activeTab->deleteCurrent();
    enableUi();
}

void KmChart::metricInfo(QString src, QString m, QString inst, bool archive)
{
    my.info->reset(src, m, inst, archive);
    my.info->show();
}

void KmChart::editTab()
{
    my.edittab->reset(chartTab->tabText(activeTab->index()),
		   activeTab->isArchiveSource() == false,
		   activeTab->sampleHistory(),
		   activeTab->visibleHistory());
    my.edittab->show();
}

void KmChart::acceptEditTab()
{
    chartTab->setTabText(activeTab->index(), my.edittab->labelLineEdit->text());
    activeTab->setSampleHistory((int)my.edittab->samplePointsCounter->value());
    activeTab->setVisibleHistory((int)my.edittab->visiblePointsCounter->value());
}

void KmChart::createNewTab(bool live)
{
    my.newtab->reset(QString::null, live,
		globalSettings.sampleHistory, globalSettings.visibleHistory);
    my.newtab->show();
}

void KmChart::acceptNewTab()
{
    Tab *tab = new Tab;

    if (my.newtab->isArchiveSource())
	tab->init(kmchart->tabWidget(),
		my.newtab->samplePointsCounter->value(),
		my.newtab->visiblePointsCounter->value(),
		archiveGroup, KmTime::ArchiveSource, (const char *)
		my.newtab->labelLineEdit->text().trimmed().toAscii(),
		kmtime->archiveInterval(), kmtime->archivePosition());
    else
	tab->init(kmchart->tabWidget(),
		my.newtab->samplePointsCounter->value(),
		my.newtab->visiblePointsCounter->value(),
		liveGroup, KmTime::HostSource, (const char *)
		my.newtab->labelLineEdit->text().trimmed().toAscii(),
		kmtime->liveInterval(), kmtime->livePosition());
    tabs.append(tab);
    enableUi();
}

void KmChart::addTab()
{
    createNewTab(activeTab->isArchiveSource() == false);
}

void KmChart::closeTab()
{
    int	index = chartTab->currentIndex();

    chartTab->removeTab(index);
    tabs.removeAt(index);
    setActiveTab(chartTab->currentIndex(), false);
    enableUi();
}

void KmChart::zoomIn()
{
    int visible = activeTab->visibleHistory();
    int samples = activeTab->sampleHistory();
    int decrease = qMax((int)((double)samples / 10), 1);

    visible = qMax(visible - decrease, minimumPoints());
    activeTab->setVisibleHistory(visible);

    zoomInAction->setEnabled(visible > minimumPoints());
    zoomOutAction->setEnabled(visible < samples);
}

void KmChart::zoomOut()
{
    int visible = activeTab->visibleHistory();
    int samples = activeTab->sampleHistory();
    int increase = qMax((int)((double)samples / 10), 1);

    visible = qMin(visible + increase, samples);
    activeTab->setVisibleHistory(visible);

    zoomInAction->setEnabled(visible > minimumPoints());
    zoomOutAction->setEnabled(visible < samples);
}

QTabWidget *KmChart::tabWidget()
{
    return chartTab;
}

void KmChart::setActiveTab(int index, bool redisplay)
{
    activeTab = tabs.at(index);
    if (tabs.at(index)->isArchiveSource()) {
	activeGroup = archiveGroup;
	timeControlAction->setChecked(!my.archiveHidden);
    } else {
	activeGroup = liveGroup;
	timeControlAction->setChecked(!my.liveHidden);
    }
    activeTab->updateTimeButton();
    activeTab->updateTimeAxis();

    if (redisplay)
	chartTab->setCurrentIndex(index);
}

void KmChart::activeTabChanged(QWidget *)
{
    int index = chartTab->currentIndex();

    if (index < tabs.size())
	setActiveTab(index, false);
    enableUi();
}

void KmChart::editSettings()
{
    my.settings->reset();
    my.settings->show();
}

void KmChart::acceptSettings()
{
    my.settings->flush();
    writeSettings();
}

void KmChart::setDateLabel(time_t seconds, QString tz)
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

void KmChart::setRecordState(Tab *tab, bool recording)
{
    tab->newButtonState(tab->kmtimeState(),
			KmTime::NormalMode, PM_CONTEXT_HOST, recording);
    setButtonState(tab->buttonState());
    enableUi();
}

void KmChart::recordStart()
{
    if (activeTab->startRecording())
	setRecordState(activeTab, true);
}

void KmChart::recordStop()
{
    activeTab->stopRecording();
}

void KmChart::recordQuery()
{
    activeTab->queryRecording();
}

void KmChart::recordDetach()
{
    activeTab->detachLoggers();
}

QList<QAction*> KmChart::toolbarActionsList()
{
    return my.toolbarActionsList;
}

QList<QAction*> KmChart::enabledActionsList()
{
    return my.enabledActionsList;
}

void KmChart::setupEnabledActionsList()
{
    // ToolbarActionsList is a list of all Actions available.
    // The SeparatorsList contains Actions that are group "breaks", and
    // which must be followed by a separator (if they are not the final
    // action in the toolbar, of course).
    // Finally the enabledActionsList lists the default enabled Actions.

    my.toolbarActionsList << fileNewChartAction
			  << editChartAction << closeChartAction;
    my.toolbarActionsList << fileOpenViewAction << fileSaveViewAction;
    addSeparatorAction();	// end of chart/view group
    my.toolbarActionsList << fileExportAction << filePrintAction;
    addSeparatorAction();	// end exported formats
    my.toolbarActionsList << addTabAction << editTabAction;
    my.toolbarActionsList << zoomInAction << zoomOutAction;
    addSeparatorAction();	// end tab group
    my.toolbarActionsList << recordStartAction << recordQueryAction
			  << recordStopAction;
    addSeparatorAction();	// end recording group
    my.toolbarActionsList << timeControlAction << newKmchartAction;
    addSeparatorAction();	// end other processes
    my.toolbarActionsList << helpManualAction << helpWhatsThisAction;

    my.enabledActionsList << fileNewChartAction << fileOpenViewAction
			  << addTabAction << zoomInAction << zoomOutAction
			  << fileExportAction << filePrintAction
			  << helpWhatsThisAction;

    if (globalSettings.toolbarActions.size() > 0) {
	setEnabledActionsList(globalSettings.toolbarActions, false);
	updateToolbarContents();
    }
}

void KmChart::addSeparatorAction()
{
    int index = my.toolbarActionsList.size() - 1;
    my.separatorsList << my.toolbarActionsList.at(index);
}

void KmChart::updateToolbarContents()
{
    bool needSeparator = false;

    toolBar->clear();
    for (int i = 0; i < my.toolbarActionsList.size(); i++) {
	QAction *action = my.toolbarActionsList.at(i);
	if (my.enabledActionsList.contains(action)) {
	    toolBar->addAction(action);
	    if (needSeparator) {
		toolBar->insertSeparator(action);
		needSeparator = false;
	    }
	}
	if (my.separatorsList.contains(action))
	    needSeparator = true;
    }
}

void KmChart::setEnabledActionsList(QStringList tools, bool redisplay)
{
    my.enabledActionsList.clear();
    for (int i = 0; i < my.toolbarActionsList.size(); i++) {
	QAction *action = my.toolbarActionsList.at(i);
	if (tools.contains(action->iconText()))
	    my.enabledActionsList.append(action);
    }

    if (redisplay) {
	my.toolbarHidden = (my.enabledActionsList.size() == 0);
	toolbarAction->setChecked(my.toolbarHidden);
	if (my.toolbarHidden)
	    toolBar->hide();
	else
	    toolBar->show();
    }
}
