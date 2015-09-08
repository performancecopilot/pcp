/*
 * Copyright (c) 2014-2015, Red Hat.
 * Copyright (c) 2007, Aconex.  All Rights Reserved.
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
#include "qed_fileiconprovider.h"
#include "qed_console.h"

#define DESPERATE 0

QedFileIconProvider *fileIconProvider;

QedFileIconProvider::QedFileIconProvider() : QFileIconProvider()
{
    // generic Qt QFileIconProvider types
    my.file = QIcon(":/images/filegeneric.png");
    my.folder = QIcon(":/images/filefolder.png");
    my.computer = QIcon(":/images/computer.png");

    // PCP GUI specific images
    my.container = QIcon(":/images/container.png");
    my.fileView = QIcon(":/images/fileview.png");
    my.fileFolio = QIcon(":/images/filefolio.png");
    my.fileArchive = QIcon(":/images/filearchive.png");

    // images for several other common file types
    my.fileHtml = QIcon(":/images/filehtml.png");
    my.fileImage = QIcon(":/images/fileimage.png");
    my.filePackage = QIcon(":/images/filepackage.png");
    my.fileSpreadSheet = QIcon(":/images/filespreadsheet.png");
    my.fileWordProcessor = QIcon(":/images/filewordprocessor.png");
}

QIcon QedFileIconProvider::icon(FileIconType type) const
{
    console->post("QedFileIconProvider::icon extended types");
    switch (type) {
    case View:
	return my.fileView;
    case Folio:
	return my.fileFolio;
    case Archive:
	return my.fileArchive;
    case Container:
	return my.container;
    case Html:
	return my.fileHtml;
    case Image:
	return my.fileImage;
    case Package:
	return my.filePackage;
    case SpreadSheet:
	return my.fileSpreadSheet;
    case WordProcessor:
	return my.fileWordProcessor;
    default:
	break;
    }
    return my.file;
}

QIcon QedFileIconProvider::icon(IconType type) const
{
    console->post("QedFileIconProvider::icon type");
    switch (type) {
    case File:
	return my.file;
    case Folder:
	return my.folder;
    case Computer:
	return my.computer;
    default:
	break;
    }
    return QFileIconProvider::icon(type);
}

QString QedFileIconProvider::type(const QFileInfo &fi) const
{
    console->post("QedFileIconProvider::type string");
    return QFileIconProvider::type(fi);
}

QIcon QedFileIconProvider::icon(const QFileInfo &fi) const
{
#if DESPERATE
    console->post("QedFileIconProvider::icon - %s",
			(const char *)fi.filePath().toAscii());
#endif

    if (fi.isFile()) {
	QFile file(fi.filePath());
	if (!file.open(QIODevice::ReadOnly))
	    return my.file;	// catch-all return code for regular files
	char block[9];
	int count = file.read(block, sizeof(block)-1);
	if (count == sizeof(block)-1) {
	    static const char *viewmagic[] = { "#kmchart", "#pmchart" };
	    static char foliomagic[] = "PCPFolio";
	    static char archmagic[] = "\0\0\0\204\120\5\46\2"; //PM_LOG_MAGIC|V2

	    if (memcmp(viewmagic[0], block, sizeof(block)-1) == 0)
		return my.fileView;
	    if (memcmp(viewmagic[1], block, sizeof(block)-1) == 0)
		return my.fileView;
	    if (memcmp(foliomagic, block, sizeof(block)-1) == 0)
		return my.fileFolio;
	    if (memcmp(archmagic, block, sizeof(block)-1) == 0)
		return my.fileArchive;
	}
#if DESPERATE
	console->post("  Got %d bytes from %s: \"%c%c%c%c%c%c%c%c\"", count,
		(const char *) fi.filePath().toAscii(), block[0], block[1],
		block[2], block[3], block[4], block[5], block[6], block[7]);
#endif
	QString ext = fi.suffix();
	if (ext == "htm" || ext == "html")
	    return my.fileHtml;
	if (ext == "svg" || ext == "gif" || ext == "jpg" || ext == "jpeg" ||
	    ext == "png" || ext == "xpm" || ext == "odg" /* ... */ )
	    return my.fileImage;
	if (ext == "tar" || ext == "tgz" || ext == "deb" || ext == "rpm" ||
	    ext == "zip" || ext == "bz2" || ext == "gz" || ext == "xz")
	    return my.filePackage;
	if (ext == "ods" || ext == "xls")
	    return my.fileSpreadSheet;
	if (ext == "odp" || ext == "doc")
	    return my.fileWordProcessor;
	return my.file;	// catch-all for every other regular file
    }
    else if (fi.isDir()) {
	return my.folder;
    }
    return QFileIconProvider::icon(fi);
}
