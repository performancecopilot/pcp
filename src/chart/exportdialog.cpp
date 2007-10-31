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
#include <QtGui/QPainter>
#include <QtGui/QMessageBox>
#include <QtGui/QImageWriter>
#include <qwt_plot_printfilter.h>
#include "main.h"
#include "exportdialog.h"

// ExportFileDialog is the one which is displayed when you click on
// the image file selection push button
ExportFileDialog::ExportFileDialog(QWidget *parent) : QFileDialog(parent)
{
    setAcceptMode(QFileDialog::AcceptSave);
    setFileMode(QFileDialog::AnyFile);
    setIconProvider(fileIconProvider);
    setConfirmOverwrite(true);
}

ExportDialog::ExportDialog(QWidget* parent) : QDialog(parent)
{
    setupUi(this);
    init();
}

ExportDialog::~ExportDialog()
{
    free(my.format);
}

void ExportDialog::languageChange()
{
    retranslateUi(this);
}

void ExportDialog::init()
{
    QString imgfile = QDir::homePath();

    my.quality = 0;
    my.format = strdup("png");
    imgfile.append("/export.png");
    fileLineEdit->setText(imgfile);

    int png = 0;
    QStringList formats;
    QList<QByteArray> array = QImageWriter::supportedImageFormats();
    for (int i = 0; i < array.size(); i++) {
	if (array.at(i) == "png")
	    png = i;
	formats << QString(array.at(i));
    }
    formatComboBox->blockSignals(true);
    formatComboBox->addItems(formats);
    formatComboBox->setCurrentIndex(png);
    formatComboBox->blockSignals(false);

    qualitySlider->setValue(100);
    qualitySlider->setRange(0, 100);
    qualitySpinBox->setValue(100);
    qualitySpinBox->setRange(0, 100);
}

void ExportDialog::reset()
{
    QSize size = imageSize();
    widthSpinBox->setValue(size.width());
    heightSpinBox->setValue(size.height());
}

void ExportDialog::selectedRadioButton_clicked()
{
    selectedRadioButton->setChecked(true);
    allChartsRadioButton->setChecked(false);
    reset();
}

void ExportDialog::allChartsRadioButton_clicked()
{
    selectedRadioButton->setChecked(false);
    allChartsRadioButton->setChecked(true);
    reset();
}

void ExportDialog::quality_valueChanged(int value)
{
    if (value != my.quality) {
	my.quality = value;
	displayQualitySpinBox();
	displayQualitySlider();
    }
}

void ExportDialog::displayQualitySpinBox()
{
    qualitySpinBox->blockSignals(true);
    qualitySpinBox->setValue(my.quality);
    qualitySpinBox->blockSignals(false);
}

void ExportDialog::displayQualitySlider()
{
    qualitySlider->blockSignals(true);
    qualitySlider->setValue(my.quality);
    qualitySlider->blockSignals(false);
}

void ExportDialog::filePushButton_clicked()
{
    ExportFileDialog file(this);

    file.setDirectory(QDir::homePath());
    if (file.exec() == QDialog::Accepted)
	fileLineEdit->setText(file.selectedFiles().at(0));
}

void ExportDialog::formatComboBox_currentIndexChanged(QString suffix)
{
    char *format = strdup((const char *)suffix.toAscii());
    QString file = fileLineEdit->text().trimmed();
    QString regex = my.format;

    regex.append("$");
    file.replace(QRegExp(regex), suffix);
    fileLineEdit->setText(file);
    free(my.format);
    my.format = format;
}

QSize ExportDialog::imageSize()
{
    Tab *tab = kmchart->activeTab();
    int height = 0, width = 0;

    for (int i = 0; i < tab->numChart(); i++) {
	Chart *cp = tab->chart(i);
	if (cp != tab->currentChart() && selectedRadioButton->isChecked())
	    continue;
	width = qMax(width, cp->width());
	height += cp->height();
    }
    height += kmchart->timeAxis()->height();

    return QSize(width, height);
}

void ExportDialog::flush()
{
    QString file = fileLineEdit->text().trimmed();

    int width = widthSpinBox->value();
    int height = heightSpinBox->value();

    enum QImage::Format rgbFormat = (transparentCheckBox->isChecked()) ?
				QImage::Format_ARGB32 : QImage::Format_RGB32;
    QImage image(width, height, rgbFormat);
    if (transparentCheckBox->isChecked() == false)
	image.invertPixels();	// white background
    QPainter qp(&image);
    kmchart->painter(&qp, width, height, selectedRadioButton->isChecked());

    if (image.save(file, my.format, my.quality) != true) {
	QString message = tr("Failed to save image file\n");
	message.append(file);
	QMessageBox::warning(this, pmProgname, message,
		    QMessageBox::Ok|QMessageBox::Default|QMessageBox::Escape,
		    QMessageBox::NoButton, QMessageBox::NoButton);
    }
}
