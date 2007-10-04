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

QString Source::makeSourceBaseName(const QmcContext *cp)
{
    if (cp->source().type() == PM_CONTEXT_ARCHIVE)
	return cp->source().source();
    return cp->source().host();
}

QString Source::makeSourceAnnotatedName(const QmcContext *cp)
{
    QString t;

    if (cp->source().type() == PM_CONTEXT_HOST) {
	t = cp->source().host();
#if 0	// TODO - find a better way to do this - column #2 in listview maybe?
	t.append(" (no proxy)");
#endif
    }
    else {
	t = cp->source().source();
#if 0	// TODO - find a better way to do this - column #2 in listview maybe?
	t.append(" (");
	t.append(cp->source().host());
	t.append(")");
#endif
    }
    return t;
}

QString Source::makeComboText(const QmcContext *ctx)
{
    if (ctx->source().isArchive() == false)
	return ctx->source().host();
    return makeSourceAnnotatedName(ctx);
}

int useSourceContext(QWidget *parent, QString &source, bool arch)
{
    int sts, type = arch ? PM_CONTEXT_ARCHIVE : PM_CONTEXT_HOST;
    QmcGroup *group = arch ? archiveGroup : liveGroup;
    Source *sources = arch ? archiveSources : liveSources;
    uint_t ctxcount = group->numContexts();

    if ((sts = group->use(type, source)) < 0) {
	QString msg;
	msg.sprintf("Failed to %s \"%s\".\n%s.\n\n",
		    (type == PM_CONTEXT_HOST) ?
		    "connect to pmcd on" : "open archive",
		    (const char *)source.toAscii(), pmErrStr(sts));
	QMessageBox::warning(parent, pmProgname, msg,
		QMessageBox::Ok | QMessageBox::Default | QMessageBox::Escape,
		QMessageBox::NoButton, QMessageBox::NoButton);
    }
    else if (group->numContexts() > ctxcount)
	sources->add(group->which());
    return sts;
}

int Source::useComboContext(QWidget *parent, QComboBox *combo, bool arch)
{
    QString source = combo->currentText().section(QChar(' '), 0, 0);;
    return useSourceContext(parent, source, arch);
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

void Source::add(QmcContext *context)
{
    bool arch = (context->source().type() == PM_CONTEXT_ARCHIVE);

    console->post("Source::add set %s context to %p\n",
			arch ? "archive" : "live", context);

    contextList.append(context);
    if (arch)
	currentArchiveContext = context;
    else
	currentLiveContext = context;
    my.context = context;

#if DESPERATE
    dump();
#endif

    // For archives, send a message to kmtime to grow its time window.
    // This is already done if we're the first, so don't do it again;
    // we also don't have a kmtime connection yet if processing args.
    
    if (kmtime && arch) {
	const QmcSource *source = &context->source();
	QString tz = source->timezone();
	QString host = source->host();
	struct timeval logStartTime = source->start();
	struct timeval logEndTime = source->end();
	kmtime->addArchive(&logStartTime, &logEndTime, tz, host);
    }
}

void Source::dump()
{
    QTextStream cerr(stderr);
    cerr << "Source::dump: currentLive: " << currentLiveContext << endl;
    cerr << "Source::dump: currentArchive: " << currentArchiveContext << endl;
    for (int i = 0; i < contextList.size(); i++) {
	contextList.at(i)->dump(cerr);
	contextList.at(i)->dumpMetrics(cerr);
    }
}

void Source::setupCombo(QComboBox *combo, bool arch)
{
    // We block signals on the source ComboBox here so that we do not
    // send spurious signals out about the list being changed.  If we
    // did, we would keep changing the current context (and incorrect
    // contexts end up being set to current).

    combo->blockSignals(true);
    combo->clear();
    for (int i = 0; i < contextList.size(); i++) {
	QmcContext *cp = contextList.at(i);
	if (cp->source().isArchive() != arch)
	    continue;
	QString source = makeComboText(cp);
	combo->insertItem(i, arch ?
			  fileIconProvider->icon(FileIconProvider::Archive) :
			  fileIconProvider->icon(QFileIconProvider::Computer),
			  source);
	if (arch && cp == currentArchiveContext)
	    combo->setCurrentIndex(i);
	else if (!arch && cp == currentLiveContext)
	    combo->setCurrentIndex(i);
    }
    combo->blockSignals(false);
}

void Source::setCurrentFromCombo(const QString name, bool arch)
{
    for (int i = 0; i < contextList.size(); i++) {
	QmcContext *cp = contextList.at(i);
	if (cp->source().isArchive() != arch)
	    continue;
	QString cname = makeComboText(cp);
	if (arch && name == cname) {
	    console->post("Source::setCurrentFromCombo"
				" set live context to %p\n", cp);
	    currentArchiveContext = cp;
	}
	else if (!arch && name == cname) {
	    console->post("Source::setCurrentFromCombo"
				" set archive context to %p\n", cp);
	    currentLiveContext = cp;
	}
	else
	    continue;
	break;
    }
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
	if ((arch && cp == currentArchiveContext) ||
	   (!arch && cp == currentLiveContext))
	    current = name;
	items.append(name);
    }
    tree->insertTopLevelItems(0, items);
    if (current)
	tree->setCurrentItem(current);
}
