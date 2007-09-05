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
#include "saveviewdialog.h"
#include <QtCore/QDir>
#include <QtGui/QCompleter>
#include <QtGui/QMessageBox>
#include "main.h"

SaveViewDialog::SaveViewDialog(QWidget* parent) : QDialog(parent)
{
    setupUi(this);
    my.dirModel = new QDirModel;
    my.dirModel->setIconProvider(fileIconProvider);
    dirListView->setModel(my.dirModel);

    my.completer = new QCompleter;
    my.completer->setModel(my.dirModel);
    fileNameLineEdit->setCompleter(my.completer);

    QString home = my.userDir = QDir::homePath();
    my.userDir.append("/.pcp/kmchart");
    my.hostDynamic = true;

    pathComboBox->addItem(fileIconProvider->icon(QFileIconProvider::Folder),
			  my.userDir);
    pathComboBox->addItem(fileIconProvider->icon(QFileIconProvider::Folder),
			  home);
}

SaveViewDialog::~SaveViewDialog()
{
    delete my.completer;
    delete my.dirModel;
}

void SaveViewDialog::reset()
{
    QDir d;
    if (!d.exists(my.userDir))
	d.mkdir(my.userDir);
    setPath(my.userDir);
}

void SaveViewDialog::setPathUi(const QString &path)
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

    userToolButton->setChecked(path == my.userDir);

    fileNameLineEdit->setModified(false);
    fileNameLineEdit->clear();
}

void SaveViewDialog::setPath(const QModelIndex &index)
{
    console->post("SaveViewDialog::setPath QModelIndex path=%s",
			(const char *)my.dirModel->filePath(index).toAscii());
    my.dirIndex = index;
    dirListView->setRootIndex(index);
    setPathUi(my.dirModel->filePath(index));
}

void SaveViewDialog::setPath(const QString &path)
{
    console->post("SaveViewDialog::setPath QString path=%s",
			(const char *)path.toAscii());
    my.dirIndex = my.dirModel->index(path);
    dirListView->setRootIndex(my.dirIndex);
    setPathUi(path);
}

void SaveViewDialog::pathComboBox_currentIndexChanged(QString path)
{
    console->post("SaveViewDialog::pathComboBox_currentIndexChanged");
    setPath(path);
}

void SaveViewDialog::parentToolButton_clicked()
{
    console->post("SaveViewDialog::parentToolButton_clicked");
    setPath(my.dirModel->parent(my.dirIndex));
}

void SaveViewDialog::userToolButton_clicked(bool enabled)
{
    if (enabled) {
	QDir dir;
	if (!dir.exists(my.userDir))
	    dir.mkdir(my.userDir);
	setPath(my.userDir);
    }
}

void SaveViewDialog::dirViewListToolButton_toggled(bool enabled)
{
    if (enabled) {
	dirListView->setAlternatingRowColors(true);
	dirListView->setViewMode(QListView::ListMode);
	dirViewIconToolButton->setChecked(false);
    }
}

void SaveViewDialog::dirViewIconToolButton_toggled(bool enabled)
{
    if (enabled) {
	dirListView->setAlternatingRowColors(false);
	dirListView->setViewMode(QListView::IconMode);
	dirViewListToolButton->setChecked(false);
    }
}

void SaveViewDialog::dirListView_clicked(const QModelIndex &)
{
    QItemSelectionModel *selections = dirListView->selectionModel();
    QModelIndexList selectedIndexes = selections->selectedIndexes();

    console->post("SaveViewDialog::dirListView_clicked");

    my.completer->setCompletionPrefix(my.dirModel->filePath(my.dirIndex));
    if (selectedIndexes.count() != 1)
	fileNameLineEdit->setText("");
    else
	fileNameLineEdit->setText(my.dirModel->fileName(selectedIndexes.at(0)));
}

void SaveViewDialog::dirListView_activated(const QModelIndex &index)
{
    QFileInfo fi = my.dirModel->fileInfo(index);

    console->post("SaveViewDialog::dirListView_activated");

    if (fi.isDir()) {
	setPath(index);
    }
    else if (fi.isFile()) {
	if (saveViewFile(fi.absoluteFilePath()) == true)
	    done(0);
    }
}

void SaveViewDialog::preserveHostCheckBox_toggled(bool hostPreserve)
{
    my.hostDynamic = (hostPreserve == false);
}

bool SaveViewDialog::saveViewFile(const QString &filename)
{
    return saveView(filename, my.hostDynamic);
}

void SaveViewDialog::savePushButton_clicked()
{
    QString msg, filename;

    if (fileNameLineEdit->isModified()) {
	filename = fileNameLineEdit->text().trimmed();
	filename.prepend(my.dirModel->filePath(my.dirIndex).append("/"));
    } else {
	QItemSelectionModel *selections = dirListView->selectionModel();
	QModelIndexList selectedIndexes = selections->selectedIndexes();

	if (selectedIndexes.count() == 1)
	    filename = my.dirModel->fileName(selectedIndexes.at(0));
    }

    if (filename.isEmpty())
	msg = tr("No View file specified");
    else {
	QFileInfo f(filename);
	if (f.exists()) {
	    msg = filename;
	    msg.prepend(tr("View file "));
	    msg.append(tr(" exists.  Overwrite?\n"));
	    if (QMessageBox::question(this, pmProgname, msg,
		QMessageBox::Cancel|QMessageBox::Default|QMessageBox::Escape,
		QMessageBox::Ok, QMessageBox::NoButton) == QMessageBox::Ok)
		if (saveViewFile(filename) == true)
		    done(0);
	    msg = "";
	}
	else if (saveViewFile(filename) == true)
	    done(0);
    }

    if (msg.isEmpty() == false) {
	QMessageBox::warning(this, pmProgname, msg,
	    QMessageBox::Ok|QMessageBox::Default|QMessageBox::Escape,
	    QMessageBox::NoButton, QMessageBox::NoButton);
    }
}
