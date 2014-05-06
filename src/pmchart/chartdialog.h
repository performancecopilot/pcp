/*
 * Copyright (c) 2013-2014, Red Hat.
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
#ifndef CHARTDIALOG_H
#define CHARTDIALOG_H

#include "ui_chartdialog.h"

class ChartDialog : public QDialog, public Ui::ChartDialog
{
    Q_OBJECT

public:
    ChartDialog(QWidget* parent);

    virtual void init();
    virtual void reset();
    virtual void reset(Chart *);
    virtual void enableUi();

    virtual Chart *chart(void);
    virtual QString title(void);
    virtual bool legend(void);
    virtual bool antiAliasing(void);
    virtual void scale(bool *, double *, double *);
    virtual void setScale(bool, double, double);
    virtual bool rateConvert();
    virtual void setRateConvert(bool);
    virtual void scheme(QString *, int *);
    virtual void setScheme(QString, int);
    virtual void setHsv(int h, int s, int v);
    virtual QRgb currentColor();
    virtual void setCurrentColor(QRgb);
    virtual void showCurrentColor();
    virtual void setCurrentScheme(QString);
    virtual void setupSchemeComboBox();
    virtual void setupChartMetricsTree();
    virtual void setupAvailableMetricsTree(bool);

    virtual void updateChartPlots(Chart *);

public slots:
    virtual void buttonOk_clicked();
    virtual void buttonApply_clicked();
    virtual void chartMetricsItemSelectionChanged();
    virtual void availableMetricsItemSelectionChanged();
    virtual void availableMetricsItemExpanded(QTreeWidgetItem *);
    virtual void availableMetricsTreeWidget_doubleClicked(QModelIndex);
    virtual void metricInfoButtonClicked();
    virtual void metricSearchButtonClicked();
    virtual void metricDeleteButtonClicked();
    virtual void metricAddButtonClicked();
    virtual void archiveButtonClicked();
    virtual void hostButtonClicked();
    virtual void sourceButtonClicked();
    virtual void legendOnClicked();
    virtual void legendOffClicked();
    virtual void antiAliasingOnClicked();
    virtual void antiAliasingOffClicked();
    virtual void autoScaleOnClicked();
    virtual void autoScaleOffClicked();
    virtual void rateConvertClicked();
    virtual void antiAliasingAutoClicked();
    virtual void yAxisMinimumValueChanged(double);
    virtual void yAxisMaximumValueChanged(double);
    virtual void newColor(QColor);
    virtual void newColorTypedIn(QRgb);
    virtual void applyColorButtonClicked();
    virtual void revertColorButtonClicked();
    virtual void newHsv(int, int, int);
    virtual void setRgb(QRgb);
    virtual void rgbEd();
    virtual void hsvEd();
    virtual void plotLabelLineEdit_editingFinished();
    virtual void colorSchemeComboBox_currentIndexChanged(int);

Q_SIGNALS:
    void newCol(QRgb);

protected slots:
    virtual void languageChange();

private:
    struct {
	bool archiveSource;

	bool chartTreeSelected;
	QTreeWidgetItem *chartTreeSingleSelected;

	bool availableTreeSelected;
	QTreeWidgetItem *availableTreeSingleSelected;

	double yMin;
	double yMax;
	Chart *chart;
	bool rateConvert;
	int sequence;
	QString scheme;

	int hue;
	int sat;
	int val;
	QRgb currentColor;
    } my;

    void resetCompletely();
    void resetPartially(Chart *);
    bool validate(QString &, int &);
    void setupChartPlots(Chart *);
    bool setupChartPlotsShortcut(Chart *);
    bool matchChartPlot(Chart *, NameSpace *, int);
    bool existsChartPlot(Chart *, NameSpace *, int *);
    void changeChartPlot(Chart *, NameSpace *, int);
    void createChartPlot(Chart *, NameSpace *);
    void deleteChartPlot(Chart *, int);
    void deleteChartPlots(Chart *);
};

#endif	// CHARTDIALOG_H
