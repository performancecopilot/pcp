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
#include <QtGui/QMessageBox>
#include <QtCore/QTextStream>
#include "main.h"

RecordDialog::RecordDialog(QWidget* parent) : QDialog(parent)
{
    setupUi(this);
    init();
}

void RecordDialog::languageChange()
{
    retranslateUi(this);
}

// Conversion from seconds into other time units
double RecordDialog::secondsToUnits(double value)
{
    switch (my.units) {
    case Milliseconds:
	return value * 1000.0;
    case Minutes:
	return value / 60.0;
    case Hours:
	return value / (60.0 * 60.0);
    case Days:
	return value / (60.0 * 60.0 * 24.0);
    case Weeks:
	return value / (60.0 * 60.0 * 24.0 * 7.0);
    case Seconds:
    default:
	break;
    }
    return value;
}

void RecordDialog::init()
{
    QDir	pmloggerDir;
    QString	pmlogger = QDir::homePath().append("/.pcp/pmlogger/");
    QString	view, folio, archive;

    view = folio = archive = pmlogger;
    pmloggerDir.mkdir(pmlogger);

    view.append(tr("[date].view"));
    viewLineEdit->setText(view);
    folio.append(tr("[date].folio"));
    folioLineEdit->setText(folio);
    archive.append(tr("[host]/[date]"));
    archiveLineEdit->setText(archive);

    my.units = Seconds;
    displayDeltaText();

    selectedRadioButton->setChecked(true);
    allChartsRadioButton->setChecked(false);
}

void RecordDialog::deltaUnitsComboBoxActivated(int value)
{
    my.units = (DeltaUnits)value;
    displayDeltaText();
}

void RecordDialog::selectedRadioButtonClicked()
{
    selectedRadioButton->setChecked(true);
    allChartsRadioButton->setChecked(false);
}

void RecordDialog::allChartsRadioButtonClicked()
{
    selectedRadioButton->setChecked(false);
    allChartsRadioButton->setChecked(true);
}

void RecordDialog::displayDeltaText()
{
    QString	text;
    double	delta = tosec(*(kmtime->liveInterval()));

    delta = secondsToUnits(delta);
    if ((double)(int)delta == delta)
	text.sprintf("%.2f", delta);
    else
	text.sprintf("%.6f", delta);
    deltaLineEdit->setText(text);
}

void RecordDialog::viewPushButtonClicked()
{
    RecordFileDialog view(this);

    view.setDirectory(QDir::homePath().append("/.pcp/pmlogger/"));
    if (view.exec() == QDialog::Accepted)
	viewLineEdit->setText(view.selectedFiles().at(0));
}

void RecordDialog::folioPushButtonClicked()
{
    RecordFileDialog folio(this);

    folio.setDirectory(QDir::homePath().append("/.pcp/pmlogger/"));
    if (folio.exec() == QDialog::Accepted)
	folioLineEdit->setText(folio.selectedFiles().at(0));
}

void RecordDialog::archivePushButtonClicked()
{
    RecordFileDialog archive(this);

    archive.setDirectory(QDir::homePath().append("/.pcp/pmlogger/"));
    if (archive.exec() == QDialog::Accepted)
	archiveLineEdit->setText(archive.selectedFiles().at(0));
}

static char *localhostname()
{
    static char name[256];

    gethostname(name, sizeof(name));
    return name;
}

int RecordDialog::saveFolio(QString folioname, QString viewname)
{
    QFile folio(folioname);

    if (!folio.open(QIODevice::WriteOnly)) {
	QString msg = tr("Cannot open file: ");
	msg.append(folioname);
	msg.append("\n");
	msg.append(folio.errorString());
	QMessageBox::warning(this, pmProgname, msg);
	return -1;
    }

    QTextStream stream(&folio);
    QString datetime;

    datetime = QDateTime::currentDateTime().toString("ddd MMM d hh:mm:ss yyyy");
    stream << "PCPFolio\n";
    stream << "Version: 1\n";
    stream << "# use pmafm(1) to process this PCP archive folio\n" << "#\n";
    stream << "Created: on " << localhostname() << " at " << datetime << "\n";
    stream << "Creator: kmchart " << viewname << "\n";
    stream << "#\t\tHost\t\tBasename\n";
    datetime = QDateTime::currentDateTime().toString("yyyyMMdd");
    QStringList::Iterator it;
    for (it = my.hosts.begin(); it != my.hosts.end(); it++) {
	QDir logDir;
	QString	logDirName = QDir::homePath().append("/.pcp/pmlogger/");
	logDirName.append(*it);
	logDir.mkdir(logDirName);
	stream << "Archive:\t" << *it << "\t\t";
	stream << logDirName << "/" << datetime << "\n";
    }
    return 0;
}

int RecordDialog::saveConfig(QString configfile, QString configdata)
{
    QFile config(configfile);

    if (!config.open(QIODevice::WriteOnly)) {
	QString msg = tr("Cannot open file: ");
	msg.append(configfile);
	msg.append("\n");
	msg.append(config.errorString());
	QMessageBox::warning(this, pmProgname, msg);
	return -1;
    }

    QTextStream stream(&config);
    stream << configdata;
    return 0;
}

void RecordDialog::extractDeltaString()
{
    double value = deltaLineEdit->text().trimmed().toDouble();

    switch (my.units) {
    case Milliseconds:
	my.deltaString.append("sec");
	value *= 1000;
	break;
    default:
    case Seconds:
	my.deltaString.append("sec");
	break;
    case Minutes:
	my.deltaString.append("min");
	break;
    case Hours:
	my.deltaString.append("hour");
	break;
    case Days:
	my.deltaString.append("day");
	break;
    case Weeks:
	my.deltaString.append("day");
	value *= 7;
	break;
    }
    my.deltaString.setNum(value, 'f');
    // TODO: pmparseinterval and error reporting
}

PmLogger::PmLogger(QObject *parent) : QProcess(parent)
{
    connect(this, SIGNAL(finished(int, QProcess::ExitStatus)),
	    this, SLOT(finished(int, QProcess::ExitStatus)));
}

void PmLogger::init(QString host, QString logfile)
{
    my.host = host;
    my.logfile = logfile;
    my.terminating = false;
}

void PmLogger::finished(int, QProcess::ExitStatus)
{
// TODO - this needs to be handled via -x option to pmlogger

    if (my.terminating == false) {
	QString msg = tr("Recording process (pmlogger) exited unexpectedly\n");
	msg.append(tr("for host "));
	msg.append(my.host);
	msg.append(tr(".\n\n"));
	msg.append(tr("Additional diagnostics may be available in the log:\n"));
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

// TODO: need an OK pressed callback which can abort dismissal of the
// dialog - it needs to validate the deltaLineEdit contents, check if
// we'll be overwriting any folio/view files, etc.  It should also do
// the first part of startLoggers() - saving files, so that problems
// like ENOSPC/EEXISTS can be dealt with before dialog is gone.

//
// write pmlogger, kmchart and pmafm configs, then start pmloggers.
//
void RecordDialog::startLoggers()
{
    QString localhost = localhostname();
    QString datetoday = QDateTime::currentDateTime().toString("yyyyMMdd");
    QString folio = folioLineEdit->text().trimmed();
    QString view = viewLineEdit->text().trimmed();

    // TODO: mkdir of all path components
    view.replace(QRegExp("\\[date\\]"), datetoday);
    view.replace(QRegExp("\\[host\\]"), localhost);
    folio.replace(QRegExp("\\[date\\]"), datetoday);
    folio.replace(QRegExp("\\[host\\]"), localhost);

    console->post("RecordDialog::startLoggers view=%s folio=%s",
	(const char *)folio.toAscii(), (const char *)view.toAscii());

    extractDeltaString();

    for (int c = 0; c < activeTab->numChart(); c++) {
	Chart *cp = activeTab->chart(c);
	if (selectedRadioButton->isChecked() && cp != activeTab->currentChart())
	    continue;
	for (int m = 0; m < cp->numPlot(); m++) {
	    QString host = cp->metricContext(m)->source().host();
	    if (!my.hosts.contains(host))
		my.hosts.append(host);
	}
    }

    SaveViewDialog::saveView(view, true);
    saveFolio(folio, view);
    activeTab->setFolio(folio);

    QString pmlogger = tr(pmGetConfig("PCP_BINADM_DIR"));
    pmlogger.append("/pmlogger");

    QStringList::Iterator it;
    for (it = my.hosts.begin(); it != my.hosts.end(); it++) {
	PmLogger *process = new PmLogger(kmchart);
	QString archive = archiveLineEdit->text().trimmed();
	QString logfile, configfile;

	archive.replace(QRegExp("\\[host\\]"), *it);
	archive.replace(QRegExp("\\[date\\]"), datetoday);
	configfile = logfile = archive;
	configfile.append(".config");
	logfile.append(".log");
	process->init(*it, logfile);

	QStringList arguments;
	arguments << "-r" << "-c" << configfile << "-h" << *it;
	arguments << "-l" << logfile << "-t" << my.deltaString << archive;

	QString configdata;
	if (selectedRadioButton->isChecked())
	    configdata.append(process->configure(activeTab->currentChart()));
	else
	    for (int c = 0; c < activeTab->numChart(); c++)
		configdata.append(process->configure(activeTab->chart(c)));
	saveConfig(configfile, configdata);

	// TODO: PMPROXY_HOST support needed here, too
	process->start(pmlogger, arguments);
	activeTab->addLogger(process);
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
