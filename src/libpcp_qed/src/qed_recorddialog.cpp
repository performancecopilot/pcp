/*
 * Copyright (c) 2014, Red Hat.
 * Copyright (c) 2007-2009, Aconex.  All Rights Reserved.
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */

#include "qed_recorddialog.h"
#include <QtCore/QDateTime>
#include <QtCore/QTextStream>
#include <QtGui/QMessageBox>
#include <QtGui/QDoubleValidator>

#include "qed_app.h"
#include "qed_console.h"
#include "qed_viewcontrol.h"
#include "qed_fileiconprovider.h"

QedRecordDialog::QedRecordDialog() : QDialog()
{
    setupUi(this);
    deltaLineEdit->setValidator(
		new QDoubleValidator(0.001, INT_MAX, 3, deltaLineEdit));
}

void QedRecordDialog::languageChange()
{
    retranslateUi(this);
}

void QedRecordDialog::init(QedViewControl *view, double userDelta)
{
    QString	pmlogger = "~/.pcp/pmlogger/";
    QString	viewName, folioName, archiveName;

    viewName = folioName = archiveName = pmlogger;

    viewName.append(tr("[date].view"));
    viewLineEdit->setText(viewName);
    folioName.append(tr("[date].folio"));
    folioLineEdit->setText(folioName);
    archiveName.append(tr("[host]/[date]"));
    archiveLineEdit->setText(archiveName);

    my.view = view;
    my.userDelta = userDelta;
    my.units = QmcTime::Seconds;
    deltaLineEdit->setText(QmcTime::deltaString(userDelta, my.units));

    selectedRadioButton->setChecked(false);
    allGadgetsRadioButton->setChecked(true);
}

void QedRecordDialog::selectedRadioButton_clicked()
{
    selectedRadioButton->setChecked(true);
    allGadgetsRadioButton->setChecked(false);
}

void QedRecordDialog::allGadgetsRadioButton_clicked()
{
    selectedRadioButton->setChecked(false);
    allGadgetsRadioButton->setChecked(true);
}

void QedRecordDialog::deltaUnitsComboBox_activated(int value)
{
    my.userDelta = QmcTime::deltaValue(deltaLineEdit->text(), my.units);
    my.units = (QmcTime::DeltaUnits)value;
    deltaLineEdit->setText(QmcTime::deltaString(my.userDelta, my.units));
}

void QedRecordDialog::viewPushButton_clicked()
{
    QedRecordFileDialog view(this);

    view.setDirectory(QDir::homePath().append("/.pcp/pmlogger/"));
    if (view.exec() == QDialog::Accepted)
	viewLineEdit->setText(view.selectedFiles().at(0));
}

void QedRecordDialog::folioPushButton_clicked()
{
    QedRecordFileDialog folio(this);

    folio.setDirectory(QDir::homePath().append("/.pcp/pmlogger/"));
    if (folio.exec() == QDialog::Accepted)
	folioLineEdit->setText(folio.selectedFiles().at(0));
}

void QedRecordDialog::archivePushButton_clicked()
{
    QedRecordFileDialog archive(this);

    archive.setDirectory(QDir::homePath().append("/.pcp/pmlogger/"));
    if (archive.exec() == QDialog::Accepted)
	archiveLineEdit->setText(archive.selectedFiles().at(0));
}

bool QedRecordDialog::saveFolio(QString folioName, QString viewName)
{
    QFile folio(folioName);

    if (!folio.open(QIODevice::WriteOnly)) {
	QString msg = tr("Cannot open file: ");
	msg.append(folioName);
	msg.append("\n");
	msg.append(folio.errorString());
	QMessageBox::warning(this, pmProgname, msg);
	return false;
    }

    QTextStream stream(&folio);
    QString datetime;

    datetime = QDateTime::currentDateTime().toString("ddd MMM d hh:mm:ss yyyy");
    stream << "PCPFolio\n";
    stream << "Version: 1\n";
    stream << "# use pmafm(1) to process this PCP archive folio\n" << "#\n";
    stream << "Created: on " << QmcSource::localHost;
    stream << " at " << datetime << "\n";
    stream << "Creator: pmchart " << viewName << "\n";
    stream << "#\t\tHost\t\tBasename\n";

    for (int i = 0; i < my.hosts.size(); i++) {
	QString host = my.hosts.at(i);
	QString archive = my.archives.at(i);
	QFileInfo logFile(archive);
	QDir logDir = logFile.dir();
	logDir.mkpath(logDir.absolutePath());
	stream << "Archive:\t" << my.hosts.at(i) << "\t\t" << archive << "\n";
    }
    return true;
}

bool QedRecordDialog::saveConfig(QString configfile, QString configdata)
{
    QFile config(configfile);

    if (!config.open(QIODevice::WriteOnly)) {
	QString msg = tr("Cannot open file: ");
	msg.append(configfile);
	msg.append("\n");
	msg.append(config.errorString());
	QMessageBox::warning(this, pmProgname, msg);
	return false;
    }

    QTextStream stream(&config);
    stream << configdata;
    return true;
}

PmLogger::PmLogger(QObject *parent) : QProcess(parent)
{
    connect(this, SIGNAL(finished(int, QProcess::ExitStatus)),
	    this, SLOT(finished(int, QProcess::ExitStatus)));
}

void PmLogger::init(QedViewControl *view, QString host, QString logfile)
{
    my.view = view;
    my.host = host;
    my.logfile = logfile;
    my.terminating = false;
}

void PmLogger::terminate()
{
    my.terminating = true;
}

void PmLogger::finished(int, QProcess::ExitStatus)
{
    QString msg;

    if (my.terminating == false) {
	my.terminating = true;
	if (my.view->stopRecording(msg) == 0) {
	    msg = "Recording process (pmlogger) exited unexpectedly\n";
	    msg.append("for host ");
	    msg.append(my.host);
	    msg.append(".\n\n");
	    msg.append("Additional diagnostics may be available in the log:\n");
	    msg.append(my.logfile);
	}
	QMessageBox::warning(NULL, pmProgname, msg);
    }
}

void QedRecordDialog::buttonOk_clicked()
{
    if (deltaLineEdit->isModified()) {
	// convert to seconds, make sure its still in range 0.001-INT_MAX
	double input = QmcTime::deltaValue(deltaLineEdit->text(), my.units);
	if (input < 0.001 || input > INT_MAX) {
	    QString msg = tr("Record Sampling Interval is invalid.\n");
	    msg.append(deltaLineEdit->text());
	    msg.append(" is out of range (0.001 to 0x7fffffff seconds)\n");
	    QMessageBox::warning(this, pmProgname, msg);
	    return;
	}
    }

    QString today = QDateTime::currentDateTime().toString("yyyyMMdd.hh.mm.ss");

    QString viewName = viewLineEdit->text().trimmed();
    viewName.replace(QRegExp("^~"), QDir::homePath());
    viewName.replace(QRegExp("\\[date\\]"), today);
    viewName.replace(QRegExp("\\[host\\]"), QmcSource::localHost);
    QFileInfo viewFile(viewName);
    QDir viewDir = viewFile.dir();
    if (viewDir.mkpath(viewDir.absolutePath()) == false) {
	QString msg = tr("Failed to create path for view:\n");
	msg.append(viewName);
	msg.append("\n");
	QMessageBox::warning(this, pmProgname, msg);
	return;
    }

    QString folioName = folioLineEdit->text().trimmed();
    folioName.replace(QRegExp("^~"), QDir::homePath());
    folioName.replace(QRegExp("\\[date\\]"), today);
    folioName.replace(QRegExp("\\[host\\]"), QmcSource::localHost);
    QFileInfo folioFile(folioName);
    QDir folioDir = folioFile.dir();
    if (folioDir.mkpath(folioDir.absolutePath()) == false) {
	QString msg = tr("Failed to create path for folio:\n");
	msg.append(folioName);
	msg.append("\n");
	QMessageBox::warning(this, pmProgname, msg);
	return;
    }

    console->post("RecordDialog verifying paths view=%s folio=%s",
	(const char *)folioName.toAscii(), (const char *)viewName.toAscii());

    my.viewName = viewName;
    my.folioName = folioName;
    my.delta.setNum(QmcTime::deltaValue(deltaLineEdit->text(), my.units), 'f');

    my.hosts = my.view->hostList(selectedRadioButton->isChecked());
    for (int h = 0; h < my.hosts.count(); h++) {
	QString archive = archiveLineEdit->text().trimmed();
	archive.replace(QRegExp("^~"), QDir::homePath());
	archive.replace(QRegExp("\\[host\\]"), my.hosts.at(h));
	archive.replace(QRegExp("\\[date\\]"), today);
	my.archives.append(archive);
    }

    if (my.view->saveConfig(viewName, false, false, false, true) == false)
	return;
    if (saveFolio(folioName, viewName) == false)
	return;
    QDialog::accept();
}

//
// write pmlogger, pmchart and pmafm configs, then start pmloggers.
//
void QedRecordDialog::startLoggers()
{
    QString pmlogger = pmGetConfig("PCP_BINADM_DIR");
    pmlogger.append("/pmlogger");

    QString regex = "^";
    regex.append(QDir::homePath());
    my.folioName.replace(QRegExp(regex), "~"); 

    my.view->addFolio(my.folioName, my.viewName);

    for (int i = 0; i < my.hosts.size(); i++) {
	PmLogger *process = new PmLogger(this);
	QString archive = my.archives.at(i);
	QString host = my.hosts.at(i);
	QString logfile, configfile;

	configfile = archive;
	configfile.append(".config");
	logfile = archive;
	logfile.append(".log");

	process->init(my.view, host, logfile);

	QStringList arguments;
	arguments << "-r" << "-c" << configfile << "-h" << host << "-x0";
	arguments << "-l" << logfile << "-t" << my.delta << archive;

	QString data("#pmlogger Version 1\n\n"); // header for file(1)
	data.append(my.view->pmloggerSyntax(selectedRadioButton->isChecked()));
	saveConfig(configfile, data);

	process->start(pmlogger, arguments);
	my.view->addLogger(process, archive);

	// Send initial control messages to pmlogger
	QStringList control;
	control << "V0\n";
	control << "F" << my.folioName << "\n";
	control << "Ppmchart\n" << "R\n";
	for (int i = 0; i < control.size(); i++)
	    process->write(control.at(i).toAscii());
    }
}

// RecordFileDialog is the one which is displayed when you click
// on one of the file selection push buttons (view/logfile/folio).

QedRecordFileDialog::QedRecordFileDialog(QWidget *parent) : QFileDialog(parent)
{
    setAcceptMode(QFileDialog::AcceptSave);
    setFileMode(QFileDialog::AnyFile);
    setIconProvider(fileIconProvider);
    setConfirmOverwrite(true);
}

void QedRecordFileDialog::setFileName(QString path)
{
    selectFile(path);
}
