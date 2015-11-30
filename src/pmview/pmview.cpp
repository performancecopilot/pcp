/*
 * Copyright (c) 2007-2009, Aconex.  All Rights Reserved.
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
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
#include <QUrl>
#include <QTimer>
#include <QLibraryInfo>
#include <QtGui/QDesktopServices>
#include <QtGui/QApplication>
#include <QtGui/QPrintDialog>
#include <QtGui/QMessageBox>
#include <QtGui/QWhatsThis>

#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoBaseColor.h>
#include <Inventor/nodes/SoCube.h>

#include <iostream>
using namespace std;

#include "barobj.h"
#include "gridobj.h"
#include "stackobj.h"
#include "labelobj.h"
#include "modlist.h"
#include "defaultobj.h"
#include "pcpcolor.h"

#include "main.h"
#include "pmview.h"
#include "scenegroup.h"
#include "qed_console.h"
#include "qed_statusbar.h"
#include "qed_timecontrol.h"
#include "qed_recorddialog.h"

QString		theConfigName;
QString		theAltConfigName;
FILE		*theConfigFile;
FILE		*theAltConfig;
float		theGlobalScale = 1.2;
char		**frontend_argv;
int		frontend_argc;

PmView::PmView() : QMainWindow(NULL)
{
    my.dialogsSetup = false;

    setIconSize(QSize(22, 22));
    setupUi(this);

    SoQt::init(widget);
    my.viewer = new SoQtExaminerViewer(widget);

    my.statusBar = new QedStatusBar;
    setStatusBar(my.statusBar);

    toolBar->setAllowedAreas(Qt::RightToolBarArea | Qt::TopToolBarArea);
    connect(toolBar, SIGNAL(orientationChanged(Qt::Orientation)),
		this, SLOT(updateToolbarOrientation(Qt::Orientation)));
    updateToolbarLocation();
    setupEnabledActionsList();
    if (!globalSettings.initialToolbar)
	toolBar->hide();

    my.liveHidden = true;
    my.archiveHidden = true;
    timeControlAction->setChecked(false);
    my.menubarHidden = false;
    my.toolbarHidden = !globalSettings.initialToolbar;
    toolbarAction->setChecked(globalSettings.initialToolbar);
    my.consoleHidden = true;
    if (!pmDebug)
	consoleAction->setVisible(false);
    consoleAction->setChecked(false);

    // Build Scene Graph
    my.root = new SoSeparator;
    my.root->ref();

    SoPerspectiveCamera *camera = new SoPerspectiveCamera;
    camera->orientation.setValue(SbVec3f(1, 0, 0), -M_PI/6.0);
    my.root->addChild(camera);

    my.drawStyle = new SoDrawStyle;
    my.drawStyle->style.setValue(SoDrawStyle::FILLED);
    my.root->addChild(my.drawStyle);

#if 0
    // TODO is this needed?
    if (outfile)
	QTimer::singleShot(0, this, SLOT(exportFile()));
    else
#endif
	QTimer::singleShot(PmView::defaultTimeout(), this, SLOT(timeout()));

}

void PmView::languageChange()
{
    retranslateUi(this);
}

void PmView::init(void)
{
    my.statusBar->init();
}

void
PmView::selectionCB(ModList *, bool redraw)
{
    RenderOptions options = PmView::metricLabel;

    if (redraw)
	options = (RenderOptions)(options | PmView::inventor);
    pmview->render(options, 0);
}

bool PmView::view(bool showAxis,
	     float xAxis, float yAxis, float zAxis, float angle, float scale)
{
    if (theModList->size() == 0) {
	warningMsg(_POS_, "No modulated objects in scene");
    }

    // Setup remainder of the scene graph
    my.root->addChild(theModList->root());

    viewer()->setSceneGraph(my.root);
    viewer()->setAutoRedraw(true);
    viewer()->setTitle(pmProgname);
    if (showAxis)
	viewer()->setFeedbackVisibility(true);

    SbBool smooth = TRUE;
    int passes = 1;
    char *sval = NULL;

#if 0	// TODO: QSettings API
    sval = VkGetResource("antiAliasSmooth", XmRString);
    if (sval && strcmp(sval, "default") != 0 && strcasecmp(sval, "false") == 0)
	smooth = FALSE;
    sval = VkGetResource("antiAliasPasses", XmRString);
    if (sval != NULL && strcmp(sval, "default"))
	passes = atoi(sval);
#endif

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1)
	cerr << "PmView::view: antialiasing set to smooth = "
	     << (smooth == TRUE ? "true" : "false")
	     << ", passes = " << passes << endl;
#endif

    if (passes > 1)
        viewer()->setAntialiasing(smooth, atoi(sval));
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1)
	cerr << "PmView::view: displaying window" << endl;
#endif

    viewer()->viewAll();

    if (angle != 0.0 || scale != 0.0) {
	SoTransform *tran = new SoTransform();
	if (angle != 0.0)
	    tran->rotation.setValue(SbVec3f(xAxis, yAxis, zAxis), angle);
	if (scale != 0.0)
	    tran->scaleFactor.setValue(scale, scale, scale);
	theModList->root()->insertChild(tran, 0);
    }

    PmView::render((RenderOptions)(PmView::inventor | PmView::metricLabel), 0);
    viewer()->saveHomePosition();

    return true;
}

void PmView::render(RenderOptions options, time_t theTime)
{
    viewer()->setAutoRedraw(false);

    if (options & PmView::metrics)
	theModList->refresh(true);

    if (options & PmView::inventor)
	viewer()->render();

    if (options & PmView::metricLabel) {
	theModList->infoText(my.text);
	if (my.text != my.prevText) {
	    my.prevText = my.text;
	    if (my.text.length() == 0)
		// TODO: clear label string
		;
	    else {
		// TODO: set label string to my.text
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL1)
		cerr << "PmView::render: metricLabel text \"" <<
			my.text() << "\"" << endl;
#endif
	    }
	}
    }

    if (options & PmView::timeLabel)
	setDateLabel(theTime, QString::null);	// TODO

    viewer()->setAutoRedraw(true);
}

QMenu *PmView::createPopupMenu(void)
{
    QMenu *menu = QMainWindow::createPopupMenu();
    menu->addAction(menubarAction);
    return menu;
}

void PmView::quit()
{
    // End any processes we may have started and close any open dialogs
    if (pmtime)
	pmtime->quit();
}

void PmView::closeEvent(QCloseEvent *)
{
    quit();
}

void PmView::enableUi(void)
{
#if 0
    recordStartAction->setEnabled(haveGadgets && haveLiveHosts && !haveLoggers);
    recordQueryAction->setEnabled(haveLoggers);
    recordStopAction->setEnabled(haveLoggers);
    recordDetachAction->setEnabled(haveLoggers);
#endif
}

void PmView::updateToolbarLocation()
{
#if QT_VERSION >= 0x040300
    setUnifiedTitleAndToolBarOnMac(globalSettings.nativeToolbar);
#endif
    if (globalSettings.toolbarLocation)
	addToolBar(Qt::RightToolBarArea, toolBar);
    else
	addToolBar(Qt::TopToolBarArea, toolBar);
}

void PmView::updateToolbarOrientation(Qt::Orientation orientation)
{
     (void)orientation;
	// TODO
}

void PmView::setButtonState(QedTimeButton::State state)
{
    my.statusBar->timeButton()->setButtonState(state);
}

void PmView::step(bool live, QmcTime::Packet *packet)
{
    if (live)
	liveGroup->step(packet);
    else
	archiveGroup->step(packet);
}

void PmView::VCRMode(bool live, QmcTime::Packet *packet, bool drag)
{
    if (live)
	liveGroup->VCRMode(packet, drag);
    else
	archiveGroup->VCRMode(packet, drag);
}

void PmView::timeZone(bool live, QmcTime::Packet *packet, char *tzdata)
{
    if (live)
	liveGroup->setTimezone(packet, tzdata);
    else
	archiveGroup->setTimezone(packet, tzdata);
}

void PmView::filePrint()
{
    QMessageBox::information(this, pmProgname, "Print, print, print... whirrr");
}

void PmView::fileQuit()
{
    QApplication::exit(0);
}

void PmView::helpManual()
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

void PmView::helpTutorial()
{
    bool ok;
    QString documents("file://");
    QString separator = QString(__pmPathSeparator());
    documents.append(pmGetConfig("PCP_HTML_DIR"));
    documents.append(separator).append("tutorial.html");
    ok = QDesktopServices::openUrl(QUrl(documents, QUrl::TolerantMode));
    if (!ok) {
	documents.prepend("Failed to open:\n");
	QMessageBox::warning(this, pmProgname, documents);
    }
}

void PmView::helpAbout()
{
#if 0
    AboutDialog about(this);
    about.exec();
#endif
}

void PmView::helpSeeAlso()
{
#if 0
    SeeAlsoDialog seealso(this);
    seealso.exec();
#endif
}

void PmView::whatsThis()
{
    QWhatsThis::enterWhatsThisMode();
}

void PmView::optionsTimeControl()
{
    if (activeView()->isArchiveSource()) {
	if (my.archiveHidden)
	    pmtime->showArchiveTimeControl();
	else
	    pmtime->hideArchiveTimeControl();
	my.archiveHidden = !my.archiveHidden;
	timeControlAction->setChecked(!my.archiveHidden);
    }
    else {
	if (my.liveHidden)
	    pmtime->showLiveTimeControl();
	else
	    pmtime->hideLiveTimeControl();
	my.liveHidden = !my.liveHidden;
	timeControlAction->setChecked(!my.liveHidden);
    }
}

void PmView::optionsToolbar()
{
    if (my.toolbarHidden)
	toolBar->show();
    else
	toolBar->hide();
    my.toolbarHidden = !my.toolbarHidden;
}

void PmView::optionsMenubar()
{
    if (my.menubarHidden)
	MenuBar->show();
    else
	MenuBar->hide();
    my.menubarHidden = !my.menubarHidden;
}

void PmView::optionsConsole()
{
#if 0
    if (pmDebug) {
	if (my.consoleHidden)
	    console->show();
	else
	    console->hide();
	my.consoleHidden = !my.consoleHidden;
    }
#endif
}

void PmView::optionsNewPmchart()
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
    buddy->start("pmview", arguments);
}

bool PmView::isViewRecording()
{
    return activeView()->isRecording();
}

bool PmView::isArchiveView()
{
    return activeView()->isArchiveSource();
}

void PmView::setDateLabel(time_t seconds, QString tz)
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

void PmView::setDateLabel(QString label)
{
    my.statusBar->setDateText(label);
}

void PmView::setRecordState(bool record)
{
    liveGroup->newButtonState(liveGroup->pmtimeState(),
				QmcTime::NormalMode, record);
    setButtonState(liveGroup->buttonState());
    enableUi();
}

void PmView::recordStart()
{
    if (activeView()->startRecording())
	setRecordState(true);
}

void PmView::recordStop()
{
    activeView()->stopRecording();
}

void PmView::recordQuery()
{
    activeView()->queryRecording();
}

void PmView::recordDetach()
{
    activeView()->detachLoggers();
}

QList<QAction*> PmView::toolbarActionsList()
{
    return my.toolbarActionsList;
}

QList<QAction*> PmView::enabledActionsList()
{
    return my.enabledActionsList;
}

void PmView::setupEnabledActionsList()
{
    // ToolbarActionsList is a list of all Actions available.
    // The SeparatorsList contains Actions that are group "breaks", and
    // which must be followed by a separator (if they are not the final
    // action in the toolbar, of course).
    // Finally the enabledActionsList lists the default enabled Actions.

    my.toolbarActionsList << filePrintAction;
    addSeparatorAction();	// end exported formats
    my.toolbarActionsList << recordStartAction << recordStopAction;
    addSeparatorAction();	// end recording group
    //my.toolbarActionsList << editSettingsAction;
    //addSeparatorAction();	// end settings group
    my.toolbarActionsList << timeControlAction;
    addSeparatorAction();	// end other processes
    my.toolbarActionsList << helpManualAction << helpWhatsThisAction;

    // needs to match pmview.ui
    my.enabledActionsList << filePrintAction;

    if (globalSettings.toolbarActions.size() > 0) {
	setEnabledActionsList(globalSettings.toolbarActions, false);
	updateToolbarContents();
    }
}

void PmView::addSeparatorAction()
{
    int index = my.toolbarActionsList.size() - 1;
    my.separatorsList << my.toolbarActionsList.at(index);
}

void PmView::updateToolbarContents()
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

void PmView::setEnabledActionsList(QStringList tools, bool redisplay)
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

void View::init(SceneGroup *group, QMenu *menu, QString title)
{
    my.group = group;
    QedViewControl::init(group, menu, title, globalSettings.loggerDelta);
}

QStringList View::hostList(bool)
{
    // TODO
    return QStringList();
}

QString View::pmloggerSyntax(bool)
{
// TODO
#if 0
    View *view = pmview->activeView();
    QString configdata;

    if (selectedOnly)
	configdata.append(pmview->activeGadget()->pmloggerSyntax());
    else
	for (int c = 0; c < view->gadgetCount(); c++)
	    configdata.append(gadget(c)->pmloggerSyntax());
    return configdata;
#else
    return NULL;
#endif
}

bool View::saveConfig(QString filename, bool hostDynamic,
			bool sizeDynamic, bool allViews, bool allCharts)
{
// TODO
#if 0
    return SaveViewDialog::saveView(filename,
				hostDynamic, sizeDynamic, allViews, allCharts);
#else
    return false;
#endif
}

bool View::stopRecording()
{
// TODO
#if 0
    QString errmsg;
    bool error = ViewControl::stopRecording(errmsg);
    QStringList archiveList = ViewControl::archiveList();

    for (int i = 0; i < archiveList.size(); i++) {
	QString archive = archiveList.at(i);
	int sts;

	console->post("View::stopRecording opening archive %s",
			(const char *)archive.toLatin1());
	if ((sts = archiveGroup->use(PM_CONTEXT_ARCHIVE, archive)) < 0) {
	    errmsg.append(QApplication::tr("Cannot open PCP archive: "));
	    errmsg.append(archive);
	    errmsg.append("\n");
	    errmsg.append(pmErrStr(sts));
	    errmsg.append("\n");
	    error = true;
	}
	else {
	    archiveGroup->updateBounds();
	    QmcSource source = archiveGroup->context()->source();
	    pmtime->addArchive(source.start(), source.end(),
				source.timezone(), source.host(), true);
	}
    }

    // If all is well, we can now create the new "Record" View.
    // Order of cleanup and changing Record mode state is different
    // in the error case to non-error case, this is important for
    // getting the window state correct (i.e. pmview->enableUi()).

    if (error) {
	cleanupRecording();
	pmview->setRecordState(false);
	QMessageBox::warning(NULL, pmProgname, errmsg,
		QMessageBox::Ok|QMessageBox::Default|QMessageBox::Escape,
		QMessageBox::NoButton, QMessageBox::NoButton);
    }
    else {
	// Make the current View stop recording before changing Views
	pmview->setRecordState(false);

	View *view = new View;
	console->post("View::stopRecording creating view: delta=%.2f pos=%.2f",
			App::timevalToSeconds(*pmtime->archiveInterval()),
			App::timevalToSeconds(*pmtime->archivePosition()));
	// TODO: may need to update archive samples/visible?
	view->init(archiveGroup, pmview->viewMenu(), "Record");
	pmview->addActiveView(view);
	OpenViewDialog::openView((const char *)ViewControl::view().toLatin1());
	cleanupRecording();
    }
    return error;
#else
    return false;
#endif
}

bool View::queryRecording(void)
{
    QString errmsg;
    bool error = QedViewControl::queryRecording(errmsg);

    if (error) {
	pmview->setRecordState(false);
	QMessageBox::warning(NULL, pmProgname, errmsg,
		QMessageBox::Ok|QMessageBox::Default|QMessageBox::Escape,
		QMessageBox::NoButton, QMessageBox::NoButton);
    }
    return error;
}

bool View::detachLoggers(void)
{
    QString errmsg;
    bool error = QedViewControl::detachLoggers(errmsg);

    if (error) {
	pmview->setRecordState(false);
	QMessageBox::warning(NULL, pmProgname, errmsg,
		QMessageBox::Ok|QMessageBox::Default|QMessageBox::Escape,
		QMessageBox::NoButton, QMessageBox::NoButton);
    }
    else {
	pmview->setRecordState(false);
	cleanupRecording();
    }
    return error;
}
