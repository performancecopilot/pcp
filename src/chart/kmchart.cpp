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
#include "seealsodialog.h"

#define DESPERATE 0

#if DESPERATE
char *_style[] = { "None", "Line", "Bar", "Stack", "Area", "Util" };
#define stylestr(x) _style[(int)x]
#endif

KmChart::KmChart() : QMainWindow(NULL)
{
    setupUi(this);
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
    connect(my.settings->buttonCancel, SIGNAL(clicked()),
				this, SLOT(revertSettings()));
    // connect(my.assistant, SIGNAL(error(const QString &)),
    //				this, SLOT(assistantError(const QString &)));
}

TimeAxis *KmChart::timeAxis(void)
{
    return timeAxisPlot;
}

void KmChart::enableUi(void)
{
    bool haveTabs = (ntabs > 1);
    bool haveCharts = (activeTab->numChart() > 0);
    bool haveLiveHosts = (!activeTab->isArchiveSource());

    closeTabAction->setEnabled(haveTabs);
    deleteTabAction->setEnabled(haveTabs);
    fileSaveViewAction->setEnabled(haveCharts);
    fileRecordAction->setEnabled(haveCharts && haveLiveHosts);
    fileExportAction->setEnabled(haveCharts);
    filePrintAction->setEnabled(haveCharts);
    editChartAction->setEnabled(haveCharts);
    deleteChartAction->setEnabled(haveCharts);
}

void KmChart::setButtonState(TimeButton::State state)
{
    timeButton->setButtonState(state);
}

void KmChart::step(bool live, KmTime::Packet *packet)
{
    for (int i = 0; i < ntabs; i++) {
	if (tabs[i]->isArchiveSource()) {
	    if (!live)
		tabs[i]->step(packet);
	}
	else if (live)
	    tabs[i]->step(packet);
    }
}

void KmChart::VCRMode(bool live, KmTime::Packet *packet, bool drag)
{
    for (int i = 0; i < ntabs; i++) {
	if (tabs[i]->isArchiveSource()) {
	    if (!live)
		tabs[i]->VCRMode(packet, drag);
	} else if (live)
	    tabs[i]->VCRMode(packet, drag);
    }
}

void KmChart::timeZone(bool live, char *tzdata)
{
    for (int i = 0; i < ntabs; i++) {
	if (tabs[i]->isArchiveSource()) {
	    if (!live)
		tabs[i]->setTimezone(tzdata);
	}
	else if (live)
	    tabs[i]->setTimezone(tzdata);
    }
}

void KmChart::setStyle(char *newlook)
{
    globalSettings.style = QApplication::setStyle(tr(newlook));
}

void KmChart::showTimeControl()
{
    activeTab->showTimeControl();
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

void KmChart::fileRecord()
{
    if (!activeTab->isRecording()) {
	if (activeTab->setRecording(true)) {
	    fileRecordAction->setText(tr("Stop &Recording ..."));
	    activeTab->newButtonState(activeTab->kmtimeState(),
			KmTime::NormalMode, PM_CONTEXT_HOST, true);
	    setButtonState(activeTab->buttonState());
	}
    } else {
	activeTab->setRecording(false);
	fileRecordAction->setText(tr("&Record ..."));
	activeTab->newButtonState(activeTab->kmtimeState(),
			KmTime::NormalMode, PM_CONTEXT_HOST, false);
	setButtonState(activeTab->buttonState());
    }
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
    my.assistant->showPage(tr("manual.html"));
}

void KmChart::helpContents()
{
    setupAssistant();
    my.assistant->showPage(tr("contents.html"));
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

void KmChart::optionsShowTimeControl()
{
    if (activeTab->isArchiveSource())
	kmtime->showArchiveTimeControl();
    else
	kmtime->showLiveTimeControl();
}

void KmChart::optionsHideTimeControl()
{
    if (activeTab->isArchiveSource())
	kmtime->hideArchiveTimeControl();
    else
	kmtime->hideLiveTimeControl();
}

void KmChart::optionsLaunchNewKmchart()
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

    // TODO: check type of New Chart dialog matches current activeTab
    // If not, dont allow dialog to be dismissed

    Chart *cp = activeTab->addChart();
    cp->setStyle((Chart::Style)(my.newchart->style() + 1));
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

    if (my.editchart->style() + 1 != cp->style())
	cp->setStyle((Chart::Style)(my.editchart->style() + 1));
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

void KmChart::deleteChart()
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
		   activeTab->isArchiveSource(),
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
    tabs = (Tab **)realloc(tabs, (ntabs+1) * sizeof(Tab *));	// TODO: NULL
    tabs[ntabs] = new Tab();
    if (my.newtab->isArchiveSource())
	tabs[ntabs]->init(kmchart->tabWidget(),
		(int)my.newtab->samplePointsCounter->value(),
		(int)my.newtab->visiblePointsCounter->value(),
		archiveGroup, KmTime::ArchiveSource, (const char *)
		my.newtab->labelLineEdit->text().trimmed().toAscii(),
		kmtime->archiveInterval(), kmtime->archivePosition());
    else
	tabs[ntabs]->init(kmchart->tabWidget(),
		(int)my.newtab->samplePointsCounter->value(),
		(int)my.newtab->visiblePointsCounter->value(),
		liveGroup, KmTime::HostSource, (const char *)
		my.newtab->labelLineEdit->text().trimmed().toAscii(),
		kmtime->liveInterval(), kmtime->livePosition());
    ntabs++;
    enableUi();
}

void KmChart::addLiveTab()
{
    createNewTab(true);
}

void KmChart::addArchiveTab()
{
    createNewTab(false);
}

void KmChart::closeTab()
{
    int	index = chartTab->currentIndex();

    chartTab->removeTab(index);
    if (index < ntabs - 1)
	memmove(&tabs[index], &tabs[index+1], sizeof(Tab *));
    ntabs--;
    setActiveTab(chartTab->currentIndex(), false);
    enableUi();
}

QTabWidget *KmChart::tabWidget()
{
    return chartTab;
}

void KmChart::setActiveTab(int index, bool redisplay)
{
    activeTab = tabs[index];
    if (tabs[index]->isArchiveSource()) {
	activeGroup = archiveGroup;
	activeSources = archiveSources;
    } else {
	activeGroup = liveGroup;
	activeSources = liveSources;
    }
    activeTab->updateTimeButton();
    activeTab->updateTimeAxis();

    if (redisplay)
	chartTab->setCurrentIndex(index);
}

void KmChart::activeTabChanged(QWidget *)
{
    int index = chartTab->currentIndex();

    if (index < ntabs)
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

void KmChart::revertSettings()
{
    my.settings->revert();
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
