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

#include <qmessagebox.h>
#include "main.h"

void InfoDialog::reset(QString src, QString m, QString inst, bool archive)
{
    pminfoTextEdit->setText(tr(""));
    pmvalTextEdit->setText(tr(""));
    pminfoStarted = FALSE;
    pmvalStarted = FALSE;
    isArchive = archive;
    metric = m;
    source = src;
    instance = inst;
}

void InfoDialog::pminfo(void)
{
    pminfoProc = new QProcess(this);
    pminfoProc->addArgument("pminfo");
    pminfoProc->addArgument("-df");
    if (isArchive) {
	pminfoProc->addArgument("-a");
	pminfoProc->addArgument(source);
    }
    else {
	pminfoProc->addArgument("-h");
	pminfoProc->addArgument(source);
	pminfoProc->addArgument("-tT");
    }
    pminfoProc->addArgument(metric);

    connect(pminfoProc, SIGNAL(readyReadStdout()),
	    this, SLOT(pminfoStdout()));
    connect(pminfoProc, SIGNAL(readyReadStderr()),
	    this, SLOT(pminfoStderr()));
    if (!pminfoProc->start())
	QMessageBox::critical(0, tr("Fatal error"),
				 tr("Could not start pminfo."), tr("Quit") );
}

void InfoDialog::pminfoStdout()
{
    QString s = pminfoProc->readStdout();
    pminfoTextEdit->append(s);
}

void InfoDialog::pminfoStderr()
{
    QString s = pminfoProc->readStderr();
    pminfoTextEdit->append(s);
}

void InfoDialog::pmval(void)
{
    QString port;
    port.setNum(kmtime->port());

    pmvalProc = new QProcess(this);
    pmvalProc->addArgument("pmval");
    pmvalProc->addArgument("-f4");
    pmvalProc->addArgument("-p");
    pmvalProc->addArgument(port);
    if (isArchive) {
	pmvalProc->addArgument("-a");
	pmvalProc->addArgument(source);
    }
    else {
	pmvalProc->addArgument("-h");
	pmvalProc->addArgument(source);
    }
    pmvalProc->addArgument(metric);

    connect(pmvalProc, SIGNAL(readyReadStdout()),
	    this, SLOT(pmvalStdout()));
    connect(pmvalProc, SIGNAL(readyReadStderr()),
	    this, SLOT(pmvalStderr()));
    if (!pmvalProc->start())
	QMessageBox::critical(0, tr("Fatal error"),
				 tr("Could not start pmval."), tr("Quit") );
}

void InfoDialog::pmvalStdout()
{
    QString s = pmvalProc->readStdout();
    s.stripWhiteSpace();
    pmvalTextEdit->append(s);
}

void InfoDialog::pmvalStderr()
{
    QString s = pmvalProc->readStderr();
    s.stripWhiteSpace();
    s.prepend("<b>");
    s.append("</b>");
    pmvalTextEdit->append(s);
}

void InfoDialog::infoTabCurrentChanged(QWidget *current)
{
    if (current == pminfoTab) {
	if (!pminfoStarted) {
	    pminfoStarted = TRUE;
	    pminfo();
	}
    }
    else if (current == pmvalTab) {
	if (!pmvalStarted) {
	    pmvalStarted = TRUE;
	    pmval();
	}
    }
}
