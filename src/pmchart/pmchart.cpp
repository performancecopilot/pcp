/*
 * Copyright (c) 2012-2015, Red Hat.
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
 * Copyright (c) 2006-2009, Aconex.  All Rights Reserved.
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
#include <QtCore/QUrl>
#include <QtCore/QTimer>
#include <QtCore/QLibraryInfo>
#include <QtGui/QDesktopServices>
#include <QtGui/QDesktopWidget>
#include <QtGui/QApplication>
#include <QtGui/QPrintDialog>
#include <QtGui/QMessageBox>
#include <QtGui/QWhatsThis>
#include <QtGui/QPainter>

#include "main.h"
#include "pmchart.h"
#include "aboutdialog.h"
#include "chartdialog.h"
#include "exportdialog.h"
#include "infodialog.h"
#include "openviewdialog.h"
#include "recorddialog.h"
#include "searchdialog.h"
#include "samplesdialog.h"
#include "saveviewdialog.h"
#include "seealsodialog.h"
#include "settingsdialog.h"
#include "tabdialog.h"
#include "statusbar.h"

PmChart::PmChart() : QMainWindow(NULL)
{
    my.dialogsSetup = false;
    setIconSize(QSize(22, 22));

    setupUi(this);

    my.statusBar = new StatusBar;
    setStatusBar(my.statusBar);
    connect(my.statusBar->timeFrame(), SIGNAL(clicked()),
				this, SLOT(editSamples()));
    connect(my.statusBar->timeButton(), SIGNAL(clicked()),
				this, SLOT(optionsShowTimeControl()));

    my.timeAxisRightAlign = toolBar->height();
    toolBar->setAllowedAreas(Qt::RightToolBarArea | Qt::TopToolBarArea);
    connect(toolBar, SIGNAL(orientationChanged(Qt::Orientation)),
		this, SLOT(updateToolbarOrientation(Qt::Orientation)));
    updateToolbarLocation();
    setupEnabledActionsList();
    if (!globalSettings.initialToolbar && !outfile)
	toolBar->hide();

    toolbarAction->setChecked(true);
    my.toolbarHidden = !globalSettings.initialToolbar;
    my.consoleHidden = true;
    if (!pmDebug)
	consoleAction->setVisible(false);
    consoleAction->setChecked(false);

    if (outfile)
	QTimer::singleShot(0, this, SLOT(exportFile()));
    else
	QTimer::singleShot(PmChart::defaultTimeout(), this, SLOT(timeout()));
}

void PmChart::languageChange()
{
    retranslateUi(this);
}

void PmChart::init(void)
{
    my.statusBar->init();
}

void PmChart::setupDialogs(void)
{
    // In order to speed startup times, we delay creation of these
    // global dialogs until after the main window is displayed. We
    // do NOT delay until the very last minute, otherwise we start
    // to hit the same problem with the dialogs (if we create them
    // on-demand there's a noticable delay after action selected).

    if (my.dialogsSetup)
	return;

    my.info = new InfoDialog(this);
    my.search = new SearchDialog(this);
    my.newtab = new TabDialog(this);
    my.edittab = new TabDialog(this);
    my.samples = new SamplesDialog(this);
    my.exporter = new ExportDialog(this);
    my.newchart = new ChartDialog(this);
    my.openview = new OpenViewDialog(this);
    my.saveview = new SaveViewDialog(this);
    my.settings = new SettingsDialog(this);

    connect(my.newtab->buttonOk, SIGNAL(clicked()),
				this, SLOT(acceptNewTab()));
    connect(my.edittab->buttonOk, SIGNAL(clicked()),
				this, SLOT(acceptEditTab()));
    connect(my.samples->buttonOk, SIGNAL(clicked()),
				this, SLOT(acceptEditSamples()));
    connect(my.exporter->buttonOk, SIGNAL(clicked()),
				this, SLOT(acceptExport()));
    my.dialogsSetup = true;
}

void PmChart::quit()
{
    // End any processes we may have started and close any open dialogs
    if (my.dialogsSetup) {
	my.info->reject();
    }
    if (pmtime)
	pmtime->quit();
#ifdef HAVE_UNSETENV
    unsetenv("PCP_STDERR");
#else
    putenv("PCP_STDERR=");
#endif
    pmflush();
}

void PmChart::setValueText(QString &string)
{
    my.statusBar->setValueText(string);
    QTimer::singleShot(PmChart::defaultTimeout(), this, SLOT(timeout()));
}

void PmChart::timeout()
{
    my.statusBar->clearValueText();
}

void PmChart::closeEvent(QCloseEvent *)
{
    quit();
}

void PmChart::enableUi(void)
{
    bool haveTabs = (chartTabWidget->size() > 1);
    bool haveCharts = (activeTab()->gadgetCount() > 0);
    bool haveLoggers = (activeTab()->isRecording());
    bool haveLiveHosts = (!activeTab()->isArchiveSource() && !Lflag);

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

    zoomInAction->setEnabled(activeGroup->visibleHistory() > minimumPoints());
    zoomOutAction->setEnabled(activeGroup->visibleHistory() < maximumPoints());
}

void PmChart::updateBackground(void)
{
    liveGroup->updateBackground();
    archiveGroup->updateBackground();
}

int PmChart::defaultFontSize()
{
#if defined(IS_DARWIN)
    return 9;
#elif defined(IS_MINGW)
    return 8;
#else
    return 7;
#endif
}

void PmChart::updateHeight(int adjustment)
{
    QSize newSize = size();
    int ideal = newSize.height() + adjustment;	// may be negative
    int sized = QApplication::desktop()->availableGeometry().height();

#ifdef DESPERATE
    console->post(PmChart::DebugUi,
	"updateHeight() oldsize h=%d,w=%d maximum=%d proposed=%d",
	newSize.height(), newSize.width(), sized, ideal);
#endif

    if (ideal > sized)
	ideal = sized;
    newSize.setHeight(ideal);
    resize(newSize);
}

void PmChart::updateToolbarLocation()
{
    if (globalSettings.toolbarLocation)
	addToolBar(Qt::RightToolBarArea, toolBar);
    else
	addToolBar(Qt::TopToolBarArea, toolBar);
    setUnifiedTitleAndToolBarOnMac(globalSettings.nativeToolbar);
}

void PmChart::updateToolbarOrientation(Qt::Orientation orientation)
{
    my.statusBar->setTimeAxisRightAlignment(
		orientation == Qt::Vertical ? my.timeAxisRightAlign : 0);
}

void PmChart::setButtonState(QedTimeButton::State state)
{
    my.statusBar->timeButton()->setButtonState(state);
}

void PmChart::step(bool live, QmcTime::Packet *packet)
{
    if (live)
	liveGroup->step(packet);
    else
	archiveGroup->step(packet);
}

void PmChart::VCRMode(bool live, QmcTime::Packet *packet, bool drag)
{
    if (live)
	liveGroup->VCRMode(packet, drag);
    else
	archiveGroup->VCRMode(packet, drag);
}

void PmChart::timeZone(bool live, QmcTime::Packet *packet, char *tzdata)
{
    if (live)
	liveGroup->setTimezone(packet, tzdata);
    else
	archiveGroup->setTimezone(packet, tzdata);
}

void PmChart::setStyle(char *newlook)
{
    QApplication::setStyle(newlook);
}

void PmChart::fileOpenView()
{
    setupDialogs();
    my.openview->reset();
    my.openview->show();
}

void PmChart::fileSaveView()
{
    // If we have one host only, we default to "host dynamic" views.
    // Otherwise (multiple hosts), default to explicit host names.
    int i, ngadgets = activeTab()->gadgetCount();
    bool hostDynamic = true;
    for (i = 0; i < ngadgets; i++) {
	Gadget *gadget = activeTab()->gadget(i);
	if (gadget->hosts().size() > 1)
	    hostDynamic = false;
    }

    setupDialogs();
    my.saveview->reset(hostDynamic);
    my.saveview->show();
}

void PmChart::fileExport()
{
    setupDialogs();
    my.exporter->reset();
    my.exporter->show();
}

void PmChart::acceptExport()
{
    my.exporter->flush();
}

void PmChart::filePrint()
{
    QPrinter printer;
    QString creator = QString("pmchart Version ");

    creator.append(pmGetConfig("PCP_VERSION"));
    printer.setCreator(creator);
    printer.setOrientation(QPrinter::Portrait);
    printer.setDocName("pmchart.pdf");

    QPrintDialog print(&printer, (QWidget *)this);
    if (print.exec()) {
	QPainter qp(&printer);
	painter(&qp, printer.width(), printer.height(), false, false);
    }
}

void PmChart::updateFont(const QString &family, const QString &style, int size)
{
    int i, ngadgets = activeTab()->gadgetCount();

    globalFont->setFamily(family);
    globalFont->setPointSize(size);
    globalFont->setStyle(QFont::StyleNormal);
    if (style.contains("Italic"))
	globalFont->setItalic(true);
    if (globalSettings.fontStyle.contains("Bold"))
	globalFont->setBold(true);

    for (i = 0; i < ngadgets; i++)
	activeTab()->gadget(i)->resetFont();
    my.statusBar->resetFont();
}

void PmChart::painter(QPainter *qp, int pw, int ph, bool transparent, bool currentOnly)
{
    double scale_h = 0;
    double scale_w = 0;
    int i, ngadgets = activeTab()->gadgetCount();
    QSize size;
    QRect rect;	// used for print layout calculations

    qp->setFont(*globalFont);

    console->post("painter() pw=%d ph=%d ngadgets=%d", pw, ph, ngadgets);
    for (i = 0; i < ngadgets; i++) {
	Gadget *gadget = activeTab()->gadget(i);
	if (currentOnly && gadget != activeTab()->currentGadget())
	    continue;
	size = gadget->size();
	console->post("  [%d] w=%d h=%d", i, size.width(), size.height());
	if (size.width() > scale_w) scale_w = size.width();
	scale_h += size.height();
    }
    console->post("  final_w=%d final_h=%d", (int)scale_w, (int)scale_h);
    // timeaxis is _always_ less narrow than the plot(s)
    // datelabel is _never_ wider than the timeaxis
    // so width calculation is done, just need to consider the heights
    size = my.statusBar->timeAxis()->size();
    console->post("  timeaxis w=%d h=%d", size.width(), size.height());
    scale_h += size.height() - TIMEAXISFUDGE;
    size = my.statusBar->dateLabel()->size();
    console->post("  datelabel w=%d h=%d", size.width(), size.height());
    scale_h += size.height();
    console->post("  final_w=%d final_h=%d", (int)scale_w, (int)scale_h);
    if (scale_w/pw > scale_h/ph) {
	// window width drives scaling
	scale_w = pw / scale_w;
	scale_h = scale_w;
    }
    else {
	// window height drives scaling
	scale_h = ph / scale_h;
	scale_w = scale_h;
    }
    console->post("  final chart scale_w=%.2f scale_h=%.2f", scale_w, scale_h);
    rect.setX(0);
    rect.setY(0);
    for (i = 0; i < ngadgets; i++) {
	Gadget *gadget = activeTab()->gadget(i);
	if (currentOnly && gadget != activeTab()->currentGadget())
	    continue;
	size = gadget->size();
	rect.setWidth((int)(size.width()*scale_w+0.5));
	rect.setHeight((int)(size.height()*scale_h+0.5));
	console->post("  [%d] @ (%d,%d) w=%d h=%d", i, rect.x(), rect.y(), rect.width(), rect.height());
	gadget->print(qp, rect, transparent);
	rect.setY(rect.y()+rect.height());
    }

    // time axis
    rect.setY(rect.y() - TIMEAXISFUDGE);
    size = my.statusBar->timeAxis()->size();
    rect.setX(pw-(int)(size.width()*scale_w+0.5));
    rect.setWidth((int)(size.width()*scale_w+0.5));
    rect.setHeight((int)(size.height()*scale_h+0.5));
    console->post("  timeaxis @ (%d,%d) w=%d h=%d", rect.x(), rect.y(), rect.width(), rect.height());
    my.statusBar->timeAxis()->print(qp, rect, transparent);
    rect.setY(rect.y()+rect.height());

    // date label below time axis
    size = my.statusBar->dateLabel()->size();
    rect.setX(pw-(int)(size.width()*scale_w+0.5));
    rect.setWidth((int)(size.width()*scale_w+0.5));
    rect.setHeight((int)(size.height()*scale_h+0.5));
    console->post("  datelabel @ (%d,%d) w=%d h=%d", rect.x(), rect.y(), rect.width(), rect.height());
    qp->drawText(rect, Qt::AlignRight, my.statusBar->dateText());
}

void PmChart::fileQuit()
{
    QApplication::exit(0);
}

void PmChart::helpManual()
{
    bool ok;
    QString documents("file://");
    QString separator = QString(__pmPathSeparator());
    documents.append(pmGetConfig("PCP_HTML_DIR"));
    documents.append(separator).append("index.html");
    ok = QDesktopServices::openUrl(QUrl(documents, QUrl::TolerantMode));
    if (!ok) {
	documents.prepend("Failed to open:\n");
	QMessageBox::warning(this, pmProgname, documents);
    }
}

void PmChart::helpAbout()
{
    AboutDialog about(this);
    about.exec();
}

void PmChart::helpAboutQt()
{
    QApplication::aboutQt();
}

void PmChart::helpSeeAlso()
{
    SeeAlsoDialog seealso(this);
    seealso.exec();
}

void PmChart::whatsThis()
{
    QWhatsThis::enterWhatsThisMode();
}

void PmChart::optionsShowTimeControl()
{
    if (activeTab()->isArchiveSource())
	pmtime->showArchiveTimeControl();
    else
	pmtime->showLiveTimeControl();
}

void PmChart::optionsHideTimeControl()
{
    if (activeTab()->isArchiveSource())
	pmtime->hideArchiveTimeControl();
    else
	pmtime->hideLiveTimeControl();
}

void PmChart::optionsToolbar()
{
    if (my.toolbarHidden)
	toolBar->show();
    else
	toolBar->hide();
    my.toolbarHidden = !my.toolbarHidden;
}

void PmChart::optionsConsole()
{
    if (pmDebug) {
	if (my.consoleHidden)
	    console->show();
	else
	    console->hide();
	my.consoleHidden = !my.consoleHidden;
    }
}

void PmChart::optionsNewPmchart()
{
    QProcess *buddy = new QProcess(this);
    QStringList arguments;
    QString port;

    port.setNum(pmtime->port());
    arguments << "-p" << port;
    for (unsigned int i = 0; i < archiveGroup->numContexts(); i++) {
	QmcSource source = archiveGroup->context(i)->source();
	arguments << "-a" << source.source();
    }
    for (unsigned int i = 0; i < liveGroup->numContexts(); i++) {
	QmcSource source = liveGroup->context(i)->source();
	arguments << "-h" << source.source();
    }
    if (Lflag)
	arguments << "-L";
    buddy->start("pmchart", arguments);
}

//
// Note: Called from both Apply and OK on New Chart dialog
// Must only called when the changes are definately going to
// be applied, so all input validation must be done by now.
//
Chart *PmChart::acceptNewChart()
{
    bool yAutoScale;
    double yMin, yMax;
    QString scheme;
    int sequence;

    Chart *cp = new Chart(activeTab(), activeTab()->splitter());
    activeGroup->addGadget(cp);
    activeTab()->addGadget(cp);

    QString newTitle = my.newchart->title().trimmed();
    if (newTitle.isEmpty() == false)
	cp->changeTitle(newTitle, true);
    cp->setLegendVisible(my.newchart->legend());
    cp->setAntiAliasing(my.newchart->antiAliasing());
    cp->setRateConvert(my.newchart->rateConvert());
    my.newchart->updateChartPlots(cp);
    my.newchart->scale(&yAutoScale, &yMin, &yMax);
    cp->setScale(yAutoScale, yMin, yMax);
    my.newchart->scheme(&scheme, &sequence);
    cp->setScheme(scheme, sequence);

    activeGroup->setupWorldView();
    activeTab()->showGadgets();

    enableUi();
    return cp;
}

void PmChart::fileNewChart()
{
    setupDialogs();
    my.newchart->reset();
    my.newchart->show();
}

void PmChart::editChart()
{
    Chart *cp = (Chart *)activeTab()->currentGadget();

    setupDialogs();
    my.newchart->reset(cp);
    my.newchart->show();
}

void PmChart::acceptEditChart()
{
    bool yAutoScale;
    double yMin, yMax;
    QString scheme;
    int sequence;

    Chart *cp = my.newchart->chart();
    QString editTitle = my.newchart->title().trimmed();
    if (editTitle.isEmpty() == false && editTitle != cp->title())
	cp->changeTitle(editTitle, true);
    cp->setLegendVisible(my.newchart->legend());
    cp->setAntiAliasing(my.newchart->antiAliasing());
    cp->setRateConvert(my.newchart->rateConvert());
    my.newchart->scale(&yAutoScale, &yMin, &yMax);
    cp->setScale(yAutoScale, yMin, yMax);
    my.newchart->updateChartPlots(cp);
    my.newchart->scheme(&scheme, &sequence);
    cp->setScheme(scheme, sequence);
    cp->replot();
    cp->show();

    enableUi();
}

void PmChart::closeChart()
{
    activeTab()->deleteCurrent();
    enableUi();
}

void PmChart::metricInfo(QString src, QString m, QString inst, int srcType)
{
    setupDialogs();
    my.info->reset(src, m, inst, srcType);
    my.info->show();
}

void PmChart::metricSearch(QTreeWidget *pmns)
{
    setupDialogs();
    my.search->reset(pmns);
    my.search->show();
}

void PmChart::editSamples()
{
    int samples = activeGroup->sampleHistory();
    int visible = activeGroup->visibleHistory();

    setupDialogs();
    my.samples->reset(samples, visible);
    my.samples->show();
}

void PmChart::acceptEditSamples()
{
    activeGroup->setSampleHistory(my.samples->samples());
    activeGroup->setVisibleHistory(my.samples->visible());
    activeGroup->setupWorldView();
    activeTab()->showGadgets();
}

void PmChart::editTab()
{
    setupDialogs();
    my.edittab->reset(chartTabWidget->tabText(chartTabWidget->currentIndex()),
			activeGroup->isArchiveSource() == false);
    my.edittab->show();
}

void PmChart::acceptEditTab()
{
    QString label =  my.edittab->label();
    chartTabWidget->setTabText(chartTabWidget->currentIndex(), label);
}

void PmChart::createNewTab(bool live)
{
    setupDialogs();
    my.newtab->reset(QString::null, live);
    my.newtab->show();
}

void PmChart::addTab()
{
    createNewTab(isArchiveTab() == false);
}

void PmChart::acceptNewTab()
{
    Tab *tab = new Tab;
    QString label = my.newtab->labelLineEdit->text().trimmed();

    if (my.newtab->isArchiveSource())
	tab->init(chartTabWidget, archiveGroup, label);
    else
	tab->init(chartTabWidget, liveGroup, label);
    chartTabWidget->insertTab(tab);
    enableUi();
}

void PmChart::addActiveTab(Tab *tab)
{
    chartTabWidget->insertTab(tab);
    setActiveTab(chartTabWidget->size() - 1, true);
    enableUi();
}

void PmChart::zoomIn()
{
    int visible = activeGroup->visibleHistory();
    int samples = activeGroup->sampleHistory();
    int decrease = qMax(qMin((int)((double)samples / 10), visible/2), 1);

    console->post("zoomIn: vis=%d s=%d dec=%d\n", visible, samples, decrease);

    visible = qMax(visible - decrease, minimumPoints());
    activeGroup->setVisibleHistory(visible);
    activeGroup->setupWorldView();
    activeTab()->showGadgets();

    zoomInAction->setEnabled(visible > minimumPoints());
    zoomOutAction->setEnabled(visible < samples);
}

void PmChart::zoomOut()
{
    int visible = activeGroup->visibleHistory();
    int samples = activeGroup->sampleHistory();
    int increase = qMax(qMin((int)((double)samples / 10), visible/2), 1);

    console->post("zoomOut: vis=%d s=%d dec=%d\n", visible, samples, increase);

    visible = qMin(visible + increase, samples);
    activeGroup->setVisibleHistory(visible);
    activeGroup->setupWorldView();
    activeTab()->showGadgets();

    zoomInAction->setEnabled(visible > minimumPoints());
    zoomOutAction->setEnabled(visible < samples);
}

bool PmChart::isTabRecording()
{
    return activeTab()->isRecording();
}

bool PmChart::isArchiveTab()
{
    return activeTab()->isArchiveSource();
}

void PmChart::closeTab()
{
    int	index = chartTabWidget->currentIndex();

    chartTabWidget->removeTab(index);
    if (index > 0)
	index--;
    setActiveTab(index, false);
    enableUi();
}

void PmChart::setActiveTab(int index, bool redisplay)
{
    console->post("PmChart::setActiveTab index=%d r=%d", index, redisplay);
    
    if (chartTabWidget->setActiveTab(index) == true)
	activeGroup = archiveGroup;
    else
	activeGroup = liveGroup;
    activeGroup->updateTimeButton();
    activeGroup->updateTimeAxis();

    if (redisplay)
	chartTabWidget->setCurrentIndex(index);
}

void PmChart::activeTabChanged(int index)
{
    if (index < chartTabWidget->size())
	setActiveTab(index, false);
    enableUi();
}

void PmChart::editSettings()
{
    setupDialogs();
    my.settings->reset();
    my.settings->show();
}

void PmChart::setDateLabel(time_t seconds, QString tz)
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
    my.statusBar->setDateText(label);
}

void PmChart::setDateLabel(QString label)
{
    my.statusBar->setDateText(label);
}

void PmChart::setRecordState(bool record)
{
    liveGroup->newButtonState(liveGroup->pmtimeState(),
				QmcTime::NormalMode, record);
    setButtonState(liveGroup->buttonState());
    enableUi();
}

void PmChart::recordStart()
{
    if (activeTab()->startRecording())
	setRecordState(true);
}

void PmChart::recordStop()
{
    activeTab()->stopRecording();
}

void PmChart::recordQuery()
{
    activeTab()->queryRecording();
}

void PmChart::recordDetach()
{
    activeTab()->detachLoggers();
}

QList<QAction*> PmChart::toolbarActionsList()
{
    return my.toolbarActionsList;
}

QList<QAction*> PmChart::enabledActionsList()
{
    return my.enabledActionsList;
}

void PmChart::setupEnabledActionsList()
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
    my.toolbarActionsList << addTabAction << editTabAction << closeTabAction;
    my.toolbarActionsList << zoomInAction << zoomOutAction;
    addSeparatorAction();	// end tab group
    my.toolbarActionsList << recordStartAction << recordStopAction;
    addSeparatorAction();	// end recording group
    my.toolbarActionsList << editSettingsAction;
    addSeparatorAction();	// end settings group
    my.toolbarActionsList << showTimeControlAction << hideTimeControlAction
			  << newPmchartAction;
    addSeparatorAction();	// end other processes
    my.toolbarActionsList << helpManualAction << helpWhatsThisAction;

    // needs to match pmchart.ui
    my.enabledActionsList << fileNewChartAction << fileOpenViewAction
				// separator
			  << zoomInAction << zoomOutAction
				// separator
			  << fileExportAction
				// separator
			  << showTimeControlAction << newPmchartAction;

    if (globalSettings.toolbarActions.size() > 0) {
	setEnabledActionsList(globalSettings.toolbarActions, false);
	updateToolbarContents();
    }
}

void PmChart::addSeparatorAction()
{
    int index = my.toolbarActionsList.size() - 1;
    my.separatorsList << my.toolbarActionsList.at(index);
}

void PmChart::updateToolbarContents()
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

void PmChart::setEnabledActionsList(QStringList tools, bool redisplay)
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

void PmChart::newScheme()
{
    my.settings->newScheme();
}

void PmChart::newScheme(QString cs)
{
    my.newchart->setCurrentScheme(cs);
    my.newchart->setupSchemeComboBox();
}

void PmChart::exportFile()
{
    int sts = ExportDialog::exportFile(outfile, outgeometry, Wflag == 0);
    QApplication::exit(sts);
}
