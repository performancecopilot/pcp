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
#include "searchdialog.h"
#include <QMessageBox>
#include "main.h"

SearchDialog::SearchDialog(QWidget* parent) : QDialog(parent)
{
    setupUi(this);
    my.count = 0;
}

void SearchDialog::languageChange()
{
    retranslateUi(this);
}

void SearchDialog::reset(QTreeWidget *pmns)
{
    my.pmns = pmns;
    buttonOk->setEnabled(false);
    if (matchList->count() > 0)
	buttonAll->setEnabled(true);
    else
	buttonAll->setEnabled(false);
    changed();
    listchanged();
}

void SearchDialog::clear()
{
    this->hostPattern->clear();
    this->metricPattern->clear();
    this->instancePattern->clear();
    this->resultStatus->setText("");
    matchList->clear();
    my.pmnsList.clear();
    buttonSearch->setEnabled(false);
    buttonOk->setEnabled(false);
    buttonAll->setEnabled(false);
}

void SearchDialog::changed()
{
    bool hostEnabled = (hostPattern->text() != QString::null);
    bool metricEnabled = (metricPattern->text() != QString::null);
    bool instanceEnabled = (instancePattern->text() != QString::null);
    buttonSearch->setEnabled(hostEnabled || metricEnabled || instanceEnabled);
}

void SearchDialog::selectall()
{
    matchList->selectAll();
}

void SearchDialog::listchanged()
{
    QList<QListWidgetItem *> check = matchList->selectedItems();
    if (check.size() == 0) {
	buttonOk->setEnabled(false);
    }
    else {
	buttonOk->setEnabled(true);
    }
}

void SearchDialog::search()
{
    QString	res;
    QTreeWidgetItemIterator iterator(my.pmns, QTreeWidgetItemIterator::All);
    int		h_match = 0;
    int		m_match = 0;
    int		i_match;
    int		count;
    QRegExp	h_rx;
    QRegExp	m_rx;
    QRegExp	i_rx;

    console->post(PmChart::DebugUi,
	 "SearchDialog::search host=\"%s\" metric=\"%s\" instance=\"%s\"",
	 (const char *)hostPattern->text().toLatin1(),
	 (const char *)metricPattern->text().toLatin1(),
	 (const char *)instancePattern->text().toLatin1());

    if (hostPattern->text() == QString::null &&
	metricPattern->text() == QString::null &&
	instancePattern->text() == QString::null) {
	// got here via pressing Enter from one of the pattern input fields,
	// and all the fields are empty ... do nothing
	return;
    }

    if (hostPattern->text() != QString::null)
	h_rx.setPattern(hostPattern->text());

    if (metricPattern->text() != QString::null)
	m_rx.setPattern(metricPattern->text());

    if (instancePattern->text() != QString::null)
	i_rx.setPattern(instancePattern->text());

    matchList->clear();
    my.pmnsList.clear();
    count = 0;
    for (; (*iterator); ++iterator) {
	NameSpace *item = (NameSpace *)(*iterator);
	if (item->isRoot()) {
	    // host name
	    if (hostPattern->text() != QString::null)
		h_match = h_rx.indexIn(item->sourceName());
	    else
		h_match = 0;
	    if (h_match >= 0) {
		console->post(PmChart::DebugUi, "SearchDialog::search "
		    "host=\"%s\" h_match=%d",
		    (const char *)item->sourceName().toLatin1(), h_match);
	    }
	    item->setExpanded(true, false);
	    m_match = -2;
	}
	else if (h_match >= 0 && item->isMetric()) {
	    // metric name
	    count++;
	    if (metricPattern->text() != QString::null)
		m_match = m_rx.indexIn(item->metricName());
	    else
		m_match = 0;
	    if (m_match >= 0) {
		if (item->isLeaf() &&
		    instancePattern->text() == QString::null) {
		    QString fqn = item->sourceName().append(":");
		    fqn.append(item->metricName());
		    matchList->addItem(fqn);
		    my.pmnsList.append(item);
		    m_match = -2;

		    console->post(PmChart::DebugUi, "SearchDialog::search "
			"host=%s h_match=%d metric=%s m_match=%d",
			(const char *)item->sourceName().toLatin1(), h_match,
			(const char *)item->metricName().toLatin1(), m_match);
		}
		if (item->isLeaf() == false) {
		    // has instance domain
		    item->setExpanded(true, false);
		    count--;
		}
	    }
	}
	else if (h_match >= 0 && m_match >= 0 && item->isInst()) {
	    // matched last metric, now related instance name ... 
	    count++;
	    if (instancePattern->text() != QString::null)
		i_match = i_rx.indexIn(item->metricInstance());
	    else
		i_match = 0;
	    if (i_match >= 0) {
		QString fqn = item->sourceName().append(":");
		fqn.append(item->metricName());
		fqn.append("[").append(item->metricInstance()).append("]");
		matchList->addItem(fqn);
		my.pmnsList.append(item);

		console->post(PmChart::DebugUi, "SearchDialog::search "
		    "host=%s h_match=%d metric=%s m_match=%d inst=%s i_match=%d",
		    (const char *)item->sourceName().toLatin1(), h_match,
		    (const char *)item->metricName().toLatin1(), m_match,
		    (const char *)item->metricInstance().toLatin1(), i_match);
	    }
	}
	else if (item->isNonLeaf()) {
	    item->setExpanded(true, false);
	    m_match = -2;
	}
	else
	    m_match = -2;
    }

    if (matchList->count() > 0) {
	buttonAll->setEnabled(true);
    }

    QTextStream(&res) << "Matched " << matchList->count() << " of " << count << " possibilities";
    this->resultStatus->setText(res);
}

void SearchDialog::ok()
{
    QList<QListWidgetItem *> selected = matchList->selectedItems();
    int			i;
    int			row;
    NameSpace		*parent;

    for (i = 0; i < selected.size(); i++) {
	row = matchList->row(selected[i]);
#if DESPERATE
	fprintf(stderr, "[%d] %s:%s[%s]\n",
	    row, (const char *)my.pmnsList[row]->sourceName().toLatin1(),
	    (const char *)my.pmnsList[row]->metricName().toLatin1(),
	    (const char *)my.pmnsList[row]->metricInstance().toLatin1());
#endif
	my.pmnsList[row]->setSelected(true);
	parent = (NameSpace *)my.pmnsList[row]->parent();
	while (parent->isRoot() == false) {
#if DESPERATE
	    fprintf(stderr, "SearchDialog::ok expand: %s\n",
			(const char *)parent->metricName().toLatin1());
#endif
	    parent->QTreeWidgetItem::setExpanded(true);
	    parent = (NameSpace *)parent->parent();
	}
    }

    accept();
}
