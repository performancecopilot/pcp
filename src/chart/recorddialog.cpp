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
#include "recorddialog.h"
#include <QtCore/QTextStream>
#include <QtGui/QMessageBox>
#include <QtGui/QDoubleValidator>
#include "main.h"
#include "tab.h"
#include "saveviewdialog.h"

RecordDialog::RecordDialog(QWidget* parent) : QDialog(parent)
{
    setupUi(this);
    deltaLineEdit->setValidator(
		new QDoubleValidator(0.001, INT_MAX, 3, deltaLineEdit));
}

void RecordDialog::languageChange()
{
    retranslateUi(this);
}

void RecordDialog::init(Tab *tab)
{
    QDir	pmloggerDir;
    QString	pmlogger = "~/.pcp/pmlogger/";
    QString	view, folio, archive;

    view = folio = archive = pmlogger;
    pmloggerDir.mkpath(pmlogger);

    view.append(tr("[date].view"));
    viewLineEdit->setText(view);
    folio.append(tr("[date].folio"));
    folioLineEdit->setText(folio);
    archive.append(tr("[host]/[date]"));
    archiveLineEdit->setText(archive);

    my.tab = tab;
    my.units = KmTime::Seconds;
    deltaLineEdit->setText(
		KmTime::deltaString(globalSettings.loggerDelta, my.units));

    selectedRadioButton->setChecked(false);
    allChartsRadioButton->setChecked(true);
}

void RecordDialog::selectedRadioButton_clicked()
{
    selectedRadioButton->setChecked(true);
    allChartsRadioButton->setChecked(false);
}

void RecordDialog::allChartsRadioButton_clicked()
{
    selectedRadioButton->setChecked(false);
    allChartsRadioButton->setChecked(true);
}

void RecordDialog::deltaUnitsComboBox_activated(int value)
{
    double delta = tosec(*(kmtime->liveInterval()));
    my.units = (KmTime::DeltaUnits)value;
    deltaLineEdit->setText(KmTime::deltaString(delta, my.units));
}

void RecordDialog::viewPushButton_clicked()
{
    RecordFileDialog view(this);

    view.setDirectory(QDir::homePath().append("/.pcp/pmlogger/"));
    if (view.exec() == QDialog::Accepted)
	viewLineEdit->setText(view.selectedFiles().at(0));
}

void RecordDialog::folioPushButton_clicked()
{
    RecordFileDialog folio(this);

    folio.setDirectory(QDir::homePath().append("/.pcp/pmlogger/"));
    if (folio.exec() == QDialog::Accepted)
	folioLineEdit->setText(folio.selectedFiles().at(0));
}

void RecordDialog::archivePushButton_clicked()
{
    RecordFileDialog archive(this);

    archive.setDirectory(QDir::homePath().append("/.pcp/pmlogger/"));
    if (archive.exec() == QDialog::Accepted)
	archiveLineEdit->setText(archive.selectedFiles().at(0));
}

bool RecordDialog::saveFolio(QString folioname, QString viewname)
{
    QFile folio(folioname);

    if (!folio.open(QIODevice::WriteOnly)) {
	QString msg = tr("Cannot open file: ");
	msg.append(folioname);
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
    stream << "Creator: kmchart " << viewname << "\n";
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

bool RecordDialog::saveConfig(QString configfile, QString configdata)
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

void PmLogger::init(Tab *tab, QString host, QString logfile)
{
    my.tab = tab;
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
    if (my.terminating == false) {
	my.terminating = true;
	my.tab->stopRecording();

	QString msg = "Recording process (pmlogger) exited unexpectedly\n";
	msg.append("for host ");
	msg.append(my.host);
	msg.append(".\n\n");
	msg.append("Additional diagnostics may be available in the log:\n");
	msg.append(my.logfile);
	QMessageBox::warning(kmchart, pmProgname, msg);
    }
}

QString PmLogger::configure(Chart *cp)
{
    QString input;
    bool beDiscrete = false;
    bool nonDiscrete = false;

    // discover whether we need separate log-once/log-every sections
    for (int m = 0; m < cp->numPlot(); m++) {
	if (cp->metricDesc(m)->desc().sem == PM_SEM_DISCRETE)
	    beDiscrete = true;
	else
	    nonDiscrete = true;
    }

    // pmlogger header for file(1)
    input.append("#pmlogger Version 1\n");

    if (beDiscrete) {
	input.append("log mandatory on once {\n");
	for (int m = 0; m < cp->numPlot(); m++) {
	    if (cp->metricDesc(m)->desc().sem != PM_SEM_DISCRETE)
		continue;
	    input.append('\t');
	    input.append(cp->pmloggerMetricSyntax(m));
	    input.append('\n');
	}
	input.append("}\n");
    }
    if (nonDiscrete) {
	input.append("log mandatory on default {\n");
	for (int m = 0; m < cp->numPlot(); m++) {
	    if (cp->metricDesc(m)->desc().sem == PM_SEM_DISCRETE)
		continue;
	    input.append('\t');
	    input.append(cp->pmloggerMetricSyntax(m));
	    input.append('\n');
	}
	input.append("}\n");
    }
    return input;
}

void RecordDialog::buttonOk_clicked()
{
    if (deltaLineEdit->isModified()) {
	// convert to seconds, make sure its still in range 0.001-INT_MAX
	double input = KmTime::deltaValue(deltaLineEdit->text(), my.units);
	if (input < 0.001 || input > INT_MAX) {
	    QString msg = tr("Record Sampling Interval is invalid.\n");
	    msg.append(deltaLineEdit->text());
	    msg.append(" is out of range (0.001 to 0x7fffffff seconds)\n");
	    QMessageBox::warning(this, pmProgname, msg);
	    return;
	}
    }

    QString today = QDateTime::currentDateTime().toString("yyyyMMdd.hh:mm:ss");

    QString view = viewLineEdit->text().trimmed();
    view.replace(QRegExp("^~"), QDir::homePath());
    view.replace(QRegExp("\\[date\\]"), today);
    view.replace(QRegExp("\\[host\\]"), QmcSource::localHost);
    QFileInfo viewFile(view);
    QDir viewDir = viewFile.dir();
    if (viewDir.mkpath(viewDir.absolutePath()) == false) {
	QString msg = tr("Failed to create path for view:\n");
	msg.append(view);
	msg.append("\n");
	QMessageBox::warning(this, pmProgname, msg);
	return;
    }

    QString folio = folioLineEdit->text().trimmed();
    folio.replace(QRegExp("^~"), QDir::homePath());
    folio.replace(QRegExp("\\[date\\]"), today);
    folio.replace(QRegExp("\\[host\\]"), QmcSource::localHost);
    QFileInfo folioFile(folio);
    QDir folioDir = folioFile.dir();
    if (folioDir.mkpath(folioDir.absolutePath()) == false) {
	QString msg = tr("Failed to create path for folio:\n");
	msg.append(folio);
	msg.append("\n");
	QMessageBox::warning(this, pmProgname, msg);
	return;
    }

    console->post("RecordDialog verifying paths view=%s folio=%s",
	(const char *)folio.toAscii(), (const char *)view.toAscii());

    my.view  = view;
    my.folio = folio;
    my.delta.setNum(KmTime::deltaValue(deltaLineEdit->text(), my.units), 'f');

    for (int c = 0; c < activeTab->numChart(); c++) {
	Chart *cp = activeTab->chart(c);
	if (selectedRadioButton->isChecked() && cp != activeTab->currentChart())
	    continue;
	for (int m = 0; m < cp->numPlot(); m++) {
	    QString host = cp->metricContext(m)->source().host();
	    if (!my.hosts.contains(host)) {
		QString archive = archiveLineEdit->text().trimmed();
		archive.replace(QRegExp("^~"), QDir::homePath());
		archive.replace(QRegExp("\\[host\\]"), host);
		archive.replace(QRegExp("\\[date\\]"), today);
		my.archives.append(archive);
		my.hosts.append(host);
	    }
	}
    }

    if (SaveViewDialog::saveView(view, false) == false)
	return;
    if (saveFolio(folio, view) == false)
	return;
    QDialog::accept();
}

//
// write pmlogger, kmchart and pmafm configs, then start pmloggers.
//
void RecordDialog::startLoggers()
{
    QString pmlogger = pmGetConfig("PCP_BINADM_DIR");
    pmlogger.append("/pmlogger");

    QString regex = "^";
    regex.append(QDir::homePath());
    my.folio.replace(QRegExp(regex), "~"); 

    activeTab->addFolio(my.folio, my.view);

    for (int i = 0; i < my.hosts.size(); i++) {
	PmLogger *process = new PmLogger(kmchart);
	QString archive = my.archives.at(i);
	QString host = my.hosts.at(i);
	QString logfile, configfile;

	configfile = archive;
	configfile.append(".config");
	logfile = archive;
	logfile.append(".log");

	process->init(my.tab, host, logfile);

	QStringList arguments;
	arguments << "-r" << "-c" << configfile << "-h" << host << "-x0";
	arguments << "-l" << logfile << "-t" << my.delta << archive;

	QString configdata;
	if (selectedRadioButton->isChecked())
	    configdata.append(process->configure(activeTab->currentChart()));
	else
	    for (int c = 0; c < activeTab->numChart(); c++)
		configdata.append(process->configure(activeTab->chart(c)));
	saveConfig(configfile, configdata);

	// PMPROXY_HOST support needed here
	process->start(pmlogger, arguments);
	activeTab->addLogger(process, archive);

	// Send initial control messages to pmlogger
	QStringList control;
	control << "V0\n";
	control << "F" << my.folio << "\n";
	control << "Pkmchart\n" << "R\n";
	for (int i = 0; i < control.size(); i++)
	    process->write(control.at(i).toAscii());
    }
}

// RecordFileDialog is the one which is displayed when you click
// on one of the file selection push buttons (view/logfile/folio).

RecordFileDialog::RecordFileDialog(QWidget *parent) : QFileDialog(parent)
{
    setAcceptMode(QFileDialog::AcceptSave);
    setFileMode(QFileDialog::AnyFile);
    setIconProvider(fileIconProvider);
    setConfirmOverwrite(true);
}

void RecordFileDialog::setFileName(QString path)
{
    selectFile(path);
}
