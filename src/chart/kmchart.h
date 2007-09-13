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
#include "infodialog.h"
#include "chartdialog.h"
#include "exportdialog.h"
#include "recorddialog.h"
#include "settingsdialog.h"
#include "openviewdialog.h"
#include "saveviewdialog.h"
#include "tabdialog.h"
#include "timeaxis.h"
#include "timebutton.h"

class KmChart : public QMainWindow, public Ui::KmChart
{
    Q_OBJECT

public:
    KmChart();

    typedef enum {
	DebugApp = 0x1,
	DebugProtocol = 0x2,
	DebugGUI = 0x4,
    } DebugOptions;

    static const int defaultSampleInterval = 2;	// seconds
    static const int defaultVisiblePoints = 24;
    static const int defaultSamplePoints = 24;
    static const int minimumPoints = 2;
    static const int maximumPoints = 360;
    static const int maximumLegendLength = 20;

    virtual TimeAxis *timeAxis();
    virtual void step(bool livemode, KmTime::Packet *kmtime);
    virtual void VCRMode(bool livemode, KmTime::Packet *kmtime, bool drag);
    virtual void timeZone(bool livemode, char *tzdata);
    virtual void setStyle(char *style);
    virtual void setupAssistant();
    virtual void createNewChart(Chart::Style style);
    virtual void metricInfo(QString src, QString m, QString inst, bool archive);
    virtual void createNewTab(bool liveMode);
    virtual QTabWidget *tabWidget();
    virtual void setActiveTab(int index, bool redisplay);
    virtual void setDateLabel(time_t seconds, QString tz);
    virtual void setButtonState(TimeButton::State state);

public slots:
    virtual void init();
    virtual void quit();
    virtual void enableUi();
    virtual void showTimeControl();
    virtual void fileOpenView();
    virtual void fileSaveView();
    virtual void fileRecord();
    virtual void fileExport();
    virtual void filePrint();
    virtual void fileQuit();
    virtual void assistantError(const QString &);
    virtual void helpManual();
    virtual void helpContents();
    virtual void helpAbout();
    virtual void helpSeeAlso();
    virtual void whatsThis();
    virtual void optionsShowTimeControl();
    virtual void optionsHideTimeControl();
    virtual void optionsLaunchNewKmchart();
    virtual void acceptNewChart();
    virtual void fileNewChart();
    virtual void editChart();
    virtual void acceptEditChart();
    virtual void deleteChart();
    virtual void editTab();
    virtual void acceptEditTab();
    virtual void acceptNewTab();
    virtual void acceptExport();
    virtual void addTab();
    virtual void closeTab();
    virtual void activeTabChanged(QWidget *);
    virtual void editSettings();
    virtual void acceptSettings();
    virtual void revertSettings();

protected slots:
    virtual void languageChange();

private:
    struct {
	bool initDone;
	QPrinter *printer;
	TimeAxis *timeaxis;
	TabDialog *newtab;
	TabDialog *edittab;
	InfoDialog *info;
	ChartDialog *newchart;
	ChartDialog *editchart;
	ExportDialog *exporter;
	OpenViewDialog *openview;
	SaveViewDialog *saveview;
	SettingsDialog *settings;
	QAssistantClient *assistant;
    } my;
};

#endif	// KMCHART_H
