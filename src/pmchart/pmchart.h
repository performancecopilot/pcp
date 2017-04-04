/*
 * Copyright (c) 2012-2014, Red Hat.
 * Copyright (c) 2007-2008, Aconex.  All Rights Reserved.
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
#ifndef PMCHART_H
#define PMCHART_H

#include "ui_pmchart.h"
#include "statusbar.h"
#include <qmc_time.h>

class TimeAxis;
class NameSpace;
class TabDialog;
class InfoDialog;
class ChartDialog;
class ExportDialog;
class SearchDialog;
class SamplesDialog;
class OpenViewDialog;
class SaveViewDialog;
class SettingsDialog;

class PmChart : public QMainWindow, public Ui::PmChart
{
    Q_OBJECT

public:
    PmChart();

    typedef enum {
	DebugApp = 0x1,
	DebugUi = 0x1,
	DebugProtocol = 0x2,
	DebugView = 0x4,
	DebugTimeless = 0x8,
	DebugForce = 0x10,
    } DebugOptions;

    static int defaultFontSize();
    static const char *defaultFontFamily() { return "Sans Serif"; }
    static double defaultChartDelta() { return 1.0; }	// seconds
    static double defaultLoggerDelta() { return 1.0; }
    static int defaultVisibleHistory() { return 60; }	// points
    static int defaultSampleHistory() { return 180; }
    static int defaultTimeout() { return 8000; }		// milliseconds
    static int minimumPoints() { return 2; }
    static int maximumPoints() { return 720; }
    static int maximumLegendLength() { return 120; }	// chars
    static int minimumChartHeight() { return 80; }	// pixels

    Tab *activeTab() { return chartTabWidget->activeTab(); }
    void setActiveTab(int index, bool redisplay);
    void addActiveTab(Tab *tab);
    bool isArchiveTab();
    bool isTabRecording();
    TabWidget *tabWidget() { return chartTabWidget; }
    TimeAxis *timeAxis() { return my.statusBar->timeAxis(); }
    QLabel *dateLabel() { return my.statusBar->dateLabel(); }
    Chart *acceptNewChart();

    virtual void step(bool livemode, QmcTime::Packet *pmtime);
    virtual void VCRMode(bool livemode, QmcTime::Packet *pmtime, bool drag);
    virtual void timeZone(bool livemode, QmcTime::Packet *pmtime, char *tzdata);
    virtual void setStyle(char *style);
    virtual void updateHeight(int);
    virtual void metricInfo(QString src, QString m, QString inst, int srcType);
    virtual void metricSearch(QTreeWidget *pmns);
    virtual void createNewTab(bool liveMode);
    virtual void setValueText(QString &text);
    virtual void setDateLabel(QString label);
    virtual void setDateLabel(time_t seconds, QString tz);
    virtual void setButtonState(QedTimeButton::State state);
    virtual void setRecordState(bool recording);

    virtual void updateToolbarContents();
    virtual void updateToolbarLocation();
    virtual QList<QAction*> toolbarActionsList();
    virtual QList<QAction*> enabledActionsList();
    virtual void setupEnabledActionsList();
    virtual void addSeparatorAction();
    virtual void setEnabledActionsList(QStringList tools, bool redisplay);

    virtual void newScheme();	// request new scheme of settings dialog
    virtual void newScheme(QString);	// reply back to requesting dialog(s)
    virtual void updateBackground();
    virtual void updateFont(const QString &family, const QString &style, int size);

    void painter(QPainter *qp, int pw, int ph, bool transparent, bool currentOnly);

    // Adjusted height for exporting images (without UI elements)
    int exportHeight()
	{ return height() - menuBar()->height() - toolBar->height(); }

public slots:
    virtual void init();
    virtual void quit();
    virtual void enableUi();
    virtual void exportFile();
    virtual void setupDialogs();
    virtual void fileOpenView();
    virtual void fileSaveView();
    virtual void fileExport();
    virtual void acceptExport();
    virtual void filePrint();
    virtual void fileQuit();
    virtual void helpManual();
    virtual void helpAbout();
    virtual void helpAboutQt();
    virtual void helpSeeAlso();
    virtual void whatsThis();
    virtual void optionsShowTimeControl();
    virtual void optionsHideTimeControl();
    virtual void optionsToolbar();
    virtual void optionsConsole();
    virtual void optionsNewPmchart();
    virtual void fileNewChart();
    virtual void editChart();
    virtual void acceptEditChart();
    virtual void closeChart();
    virtual void editTab();
    virtual void acceptEditTab();
    virtual void editSamples();
    virtual void acceptEditSamples();
    virtual void addTab();
    virtual void acceptNewTab();
    virtual void closeTab();
    virtual void activeTabChanged(int);
    virtual void editSettings();
    virtual void recordStart();
    virtual void recordQuery();
    virtual void recordStop();
    virtual void recordDetach();
    virtual void timeout();
    virtual void zoomIn();
    virtual void zoomOut();
    virtual void updateToolbarOrientation(Qt::Orientation);

protected slots:
    virtual void languageChange();
    virtual void closeEvent(QCloseEvent *);

private:
    struct {
	bool dialogsSetup;
	bool toolbarHidden;
	bool consoleHidden;

	TabDialog *newtab;
	TabDialog *edittab;
	InfoDialog *info;
	ChartDialog *newchart;	// shared by New and Edit Chart actions
	ExportDialog *exporter;
	SearchDialog *search;
	SamplesDialog *samples;
	OpenViewDialog *openview;
	SaveViewDialog *saveview;
	SettingsDialog *settings;

	QList<QAction*> separatorsList;		// separator follow these
	QList<QAction*> toolbarActionsList;	// all toolbar actions
	QList<QAction*> enabledActionsList;	// currently visible actions

	int timeAxisRightAlign;
	StatusBar *statusBar;
    } my;

    void editTab(int index);
};

#endif	// PMCHART_H
