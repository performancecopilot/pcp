/****************************************************************************
** ui.h extension file, included from the uic-generated form implementation.
**
** If you want to add, delete, or rename functions or slots, use
** Qt Designer to update this file, preserving your code.
**
** You should not define a constructor or destructor in this file.
** Instead, write your code in functions called init() and destroy().
** These will automatically be called by the form's constructor and
** destructor.
*****************************************************************************/

#include "main.h"
#include <qwt/qwt_plot_printfilter.h>

void ExportDialog::init()
{
    QString	imgfile = QDir::homeDirPath();

    // TODO: update the filename to display the file extension
    // when the combobox entry is updated?

    quality = 0.0;
    imgfile.append("/export");
    fileLineEdit->setText(imgfile);
    formatComboBox->insertStrList(QImage::outputFormats());

    selectedRadioButton->setChecked(TRUE);
    allChartsRadioButton->setChecked(FALSE);
    qualityCounter->setValue(80.0);
    qualitySlider->setValue(80.0);
    qualitySlider->setRange(0.0, 100.0);
}

void ExportDialog::selectedRadioButtonClicked()
{
    selectedRadioButton->setChecked(TRUE);
    allChartsRadioButton->setChecked(FALSE);
}

void ExportDialog::allChartsRadioButtonClicked()
{
    selectedRadioButton->setChecked(FALSE);
    allChartsRadioButton->setChecked(TRUE);
}

void ExportDialog::displayQualitySlider()
{
    qualitySlider->blockSignals(TRUE);
    qualitySlider->setValue(quality);
    qualitySlider->blockSignals(FALSE);
}

void ExportDialog::displayQualityCounter()
{
    qualityCounter->blockSignals(TRUE);
    qualityCounter->setValue(quality);
    qualityCounter->blockSignals(FALSE);
}

void ExportDialog::qualityValueChanged(double value)
{
    if (value != quality) {
	quality = (double)(int)value;
	displayQualityCounter();
	displayQualitySlider();
    }
}

void ExportDialog::filePushButtonClicked()
{
    ExportFileDialog file(this);

    file.setDir(QDir::homeDirPath());
    if (file.exec() == QDialog::Accepted)
	fileLineEdit->setText(file.selectedFile());
}

void ExportDialog::flush()
{
    QString file = fileLineEdit->text().stripWhiteSpace();

    if (file.isEmpty())	// TODO: improve error handling in file dialogs
	return;

    QwtPlotPrintFilter	filter;
    filter.setOptions(QwtPlotPrintFilter::PrintAll &
		     ~QwtPlotPrintFilter::PrintCanvasBackground &
		     ~QwtPlotPrintFilter::PrintWidgetBackground &
		     ~QwtPlotPrintFilter::PrintGrid);

    // TODO: make this code work properly (only dumps current chart now)
    QPixmap pixmap(0,0);
    for (int i = 0; i < activeTab->numChart(); i++) {
	Chart *cp = activeTab->chart(i);
	if (cp != activeTab->currentChart() &&
	    selectedRadioButton->isChecked())
	    continue;
	pixmap.resize(cp->width(), pixmap.height() + cp->height());
	cp->print(pixmap, filter);
    }
    pixmap.resize(pixmap.width(), pixmap.height() +
		  kmchart->timeAxis()->height());
    kmchart->timeAxis()->print(pixmap, filter);

    pixmap.save(tr(file), formatComboBox->currentText().ascii(), (int)quality);
}
