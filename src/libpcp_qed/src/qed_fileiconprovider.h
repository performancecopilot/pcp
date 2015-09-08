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
#ifndef QED_FILEICONPROVIDER_H
#define QED_FILEICONPROVIDER_H

#include <QtGui/QApplication>
#include <QtGui/QFileIconProvider>

class QedFileIconProvider : public QFileIconProvider
{
public:
    QedFileIconProvider();

    typedef enum { View, Folio, Archive, Container, Html, Image,
		   Package, SpreadSheet, WordProcessor } FileIconType;
    QIcon icon(FileIconType type) const;

    QIcon icon(IconType type) const;
    QIcon icon(const QFileInfo &info) const;
    QString type(const QFileInfo &info) const;

private:
    struct {
	QIcon file;
	QIcon folder;
	QIcon computer;
	QIcon container;

	QIcon fileView;		// pmchart view
	QIcon fileFolio;	// PCP folio
	QIcon fileArchive;	// PCP archive
	QIcon fileHtml;
	QIcon fileImage;
	QIcon filePackage;
	QIcon fileSpreadSheet;
	QIcon fileWordProcessor;
    } my;
};

extern QedFileIconProvider *fileIconProvider;

#endif	// QED_FILEICONPROVIDER_H
