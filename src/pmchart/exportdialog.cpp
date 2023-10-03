/*
 * Copyright (c) 2013,2021,2023 Red Hat.
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
#include <QDir>
#include <QPainter>
#include <QMessageBox>
#include <QImageWriter>
#include "main.h"
#include "exportdialog.h"

// ExportFileDialog is the one which is displayed when you click on
// the image file selection push button
ExportFileDialog::ExportFileDialog(QWidget *parent) : QFileDialog(parent)
{
    setAcceptMode(QFileDialog::AcceptSave);
    setFileMode(QFileDialog::AnyFile);
    setIconProvider(fileIconProvider);
    setOption(DontConfirmOverwrite, false);
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
    QChar	sep(pmPathSeparator());
    QString	imgfile = globalSettings.lastExportPath;

    connect(buttonOk, SIGNAL(clicked()),
		this, SLOT(accept()));
    connect(buttonCancel, SIGNAL(clicked()),
		this, SLOT(reject()));
    connect(selectedRadioButton, SIGNAL(clicked()),
		this, SLOT(selectedRadioButton_clicked()));
    connect(allChartsRadioButton, SIGNAL(clicked()),
		this, SLOT(allChartsRadioButton_clicked()));
    connect(filePushButton, SIGNAL(clicked()),
		this, SLOT(filePushButton_clicked()));
    connect(qualitySlider, SIGNAL(valueChanged(int)),
		this, SLOT(quality_valueChanged(int)));
    connect(qualitySpinBox, SIGNAL(valueChanged(int)),
		this, SLOT(quality_valueChanged(int)));
    connect(formatComboBox, SIGNAL(currentIndexChanged(int)),
		this, SLOT(formatComboBox_currentIndexChanged(int)));

    my.quality = 0;
    my.format = strdup("png");
    imgfile.append(sep);
    imgfile.append("export.png");
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
    file.setDirectory(globalSettings.lastExportPath);

    if (file.exec() == QDialog::Accepted) {
	QString selection = file.selectedFiles().at(0);
	QString path = QFileInfo(selection).dir().absolutePath();
	if (globalSettings.lastExportPath != path) {
	    globalSettings.lastExportPath = path;
	    globalSettings.lastExportPathModified = true;
	}
	fileLineEdit->setText(selection);
    }
}

void ExportDialog::formatComboBox_currentIndexChanged(int index)
{
    QString suffix = formatComboBox->itemText(index);
    char *format = strdup((const char *)suffix.toLatin1());
    QString file = fileLineEdit->text().trimmed();
    QString regex = my.format;

    regex.append("$");
    file.replace(QRegularExpression(regex), suffix);
    fileLineEdit->setText(file);
    free(my.format);
    my.format = format;
}

QSize ExportDialog::imageSize()
{
    Tab *tab = pmchart->activeTab();
    int height = 0, width = 0;

    for (int i = 0; i < tab->gadgetCount(); i++) {
	Gadget *gadget = tab->gadget(i);
	if (gadget != tab->currentGadget() && selectedRadioButton->isChecked())
	    continue;
	width = qMax(width, gadget->width());
	height += gadget->height();
    }
    height += pmchart->timeAxis()->height() + pmchart->dateLabel()->height();
    height -= TIMEAXISFUDGE;

    return QSize(width, height);
}

void ExportDialog::flush()
{
    QString file = fileLineEdit->text().trimmed();
    int width = widthSpinBox->value();
    int height = heightSpinBox->value();
    bool everything = allChartsRadioButton->isChecked();
    bool transparent = transparentCheckBox->isChecked();

    if (ExportDialog::exportFile(file, my.format, my.quality, width, height,
				 transparent, everything) == false) {
	QString message = tr("Failed to save image file\n");
	message.append(file);
	QMessageBox::warning(this, pmGetProgname(), message);
    }
}

bool ExportDialog::exportFile(QString &file, const char *format, int quality,
		int width, int height, bool transparent, bool everything)
{
    enum QImage::Format rgbFormat = transparent ?
				QImage::Format_ARGB32 : QImage::Format_RGB32;
    QImage image(width, height, rgbFormat);
    QPainter qp(&image);

    console->post("ExportDialog::exportFile file=%s fmt=%s qual=%d w=%d h=%d trans=%d every=%d\n",
	(const char *)file.toLatin1(), format, quality, width, height, transparent, everything);

    if (transparent) {
	image.fill(qRgba(255, 255, 255, 0));
    }
    else {
	image.fill(qRgba(255, 255, 255, 255));
    }

    pmchart->painter(&qp, width, height, transparent, everything == false);
    QImageWriter writer(file, format);
    writer.setQuality(quality);
    bool sts = writer.write(image);
    if (!sts)
	fprintf(stderr, "%s: error writing %s (%s): %s\n",
		pmGetProgname(), (const char *) file.toLatin1(), format,
		(const char *) writer.errorString().toLatin1());
    return sts;
}

int ExportDialog::exportFile(char *path, char *geometry, bool transparent)
{
    QRegularExpression regex("\\.([a-z]+)$", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match;
    QString file(path), format;
    bool noFormat = false;
    char suffix[32];
    int i;

    // Ensure the requested image format is supported, else use GIF
    match = regex.match(file);
    if (match.hasMatch() == false) {
	noFormat = true;
    }
    else {
	format = match.captured(1);
	QList<QByteArray> array = QImageWriter::supportedImageFormats();
	for (i = 0; i < array.size(); i++) {
	    if (strcmp(array.at(i), (const char *)format.toLatin1()) == 0)
		break;
	}
	if (i == array.size())
	    noFormat = true;
    }
    if (noFormat) {
	file.append(".png");
	format = QString("png");
    }
    strncpy(suffix, (const char *)format.toLatin1(), sizeof(suffix)-1);
    suffix[sizeof(suffix)-1] = '\0';	/* buffer overrun guard */

    regex.setPattern("(\\d+)x(\\d+)");
    match = regex.match(geometry);
    if (match.hasMatch()) {
	QSize fixed = QSize(match.captured(1).toInt(), match.captured(2).toInt());
	pmchart->setFixedSize(fixed);
    }

    return ExportDialog::exportFile(file, suffix, 100, pmchart->width(),
			pmchart->exportHeight(), transparent, true) == false;
}
