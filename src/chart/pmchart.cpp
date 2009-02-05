/*
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
 * Copyright (c) 2006-2008, Aconex.  All Rights Reserved.
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
#include <QtCore/QTimer>
#include <QtCore/QLibraryInfo>
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
#include "version.h"

KmChart::KmChart() : QMainWindow(NULL)
{
    my.assistant = NULL;
    my.dialogsSetup = false;
    setIconSize(QSize(22, 22));

    setupUi(this);

    my.statusBar = new StatusBar;
    setStatusBar(my.statusBar);

    my.timeAxisRightAlign = toolBar->height();
    toolBar->setAllowedAreas(Qt::RightToolBarArea | Qt::TopToolBarArea);
    connect(toolBar, SIGNAL(orientationChanged(Qt::Orientation)),
		this, SLOT(updateToolbarOrientation(Qt::Orientation)));
    updateToolbarLocation();
    setupEnabledActionsList();
    if (!globalSettings.initialToolbar && !outfile)
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

    if (outfile)
	QTimer::singleShot(0, this, SLOT(exportFile()));
    else
	QTimer::singleShot(KmChart::defaultTimeout(), this, SLOT(timeout()));
}

void KmChart::languageChange()
{
    retranslateUi(this);
}

void KmChart::init(void)
{
    my.statusBar->init();
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
    my.search = new SearchDialog(this);
    my.newtab = new TabDialog(this);
    my.edittab = new TabDialog(this);
    my.samples = new SamplesDialog(this);
    my.exporter = new ExportDialog(this);
    my.newchart = new ChartDialog(this);
    my.editchart = new ChartDialog(this);
    my.openview = new OpenViewDialog(this);
    my.saveview = new SaveViewDialog(this);
    my.settings = new SettingsDialog(this);

    connect(my.statusBar->timeFrame(), SIGNAL(clicked()),
				this, SLOT(editSamples()));
    connect(my.statusBar->timeButton(), SIGNAL(clicked()),
				this, SLOT(optionsTimeControl()));
    connect(my.newtab->buttonOk, SIGNAL(clicked()),
				this, SLOT(acceptNewTab()));
    connect(my.edittab->buttonOk, SIGNAL(clicked()),
				this, SLOT(acceptEditTab()));
    connect(my.samples->buttonOk, SIGNAL(clicked()),
				this, SLOT(acceptEditSamples()));
    connect(my.newchart->buttonOk, SIGNAL(clicked()),
				this, SLOT(acceptNewChart()));
    connect(my.editchart->buttonOk, SIGNAL(clicked()),
				this, SLOT(acceptEditChart()));
    connect(my.exporter->buttonOk, SIGNAL(clicked()),
				this, SLOT(acceptExport()));
    my.dialogsSetup = true;
}

void KmChart::quit()
{
    // End any processes we may have started and close any open dialogs
    if (my.dialogsSetup) {
	my.info->reject();
	my.search->reject();
	my.newtab->reject();
	my.edittab->reject();
	my.samples->reject();
	my.exporter->reject();
	my.newchart->reject();
	my.editchart->reject();
	my.openview->reject();
	my.saveview->reject();
	my.settings->reject();
    }
    if (my.assistant)
	my.assistant->closeAssistant();
    if (kmtime)
	kmtime->quit();
}

void KmChart::setValueText(QString &string)
{
    my.statusBar->setValueText(string);
    QTimer::singleShot(KmChart::defaultTimeout(), this, SLOT(timeout()));
}

void KmChart::timeout()
{
    setupDialogs();
    my.statusBar->clearValueText();
}

void KmChart::closeEvent(QCloseEvent *)
{
    quit();
}

void KmChart::enableUi(void)
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

void KmChart::updateBackground(void)
{
    liveGroup->updateBackground();
    archiveGroup->updateBackground();
}

int KmChart::defaultFontSize()
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
#if QT_VERSION >= 0x040300
    setUnifiedTitleAndToolBarOnMac(globalSettings.nativeToolbar);
#endif
    if (globalSettings.toolbarLocation)
	addToolBar(Qt::RightToolBarArea, toolBar);
    else
	addToolBar(Qt::TopToolBarArea, toolBar);
}

void KmChart::updateToolbarOrientation(Qt::Orientation orientation)
{
    my.statusBar->setTimeAxisRightAlignment(
		orientation == Qt::Vertical ? my.timeAxisRightAlign : 0);
}

void KmChart::setButtonState(TimeButton::State state)
{
    my.statusBar->timeButton()->setButtonState(state);
}

void KmChart::step(bool live, KmTime::Packet *packet)
{
    if (live)
	liveGroup->step(packet);
    else
	archiveGroup->step(packet);
}

void KmChart::VCRMode(bool live, KmTime::Packet *packet, bool drag)
{
    if (live)
	liveGroup->VCRMode(packet, drag);
    else
	archiveGroup->VCRMode(packet, drag);
}

void KmChart::timeZone(bool live, KmTime::Packet *packet, char *tzdata)
{
    if (live)
	liveGroup->setTimezone(packet, tzdata);
    else
	archiveGroup->setTimezone(packet, tzdata);
}

void KmChart::setStyle(char *newlook)
{
    QApplication::setStyle(newlook);
}

void KmChart::fileOpenView()
{
    setupDialogs();
    my.openview->reset();
    my.openview->show();
}

void KmChart::fileSaveView()
{
    setupDialogs();
    my.saveview->reset();
    my.saveview->show();
}

void KmChart::fileExport()
{
    setupDialogs();
    my.exporter->reset();
    my.exporter->show();
}

void KmChart::acceptExport()
{
    my.exporter->flush();
}

void KmChart::filePrint()
{
    QPrinter printer;
    QString creator = QString("pmchart Version ");

    creator.append(VERSION);
    printer.setCreator(creator);
    printer.setOrientation(QPrinter::Portrait);
    printer.setDocName("pmchart.pdf");

    QPrintDialog print(&printer, (QWidget *)this);
    if (print.exec()) {
	QPainter qp(&printer);
	painter(&qp, printer.width(), printer.height(), false);
    }
}

void KmChart::painter(QPainter *qp, int pw, int ph, bool currentOnly)
{
    double scale_h = 0;
    double scale_w = 0;
    int i, ngadgets = activeTab()->gadgetCount();
    QSize size;
    QRect rect;	// used for print layout calculations

    qp->setFont(globalFont);

    console->post("painter() pw=%d ph=%d ngadgets=%d", pw, ph, ngadgets);
    for (i = 0; i < ngadgets; i++) {
	Gadget *gadget = activeTab()->gadget(i);
	if (currentOnly && gadget != activeTab()->currentGadget())
	    continue;
	size = gadget->size();
	if (size.width() > scale_w) scale_w = size.width();
	scale_h += size.height();
	console->post("  scale_w=%.2f scale_h=%.2f", scale_w, scale_h);
    }
    size = my.statusBar->timeAxis()->size();
    console->post("  timeaxis w=%d h=%d", size.width(), size.height());
    if (size.width() > scale_w) scale_w = size.width();
    scale_h += size.height();
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
	gadget->print(qp, rect);
	rect.setY(rect.y()+rect.height());
    }

    // timeButton icon -- not actually painted, just shift coords
    size = my.statusBar->timeButton()->size();
    console->post("  timebutton w=%d h=%d", size.width(), size.height());
    rect.setWidth((int)(size.width()*scale_w+0.5));
    rect.setHeight((int)(size.height()*scale_h+0.5));
    rect.setX(rect.width()-5);	// timeframe layout margin adjustment (5)

    // time axis
    size = my.statusBar->timeAxis()->size();
    console->post("  timeaxis w=%d h=%d", size.width(), size.height());
    rect.setWidth((int)(size.width()*scale_w+0.5));
    rect.setHeight((int)(size.height()*scale_h+0.5));
    my.statusBar->timeAxis()->print(qp, rect);
    rect.setY(rect.y()+rect.height());

    // date label below time axis
    size = my.statusBar->dateLabel()->size();
    size.setWidth(size.width());
    rect.setX(rect.x()+rect.width()-(int)(size.width()*scale_w+0.5));
    console->post("  datelabel w=%d h=%d", size.width(), size.height());
    rect.setWidth((int)(size.width()*scale_w+0.5));
    rect.setHeight((int)(size.height()*scale_h+0.5));
    qp->drawText(rect, Qt::AlignLeft, my.statusBar->dateText());
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
    if (my.assistant)
	return;
    my.assistant = new QAssistantClient(
		QLibraryInfo::location(QLibraryInfo::BinariesPath), this);
    connect(my.assistant, SIGNAL(error(const QString &)),
    		    this, SLOT(assistantError(const QString &)));
    QStringList arguments;
    QString documents = HTMLDIR;
    arguments << "-profile" << documents.append("/pmchart.adp");
    my.assistant->setArguments(arguments);
}

void KmChart::helpManual()
{
    setupAssistant();
    QString documents = HTMLDIR;
    my.assistant->showPage(documents.append("/index.html"));
}

void KmChart::helpTutorial()
{
    setupAssistant();
    QString documents = HTMLDIR;
    my.assistant->showPage(documents.append("/tutorial.html"));
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
    if (activeTab()->isArchiveSource()) {
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

void KmChart::createNewChart(Chart::Style style)
{
    setupDialogs();
    my.newchart->reset(NULL, (int)style - 1, QString::null);
    my.newchart->show();
}

void KmChart::acceptNewChart()
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
    if (my.newchart->setupChartPlotsShortcut(cp) == false)
	my.newchart->setupChartPlots(cp);
    my.newchart->scale(&yAutoScale, &yMin, &yMax);
    cp->setScale(yAutoScale, yMin, yMax);
    my.newchart->scheme(&scheme, &sequence);
    cp->setScheme(scheme, sequence);

    activeGroup->setupWorldView();
    activeTab()->showGadgets();

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
    Chart *cp = (Chart *)activeTab()->currentGadget();

    setupDialogs();
    my.editchart->reset(cp, cp->style() - 1, cp->scheme());
    my.editchart->titleLineEdit->setText(cp->title());
    my.editchart->legendOn->setChecked(cp->legendVisible());
    my.editchart->legendOff->setChecked(!cp->legendVisible());
    my.editchart->antiAliasingOn->setChecked(cp->antiAliasing());
    my.editchart->antiAliasingOff->setChecked(!cp->antiAliasing());
    my.editchart->antiAliasingAuto->setChecked(false);
    cp->scale(&yAutoScale, &yMin, &yMax);
    my.editchart->setScale(yAutoScale, yMin, yMax);
    my.editchart->setScheme(cp->scheme(), cp->sequence());
    my.editchart->show();
}

void KmChart::acceptEditChart()
{
    bool yAutoScale;
    double yMin, yMax;
    QString scheme;
    int sequence;

    Chart *cp = my.editchart->chart();
    QString editTitle = my.editchart->title().trimmed();
    if (editTitle.isEmpty() == false && editTitle != cp->title())
	cp->changeTitle(editTitle, true);
    cp->setLegendVisible(my.editchart->legend());
    cp->setAntiAliasing(my.editchart->antiAliasing());
    my.editchart->scale(&yAutoScale, &yMin, &yMax);
    cp->setScale(yAutoScale, yMin, yMax);
    my.editchart->setupChartPlots(cp);
    my.editchart->scheme(&scheme, &sequence);
    cp->setScheme(scheme, sequence);
    if (cp->metricCount() > 0) {
	cp->replot();
	cp->show();
    }
    else
	activeTab()->deleteGadget(cp);

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
    activeTab()->deleteCurrent();
    enableUi();
}

void KmChart::metricInfo(QString src, QString m, QString inst, bool archive)
{
    setupDialogs();
    my.info->reset(src, m, inst, archive);
    my.info->show();
}

void KmChart::metricSearch(QTreeWidget *pmns)
{
    setupDialogs();
    my.search->reset(pmns);
    my.search->show();
}

void KmChart::editSamples()
{
    setupDialogs();
    my.samples->reset(globalSettings.sampleHistory,
			globalSettings.visibleHistory);
    my.samples->show();
}

void KmChart::acceptEditSamples()
{
    activeGroup->setSampleHistory(my.samples->samples());
    activeGroup->setVisibleHistory(my.samples->visible());
    activeGroup->setupWorldView();
    activeTab()->showGadgets();
}

void KmChart::editTab()
{
    setupDialogs();
    my.edittab->reset(chartTabWidget->tabText(chartTabWidget->currentIndex()),
			activeGroup->isArchiveSource() == false);
    my.edittab->show();
}

void KmChart::acceptEditTab()
{
    QString label =  my.edittab->label();
    chartTabWidget->setTabText(chartTabWidget->currentIndex(), label);
}

void KmChart::createNewTab(bool live)
{
    setupDialogs();
    my.newtab->reset(QString::null, live);
    my.newtab->show();
}

void KmChart::addTab()
{
    createNewTab(isArchiveTab() == false);
}

void KmChart::acceptNewTab()
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

void KmChart::addActiveTab(Tab *tab)
{
    chartTabWidget->insertTab(tab);
    setActiveTab(chartTabWidget->size() - 1, true);
    enableUi();
}

void KmChart::zoomIn()
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

void KmChart::zoomOut()
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

bool KmChart::isTabRecording()
{
    return activeTab()->isRecording();
}

bool KmChart::isArchiveTab()
{
    return activeTab()->isArchiveSource();
}

void KmChart::closeTab()
{
    int	index = chartTabWidget->currentIndex();

    chartTabWidget->removeTab(index);
    if (index > 0)
	index--;
    setActiveTab(index, false);
    enableUi();
}

void KmChart::setActiveTab(int index, bool redisplay)
{
    console->post("KmChart::setActiveTab index=%d r=%d", index, redisplay);
    
    if (chartTabWidget->setActiveTab(index) == true) {
	activeGroup = archiveGroup;
	timeControlAction->setChecked(!my.archiveHidden);
    } else {
	activeGroup = liveGroup;
	timeControlAction->setChecked(!my.liveHidden);
    }
    activeGroup->updateTimeButton();
    activeGroup->updateTimeAxis();

    if (redisplay)
	chartTabWidget->setCurrentIndex(index);
}

void KmChart::activeTabChanged(int index)
{
    if (index < chartTabWidget->size())
	setActiveTab(index, false);
    enableUi();
}

void KmChart::editSettings()
{
    setupDialogs();
    my.settings->reset();
    my.settings->show();
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
    my.statusBar->setDateText(label);
}

void KmChart::setDateLabel(QString label)
{
    my.statusBar->setDateText(label);
}

void KmChart::setRecordState(bool record)
{
    liveGroup->newButtonState(liveGroup->kmtimeState(),
				KmTime::NormalMode, record);
    setButtonState(liveGroup->buttonState());
    enableUi();
}

void KmChart::recordStart()
{
    if (activeTab()->startRecording())
	setRecordState(true);
}

void KmChart::recordStop()
{
    activeTab()->stopRecording();
}

void KmChart::recordQuery()
{
    activeTab()->queryRecording();
}

void KmChart::recordDetach()
{
    activeTab()->detachLoggers();
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
    my.toolbarActionsList << addTabAction << editTabAction << closeTabAction;
    my.toolbarActionsList << zoomInAction << zoomOutAction;
    addSeparatorAction();	// end tab group
    my.toolbarActionsList << recordStartAction << recordStopAction;
    addSeparatorAction();	// end recording group
    my.toolbarActionsList << editSettingsAction;
    addSeparatorAction();	// end settings group
    my.toolbarActionsList << timeControlAction << newKmchartAction;
    addSeparatorAction();	// end other processes
    my.toolbarActionsList << helpManualAction << helpWhatsThisAction;

    // needs to match pmchart.ui
    my.enabledActionsList << fileNewChartAction << fileOpenViewAction
				// separator
			  << zoomInAction << zoomOutAction
				// separator
			  << fileExportAction << filePrintAction
				// separator
			  << newKmchartAction;

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

void KmChart::newScheme()
{
    my.settings->newScheme();
}

void KmChart::newScheme(QString cs)
{
    my.newchart->setCurrentScheme(cs);
    my.newchart->setupSchemeComboBox();
    my.editchart->setCurrentScheme(cs);
    my.editchart->setupSchemeComboBox();
}

void KmChart::exportFile()
{
    int sts = ExportDialog::exportFile(outfile, outgeometry, Wflag == 0);
    QApplication::exit(sts);
}
