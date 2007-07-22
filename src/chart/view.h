/*
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
 * Copyright (c) 2007, Nathan Scott.  All Rights Reserved.
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
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 * 
 * Contact information: Ken McDonell, kenj At internode DoT on DoT net
 *                      Nathan Scott, nathans At debian DoT org
 */

#ifndef VIEW_H
#define VIEW_H

#include <qapplication.h>
#include <qfiledialog.h>
#include <qtoolbutton.h>
#include <qcombobox.h>
#include <qprocess.h>

class FileIconProvider : public QFileIconProvider
{
    Q_OBJECT
public:
    FileIconProvider(QObject *, const char *);
    const QPixmap *pixmap(const QFileInfo &);

private:
    QPixmap fileView;		// kmchart view
    QPixmap fileFolio;		// PCP folio
    QPixmap fileArchive;	// PCP archive
    QPixmap fileHtml;
    QPixmap fileImage;
    QPixmap fileGeneric;
    QPixmap filePackage;
    QPixmap fileSpreadSheet;
    QPixmap fileWordProcessor;
};

class OpenViewDialog : public QFileDialog
{
    Q_OBJECT
public:
    OpenViewDialog(QWidget *);
    void reset();

    static void openView(const char *);

private slots:
    void sourceAdd();
    void sourceChange(int);
    void usrDirClicked();
    void sysDirClicked();
    void usrDirToggled(bool);
    void sysDirToggled(bool);
    void viewDirEntered(const QString &);
    void openViewFiles(const QStringList &);

private:
    QString	usrDir;
    QString	sysDir;
    QLabel	*srcLabel;
    QComboBox	*srcCombo;
    QToolButton *usrButton;
    QToolButton *sysButton;
    QPushButton *srcButton;
    bool	archiveMode;
};

class SaveViewDialog : public QFileDialog
{
    Q_OBJECT
public:
    SaveViewDialog(QWidget *);
    void reset();

    static void saveView(const char *, bool);

private slots:
    void saveViewFile(const QString &);

private:
    QString	usrDir;
    bool	hostDynamic;	// on-the-fly or explicit-hostnames-in-view
};

class RecordViewDialog : public QFileDialog
{
    Q_OBJECT
public:
    RecordViewDialog(QWidget *);
    void setFileName(QString);
};

class ExportFileDialog : public QFileDialog
{
    Q_OBJECT
public:
    ExportFileDialog(QWidget *);
};

class Chart;

class PmLogger : public QProcess
{
    Q_OBJECT
public:
    PmLogger(QObject *parent, const char *name);
    void init(QString d, QString h, QString a, QString l, QString c);
    QString configure(Chart *cp);
    void setTerminating() { _terminating = TRUE; }

private slots:
    void exited();

private:
    QString _host;
    QString _logfile;
    bool _terminating;
};

#endif
