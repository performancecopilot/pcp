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

#define DESPERATE 1

QList<QmcContext *>contextList;
QmcContext *currentContext;

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
	// TODO - pmproxy host support needed here.
	t.append(" (no proxy)");
    }
    else {
	t = cp->source().source();
	t.append(" (");
	t.append(cp->source().host());
	t.append(")");
    }
    return t;
}

QString Source::makeComboText(const QmcContext *ctx)
{
    return makeSourceAnnotatedName(ctx);
}

int useSourceContext(QWidget *parent, QString &source)
{
    int sts;
    QmcGroup *group = activeTab->group();
    uint_t ctxcount = group->numContexts();
    const char *src = source.toAscii();
    char *end, *proxy, *host = strdup(src);

    // TODO: proxy support needed (we toss proxy hostname atm)
    end = host;
    strsep(&end, " ");
    proxy = strsep(&end, ")") + 1;
    *end = '\0';

    console->post("useSourceContext trying new source: %s; host=%s proxy=%s\n",
			src, host, proxy);

    if (strcmp(proxy, "no proxy") == 0)
	unsetenv("PMPROXY_HOST");
    else
	setenv("PMPROXY_HOST", proxy, 1);
    if ((sts = group->use(activeSources->type(), host)) < 0) {
	QString msg = QString();
	msg.sprintf("Failed to %s \"%s\".\n%s.\n\n",
		    (activeSources->type() == PM_CONTEXT_HOST) ?
		    "connect to pmcd on" : "open archive", host, pmErrStr(sts));
	QMessageBox::warning(parent, pmProgname, msg,
		QMessageBox::Ok | QMessageBox::Default | QMessageBox::Escape,
		QMessageBox::NoButton, QMessageBox::NoButton);
    }
    else if (group->numContexts() > ctxcount)
	activeSources->add(group->which());
    free(host);
    return sts;
}

int Source::useComboContext(QWidget *parent, QComboBox *combo)
{
    QString source = combo->currentText();
    int sts = useSourceContext(parent, source);
    if (sts < 0)
	combo->removeItem(0);
    return sts;
}

int Source::type()
{
    return currentContext ? currentContext->source().type() : -1;
}

QString Source::host()
{
    // TODO: nathans - these aint QString's, theyre char* ... hmm?
    return my.context ? my.context->source().host() : NULL;
}

const char *Source::source()
{
#if DESPERATE
    console->post("Source::source(): currentContext=%p", currentContext);
#endif

    if (!currentContext)
    	return NULL;

#if DESPERATE
    console->post("  currentContext handle=%d source=%s",
		  currentContext->handle(),
		  currentContext->source().sourceAscii());
#endif

    return currentContext->source().sourceAscii();
}

void Source::add(QmcContext *context)
{
    bool send_bounds = (context->source().type() == PM_CONTEXT_ARCHIVE);

    contextList.append(context);
    currentContext = context;
    my.context = context;

#if DESPERATE
    dump();
#endif

    // For archives, send a message to kmtime to grow its time window.
    // This is already done if we're the first, so don't do it again;
    // we also don't have a kmtime connection yet if processing args.
    
    if (kmtime && send_bounds) {
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
    cerr << "Source::dump: current: " << currentContext;
    for (int i = 0; i < contextList.size(); i++) {
	contextList.at(i)->dump(cerr);
	contextList.at(i)->dumpMetrics(cerr);
    }
}

void Source::setupCombo(QComboBox *combo)
{
    combo->clear();
    for (int i = 0; i < contextList.size(); i++) {
	QmcContext *cp = contextList.at(i);
	QString source = makeComboText(cp);
	combo->insertItem(i, source);
	if (cp == currentContext)
	    combo->setCurrentIndex(i);
    }
}

void Source::setCurrentInCombo(QComboBox *combo)
{
    if (!currentContext)
	return;

    QString source = makeComboText(currentContext);
    for (int i = 0; i < combo->count(); i++) {
	if (combo->itemText(i) == source) {
	    combo->setCurrentIndex(i);
	    return;
	}
    }
}

// Called from a QComboBox widget, where name is setup by Source::setupCombo
//
void Source::setCurrentFromCombo(const QString name)
{
    for (int i = 0; i < contextList.size(); i++) {
	QmcContext *cp = contextList.at(i);
	if (name == makeComboText(cp)) {
	    currentContext = cp;
	    return;
	}
    }
}

void Source::setupTree(QTreeWidget *tree)
{
    NameSpace *current = NULL;
    QList<QTreeWidgetItem*> items;

    tree->clear();
    for (int i = 0; i < contextList.size(); i++) {
	QmcContext *cp = contextList.at(i);
	NameSpace *name = new NameSpace(tree, cp, activeTab->isArchiveSource());
	name->setOpen(true);
	name->setSelectable(false);
	tree->addTopLevelItem(name);
	if (cp == currentContext)
	    current = name;
	items.append(name);
    }
    tree->insertTopLevelItems(0, items);
    if (current)
    	tree->setCurrentItem(current);
}
