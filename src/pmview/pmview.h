/*
 * Copyright (c) 2007-2009, Aconex.  All Rights Reserved.
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
#ifndef PMVIEW_H
#define PMVIEW_H

#include <Inventor/Qt/SoQt.h>
#include <Inventor/Qt/viewers/SoQtExaminerViewer.h>
#include <Inventor/nodes/SoDrawStyle.h>

#include "ui_pmview.h"

#include "qmc_time.h"
#include "qed_statusbar.h"
#include "qed_viewcontrol.h"

class ModList;
class SceneGroup;

class View : public QedViewControl
{
public:
    View() : QedViewControl() { };
    virtual ~View() { };

    void init(SceneGroup *, QMenu *, QString);
    SceneGroup *group() const { return my.group; }

    bool saveConfig(QString, bool, bool, bool, bool);
    QStringList hostList(bool);
    QString pmloggerSyntax(bool);

    bool stopRecording();
    bool queryRecording();
    bool detachLoggers();

private:
    struct {
	SceneGroup	*group;
    } my;
};

class PmView : public QMainWindow, public Ui::PmView
{
    Q_OBJECT

public:
    PmView();

    typedef enum {
	nothing = 0,
	// fetch = 0x1,		-- TODO
	metrics = 0x2,
	inventor = 0x4,
	metricLabel = 0x8,
        timeLabel = 0x10,
	all = 0xffffffff,
    } RenderOptions;

    static int defaultFontSize();
    static double defaultViewDelta() { return 1.0; }	// seconds
    static double defaultLoggerDelta() { return 1.0; }
    static int defaultTimeout() { return 3000; }		// milliseconds
    static int minimumPoints() { return 2; }
    static int maximumPoints() { return 360; }
    static int maximumLegendLength() { return 120; }	// chars
    static int minimumViewHeight() { return 80; }	// pixels

    bool view(bool, float, float, float, float, float);
    void render(RenderOptions options, time_t);
    View *activeView() { return my.viewList.at(my.activeView); }
    bool isViewRecording();
    bool isArchiveView();

    virtual void step(bool livemode, QmcTime::Packet *pmtime);
    virtual void VCRMode(bool livemode, QmcTime::Packet *pmtime, bool drag);
    virtual void timeZone(bool livemode, QmcTime::Packet *pmtime, char *tzdata);
    virtual void setDateLabel(QString label);
    virtual void setDateLabel(time_t seconds, QString tz);
    virtual void setButtonState(QedTimeButton::State state);
    virtual void setRecordState(bool recording);

    virtual QMenu *createPopupMenu();
    virtual void updateToolbarContents();
    virtual void updateToolbarLocation();
    virtual QList<QAction*> toolbarActionsList();
    virtual QList<QAction*> enabledActionsList();
    virtual void setupEnabledActionsList();
    virtual void addSeparatorAction();
    virtual void setEnabledActionsList(QStringList tools, bool redisplay);

    // Adjusted height for exporting images (without UI elements)
    int exportHeight()
	{ return height() - menuBar()->height() - toolBar->height(); }

    SoQtExaminerViewer *viewer() { return my.viewer; }
    static void selectionCB(ModList *, bool);

public slots:
    virtual void init();
    virtual void quit();
    virtual void enableUi();
    virtual void filePrint();
    virtual void fileQuit();
    virtual void helpManual();
    virtual void helpTutorial();
    virtual void helpAbout();
    virtual void helpSeeAlso();
    virtual void whatsThis();
    virtual void optionsNewPmchart();
    virtual void optionsTimeControl();
    virtual void optionsMenubar();
    virtual void optionsToolbar();
    virtual void optionsConsole();
    virtual void recordStart();
    virtual void recordQuery();
    virtual void recordStop();
    virtual void recordDetach();
    virtual void updateToolbarOrientation(Qt::Orientation);

protected slots:
    virtual void languageChange();
    virtual void closeEvent(QCloseEvent *);

private:
    struct {
	bool dialogsSetup;
	bool liveHidden;
	bool archiveHidden;
	bool menubarHidden;
	bool toolbarHidden;
	bool consoleHidden;

	QMenu *viewMenu;
	QList<QAction*> separatorsList;		// separator follow these
	QList<QAction*> toolbarActionsList;	// all toolbar actions
	QList<QAction*> enabledActionsList;	// currently visible actions

	QList<View *>viewList;
	int activeView;

	SoSeparator *root;
	SoDrawStyle *drawStyle;
	SoQtExaminerViewer *viewer;		// The examiner window

	QString text;
	QString prevText;
	QedStatusBar *statusBar;
    } my;
};

#endif	// PMVIEW_H
