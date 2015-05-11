/*
 * Copyright (c) 2013-2015, Red Hat.
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
#include <QtGui/QHeaderView>
#include <QtGui/QFileDialog>
#include <QtGui/QMessageBox>
#include "chartdialog.h"
#include "qed_colorpicker.h"
#include "hostdialog.h"
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
    my.rateConvert = true;
    my.chartTreeSelected = false;
    my.availableTreeSelected = false;
    my.chartTreeSingleSelected = NULL;
    my.availableTreeSingleSelected = NULL;
    connect(chartMetricsTreeWidget, SIGNAL(itemSelectionChanged()),
		this, SLOT(chartMetricsItemSelectionChanged()));
    connect(availableMetricsTreeWidget, SIGNAL(itemSelectionChanged()),
		this, SLOT(availableMetricsItemSelectionChanged()));
    connect(availableMetricsTreeWidget,
		SIGNAL(itemExpanded(QTreeWidgetItem *)), this,
		SLOT(availableMetricsItemExpanded(QTreeWidgetItem *)));

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
    connect(colorLineEdit,
	SIGNAL(newColor(QColor)), this, SLOT(newColor(QColor)));
    connect(this,
	SIGNAL(newCol(QRgb)), this, SLOT(newColorTypedIn(QRgb)));
}

void ChartDialog::reset()
{
    my.chart = NULL;

    setWindowTitle(tr("New Chart"));
    tabWidget->setCurrentIndex(1);
    chartMetricsTreeWidget->clear();
    titleLineEdit->setText(tr(""));
    typeComboBox->setCurrentIndex((int)Chart::LineStyle - 1);
    legendOn->setChecked(true);
    legendOff->setChecked(false);
    antiAliasingOn->setChecked(false);
    antiAliasingOff->setChecked(false);
    antiAliasingAuto->setChecked(true);
    rateConvertCheckBox->setChecked(true);
    rateConvertCheckBox->setEnabled(true);
    setCurrentScheme(QString::null);
    my.sequence = 0;

    resetCompletely();
    enableUi();
}

void ChartDialog::reset(Chart *cp)
{
    bool yAutoScale;
    double yMin, yMax;

    my.chart = cp;

    setWindowTitle(tr("Edit Chart"));
    tabWidget->setCurrentIndex(0);
    setupChartMetricsTree();
    titleLineEdit->setText(cp->title());
    typeComboBox->setCurrentIndex(cp->style() - 1);
    legendOn->setChecked(cp->legendVisible());
    legendOff->setChecked(!cp->legendVisible());
    antiAliasingOn->setChecked(cp->antiAliasing());
    antiAliasingOff->setChecked(!cp->antiAliasing());
    antiAliasingAuto->setChecked(false);
    my.rateConvert = cp->rateConvert();
    rateConvertCheckBox->setChecked(my.rateConvert);
    rateConvertCheckBox->setEnabled(false);
    cp->scale(&yAutoScale, &yMin, &yMax);
    setScale(yAutoScale, yMin, yMax);
    setScheme(cp->scheme(), cp->sequence());
    setupSchemeComboBox();

    resetCompletely();
    enableUi();
}

void ChartDialog::resetPartially(Chart *cp)
{
    my.chart = cp;

    setWindowTitle(tr("Edit Chart"));
    setupChartMetricsTree();
    enableUi();
}

//
// Code paths common to both Create/Edit dialog uses
//
void ChartDialog::resetCompletely()
{
    if ((my.archiveSource = pmchart->isArchiveTab()) == true) {
	sourceButton->setToolTip(tr("Add archives"));
	sourceButton->setIcon(QIcon(":/images/archive.png"));
    }
    else {
	sourceButton->setToolTip(tr("Add a host"));
	sourceButton->setIcon(QIcon(":/images/computer.png"));
    }
    setupAvailableMetricsTree(my.archiveSource == true);
    my.yMin = yAxisMinimum->value();
    my.yMax = yAxisMaximum->value();
    my.chartTreeSelected = false;
    my.availableTreeSelected = false;
    my.chartTreeSingleSelected = NULL;
    my.availableTreeSingleSelected = NULL;
}

void ChartDialog::enableUi()
{
    bool selfScaling = autoScaleOff->isChecked();
    minTextLabel->setEnabled(selfScaling);
    maxTextLabel->setEnabled(selfScaling);
    yAxisMinimum->setEnabled(selfScaling);
    yAxisMaximum->setEnabled(selfScaling);

    chartMetricLineEdit->setText(my.chartTreeSingleSelected ?
	((NameSpace *)my.chartTreeSingleSelected)->metricName() : tr(""));
    availableMetricLineEdit->setText(my.availableTreeSingleSelected ?
	((NameSpace *)my.availableTreeSingleSelected)->metricName() : tr(""));
    metricInfoButton->setEnabled(	// there can be only one source
	(my.availableTreeSingleSelected && !my.chartTreeSingleSelected) ||
	(!my.availableTreeSingleSelected && my.chartTreeSingleSelected));
    metricDeleteButton->setEnabled(my.chartTreeSelected);
    metricAddButton->setEnabled(my.availableTreeSelected);
    metricSearchButton->setEnabled(true);

    revertColorButton->setEnabled(my.chartTreeSingleSelected != NULL);
    applyColorButton->setEnabled(my.chartTreeSingleSelected != NULL);
    plotLabelLineEdit->setEnabled(my.chartTreeSingleSelected != NULL);
    if (my.chartTreeSingleSelected != NULL) {
	NameSpace *n = (NameSpace *)my.chartTreeSingleSelected;
	revertColorLabel->setColor(n->originalColor());
	setCurrentColor(n->currentColor().rgb());
	plotLabelLineEdit->setText(n->label());
    }
    else {
	revertColorLabel->setColor(QColor(0xff, 0xff, 0xff));
	setCurrentColor(QColor(0x00, 0x00, 0x00).rgb());
	plotLabelLineEdit->setText("");
    }
}

//
// Verify user input and don't dismiss dialog (OK) if problems found.
// Needs to handle many cases: New Chart (!my.chart) and Edit Chart,
// as well as Apply and OK.
//
bool ChartDialog::validate(QString &message, int &index)
{
    bool validInput;

    // Check some plots have been selected.
    if (!my.chart && chartMetricsTreeWidget->topLevelItemCount() == 0 &&
	my.availableTreeSelected == false) {
	message = tr("No metrics have been selected for plotting.\n");
	validInput = false;
	index = 1;
    }
    // Validate Values Axis scale range if not auto-scaling
    else if (autoScaleOn->isChecked() == false && my.yMin >= my.yMax) {
	message = tr("Values Axis scale minimum/maximum range is invalid.");
	validInput = false;
	index = 0;
    }
    // Check the archive/live type still matches the current Tab
    else if (!my.chart && my.archiveSource && !pmchart->isArchiveTab()) {
	message = tr("Cannot add an archive Chart to a live Tab");
	validInput = false;
	index = 1;
    }
    else if (!my.chart && !my.archiveSource && pmchart->isArchiveTab()) {
	message = tr("Cannot add a live host Chart to an archive Tab");
	validInput = false;
	index = 1;
    }
    else {
	validInput = true;
	index = 0;
    }
    return validInput;
}

void ChartDialog::buttonOk_clicked()
{
    QString message;
    int index;

    if (validate(message, index)) {
	if (my.chart)
	    pmchart->acceptEditChart();
	else
	    pmchart->acceptNewChart();
	QDialog::accept();
    } else {
	tabWidget->setCurrentIndex(index);
	QMessageBox::warning(this, pmProgname, message,
		    QMessageBox::Ok|QMessageBox::Default|QMessageBox::Escape,
		    Qt::NoButton, Qt::NoButton);
    }
}

void ChartDialog::buttonApply_clicked()
{
    QString message;
    int index;

    if (validate(message, index)) {
	if (my.chart)
	    pmchart->acceptEditChart();
	else	// New Chart to Edit Chart transition:
	    resetPartially(pmchart->acceptNewChart());
    }
    else {
	tabWidget->setCurrentIndex(index);
	QMessageBox::warning(this, pmProgname, message,
		    QMessageBox::Ok|QMessageBox::Default|QMessageBox::Escape,
		    Qt::NoButton, Qt::NoButton);
    }
}

Chart *ChartDialog::chart()
{
    return my.chart;
}

void ChartDialog::chartMetricsItemSelectionChanged()
{
    QTreeWidgetItemIterator iterator(chartMetricsTreeWidget,
					QTreeWidgetItemIterator::Selected);
    my.chartTreeSingleSelected = *iterator;
    if ((my.chartTreeSelected = (my.chartTreeSingleSelected != NULL)))
	if (*(++iterator) != NULL)
	    my.chartTreeSingleSelected = NULL;	// multiple selections
    enableUi();
}

void ChartDialog::availableMetricsItemSelectionChanged()
{
    QTreeWidgetItemIterator iterator(availableMetricsTreeWidget,
					QTreeWidgetItemIterator::Selected);
    my.availableTreeSingleSelected = *iterator;
    if ((my.availableTreeSelected = (my.availableTreeSingleSelected != NULL)))
	if (*(++iterator) != NULL)
	    my.availableTreeSingleSelected = NULL;	// multiple selections
    enableUi();
}

void ChartDialog::availableMetricsItemExpanded(QTreeWidgetItem *item)
{
    console->post(PmChart::DebugUi,
		 "ChartDialog::availableMetricsItemExpanded %p", item);
    NameSpace *metricName = (NameSpace *)item;
    metricName->setExpanded(true, true);
}

void ChartDialog::metricInfoButtonClicked()
{
    NameSpace *name = (NameSpace *)(my.chartTreeSingleSelected ?
		my.chartTreeSingleSelected : my.availableTreeSingleSelected);
    pmchart->metricInfo(name->sourceName(), name->metricName(),
			name->metricInstance(), name->sourceType());
}

void ChartDialog::metricDeleteButtonClicked()
{
    QList<QTreeWidgetItem *> items = chartMetricsTreeWidget->selectedItems();
    for (int i = 0; i < items.size(); i++) {
	NameSpace *name = (NameSpace *)items.at(i);
	name->removeFromTree(chartMetricsTreeWidget);
    }
}

void ChartDialog::metricSearchButtonClicked()
{
    pmchart->metricSearch(availableMetricsTreeWidget);
}

void ChartDialog::availableMetricsTreeWidget_doubleClicked(QModelIndex)
{
    metricAddButtonClicked();
}

void ChartDialog::metricAddButtonClicked()
{
    QList<NameSpace *> list;
    QTreeWidgetItemIterator iterator(availableMetricsTreeWidget,
					QTreeWidgetItemIterator::Selected);
    for (; (*iterator); ++iterator) {
	NameSpace *item = (NameSpace *)(*iterator);

	if (QmcMetric::real(item->desc().type) == true)
            list.append(item);
	else {
	    QString message = item->metricName();
	    message.prepend(tr("Cannot plot metric: "));
	    message.append(tr("\nThis metric does not have a numeric type."));
	    QMessageBox::warning(this, pmProgname, message,
		    QMessageBox::Ok|QMessageBox::Default|QMessageBox::Escape,
		    Qt::NoButton, Qt::NoButton);
	}
    }

    QString scheme = my.chart ? my.chart->scheme() : my.scheme;
    int sequence = my.chart ? my.chart->sequence() : my.sequence;

    availableMetricsTreeWidget->clearSelection();
    chartMetricsTreeWidget->clearSelection();	// selection(s) made below
    for (int i = 0; i < list.size(); i++)
	list.at(i)->addToTree(chartMetricsTreeWidget, scheme, &sequence);

    if (my.chart)
	my.chart->setSequence(sequence);
    else
	my.sequence = sequence;
}

void ChartDialog::archiveButtonClicked()
{
    QFileDialog *af = new QFileDialog(this);
    QStringList al;
    int sts;

    af->setFileMode(QFileDialog::ExistingFiles);
    af->setAcceptMode(QFileDialog::AcceptOpen);
    af->setIconProvider(fileIconProvider);
    af->setWindowTitle(tr("Add Archive"));
    af->setDirectory(QDir::toNativeSeparators(QDir::homePath()));

    if (af->exec() == QDialog::Accepted)
	al = af->selectedFiles();
    for (QStringList::Iterator it = al.begin(); it != al.end(); ++it) {
	QString archive = *it;
	if ((sts = archiveGroup->use(PM_CONTEXT_ARCHIVE, archive)) < 0) {
	    archive.prepend(tr("Cannot open PCP archive: "));
	    archive.append(tr("\n"));
	    archive.append(tr(pmErrStr(sts)));
	    QMessageBox::warning(this, pmProgname, archive,
		    QMessageBox::Ok|QMessageBox::Default|QMessageBox::Escape,
		    Qt::NoButton, Qt::NoButton);
	} else {
	    setupAvailableMetricsTree(true);
	    archiveGroup->updateBounds();
	    const QmcSource source = archiveGroup->context()->source();
	    pmtime->addArchive(source.start(), source.end(),
				source.timezone(), source.host(), false);
	}
    }
    delete af;
}

void ChartDialog::hostButtonClicked()
{
    HostDialog *host = new HostDialog(this);

    if (host->exec() == QDialog::Accepted) {
	QString hostspec = host->getHostSpecification();
	int sts, flags = host->getContextFlags();

	if (hostspec == QString::null || hostspec.length() == 0) {
	    hostspec.append(tr("Hostname not specified\n"));
	    QMessageBox::warning(this, pmProgname, hostspec,
		    QMessageBox::Ok|QMessageBox::Default|QMessageBox::Escape,
		    Qt::NoButton, Qt::NoButton);
	} else if ((sts = liveGroup->use(PM_CONTEXT_HOST, hostspec, flags)) < 0) {
	    hostspec.prepend(tr("Cannot connect to host: "));
	    hostspec.append(tr("\n"));
	    hostspec.append(tr(pmErrStr(sts)));
	    QMessageBox::warning(this, pmProgname, hostspec,
		    QMessageBox::Ok|QMessageBox::Default|QMessageBox::Escape,
		    Qt::NoButton, Qt::NoButton);
	} else {
	    console->post(PmChart::DebugUi,
			"ChartDialog::newHost: %s (flags=0x%x)",
			(const char *)hostspec.toAscii(), flags);
	    setupAvailableMetricsTree(false);
	}
    }
    delete host;
}

void ChartDialog::sourceButtonClicked()
{
    if (my.archiveSource)
	archiveButtonClicked();
    else
	hostButtonClicked();
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

bool ChartDialog::antiAliasing(void)
{
    if (antiAliasingAuto->isChecked()) {
	Chart::Style style = (Chart::Style)(typeComboBox->currentIndex() + 1);
	return (style != Chart::LineStyle);
    }
    return antiAliasingOn->isChecked();
}

void ChartDialog::antiAliasingOnClicked()
{
    antiAliasingOn->setChecked(true);
    antiAliasingOff->setChecked(false);
    antiAliasingAuto->setChecked(false);
}

void ChartDialog::antiAliasingOffClicked()
{
    antiAliasingOn->setChecked(false);
    antiAliasingOff->setChecked(true);
    antiAliasingAuto->setChecked(false);
}

void ChartDialog::antiAliasingAutoClicked()
{
    antiAliasingOn->setChecked(false);
    antiAliasingOff->setChecked(false);
    antiAliasingAuto->setChecked(true);
}

void ChartDialog::scheme(QString *scheme, int *sequence)
{
    *scheme = my.scheme;
    *sequence = my.sequence;
}

void ChartDialog::setScheme(QString scheme, int sequence)
{
    my.scheme = scheme;
    my.sequence = sequence;
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
    yAxisMaximum->setValue(yMax);
    yAxisMinimum->setValue(yMin);
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
    my.yMin = value;
}

void ChartDialog::yAxisMaximumValueChanged(double value)
{
    my.yMax = value;
}

void ChartDialog::rateConvertClicked()
{
    my.rateConvert = rateConvertCheckBox->isChecked();
}

bool ChartDialog::rateConvert()
{
    return my.rateConvert;
}

void ChartDialog::setRateConvert(bool rateConvert)
{
    my.rateConvert = rateConvert;
}

// Sets all widgets to display h,s,v
void ChartDialog::newHsv(int h, int s, int v)
{
    setHsv(h, s, v);
    colorPicker->setCol(h, s);
    luminancePicker->setCol(h, s, v);
    colorLineEdit->setCol(h, s, v);
}

// Sets all widgets to display rgb
void ChartDialog::setCurrentColor(QRgb rgb)
{
    setRgb(rgb);
    newColorTypedIn(rgb);
}

// Sets all widgets except cle to display color
void ChartDialog::newColor(QColor col)
{
    console->post(PmChart::DebugUi, "ChartDialog::newColor");
    int h, s, v;
    col.getHsv(&h, &s, &v);
    colorPicker->setCol(h, s);
    luminancePicker->setCol(h, s, v);
    setRgb(col.rgb());
}

// Sets all widgets except cs to display rgb
void ChartDialog::newColorTypedIn(QRgb rgb)
{
    console->post(PmChart::DebugUi, "ChartDialog::newColorTypedIn");
    int h, s, v;
    rgb2hsv(rgb, h, s, v);
    colorPicker->setCol(h, s);
    luminancePicker->setCol(h, s, v);
    colorLineEdit->setCol(h, s, v);
}

void ChartDialog::setRgb(QRgb rgb)
{
    console->post(PmChart::DebugUi, "ChartDialog::setRgb");
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
    console->post(PmChart::DebugUi, "ChartDialog::setHsv h=%d s=%d v=%d",h,s,v);
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
    Q_EMIT newCol(my.currentColor);
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
    Q_EMIT newCol(my.currentColor);
}

void ChartDialog::showCurrentColor()
{
    console->post(PmChart::DebugUi, "ChartDialog::showCurrentColor");
    applyColorLabel->setColor(my.currentColor);
    colorLineEdit->setColor(my.currentColor);
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

void ChartDialog::plotLabelLineEdit_editingFinished()
{
    NameSpace *ns = (NameSpace *)my.chartTreeSingleSelected;
    ns->setLabel(plotLabelLineEdit->text().trimmed());
}

void ChartDialog::setupChartMetricsTree()
{
    chartMetricsTreeWidget->clear();
    my.chart->setupTree(chartMetricsTreeWidget);
}

void ChartDialog::setupAvailableMetricsTree(bool arch)
{
    NameSpace *current = NULL;
    QList<QTreeWidgetItem*> items;
    QmcGroup *group = arch ? archiveGroup : liveGroup;

    availableMetricsTreeWidget->clear();
    for (unsigned int i = 0; i < group->numContexts(); i++) {
	QmcContext *cp = group->context(i);
	if (cp->status() < 0)
	    continue;
	NameSpace *name = new NameSpace(availableMetricsTreeWidget, cp);
	name->setExpanded(true, group->numContexts() == 1);
	name->setSelectable(false);
	availableMetricsTreeWidget->addTopLevelItem(name);
	if (i == group->contextIndex())
	    current = name;
	items.append(name);
    }
    if (items.size() > 0)
	availableMetricsTreeWidget->insertTopLevelItems(0, items);
    if (current)
	availableMetricsTreeWidget->setCurrentItem(current);
}


void ChartDialog::updateChartPlots(Chart *cp)
{
    deleteChartPlots(cp);
    if (setupChartPlotsShortcut(cp) == false)
	setupChartPlots(cp);
}

void ChartDialog::deleteChartPlots(Chart *cp)
{
    int m, nplots = cp->metricCount(); // Copy, as we change it in the loop body

    // Iterate over the current Charts metrics, removing any
    // that are no longer in the chartMetricsTreeWidget.
    // This is a no-op in the createChart case, of course.

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
}

void ChartDialog::setupChartPlots(Chart *cp)
{
    // Second step is to iterate over all the chartMetricsTreeWidget
    // entries, and either create new plots or edit existing ones.

    QTreeWidgetItemIterator iterator2(chartMetricsTreeWidget,
				    QTreeWidgetItemIterator::Selectable);
    for (; *iterator2; ++iterator2) {
	NameSpace *n = (NameSpace *)(*iterator2);
	int m;

	if (existsChartPlot(cp, n, &m))
	    changeChartPlot(cp, n, m);
	else
	    createChartPlot(cp, n);
    }
}

bool ChartDialog::setupChartPlotsShortcut(Chart *cp)
{
    // This "shortcut" is used in both the New Chart and Edit Chart
    // case.  It allows the user to bypass the step of moving plots
    // from the Available Metrics list to the Chart Plots list.
    // 
    // Return value indicates whether the Chart change is complete
    // at the end (i.e. used the shortcut), or whether we need to
    // continue on populating the new chart with Chart Plots.

    if (chartMetricsTreeWidget->topLevelItemCount() > 0)
	return false;	// go do regular creation paths

    int i, m, seq = 0;
    QTreeWidgetItemIterator iterator(availableMetricsTreeWidget,
				   QTreeWidgetItemIterator::Selected);
    for (i = 0; (*iterator); ++iterator, i++) {
	NameSpace *n = (NameSpace *)(*iterator);
	if (existsChartPlot(cp, n, &m)) {
	    changeChartPlot(cp, n, m);
	} else {
	    QColor c = nextColor(cp->scheme(), &seq);
	    n->setCurrentColor(c, NULL);
	    createChartPlot(cp, n);
	    my.sequence = seq;
	}
    }
    return true;	// either way, we're finished now
}

bool ChartDialog::matchChartPlot(Chart *cp, NameSpace *name, int m)
{
    if (cp->metricContext(m) != name->metricContext())
	return false;
    if (cp->metricName(m) != name->metricName())
	return false;
    if (cp->metricInstance(m) != name->metricInstance())
	return false;
    return true;
}

bool ChartDialog::existsChartPlot(Chart *cp, NameSpace *name, int *m)
{
    for (int i = 0; i < cp->metricCount(); i++) {
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
    Chart::Style style = (Chart::Style)(typeComboBox->currentIndex() + 1);
    cp->setStroke(m, style, name->currentColor());
    cp->setLabel(m, name->label());
    cp->reviveItem(m);
}

void ChartDialog::createChartPlot(Chart *cp, NameSpace *name)
{
    Chart::Style style = (Chart::Style)(typeComboBox->currentIndex() + 1);
    pmMetricSpec pms;
    QString label;

    label = name->label().isEmpty() ? QString::null : name->label();
    pms.isarch = (name->sourceType() == PM_CONTEXT_LOCAL) ? 2 :
		((name->sourceType() == PM_CONTEXT_ARCHIVE) ? 1 : 0);
    pms.source = strdup((const char *)name->source().toAscii());
    pms.metric = strdup((const char *)name->metricName().toAscii());
    if (!pms.source || !pms.metric)
	nomem();
    if (name->isInst()) {
	pms.ninst = 1;
	pms.inst[0] = strdup((const char *)name->metricInstance().toAscii());
	if (!pms.inst[0])
	    nomem();
    }
    else {
	pms.ninst = 0;
	pms.inst[0] = NULL;
    }
    cp->setStyle(style);
    int m = cp->addItem(&pms, label);
    if (m < 0) {
	QString	msg;
	if (pms.inst[0] != NULL)
	    msg.sprintf("Error:\nFailed to plot metric \"%s[%s]\" for\n%s %s:\n",
		pms.metric, pms.inst[0],
		pms.isarch ? "archive" : "host",
		pms.source);
	else
	    msg.sprintf("Error:\nFailed to plot metric \"%s\" for\n%s %s:\n",
		pms.metric, pms.isarch ? "archive" : "host",
		pms.source);
	if (m == PM_ERR_CONV) {
	    msg.append("Units for this metric are not compatible with other plots in this chart");
	}
	else
	    msg.append(pmErrStr(m));
	QMessageBox::critical(pmchart, pmProgname,  msg);
    }
    else {
	cp->setStroke(m, style, name->currentColor());
	cp->setLabel(m, name->label());
    }

    if (pms.ninst == 1)
	free(pms.inst[0]);
    free(pms.metric);
    free(pms.source);
}

void ChartDialog::deleteChartPlot(Chart *cp, int m)
{
    cp->removeItem(m);
}

void ChartDialog::setCurrentScheme(QString scheme)
{
    my.scheme = scheme;
    setupSchemeComboBox();
}

void ChartDialog::setupSchemeComboBox()
{
    int index = 0;

    colorSchemeComboBox->blockSignals(true);
    colorSchemeComboBox->clear();
    colorSchemeComboBox->addItem("Default Scheme");
    colorSchemeComboBox->addItem("New Scheme");
    for (int i = 0; i < globalSettings.colorSchemes.size(); i++) {
	QString name = globalSettings.colorSchemes[i].name();
	if (name == my.scheme)
	    index = i + 2;
	colorSchemeComboBox->addItem(name);
    }
    colorSchemeComboBox->setCurrentIndex(index);
    colorSchemeComboBox->blockSignals(false);
}

void ChartDialog::colorSchemeComboBox_currentIndexChanged(int index)
{
    if (index == 0)
	my.scheme = QString::null;
    else if (index == 1)
	pmchart->newScheme();
    else
	my.scheme = colorSchemeComboBox->itemText(index);
}
