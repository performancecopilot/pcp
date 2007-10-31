/*
 * Copyright (c) 2007, Aconex.  All Rights Reserved.
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
#ifndef KMCHART_H
#define KMCHART_H

#include "ui_kmchart.h"
#include <kmtime.h>

class TimeAxis;
class NameSpace;
class TabDialog;
class InfoDialog;
class ChartDialog;
class ExportDialog;
class SearchDialog;
class OpenViewDialog;
class SaveViewDialog;
class SettingsDialog;
class QAssistantClient;

class KmChart : public QMainWindow, public Ui::KmChart
{
    Q_OBJECT

public:
    KmChart();

    typedef enum {
	DebugApp = 0x1,
	DebugUi = 0x1,
	DebugProtocol = 0x2,
	DebugView = 0x4,
	DebugTimeless = 0x8,
    } DebugOptions;

    static const int defaultFontSize();
    static const double defaultChartDelta() { return 1.0; }	// seconds
    static const double defaultLoggerDelta() { return 1.0; }
    static const int defaultVisibleHistory() { return 60; }	// points
    static const int defaultSampleHistory() { return 180; }
    static const int defaultTimerTimeout() { return 3000; }	// milliseconds
    static const int minimumPoints() { return 2; }
    static const int maximumPoints() { return 360; }
    static const int maximumLegendLength() { return 20; }	// chars
    static const int minimumChartHeight() { return 80; }	// pixels

    Tab *activeTab() { return chartTabWidget->activeTab(); }
    void setActiveTab(int index, bool redisplay);
    bool isArchiveTab();
    TabWidget *tabWidget() { return chartTabWidget; }
    TimeAxis *timeAxis() { return timeAxisPlot; }

    virtual void step(bool livemode, KmTime::Packet *kmtime);
    virtual void VCRMode(bool livemode, KmTime::Packet *kmtime, bool drag);
    virtual void timeZone(bool livemode, char *tzdata);
    virtual void setStyle(char *style);
    virtual void setupAssistant();
    virtual void updateHeight(int);
    virtual void createNewChart(Chart::Style style);
    virtual void metricInfo(QString src, QString m, QString inst, bool archive);
    virtual void metricSearch(QTreeWidget *pmns);
    virtual void createNewTab(bool liveMode);
    virtual void setDateLabel(time_t seconds, QString tz);
    virtual void setButtonState(TimeButton::State state);
    virtual void setRecordState(Tab *tab, bool recording);

    virtual void updateToolbarContents();
    virtual void updateToolbarLocation();
    virtual QList<QAction*> toolbarActionsList();
    virtual QList<QAction*> enabledActionsList();
    virtual void setupEnabledActionsList();
    virtual void addSeparatorAction();
    virtual void setEnabledActionsList(QStringList tools, bool redisplay);

    virtual void newScheme();	// request new scheme of settings dialog
    virtual void newScheme(QString);	// reply back to requesting dialog(s)

    virtual void resetTimer();

    void painter(QPainter *, int w, int h, bool);

public slots:
    virtual void init();
    virtual void quit();
    virtual void enableUi();
    virtual void setupDialogs();
    virtual void fileOpenView();
    virtual void fileSaveView();
    virtual void fileExport();
    virtual void filePrint();
    virtual void fileQuit();
    virtual void assistantError(const QString &);
    virtual void helpManual();
    virtual void helpTutorial();
    virtual void helpAbout();
    virtual void helpSeeAlso();
    virtual void whatsThis();
    virtual void optionsTimeControl();
    virtual void optionsToolbar();
    virtual void optionsConsole();
    virtual void optionsNewKmchart();
    virtual void acceptNewChart();
    virtual void fileNewChart();
    virtual void editChart();
    virtual void acceptEditChart();
    virtual void closeChart();
    virtual void editTab();
    virtual void acceptEditTab();
    virtual void acceptNewTab();
    virtual void acceptExport();
    virtual void addTab();
    virtual void closeTab();
    virtual void activeTabChanged(int);
    virtual void editSettings();
    virtual void acceptSettings();
    virtual void recordStart();
    virtual void recordQuery();
    virtual void recordStop();
    virtual void recordDetach();
    virtual void timeout();
    virtual void zoomIn();
    virtual void zoomOut();

protected slots:
    virtual void languageChange();
    virtual void closeEvent(QCloseEvent *);

private:
    struct {
	QTimer *timer;
	bool dialogsSetup;
	bool liveHidden;
	bool archiveHidden;
	bool toolbarHidden;
	bool consoleHidden;
	TabWidget *tabs;
	QPrinter *printer;
	TimeAxis *timeaxis;
	TabDialog *newtab;
	TabDialog *edittab;
	InfoDialog *info;
	SearchDialog *search;
	ChartDialog *newchart;
	ChartDialog *editchart;
	ExportDialog *exporter;
	OpenViewDialog *openview;
	SaveViewDialog *saveview;
	SettingsDialog *settings;
	QAssistantClient *assistant;
	QList<QAction*> separatorsList;		// separator follow these
	QList<QAction*> toolbarActionsList;	// all toolbar actions
	QList<QAction*> enabledActionsList;	// currently visible actions
    } my;
};

#endif	// KMCHART_H
