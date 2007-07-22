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
#include <qmessagebox.h>

typedef enum { Msec, Sec, Min, Hour, Day, Week } delta_units;

// conversion from seconds into other time units
static double secondsToUnits(double value, delta_units units)
{
    if (units == Msec)
	return value * 1000.0;
    else if (units == Min)
	return value / 60.0;
    else if (units == Hour)
	return value / (60.0 * 60.0);
    else if (units == Day)
	return value / (60.0 * 60.0 * 24.0);
    else if (units == Week)
	return value / (60.0 * 60.0 * 24.0 * 7.0);
    return value;
}


void RecordDialog::init()
{
    QDir	pmloggerDir;
    QString	pmlogger = QDir::homeDirPath().append("/.pcp/pmlogger/");
    QString	view, folio, archive;

    view = folio = archive = pmlogger;
    pmloggerDir.mkdir(pmlogger);

    view.append(tr("[date].view"));
    viewLineEdit->setText(view);
    folio.append(tr("[date].folio"));
    folioLineEdit->setText(folio);
    archive.append(tr("[host]/[date]"));
    archiveLineEdit->setText(archive);

    _units = Sec;
    displayDeltaText();

    selectedRadioButton->setChecked(TRUE);
    allChartsRadioButton->setChecked(FALSE);
}

void RecordDialog::deltaUnitsComboBoxActivated(int value)
{
    _units = (delta_units)value;
    displayDeltaText();
}

void RecordDialog::selectedRadioButtonClicked()
{
    selectedRadioButton->setChecked(TRUE);
    allChartsRadioButton->setChecked(FALSE);
}

void RecordDialog::allChartsRadioButtonClicked()
{
    selectedRadioButton->setChecked(FALSE);
    allChartsRadioButton->setChecked(TRUE);
}

void RecordDialog::displayDeltaText()
{
    QString	text;
    double	delta = secondsFromTV(kmtime->liveInterval());

    delta = secondsToUnits(delta, (delta_units)_units);
    if ((double)(int)delta == delta)
	text.sprintf("%.2f", delta);
    else
	text.sprintf("%.6f", delta);
    deltaLineEdit->setText(text);
}

void RecordDialog::viewPushButtonClicked()
{
    RecordViewDialog view(this);

    view.setDir(QDir::homeDirPath().append("/.pcp/pmlogger/"));
    if (view.exec() == QDialog::Accepted)
	viewLineEdit->setText(view.selectedFile());
}

void RecordDialog::folioPushButtonClicked()
{
    RecordViewDialog folio(this);

    folio.setDir(QDir::homeDirPath().append("/.pcp/pmlogger/"));
    if (folio.exec() == QDialog::Accepted)
	folioLineEdit->setText(folio.selectedFile());
}

void RecordDialog::archivePushButtonClicked()
{
    RecordViewDialog archive(this);

    archive.setDir(QDir::homeDirPath().append("/.pcp/pmlogger/"));
    if (archive.exec() == QDialog::Accepted)
	archiveLineEdit->setText(archive.selectedFile());
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

    if (!folio.open(IO_WriteOnly)) {
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
    for (QStringList::Iterator it = _hosts.begin(); it != _hosts.end(); it++) {
	QDir logDir;
	QString	logDirName = QDir::homeDirPath().append("/.pcp/pmlogger/");
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

    if (!config.open(IO_WriteOnly)) {
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
    double value = deltaLineEdit->text().stripWhiteSpace().toDouble();

    switch (_units) {
    case Msec:
	_deltaString.append("sec");
	value *= 1000;
	break;
    default:
    case Sec:
	_deltaString.append("sec");
	break;
    case Min:
	_deltaString.append("min");
	break;
    case Hour:
	_deltaString.append("hour");
	break;
    case Day:
	_deltaString.append("day");
	break;
    case Week:
	_deltaString.append("day");
	value *= 7;
	break;
    }
    _deltaString.setNum(value, 'f');
    // TODO: pmparseinterval and error reporting
}

PmLogger::PmLogger(QObject *parent, const char *name) : QProcess(parent, name)
{
    connect(this, SIGNAL(processExited()), this, SLOT(exited()));
}

void PmLogger::init(QString delta, QString host,
		    QString archive, QString log, QString conf)
{
    QString pmlogger = tr(pmGetConfig("PCP_SHARE_DIR"));

    addArgument(pmlogger.append("/bin/pmlogger"));
    addArgument("-r");	// write archive size info into logfile
    addArgument("-c");
    addArgument(conf);
    addArgument("-h");
    addArgument(host);
    addArgument("-l");
    addArgument(log);
    addArgument("-t");
    addArgument(delta);
    addArgument(archive);

    _host = host;
    _logfile = log;
    _terminating = FALSE;

    // # pmlogger -c <config> -r -h <host> -l <archive>.log -t <delta> <archive>
fprintf(stderr, "FORK: %s -r -c %s -h %s -l %s -t %s %s\n", pmlogger.ascii(),
	conf.ascii(), host.ascii(), log.ascii(), delta.ascii(), archive.ascii());
}

void PmLogger::exited()
{
    if (_terminating == FALSE) {
	QString msg = tr("Recording process (pmlogger) exited unexpectedly\n");
	msg.append(tr("for host "));
	msg.append(_host);
	msg.append(tr(".\n\n"));
	msg.append(tr("Additional diagnostics may be available in the log:\n"));
	msg.append(_logfile);
	QMessageBox::warning(kmchart, pmProgname, msg);
    }
}

QString PmLogger::configure(Chart *cp)
{
    QString input;
    bool be_discrete = FALSE;
    bool non_discrete = FALSE;

    // discover whether we need separate log-once/log-every sections
    for (int m = 0; m < cp->numPlot(); m++) {
	if (cp->metricDesc(m)->desc().sem == PM_SEM_DISCRETE)
	    be_discrete = TRUE;
	else
	    non_discrete = TRUE;
    }

    if (be_discrete) {
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
    if (non_discrete) {
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
    QString folio = folioLineEdit->text().stripWhiteSpace();
    QString view = viewLineEdit->text().stripWhiteSpace();
    QString pmlogger = QDir::homeDirPath().append("/.pcp/pmlogger/");

    // TODO: mkdir of all path components
    view.replace(QRegExp("\\[date\\]"), datetoday);
    view.replace(QRegExp("\\[host\\]"), localhost);
    folio.replace(QRegExp("\\[date\\]"), datetoday);
    folio.replace(QRegExp("\\[host\\]"), localhost);

fprintf(stderr, "%s view=%s folio=%s\n", __func__, folio.ascii(), view.ascii());

    extractDeltaString();

    for (int c = 0; c < activeTab->numChart(); c++) {
	Chart *cp = activeTab->chart(c);
	if (selectedRadioButton->isChecked() && cp != activeTab->currentChart())
	    continue;
	for (int m = 0; m < cp->numPlot(); m++) {
	    QString host = tr(cp->metricContext(m)->source().host().ptr());
	    if (!_hosts.contains(host))
		_hosts.append(host);
	}
    }

    SaveViewDialog::saveView(view.ascii(), true);
    saveFolio(folio, view);
    activeTab->setFolio(folio);

    for (QStringList::Iterator it = _hosts.begin(); it != _hosts.end(); it++) {
	PmLogger *pmlogger = new PmLogger(kmchart, "pmlogger");
	QString archive = archiveLineEdit->text().stripWhiteSpace();
	QString logfile, configfile;

	archive.replace(QRegExp("\\[host\\]"), *it);
	archive.replace(QRegExp("\\[date\\]"), datetoday);
	logfile = configfile = archive;
	logfile.append(".log");
	configfile.append(".config");

	pmlogger->init(_deltaString, *it, archive, logfile, configfile);

	QString configdata;
	if (selectedRadioButton->isChecked())
	    configdata.append(pmlogger->configure(activeTab->currentChart()));
	else
	    for (int c = 0; c < activeTab->numChart(); c++)
		configdata.append(pmlogger->configure(activeTab->chart(c)));
	saveConfig(configfile, configdata);

	// TODO: PMPROXY_HOST support needed here, too
	if (!pmlogger->start()) {
	    QString msg = tr("Recording process (pmlogger) failed to start\n");
	    msg.append("for monitored host ");
	    msg.append(*it);
	    QMessageBox::warning(this, pmProgname, msg);
	    delete pmlogger;
	}
	else {
	    activeTab->addLogger(pmlogger);
	}
    }
}
