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
#include <QHeaderView>
#include <QMessageBox>
#include "chartdialog.h"
#include "qcolorpicker.h"
#include "hostdialog.h"
#include "source.h"
#include "chart.h"
#include "tab.h"
#include "main.h"

ChartDialog::ChartDialog(QWidget* parent) : QDialog(parent)
{
    setupUi(this);
    init();
}

void ChartDialog::languageChange()
{
    retranslateUi(this);
}

void ChartDialog::init()
{
    my.chartTreeSelected = false;
    my.availableTreeSelected = false;
    my.chartTreeSingleSelected = NULL;
    my.availableTreeSingleSelected = NULL;
//    chartMetricsTreeWidget->header()->hideSection(0);
//    availableMetricsTreeWidget->header()->hideSection(0);
    connect(chartMetricsTreeWidget, SIGNAL(itemSelectionChanged()),
		this, SLOT(chartMetricsSelectionChanged()));
    connect(availableMetricsTreeWidget, SIGNAL(itemSelectionChanged()),
		this, SLOT(availableMetricsSelectionChanged()));

    my.currentColor = qRgb( -1, -1, -1 );
    hEd->setRange(0, 359);

    connect(hEd, SIGNAL(valueChanged(int)), this, SLOT(hsvEd()));
    connect(sEd, SIGNAL(valueChanged(int)), this, SLOT(hsvEd()));
    connect(vEd, SIGNAL(valueChanged(int)), this, SLOT(hsvEd()));
    connect(rEd, SIGNAL(valueChanged(int)), this, SLOT(rgbEd()));
    connect(gEd, SIGNAL(valueChanged(int)), this, SLOT(rgbEd()));
    connect(bEd, SIGNAL(valueChanged(int)), this, SLOT(rgbEd()));

    connect(applyColorLabel,
	SIGNAL(colorDropped(QRgb)), this, SIGNAL(newCol(QRgb)));
    connect(applyColorLabel,
	SIGNAL(colorDropped(QRgb)), this, SLOT(setRgb(QRgb)));
    connect(colorPicker,
	SIGNAL(newCol(int,int)), luminancePicker, SLOT(setCol(int,int)));
    connect(luminancePicker,
	SIGNAL(newHsv(int,int,int)), this, SLOT(newHsv(int,int,int)));
    connect(this,
	SIGNAL(newCol(QRgb)), this, SLOT(newColorTypedIn(QRgb)));
}

void ChartDialog::reset(Chart *chart, int style)
{
    my.chart = chart;
    if (!chart) {
	setWindowTitle(tr("New Chart"));
	tabWidget->setCurrentIndex(1);
	chartMetricsTreeWidget->clear();
    }
    else {
	setWindowTitle(tr("Edit Chart"));
	tabWidget->setCurrentIndex(0);
	chart->setupTree(chartMetricsTreeWidget);
    }
    if ((my.archiveSource = activeTab->isArchiveSource()) == true) {
	sourceButton->setToolTip(tr("Add archives"));
	sourceButton->setIcon(QIcon(":/archive.png"));
    }
    else {
	sourceButton->setToolTip(tr("Add a host"));
	sourceButton->setIcon(QIcon(":/computer.png"));
    }
    titleLineEdit->setText(tr(""));
    typeComboBox->setCurrentIndex(style);
    legendOn->setChecked(true);
    legendOff->setChecked(false);
    activeSources->setupTree(availableMetricsTreeWidget);
    my.yMin = yAxisMinimum->value();
    my.yMax = yAxisMaximum->value();

    my.chartTreeSelected = false;
    my.availableTreeSelected = false;
    my.chartTreeSingleSelected = NULL;
    my.availableTreeSingleSelected = NULL;
    enableUI();
}

void ChartDialog::enableUI()
{
    // TODO: if Utilisation mode, set Y-axis to 0-100 and disable change?

    // TODO: if (no) entries in chartMetricList, enable/disable OK button

    chartMetricLineEdit->setText(my.chartTreeSingleSelected ?
	((NameSpace *)my.chartTreeSingleSelected)->metricName() : tr(""));
    availableMetricLineEdit->setText(my.availableTreeSingleSelected ?
	((NameSpace *)my.availableTreeSingleSelected)->metricName() : tr(""));
    metricInfoButton->setEnabled(	// there can be only one
	(my.availableTreeSingleSelected && !my.chartTreeSingleSelected) ||
	(!my.availableTreeSingleSelected && my.chartTreeSingleSelected));
    metricDeleteButton->setEnabled(my.chartTreeSelected);
    metricAddButton->setEnabled(my.availableTreeSelected);

    revertColorButton->setEnabled(my.chartTreeSingleSelected != NULL);
    applyColorButton->setEnabled(my.chartTreeSingleSelected != NULL);
    if (my.chartTreeSingleSelected != NULL) {
	NameSpace *n = (NameSpace *)my.chartTreeSingleSelected;
	revertColorLabel->setColor(n->originalColor());
	setCurrentColor(n->currentColor().rgb());
    }
    else {
	revertColorLabel->setColor(QColor(0xff, 0xff, 0xff));
	setCurrentColor(QColor(0x00, 0x00, 0x00).rgb());
    }
}

Chart *ChartDialog::chart()
{
    return my.chart;
}

void ChartDialog::chartMetricsSelectionChanged()
{
    QTreeWidgetItemIterator iterator(chartMetricsTreeWidget,
					QTreeWidgetItemIterator::Selected);
    my.chartTreeSingleSelected = *iterator;
    if ((my.chartTreeSelected = (my.chartTreeSingleSelected != NULL)))
	if (*(++iterator) != NULL)
	    my.chartTreeSingleSelected = NULL;	// multiple selections
    enableUI();
}

void ChartDialog::availableMetricsSelectionChanged()
{
    QTreeWidgetItemIterator iterator(availableMetricsTreeWidget,
					QTreeWidgetItemIterator::Selected);
    my.availableTreeSingleSelected = *iterator;
    if ((my.availableTreeSelected = (my.availableTreeSingleSelected != NULL)))
	if (*(++iterator) != NULL)
	    my.availableTreeSingleSelected = NULL;	// multiple selections
    enableUI();
}

void ChartDialog::metricInfoButtonClicked()
{
    NameSpace *name = (NameSpace *)(my.chartTreeSingleSelected ?
		my.chartTreeSingleSelected : my.availableTreeSingleSelected);
    kmchart->metricInfo(name->sourceName(), name->metricName(),
			name->instanceName(), name->isArchiveMode());
}

void ChartDialog::metricDeleteButtonClicked()
{
    QTreeWidgetItemIterator iterator(chartMetricsTreeWidget,
					QTreeWidgetItemIterator::Selected);
    for (; (*iterator); ++iterator) {
	NameSpace *name = (NameSpace *)(*iterator);
	name->removeFromTree(chartMetricsTreeWidget);
    }
}

void ChartDialog::metricAddButtonClicked()
{
    QList<NameSpace *> list;
    QTreeWidgetItemIterator iterator(availableMetricsTreeWidget,
					QTreeWidgetItemIterator::Selected);
    for (; (*iterator); ++iterator)
        list.append((NameSpace *)(*iterator));
    availableMetricsTreeWidget->clearSelection();
    chartMetricsTreeWidget->clearSelection();	// selection(s) made below
    for (int i = 0; i < list.size(); i++) {
	list.at(i)->addToTree(chartMetricsTreeWidget);
    }
}

void ChartDialog::sourceButtonClicked()
{
    int sts;

    if (my.archiveSource) {
	ArchiveDialog *a = new ArchiveDialog(this);
	QStringList al;

	if (a->exec() == QDialog::Accepted)
	    al = a->selectedFiles();
	for (QStringList::Iterator it = al.begin(); it != al.end(); ++it) {
	    QString ar = *it;
	    if ((sts = archiveGroup->use(PM_CONTEXT_ARCHIVE,
					 (const char *)ar.toAscii())) < 0) {
		ar.prepend(tr("Cannot open PCP archive: "));
		ar.append(tr("\n"));
		ar.append(tr(pmErrStr(sts)));
		QMessageBox::warning(this, pmProgname, ar,
		    QMessageBox::Ok|QMessageBox::Default|QMessageBox::Escape,
		    Qt::NoButton, Qt::NoButton);
	    } else {
		archiveSources->add(archiveGroup->which());
		archiveSources->setupTree(availableMetricsTreeWidget);
		archiveGroup->updateBounds();
	    }
	}
	delete a;
    } else {
	HostDialog *h = new HostDialog(this);

	h->portLabel->setEnabled(false);
	h->portLineEdit->setEnabled(false);
	if (h->exec() == QDialog::Accepted) {
	    QString proxy = h->portLineEdit->text().trimmed();
	    QString host = h->hostLineEdit->text().trimmed();
	    if ((sts = liveGroup->use(PM_CONTEXT_HOST,
					(const char *)host.toAscii())) < 0) {
		host.prepend(tr("Cannot connect to host: "));
		host.append(tr("\n"));
		if (!proxy.isEmpty()) {
			host.append(tr(" proxy: "));
			host.append(proxy);
			host.append("\n");
		}
		host.append(tr(pmErrStr(sts)));
		QMessageBox::warning(this, pmProgname, host,
		    QMessageBox::Ok|QMessageBox::Default|QMessageBox::Escape,
		    Qt::NoButton, Qt::NoButton);
	    } else {
		liveSources->add(liveGroup->which());
		liveSources->setupTree(availableMetricsTreeWidget);
	    }
	}
	delete h;
    }
}

int ChartDialog::style(void)
{
    return typeComboBox->currentIndex();
}

QString ChartDialog::title(void)
{
    return titleLineEdit->text();
}

bool ChartDialog::legend(void)
{
    return legendOn->isChecked();
}

void ChartDialog::legendOnClicked()
{
    legendOn->setChecked(true);
    legendOff->setChecked(false);
}

void ChartDialog::legendOffClicked()
{
    legendOn->setChecked(false);
    legendOff->setChecked(true);
}

void ChartDialog::scale(bool *autoScale, double *yMin, double *yMax)
{
    *autoScale = autoScaleOn->isChecked();
    *yMin = my.yMin;
    *yMax = my.yMax;
}

void ChartDialog::setScale(bool autoScale, double yMin, double yMax)
{
    autoScaleOn->setChecked(autoScale);
    autoScaleOff->setChecked(!autoScale);
    my.yMin = yMin;
    my.yMax = yMax;
}

void ChartDialog::autoScaleOnClicked()
{
    autoScaleOn->setChecked(true);
    autoScaleOff->setChecked(false);
    minTextLabel->setEnabled(false);
    maxTextLabel->setEnabled(false);
    yAxisMinimum->setEnabled(false);
    yAxisMaximum->setEnabled(false);
}

void ChartDialog::autoScaleOffClicked()
{
    autoScaleOn->setChecked(false);
    autoScaleOff->setChecked(true);
    minTextLabel->setEnabled(true);
    maxTextLabel->setEnabled(true);
    yAxisMinimum->setEnabled(true);
    yAxisMaximum->setEnabled(true);
}

void ChartDialog::yAxisMinimumValueChanged(double value)
{
    if (my.yMin != value) {
	my.yMin = value;
	if (my.yMin > my.yMax)
	    yAxisMaximum->setValue(value);
    }
}

void ChartDialog::yAxisMaximumValueChanged(double value)
{
    if (my.yMax != value) {
	my.yMax = value;
	if (my.yMax < my.yMin)
	    yAxisMinimum->setValue(value);
    }	
}

// Sets all widgets to display h,s,v
void ChartDialog::newHsv(int h, int s, int v)
{
    setHsv(h, s, v);
    colorPicker->setCol(h, s);
    luminancePicker->setCol(h, s, v);
}

// Sets all widgets to display rgb
void ChartDialog::setCurrentColor(QRgb rgb)
{
    setRgb(rgb);
    newColorTypedIn(rgb);
}

// Sets all widgets exept cs to display rgb
void ChartDialog::newColorTypedIn(QRgb rgb)
{
    int h, s, v;
    rgb2hsv(rgb, h, s, v);
    colorPicker->setCol(h, s);
    luminancePicker->setCol(h, s, v);
}

void ChartDialog::setRgb(QRgb rgb)
{
    my.currentColor = rgb;
    rgb2hsv(my.currentColor, my.hue, my.sat, my.val);
    hEd->setValue(my.hue);
    sEd->setValue(my.sat);
    vEd->setValue(my.val);
    rEd->setValue(qRed(my.currentColor));
    gEd->setValue(qGreen(my.currentColor));
    bEd->setValue(qBlue(my.currentColor));
    showCurrentColor();
}

void ChartDialog::setHsv(int h, int s, int v)
{
    QColor c;
    c.setHsv(h, s, v);
    my.currentColor = c.rgb();
    my.hue = h; my.sat = s; my.val = v;
    hEd->setValue(my.hue);
    sEd->setValue(my.sat);
    vEd->setValue(my.val);
    rEd->setValue(qRed(my.currentColor));
    gEd->setValue(qGreen(my.currentColor));
    bEd->setValue(qBlue(my.currentColor));
    showCurrentColor();
}

QRgb ChartDialog::currentColor()
{
    return my.currentColor;
}

void ChartDialog::rgbEd()
{
    my.currentColor = qRgb(rEd->value(), gEd->value(), bEd->value());
    rgb2hsv(my.currentColor, my.hue, my.sat, my.val);
    hEd->setValue(my.hue);
    sEd->setValue(my.sat);
    vEd->setValue(my.val);
    showCurrentColor();
    emit newCol(my.currentColor);
}

void ChartDialog::hsvEd()
{
    my.hue = hEd->value();
    my.sat = sEd->value();
    my.val = vEd->value();
    QColor c;
    c.setHsv(my.hue, my.sat, my.val);
    my.currentColor = c.rgb();
    rEd->setValue(qRed(my.currentColor));
    gEd->setValue(qGreen(my.currentColor));
    bEd->setValue(qBlue(my.currentColor));
    showCurrentColor();
    emit newCol(my.currentColor);
}

void ChartDialog::showCurrentColor()
{
    applyColorLabel->setColor(my.currentColor);
}

void ChartDialog::applyColorButtonClicked()
{
    NameSpace *ns = (NameSpace *)my.chartTreeSingleSelected;
    ns->setCurrentColor(my.currentColor, chartMetricsTreeWidget);
}

void ChartDialog::revertColorButtonClicked()
{
    NameSpace *ns = (NameSpace *)my.chartTreeSingleSelected;
    ns->setCurrentColor(ns->originalColor(), NULL);
}

void ChartDialog::setupChartPlots(Chart *cp)
{
    // First iterate over the current Charts metrics, removing any
    // that are no longer in the chartMetricsTreeWidget.  This is a
    // no-op in the createChart case, of course.

    int m;
    int nplots = cp->numPlot();	// Use a copy as we change it in the loop body
    for (m = 0; m < nplots; m++) {
	QTreeWidgetItemIterator iterator1(chartMetricsTreeWidget,
				    QTreeWidgetItemIterator::Selectable);
	for (; (*iterator1); ++iterator1) {
	    if (matchChartPlot(cp, (NameSpace *)(*iterator1), m))
		break;
	}
	if ((*iterator1) == NULL)
	    deleteChartPlot(cp, m);
    }

    // Second step is to iterate over all the chartMetricsTreeWidget
    // entries, and either create new plots or edit existing ones.

    QTreeWidgetItemIterator iterator2(chartMetricsTreeWidget,
				    QTreeWidgetItemIterator::Selectable);
    for (; *iterator2; ++iterator2) {
	NameSpace *n = (NameSpace *)(*iterator2);
	if (existsChartPlot(cp, n, &m))
	    changeChartPlot(cp, n, m);
	else
	    createChartPlot(cp, n);
    }
}

bool ChartDialog::setupChartPlotsShortcut(Chart *cp)
{
    // This "shortcut" is used in the New Chart case - for speed in
    // creating new charts (a common operation), we allow the user
    // to bypass the step of moving plots from the Available Metrics
    // list to the Chart Metrics list.
    // IOW, if the Chart Metrics list is empty, but we do find one
    // or more Available Metrics selections, create a chart with them.
    // 
    // Return value indicates whether New Chart creation process is
    // complete at the end, or whether we need to continue on with
    // populating the new chart with Chart Metrics list plots.

    if (chartMetricsTreeWidget->invisibleRootItem()->childCount() > 0)
	return false;	// go do regular creation paths

    int i;
    QTreeWidgetItemIterator iterator(availableMetricsTreeWidget,
				   QTreeWidgetItemIterator::Selected);
    for (i = 0; (*iterator); ++iterator, i++) {
	NameSpace *n = (NameSpace *)(*iterator);
	QColor c = Chart::defaultColor(-1);
	n->setCurrentColor(c, NULL);
	createChartPlot(cp, n);
    }
    if (i == 0) {
	QString msg;
	msg.append(tr("No metrics were selected.\n"));
	msg.append(tr("Cannot create a new Chart\n"));
	QMessageBox::warning(this, pmProgname, msg,
		    QMessageBox::Ok|QMessageBox::Default|QMessageBox::Escape,
		    Qt::NoButton, Qt::NoButton);
    }
    return true;	// either way, we're finished now
}

bool ChartDialog::matchChartPlot(Chart *cp, NameSpace *name, int m)
{
    // compare: name, source, proxy
    if (cp->metricName(m) != name->metricName())
	return false;
    if (cp->metricContext(m) != name->metricContext())
	return false;
    // TODO: proxy support needed here (string compare on proxy name)
    return true;
}

bool ChartDialog::existsChartPlot(Chart *cp, NameSpace *name, int *m)
{
    for (int i = 0; i < cp->numPlot(); i++) {
	if (matchChartPlot(cp, name, i)) {
	    *m = i;
	    return true;
	}
    }
    *m = -1;
    return false;
}

void ChartDialog::changeChartPlot(Chart *cp, NameSpace *name, int m)
{
    cp->setColor(m, name->currentColor());
    // TODO: support for plot legend labels (at least preserve 'em!)
}

void ChartDialog::createChartPlot(Chart *cp, NameSpace *name)
{
    pmMetricSpec pms;

    pms.isarch = name->isArchiveMode();
    // TODO: null, leak later?
    pms.source = strdup((const char *)name->sourceName().toAscii()); 
    pms.metric = strdup((const char *)name->metricName().toAscii());
    if (name->isInst()) {
	pms.ninst = 1;
	pms.inst[0] = strdup((const char *)name->instanceName().toAscii());
    }
    else {
	pms.ninst = 0;
	pms.inst[0] = NULL;
    }
    int m = cp->addPlot(&pms, NULL);	// TODO: legend label support here
    cp->setColor(m, name->currentColor());
}

void ChartDialog::deleteChartPlot(Chart *cp, int m)
{
    cp->delPlot(m);
}
