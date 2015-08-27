/*
 * Copyright (c) 2013, Red Hat.
 * Copyright (c) 2007-2009, Aconex.  All Rights Reserved.
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
#include "openviewdialog.h"
#include <QtGui/QCompleter>
#include <QtGui/QFileDialog>
#include <QtGui/QMessageBox>
#include "main.h"
#include "hostdialog.h"

OpenViewDialog::OpenViewDialog(QWidget *parent) : QDialog(parent)
{
    setupUi(this);
    my.dirModel = new QDirModel;
    my.dirModel->setIconProvider(fileIconProvider);
    dirListView->setModel(my.dirModel);

    my.completer = new QCompleter;
    my.completer->setModel(my.dirModel);
    fileNameLineEdit->setCompleter(my.completer);

    connect(dirListView->selectionModel(),
	SIGNAL(selectionChanged(const QItemSelection&, const QItemSelection&)),
	this, SLOT(dirListView_selectionChanged()));

    QDir dir;
    QChar sep(__pmPathSeparator());
    QString sys = my.systemDir = pmGetConfig("PCP_VAR_DIR");
    my.systemDir.append(sep);
    my.systemDir.append("config");
    my.systemDir.append(sep);
    my.systemDir.append("kmchart");
    if (dir.exists(my.systemDir))
	pathComboBox->addItem(fileIconProvider->icon(QFileIconProvider::Folder),
			  my.systemDir);
    my.systemDir = sys;
    my.systemDir.append(sep);
    my.systemDir.append("config");
    my.systemDir.append(sep);
    my.systemDir.append("pmchart");
    if (dir.exists(my.systemDir))
	pathComboBox->addItem(fileIconProvider->icon(QFileIconProvider::Folder),
			  my.systemDir);

    QString home = my.userDir = QDir::toNativeSeparators(QDir::homePath());
    my.userDir.append(sep);
    my.userDir.append(".pcp");
    my.userDir.append(sep);
    my.userDir.append("kmchart");
    if (dir.exists(my.userDir))
	pathComboBox->addItem(fileIconProvider->icon(QFileIconProvider::Folder),
			  my.userDir);
    my.userDir = home;
    my.userDir.append(sep);
    my.userDir.append(".pcp");
    my.userDir.append(sep);
    my.userDir.append("pmchart");
    if (dir.exists(my.userDir))
	pathComboBox->addItem(fileIconProvider->icon(QFileIconProvider::Folder),
			  my.userDir);

    pathComboBox->addItem(fileIconProvider->icon(QFileIconProvider::Folder),
			  home);
}

OpenViewDialog::~OpenViewDialog()
{
    delete my.completer;
    delete my.dirModel;
}

void OpenViewDialog::reset()
{
    if ((my.archiveSource = pmchart->isArchiveTab())) {
	sourceLabel->setText(tr("Archive:"));
	sourcePushButton->setIcon(QIcon(":/images/archive.png"));
    } else {
	sourceLabel->setText(tr("Host:"));
	sourcePushButton->setIcon(QIcon(":/images/computer.png"));
    }
    setupComboBoxes(my.archiveSource);
    setPath(my.systemDir);
}

void OpenViewDialog::setPathUi(const QString &path)
{
    if (path.isEmpty())
	return;

    int index = pathComboBox->findText(path);
    if (index == -1) {
	pathComboBox->addItem(fileIconProvider->icon(QFileIconProvider::Folder),
				path);
	index = pathComboBox->count() - 1;
    }
    pathComboBox->setCurrentIndex(index);
    dirListView->selectionModel()->clear();

    userToolButton->setChecked(path == my.userDir);
    systemToolButton->setChecked(path == my.systemDir);

    fileNameLineEdit->setModified(false);
    fileNameLineEdit->clear();
}

void OpenViewDialog::setPath(const QModelIndex &index)
{
    console->post("OpenViewDialog::setPath QModelIndex path=%s",
			(const char *)my.dirModel->filePath(index).toAscii());
    my.dirIndex = index;
    my.dirModel->refresh(index);
    dirListView->setRootIndex(index);
    setPathUi(my.dirModel->filePath(index));
}

void OpenViewDialog::setPath(const QString &path)
{
    console->post("OpenViewDialog::setPath QString path=%s",
			(const char *)path.toAscii());
    my.dirIndex = my.dirModel->index(path);
    my.dirModel->refresh(my.dirIndex);
    dirListView->setRootIndex(my.dirIndex);
    setPathUi(path);
}

void OpenViewDialog::pathComboBox_currentIndexChanged(QString path)
{
    console->post("OpenViewDialog::pathComboBox_currentIndexChanged");
    setPath(path);
}

void OpenViewDialog::parentToolButton_clicked()
{
    console->post("OpenViewDialog::parentToolButton_clicked");
    setPath(my.dirModel->parent(my.dirIndex));
}

void OpenViewDialog::userToolButton_clicked(bool enabled)
{
    if (enabled) {
	QDir dir;
	if (!dir.exists(my.userDir))
	    dir.mkpath(my.userDir);
	setPath(my.userDir);
    }
}

void OpenViewDialog::systemToolButton_clicked(bool enabled)
{
    if (enabled)
	setPath(my.systemDir);
}

void OpenViewDialog::dirListView_selectionChanged()
{
    QItemSelectionModel *selections = dirListView->selectionModel();
    QModelIndexList selectedIndexes = selections->selectedIndexes();

    console->post("OpenViewDialog::dirListView_clicked");

    my.completer->setCompletionPrefix(my.dirModel->filePath(my.dirIndex));
    if (selectedIndexes.count() != 1)
	fileNameLineEdit->setText("");
    else
	fileNameLineEdit->setText(my.dirModel->fileName(selectedIndexes.at(0)));
}

void OpenViewDialog::dirListView_activated(const QModelIndex &index)
{
    QFileInfo fi = my.dirModel->fileInfo(index);

    console->post("OpenViewDialog::dirListView_activated");

    if (fi.isDir())
	setPath(index);
    else if (fi.isFile()) {
	QStringList files;
	files << fi.absoluteFilePath();
	if (openViewFiles(files) == true)
	    done(0);
    }
}

int OpenViewDialog::setupArchiveComboBoxes()
{
    QIcon archiveIcon = fileIconProvider->icon(QedFileIconProvider::Archive);
    int index = 0;

    for (unsigned int i = 0; i < archiveGroup->numContexts(); i++) {
	QmcSource source = archiveGroup->context(i)->source();
	sourceComboBox->insertItem(i, archiveIcon, source.source());
	if (i == archiveGroup->contextIndex())
	    index = i;
    }
    return index;
}

int OpenViewDialog::setupLiveComboBoxes()
{
    QIcon containerIcon = fileIconProvider->icon(QedFileIconProvider::Container);
    QIcon hostIcon = fileIconProvider->icon(QFileIconProvider::Computer);
    int index = 0;

    for (unsigned int i = 0; i < liveGroup->numContexts(); i++) {
	QmcSource source = liveGroup->context(i)->source();
	QIcon icon = source.isContainer() ? containerIcon : hostIcon;

	sourceComboBox->insertItem(i, icon, source.hostLabel());
	if (i == liveGroup->contextIndex())
	    index = i;
    }
    return index;
}

void OpenViewDialog::setupComboBoxes(bool arch)
{
    // We block signals on the target combo boxes so that we don't
    // send spurious signals out about their lists being changed.
    // If we did that, we would keep changing the current context.
    sourceComboBox->blockSignals(true);
    sourceComboBox->clear();
    int index = arch ? setupArchiveComboBoxes() : setupLiveComboBoxes();
    sourceComboBox->setCurrentIndex(index);
    sourceComboBox->blockSignals(false);
}

void OpenViewDialog::archiveAdd()
{
    QFileDialog *af = new QFileDialog(this);
    QStringList al;
    int sts;

    af->setFileMode(QFileDialog::ExistingFiles);
    af->setAcceptMode(QFileDialog::AcceptOpen);
    af->setWindowTitle(tr("Add Archive"));
    af->setIconProvider(fileIconProvider);
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
		    QMessageBox::NoButton, QMessageBox::NoButton);
	} else {
	    setupComboBoxes(true);
	    archiveGroup->updateBounds();
	    QmcSource source = archiveGroup->context()->source();
	    pmtime->addArchive(source.start(), source.end(),
				source.timezone(), source.host(), false);
	}
    }
    delete af;
}

void OpenViewDialog::hostAdd()
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
			"OpenViewDialog::newHost: %s (flags=0x%x)",
			(const char *)hostspec.toAscii(), flags);
	    setupComboBoxes(false);
	}
    }
    delete host;
}

void OpenViewDialog::sourcePushButton_clicked()
{
    if (my.archiveSource)
	archiveAdd();
    else
	hostAdd();
}

void OpenViewDialog::sourceComboBox_currentIndexChanged(int index)
{
    console->post("OpenViewDialog::sourceComboBox_currentIndexChanged %d", index);
    if (my.archiveSource == false)
	liveGroup->use((unsigned int)index);
    else
	archiveGroup->use((unsigned int)index);
}

bool OpenViewDialog::useLiveContext(int index)
{
    if (liveGroup->numContexts() == 0) {
	QString msg("No host connections have been established yet\n");
	QMessageBox::warning(NULL, pmProgname, msg,
		QMessageBox::Ok | QMessageBox::Default | QMessageBox::Escape,
		QMessageBox::NoButton, QMessageBox::NoButton);
	return false;
    }

    Q_ASSERT(index <= liveGroup->numContexts());
    QmcSource source = liveGroup->context(index)->source();
    char *sourceName = source.sourceAscii();
    bool result = true;
    int sts;

    if ((sts = liveGroup->use(PM_CONTEXT_HOST, source.source())) < 0) {
	QString msg;
	msg.sprintf("Failed to connect to pmcd on \"%s\".\n%s.\n\n",
		    sourceName, pmErrStr(sts));
	QMessageBox::warning(NULL, pmProgname, msg,
		QMessageBox::Ok | QMessageBox::Default | QMessageBox::Escape,
		QMessageBox::NoButton, QMessageBox::NoButton);
	result = false;
    }
    free(sourceName);
    return result;
}

bool OpenViewDialog::useArchiveContext(int index)
{
    if (archiveGroup->numContexts() == 0) {
	QString msg("No PCP archives have been opened yet\n");
	QMessageBox::warning(NULL, pmProgname, msg,
		QMessageBox::Ok | QMessageBox::Default | QMessageBox::Escape,
		QMessageBox::NoButton, QMessageBox::NoButton);
	return false;
    }

    Q_ASSERT(index <= archiveGroup->numContexts());
    QmcSource source = archiveGroup->context(index)->source();
    char *sourceName = source.sourceAscii();
    bool result = true;
    int sts;

    if ((sts = archiveGroup->use(PM_CONTEXT_ARCHIVE, source.source())) < 0) {
	QString msg;
	msg.sprintf("Failed to open archive \"%s\".\n%s.\n\n",
		    sourceName, pmErrStr(sts));
	QMessageBox::warning(NULL, pmProgname, msg,
		QMessageBox::Ok | QMessageBox::Default | QMessageBox::Escape,
		QMessageBox::NoButton, QMessageBox::NoButton);
	result = false;
    }
    free(sourceName);
    return result;
}

bool OpenViewDialog::useComboBoxContext(bool arch)
{
    if (arch == false)
	return useLiveContext(sourceComboBox->currentIndex());
    else
	return useArchiveContext(sourceComboBox->currentIndex());
}

bool OpenViewDialog::openViewFiles(const QStringList &fl)
{
    QString msg;
    bool result = true;

    if (pmchart->isArchiveTab() != my.archiveSource) {
	if (pmchart->isArchiveTab())
	    msg = tr("Cannot open Host View(s) in an Archive Tab\n");
	else
	    msg = tr("Cannot open Archive View(s) in a Host Tab\n");
	QMessageBox::warning(this, pmProgname, msg,
	    QMessageBox::Ok|QMessageBox::Default|QMessageBox::Escape,
	    QMessageBox::NoButton, QMessageBox::NoButton);
	return false;
    }
    if (useComboBoxContext(my.archiveSource) == false)
	return false;
    QStringList files = fl;
    for (QStringList::Iterator it = files.begin(); it != files.end(); ++it)
	if (openView((const char *)(*it).toAscii()) == false)
	    result = false;
    pmchart->enableUi();
    return result;
}

void OpenViewDialog::openPushButton_clicked()
{
    QStringList files;
    QString msg;

    if (fileNameLineEdit->isModified()) {
	QString filename = fileNameLineEdit->text().trimmed();
	if (filename.isEmpty())
	    msg = tr("No View file(s) specified");
	else {
	    QFileInfo f(filename);
	    if (f.isDir()) {
		setPath(filename);
	    }
	    else if (f.exists()) {
		files << filename;
		if (openViewFiles(files) == true)
		    done(0);
	    }
	    else {
		msg = tr("No such View file exists:\n");
		msg.append(filename);
	    }
	}
    }
    else {
	QItemSelectionModel *selections = dirListView->selectionModel();
	QModelIndexList selectedIndexes = selections->selectedIndexes();

	if (selectedIndexes.count() == 0)
	    msg = tr("No View file(s) selected");
	else {
	    for (int i = 0; i < selectedIndexes.count(); i++) {
		QString filename = my.dirModel->filePath(selectedIndexes.at(i));
		QFileInfo f(filename);
		if (f.isDir())
		    continue;
		files << filename;
	    }
	    if (files.size() > 0)
		if (openViewFiles(files) == true)
		    done(0);
	}
    }

    if (msg.isEmpty() == false) {
	QMessageBox::warning(this, pmProgname, msg,
	    QMessageBox::Ok|QMessageBox::Default|QMessageBox::Escape,
	    QMessageBox::NoButton, QMessageBox::NoButton);
    }
}
