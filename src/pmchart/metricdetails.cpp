/*
 * Copyright (c) 2016-2017, Red Hat.
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
#include "chart.h"
#include "metricdetails.h"
#include <QTableWidget>

MetricDetailsWindow::MetricDetailsWindow(QWidget* parent) :
    QDialog(parent)
{
    setupUi(this);
}

void
MetricDetailsWindow::setupTable(Chart *chart)
{
    // Populate the table cells.
    // The documentation for QTableWidget warns against doing this while
    // column sorting is enabled.
    tableWidget->setSortingEnabled(false);
    tableWidget->clearContents();

    // We need to know how many rows there will be first
    int rows = 0;
    QList<ChartItem *> &items = chart->items();
    for (int i = 0; i < items.size(); i++) {
	ChartItem *item = items[i];
	// Make sure this item is active.
	if (item->removed())
	    continue;
	const QString &cursorInfo = item->cursorInfo();
	if (!cursorInfo.isEmpty())
	    ++rows;
    }

    // Now add the data. Leave an extra empty row to make the table look better
    // When stretched vertically.
    ++rows;
    tableWidget->setRowCount(rows);
    int row = 0;
    for (int i = 0; i < items.size(); i++) {
	// Make sure this item is active.
	ChartItem *item = items[i];
	if (item->removed())
	    continue;

	// Add this item's cursor info, if it is not empty.
	const QString &cursorInfo = item->cursorInfo();
	if (!cursorInfo.isEmpty()) {
	    // The host name.
	    TableWidgetItem *twItem = new TableWidgetItem(item->hostname());
	    tableWidget->setItem(row, hostNameColumn(), twItem);

	    // The metric name.
	    twItem = new TableWidgetItem(item->metricName());
	    tableWidget->setItem(row, metricColumn(), twItem);

	    // The instance name, if there is one
	    if (item->metricHasInstances()) {
		twItem = new TableWidgetItem(item->metricInstance());
		tableWidget->setItem(row, instanceColumn(), twItem);
	    }

	    // The metric value.
	    QString dataStr = cursorInfo;
	    Q_ASSERT(dataStr[0] == '[');
	    dataStr.remove(0, 1);
	    int dataEnd = dataStr.indexOf(' ');
	    Q_ASSERT(dataEnd != -1);
	    QString itemStr = dataStr.left(dataEnd);
	    dataStr.remove(0, dataEnd + 1);

	    // Store it as double, if possible, so that it will sort properly.
	    bool isOk;
	    double itemVal = itemStr.toDouble(&isOk);
	    if (isOk) {
		twItem = new TableWidgetItem;
		twItem->setData(Qt::EditRole, itemVal);    
	    }
	    else
		twItem = new TableWidgetItem(itemStr);
	    tableWidget->setItem(row, valueColumn(), twItem);

	    // The time of the sample -- located after "at " in the data string.
	    int atIx = dataStr.indexOf("at ");
	    Q_ASSERT(atIx != -1);
	    dataStr.remove(0, atIx + 3);
	    dataEnd = dataStr.indexOf(']');
	    Q_ASSERT(dataEnd != -1);
	    itemStr = dataStr.left(dataEnd);
	    twItem = new TableWidgetItem(itemStr);
	    tableWidget->setItem(row, timeColumn(), twItem);

	    ++row;
	}
    }

    // Re-enable sorting.
    tableWidget->setSortingEnabled(true);
}

