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
#include <QtCore/QDir>
#include <QtGui/QMessageBox>
#include <QtGui/QImageWriter>
#include <qwt/qwt_plot_printfilter.h>
#include "main.h"
#include "exportdialog.h"

ExportDialog::ExportDialog(QWidget* parent) : QDialog(parent)
{
    setupUi(this);
    init();
}

void ExportDialog::languageChange()
{
    retranslateUi(this);
}

void ExportDialog::init()
{
    QString	imgfile = QDir::homePath();

    // TODO: update the filename to display the file extension
    // when the combobox entry is updated?

    my.quality = 0.0;
    imgfile.append("/export");
    fileLineEdit->setText(imgfile);

    QStringList formats;
    QList<QByteArray> array = QImageWriter::supportedImageFormats();
    for (int i = 0; i < array.size(); i++)
	formats << QString(array.at(i));
    formatComboBox->addItems(formats);

    selectedRadioButton->setChecked(true);
    allChartsRadioButton->setChecked(false);
    qualityCounter->setValue(80.0);
    qualitySlider->setValue(80.0);
    qualitySlider->setRange(0.0, 100.0);
}

void ExportDialog::selectedRadioButtonClicked()
{
    selectedRadioButton->setChecked(true);
    allChartsRadioButton->setChecked(false);
}

void ExportDialog::allChartsRadioButtonClicked()
{
    selectedRadioButton->setChecked(false);
    allChartsRadioButton->setChecked(true);
}

void ExportDialog::qualityValueChanged(double value)
{
    if (value != my.quality) {
	my.quality = (double)(int)value;
	displayQualityCounter();
	displayQualitySlider();
    }
}

void ExportDialog::displayQualityCounter()
{
    qualityCounter->blockSignals(true);
    qualityCounter->setValue(my.quality);
    qualityCounter->blockSignals(false);
}

void ExportDialog::displayQualitySlider()
{
    qualitySlider->blockSignals(true);
    qualitySlider->setValue(my.quality);
    qualitySlider->blockSignals(false);
}

void ExportDialog::filePushButtonClicked()
{
    ExportFileDialog file(this);

    file.setDirectory(QDir::homePath());
    if (file.exec() == QDialog::Accepted)
	fileLineEdit->setText(file.selectedFiles().at(0));
}

void ExportDialog::flush()
{
    QString file = fileLineEdit->text().trimmed();

    QwtPlotPrintFilter filter;
    filter.setOptions(QwtPlotPrintFilter::PrintAll &
		     ~QwtPlotPrintFilter::PrintCanvasBackground &
		     ~QwtPlotPrintFilter::PrintWidgetBackground &
		     ~QwtPlotPrintFilter::PrintGrid);

    // Firstly, calculate the size pixmap we'll need here.
    int height = 0, width = 0;
    for (int i = 0; i < activeTab->numChart(); i++) {
	Chart *cp = activeTab->chart(i);
	if (cp != activeTab->currentChart() &&
	    selectedRadioButton->isChecked())
	    continue;
	width = qMax(width, cp->width());
	height += cp->height();
    }
    height += kmchart->timeAxis()->height();

    // TODO: make this code work properly (only dumps current chart now)
    QPixmap pixmap(width, height);
    for (int i = 0; i < activeTab->numChart(); i++) {
	Chart *cp = activeTab->chart(i);
	if (cp != activeTab->currentChart() &&
	    selectedRadioButton->isChecked())
	    continue;
	cp->print(pixmap, filter);
    }
    kmchart->timeAxis()->print(pixmap, filter);

    if (pixmap.save(file, (const char *)formatComboBox->currentText().toAscii(),
		    (int)my.quality) != true) {
	QString message = tr("Failed to save image file\n");
	message.append(file);
	QMessageBox::warning(this, pmProgname, message,
		    QMessageBox::Ok|QMessageBox::Default|QMessageBox::Escape,
		    QMessageBox::NoButton, QMessageBox::NoButton);
    }
}

// ExportFileDialog is the one which is displayed when you click on
// the image file selection push button

ExportFileDialog::ExportFileDialog(QWidget *parent) : QFileDialog(parent)
{
    setAcceptMode(QFileDialog::AcceptSave);
    setFileMode(QFileDialog::AnyFile);
    setIconProvider(fileIconProvider);
    setConfirmOverwrite(true);
}
