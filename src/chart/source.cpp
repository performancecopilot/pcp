/*
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
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

//
// Source Class
//
// manipulate one or more PMAPI contexts

#include <QtGui/QMessageBox>
#include "namespace.h"
#include "main.h"

#define DESPERATE 0

QList<QmcContext *>contextList;
QmcContext *currentLiveContext;
QmcContext *currentArchiveContext;

Source::Source(QmcGroup *group)
{
    my.fetchGroup = group;
    my.context = NULL;
}

int Source::type()
{
    return my.context ? my.context->source().type() : -1;
}

QString Source::host()
{
    return my.context ? my.context->source().host() : QString::null;
}

const char *Source::sourceAscii()
{
    return my.context ? my.context->source().sourceAscii() : NULL;
}

void Source::addLive(QmcContext *context)
{
    console->post("Source::addLive set current context to %p", context);
    contextList.append(context);
    currentLiveContext = context;
    my.context = context;
}

void Source::addArchive(QmcContext *context)
{
    console->post("Source::addArchive set current context to %p", context);
    contextList.append(context);
    currentArchiveContext = context;
    my.context = context;

    // For archives, send a message to kmtime to grow its time window.
    // This is already done if we're the first, so don't do it again;
    // we also don't have a kmtime connection yet if processing args.
    
    if (kmtime) {
	const QmcSource *source = &context->source();
	QString tz = source->timezone();
	QString host = source->host();
	struct timeval logStartTime = source->start();
	struct timeval logEndTime = source->end();
	kmtime->addArchive(&logStartTime, &logEndTime, tz, host);
    }
}

void Source::add(QmcContext *context, bool arch)
{
    if (arch == false)
	addLive(context);
    else
	addArchive(context);
}

int Source::useLiveContext(QString source)
{
    int sts;
    uint ctxcount = liveGroup->numContexts();

    if ((sts = liveGroup->use(PM_CONTEXT_HOST, source)) < 0) {
	QString msg;
	msg.sprintf("Failed to connect to pmcd on \"%s\".\n%s.\n\n",
		    (const char *)source.toAscii(), pmErrStr(sts));
	QMessageBox::warning(NULL, pmProgname, msg,
		QMessageBox::Ok | QMessageBox::Default | QMessageBox::Escape,
		QMessageBox::NoButton, QMessageBox::NoButton);
    }
    else if (liveGroup->numContexts() > ctxcount)
	liveSources->addLive(liveGroup->which());
    return sts;
}

int Source::useArchiveContext(QString source)
{
    int sts;
    uint ctxcount = archiveGroup->numContexts();

    if ((sts = archiveGroup->use(PM_CONTEXT_ARCHIVE, source)) < 0) {
	QString msg;
	msg.sprintf("Failed to open archive \"%s\".\n%s.\n\n",
		    (const char *)source.toAscii(), pmErrStr(sts));
	QMessageBox::warning(NULL, pmProgname, msg,
		QMessageBox::Ok | QMessageBox::Default | QMessageBox::Escape,
		QMessageBox::NoButton, QMessageBox::NoButton);
    }
    else if (archiveGroup->numContexts() > ctxcount)
	archiveSources->addArchive(archiveGroup->which());
    return sts;
}

int Source::useComboContext(QComboBox *combo, bool arch)
{
    if (arch == false)
	return useLiveContext(combo->currentText());
    return useArchiveContext(combo->currentText());
}

void Source::setArchiveFromCombo(QString name)
{
    for (int i = 0; i < contextList.size(); i++) {
	QmcContext *cp = contextList.at(i);
	if (cp->source().isArchive() == false)
	    continue;
	if (name != cp->source().source())
	    continue;
	console->post("Source::setCurrentFromCombo set arch context=%p", cp);
	currentLiveContext = cp;
	break;
    }
}

void Source::setLiveFromCombo(QString name)
{
    for (int i = 0; i < contextList.size(); i++) {
	QmcContext *cp = contextList.at(i);
	if (cp->source().isArchive() == true)
	    continue;
	if (name != cp->source().host())
	    continue;
	console->post("Source::setCurrentFromCombo set live context=%p", cp);
	currentLiveContext = cp;
	break;
    }
}

void Source::setCurrentFromCombo(QString name, bool arch)
{
    if (arch == false)
	setLiveFromCombo(name);
    setArchiveFromCombo(name);
}

void Source::setupCombos(QComboBox *combo, QComboBox *proxy, bool arch)
{
    QIcon archiveIcon = fileIconProvider->icon(FileIconProvider::Archive);
    QIcon hostIcon = fileIconProvider->icon(QFileIconProvider::Computer);
    int index = 0, count = 0;

    console->post("Source::setupCombos current context=%p",
			arch ? currentArchiveContext : currentLiveContext);

    // We block signals on the target combo boxes so that we do not
    // send spurious signals out about their lists being changed.
    // If we did that, we would keep changing the current context.

    combo->blockSignals(true);
    proxy->blockSignals(true);
    combo->clear();
    proxy->clear();

    if (arch) {
	for (int i = 0; i < contextList.size(); i++) {
	    QmcContext *cp = contextList.at(i);
	    if (cp->source().isArchive() == false)
		continue;
	    combo->insertItem(count, archiveIcon, cp->source().source());
	    proxy->insertItem(count, hostIcon, cp->source().host());
	    if (cp == currentArchiveContext)
		index = count;
	    count++;
	}
    }
    else {
	for (int i = 0; i < contextList.size(); i++) {
	    QmcContext *cp = contextList.at(i);
	    if (cp->source().isArchive() == true)
		continue;
	    combo->insertItem(count, hostIcon, cp->source().host());
	    proxy->insertItem(count, hostIcon, cp->source().proxy());
	    if (cp == currentLiveContext)
		index = count;
	    count++;
	}
    }
    combo->blockSignals(false);
    proxy->blockSignals(false);

    console->post("Source::setupCombos setting current index=%d", index);
    combo->setCurrentIndex(index);
    proxy->setCurrentIndex(index);
}

void Source::setupTree(QTreeWidget *tree, bool arch)
{
    NameSpace *current = NULL;
    QList<QTreeWidgetItem*> items;

    tree->clear();
    for (int i = 0; i < contextList.size(); i++) {
	QmcContext *cp = contextList.at(i);
	if (cp->source().isArchive() != arch)
	    continue;
	NameSpace *name = new NameSpace(tree, cp, arch);
	name->setExpanded(true);
	name->setSelectable(false);
	tree->addTopLevelItem(name);
	if ((arch && (cp == currentArchiveContext)) ||
	   (!arch && (cp == currentLiveContext)))
	    current = name;
	items.append(name);
    }
    tree->insertTopLevelItems(0, items);
    if (current)
	tree->setCurrentItem(current);
}
