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
#include "tabwidget.h"
#include "tab.h"
#include "main.h"

TabWidget::TabWidget(QWidget *parent) : QTabWidget(parent)
{
    setFont(*globalFont);
    my.activeTab = NULL;
}

bool TabWidget::setActiveTab(Tab *tab)
{
    my.activeTab = tab;
    return tab->isArchiveSource();
}

bool TabWidget::setActiveTab(int index)
{
    my.activeTab = my.tabs.at(index);
    return my.activeTab->isArchiveSource();
}

void TabWidget::insertTab(Tab *tab)
{
    my.tabs.append(tab);
    tabBar()->setVisible(my.tabs.size() > 1);
}

void TabWidget::removeTab(int index)
{
    my.tabs.removeAt(index);
    tabBar()->setVisible(my.tabs.size() > 1);
    QTabWidget::removeTab(index);
}
