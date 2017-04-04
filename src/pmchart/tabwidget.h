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
#ifndef TABWIDGET_H
#define TABWIDGET_H

#include <QList>
#include <QTabBar>
#include <QTabWidget>

class Tab;

class TabWidget : public QTabWidget
{
    Q_OBJECT

public:
    TabWidget(QWidget *parent);

    int size() { return my.tabs.size(); }
    Tab *at(int i) { return my.tabs.at(i); }
    Tab *activeTab() { return my.activeTab; }

    bool setActiveTab(Tab *);
    bool setActiveTab(int);
    void insertTab(Tab *);
    void removeTab(int);

private:
    struct {
	Tab *activeTab;
	QList<Tab*> tabs;
    } my;
};

#endif	// TABWIDGET_H
