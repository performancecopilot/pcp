/*
 * Copyright (c) 2015, Red Hat.
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

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtGui/QApplication>
#include <QtGui/QMessageBox>
#include <QtGui/QListView>
#include "namespace.h"
#include "chart.h"
#include "main.h"

#define DESPERATE 0

NameSpace::NameSpace(NameSpace *parent, const QString &name, bool inst)
    : QTreeWidgetItem(parent, QTreeWidgetItem::UserType)
{
    my.expanded = false;
    my.back = parent;
    my.desc = parent->my.desc;
    my.context = parent->my.context;
    my.basename = name;
    if (name == QString::null)
	my.type = ChildMinder;
    else if (!inst)
	my.type = NoType;
    else {
	my.type = InstanceName;
	QFont font = QTreeWidgetItem::font(0);
	font.setItalic(true);
	setFont(0, font);
    }
    setText(0, my.basename);
#if DESPERATE
    if (my.type == ChildMinder) {
	console->post(PmChart::DebugUi, "Added namespace childminder");
    }
    else {
	console->post(PmChart::DebugUi, "Added non-root namespace node %s (inst=%d)",
		  (const char *)my.basename.toAscii(), inst);
    }
#endif
}

NameSpace::NameSpace(QTreeWidget *list, const QmcContext *context)
    : QTreeWidgetItem(list, QTreeWidgetItem::UserType)
{
    QmcSource source = context->source();

    my.basename = source.hostLabel();
    my.context = (QmcContext *)context;
    memset(&my.desc, 0, sizeof(my.desc));
    my.expanded = false;
    my.back = this;

    if (source.isArchive()) {
	my.basename = source.source();
	my.icon = QIcon(":/images/archive.png");
	my.type = ArchiveRoot;
    } else if (source.isContainer()) {
	my.icon = QIcon(":/images/container.png");
	my.type = ContainerRoot;
    } else if (source.type() == PM_CONTEXT_LOCAL) {
	my.basename = QString("Local context");
	my.icon = QIcon(":/images/emblem-system.png");
	my.type = LocalRoot;
    } else {
	my.icon = QIcon(":/images/computer.png");
	my.type = HostRoot;
    }
    setToolTip(0, sourceTip());
    setText(0, my.basename);
    setIcon(0, my.icon);

    console->post(PmChart::DebugUi, "Added root namespace node %s",
		  (const char *)my.basename.toAscii());
}

QString NameSpace::sourceTip()
{
    QString tooltip;
    QmcSource source = my.context->source();

    tooltip = "Performance metrics from host ";
    tooltip.append(source.hostLabel());

    if (source.type() == PM_CONTEXT_ARCHIVE) {
	tooltip.append("\n  commencing ");
	tooltip.append(source.startTime());
	tooltip.append("\n  ending            ");
	tooltip.append(source.endTime());
    } else if (source.attributes() != QString::null) {
	tooltip.append("\nAttributes: ");
	tooltip.append(source.attributes());
    }

    tooltip.append("\nTimezone: ");
    tooltip.append(source.timezone());
    return tooltip;
}

int NameSpace::sourceType()
{
    return my.context->source().type();
}

QString NameSpace::source()
{
    return my.context->source().source();
}

QString NameSpace::sourceName()
{
    return my.context->source().hostLabel();
}

QString NameSpace::metricName()
{
    QString s;

    if (my.back->isRoot())
	s = text(0);
    else if (my.type == InstanceName)
	s = my.back->metricName();
    else {
	s = my.back->metricName();
	s.append(".");
	s.append(text(0));
    }
    return s;
}

QString NameSpace::metricInstance()
{
    if (my.type == InstanceName)
	return text(0);
    return QString::null;
}

void NameSpace::setExpanded(bool expand, bool show)
{
#if DESPERATE
    console->post(PmChart::DebugUi, "NameSpace::setExpanded "
		  "on %p %s (type=%d expanded=%s, expand=%s, show=%s)",
		  this, (const char *)metricName().toAscii(),
		  my.type,
		  my.expanded? "y" : "n", expand? "y" : "n", show? "y" : "n");
#endif

    if (expand && !my.expanded) {
	NameSpace *kid = (NameSpace *)child(0);

	if (kid && kid->isChildMinder()) {
	    takeChild(0);
	    delete kid;
	}
	my.expanded = true;
	pmUseContext(my.context->handle());
 
	if (my.type == LeafWithIndom)
	    expandInstanceNames(show);
	else if (my.type != InstanceName) {
	    expandMetricNames(isRoot() ? "" : metricName(), show);
	}
    }

    if (show) {
	QTreeWidgetItem::setExpanded(expand);
    }

}

void NameSpace::setSelectable(bool selectable)
{
    if (selectable)
	setFlags(flags() | Qt::ItemIsSelectable);
    else
	setFlags(flags() & ~Qt::ItemIsSelectable);
}

void NameSpace::setExpandable(bool expandable)
{
    console->post(PmChart::DebugUi, "NameSpace::setExpandable "
		  "on %p %s (expanded=%s, expandable=%s)",
		  this, (const char *)metricName().toAscii(),
		  my.expanded ? "y" : "n", expandable ? "y" : "n");

    // NOTE: QT4.3 has setChildIndicatorPolicy for this workaround, but we want
    // to work on QT4.2 as well (this is used on Debian 4.0 - i.e. my laptop!).
    // This is the ChildMinder workaround - we insert a "dummy" child into items
    // that we know have children (since we have no way to explicitly set it and
    // we want to delay finding the actual children as late as possible).
    // When we later do fill in the real kids, we first delete the ChildMinder.

    if (expandable)
	addChild(new NameSpace(this, QString::null, false));
}

static char *namedup(const char *name, const char *suffix)
{
    char *n;

    if (strlen(name) > 0) {
	n = (char *)malloc(strlen(name) + 1 + strlen(suffix) + 1);
	sprintf(n, "%s.%s", name, suffix);
    }
    else {
	n = strdup(suffix);
    }
    return n;
}

void NameSpace::setFailed(bool failed)
{
    bool selectable = (failed == false &&
		      (my.type == LeafNullIndom || my.type == InstanceName));
    setSelectable(selectable);
    bool expandable = (failed == false && 
		      (my.type != LeafNullIndom && my.type != InstanceName));
    setExpandable(expandable);

    QFont font = QTreeWidgetItem::font(0);
    font.setStrikeOut(failed);
    setFont(0, font);
}

void NameSpace::expandMetricNames(QString parent, bool show)
{
    char	**offspring = NULL;
    int		*status = NULL;
    pmID	*pmidlist = NULL;
    int		i, nleaf = 0;
    int		sts, noffspring;
    NameSpace	*m, **leaflist = NULL;
    char	*name = strdup(parent.toAscii());
    int		sort_done, fail_count = 0;
    QString	failmsg;

    sts = pmGetChildrenStatus(name, &offspring, &status);
    if (sts < 0) {
	if (!show)
	    goto done;
	QString msg = QString();
	if (isRoot())
	    msg.sprintf("Cannot get metric names from source\n%s: %s.\n\n",
		(const char *)my.basename.toAscii(), pmErrStr(sts));
	else
	    msg.sprintf("Cannot get children of node\n\"%s\".\n%s.\n\n",
		name, pmErrStr(sts));
	QMessageBox::warning(NULL, pmProgname, msg,
		QMessageBox::Ok | QMessageBox::Default | QMessageBox::Escape,
		QMessageBox::NoButton, QMessageBox::NoButton);
	goto done;
    }
    else {
	noffspring = sts;
    }

    // Ugliness ahead.
    // The Qt routine sortChildren() does not work as we maintain
    // our own pointers into the tree items via my.back ... if
    // sortChildren() is used, our expansion picking does not work later
    // on.
    // Sort the PMNS children lexicographically by name before adding them
    // into the QTreeWidget ... only tricky part is that we need to sort
    // offspring[] AND status[]
    // Bubble Sort is probably OK as the number of descendents in the
    // PMNS is never huge.
    //
    sort_done = 0;
    while (!sort_done) {
	char	*ctmp;
	int	itmp;
	sort_done = 1;
	for (i = 0; i < noffspring-1; i++) {
	    if (strcmp(offspring[i], offspring[i+1]) <= 0)
		continue;
	    // swap
	    ctmp = offspring[i];
	    offspring[i] = offspring[i+1];
	    offspring[i+1] = ctmp;
	    itmp = status[i];
	    status[i] = status[i+1];
	    status[i+1] = itmp;
	    sort_done = 0;
	}
    }

    for (i = 0; i < noffspring; i++) {
	m = new NameSpace(this, offspring[i], false);

	if (status[i] == PMNS_NONLEAF_STATUS) {
	    m->setSelectable(false);
	    m->setExpandable(true);
	    m->my.type = NonLeafName;
	}
	else {
	    // type still not set, could be LEAF_NULL_INDOM or LEAF_WITH_INDOM
	    // here we add this NameSpace pointer to the leaf list, and also
	    // modify the offspring list to only contain names (when done).
	    leaflist = (NameSpace **)realloc(leaflist,
					    (nleaf + 1) * sizeof(*leaflist));
	    leaflist[nleaf] = m;
	    offspring[nleaf] = namedup(name, offspring[i]);
	    nleaf++;
	}
    }

    if (nleaf == 0) {
	my.expanded = true;
	goto done;
    }

    pmidlist = (pmID *)malloc(nleaf * sizeof(*pmidlist));
    if ((sts = pmLookupName(nleaf, offspring, pmidlist)) < 0) {
	if (!show)
	    goto done;
	failmsg.sprintf("Cannot find PMIDs: %s.\n", pmErrStr(sts));
	for (i = 0; i < nleaf; i++) {
	    leaflist[i]->setFailed(true);
	    if (pmidlist[i] == PM_ID_NULL) {
		if (fail_count == 0)
		    failmsg.append("Metrics:\n");
		if (fail_count < 5)
		    failmsg.append("  ").append(offspring[i]).append("\n");
		else if (fail_count == 5)
		    failmsg.append("... (further metrics omitted).\n");
		fail_count++;
	    }
	}
	fail_count = fail_count ? fail_count : 1;
    }
    else {
	for (i = 0; i < nleaf; i++) {
	    m = leaflist[i];
	    sts = pmLookupDesc(pmidlist[i], &m->my.desc);
	    if (sts < 0) {
		if (!show)
		    goto done;
		if (fail_count < 3) {
		    failmsg.append("Cannot find metric descriptor at \"");
		    failmsg.append(offspring[i]).append("\":\n  ");
		    failmsg.append(pmErrStr(sts)).append(".\n\n");
		}
		else if (fail_count == 3) {
		    failmsg.append("... (further errors omitted).\n");
		}
		m->setFailed(true);
		fail_count++;
	    }
	    else if (m->my.desc.indom == PM_INDOM_NULL) {
		m->my.type = LeafNullIndom;
		m->setExpandable(false);
		m->setSelectable(true);
	    }
	    else {
		m->my.type = LeafWithIndom;
		m->setExpandable(true);
		m->setSelectable(false);
	    }
	}
	my.expanded = true;
    }

done:
    if (fail_count)
	QMessageBox::warning(NULL, pmProgname, failmsg,
		QMessageBox::Ok | QMessageBox::Default | QMessageBox::Escape,
		QMessageBox::NoButton, QMessageBox::NoButton);
    if (pmidlist)
	free(pmidlist);
    if (leaflist)
	free(leaflist);
    if (offspring) {
	for (i = 0; i < nleaf; i++)
	    free(offspring[i]);
	free(offspring);
    }
    if (status)
	free(status);
    free(name);
}

void NameSpace::expandInstanceNames(bool show)
{
    int		sts, i;
    int		ninst = 0;
    int		*instlist = NULL;
    char	**namelist = NULL;
    bool	live = (my.context->source().type() != PM_CONTEXT_ARCHIVE);

    sts = live ? pmGetInDom(my.desc.indom, &instlist, &namelist) :
		 pmGetInDomArchive(my.desc.indom, &instlist, &namelist);
    if (sts < 0) {
	if (!show)
	    goto done;
	QString msg = QString();
	msg.sprintf("Error fetching instance domain at node \"%s\".\n%s.\n\n",
		(const char *)metricName().toAscii(), pmErrStr(sts));
	QMessageBox::warning(NULL, pmProgname, msg,
		QMessageBox::Ok | QMessageBox::Default |
			QMessageBox::Escape,
		QMessageBox::NoButton, QMessageBox::NoButton);
    }
    else {
	ninst = sts;
	my.expanded = true;
    }

    for (i = 0; i < ninst; i++) {
	NameSpace *m = new NameSpace(this, namelist[i], true);

	m->setExpandable(false);
	m->setSelectable(true);
    }

done:
    if (instlist)
	free(instlist);
    if (namelist)
	free(namelist);
}

QString NameSpace::text(int column) const
{
    if (column > 0)
	return QString::null;
    return my.basename;
}

QIcon NameSpace::icon(int) const
{
    return my.icon;
}

void NameSpace::setIcon(int i, const QIcon &icon)
{
    my.icon = icon;
    QTreeWidgetItem::setIcon(i, icon);
}

NameSpace *NameSpace::dup(QTreeWidget *list)
{
    NameSpace *n;

    n = new NameSpace(list, my.context);
    n->expand();
    n->setSelectable(false);
    return n;
}

NameSpace *NameSpace::dup(QTreeWidget *, NameSpace *tree,
			  QString scheme, int *sequence)
{
    NameSpace *n;

    n = new NameSpace(tree, my.basename, my.type == InstanceName);
    n->my.context = my.context;
    n->my.desc = my.desc;
    n->my.type = my.type;

    if (my.type == NoType || my.type == ChildMinder) {
	console->post("NameSpace::dup bad type=%d on %p %s)",
		  my.type, this, (const char *)metricName().toAscii());
	abort();
    }
    else if (!isLeaf()) {
	n->expand();
	n->setSelectable(false);
    }
    else {
	n->expand();
	n->setSelectable(true);

	QColor c = nextColor(scheme, sequence);
	n->setOriginalColor(c);
	n->setCurrentColor(c, NULL);

	// this is a leaf so walk back up to the root, opening each node up
	NameSpace *up;
	for (up = tree; up->my.back != up; up = up->my.back) {
	    up->expand();
	    up->setExpanded(true, true);
	}
	up->expand();
	up->setExpanded(true, true);	// add the host/archive root as well.

	n->setSelected(true);
    }
    return n;
}

bool NameSpace::cmp(NameSpace *item)
{
    if (!item)	// empty list
	return false;
    return (item->text(0) == my.basename);
}

void NameSpace::addToTree(QTreeWidget *target, QString scheme, int *sequence)
{
    QList<NameSpace *> nodelist;
    NameSpace *node;

    // Walk through each component of this name, creating them in the
    // target list (if not there already), right down to the leaf.
    // We hold everything in a temporary list since we need to add to
    // the target in root-to-leaf order but we're currently at a leaf.

    for (node = this; node->my.back != node; node = node->my.back)
	nodelist.prepend(node);
    nodelist.prepend(node);	// add the host/archive root as well.

    NameSpace *tree = (NameSpace *)target->invisibleRootItem();
    NameSpace *item = NULL;

    for (int n = 0; n < nodelist.size(); n++) {
	node = nodelist.at(n);
	bool foundMatchingName = false;
	for (int i = 0; i < tree->childCount(); i++) {
	    item = (NameSpace *)tree->child(i);
	    if (node->cmp(item)) {
		// no insert at this level necessary, move down a level
		if (!node->isLeaf()) {
		    tree = item;
		    item = (NameSpace *)item->child(0);
		}
		// already there, just select the existing name
		else {
		    item->setSelected(true);
		}
		foundMatchingName = true;
		break;
	    }
	}

	// When no more children and no match so far, we dup & insert
	if (foundMatchingName == false) {
	    if (node->isRoot()) {
		tree = node->dup(target);
	    }
	    else if (tree) {
		tree = node->dup(target, tree, scheme, sequence);
	    }
	}
    }
}

void NameSpace::removeFromTree(QTreeWidget *)
{
    NameSpace *self, *tree, *node = this;

    this->setSelected(false);
    do {
	self = node;
	tree = node->my.back;
	if (node->childCount() == 0)
	    delete node;
	node = tree;
    } while (self != tree);
}

void NameSpace::setCurrentColor(QColor color, QTreeWidget *treeview)
{
    QPixmap pix(8, 8);

    // Set the current metric color, and (optionally) move on to the next
    // leaf node in the view (if given treeview parameter is non-NULL).
    // We're taking a punt here that the user will move down through the
    // metrics just added, and set their prefered color on each one; so,
    // by doing the next selection automatically, we save some clicks.

    my.current = color;
    pix.fill(color);
    setIcon(0, QIcon(pix));

    if (treeview) {
	QTreeWidgetItemIterator it(this, QTreeWidgetItemIterator::Selectable);
	if (*it) {
	    (*it)->setSelected(true);
	    this->setSelected(false);
	}
    }
}
