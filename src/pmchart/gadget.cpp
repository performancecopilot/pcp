/*
 * Copyright (c) 2014-2015, Red Hat.
 * Copyright (c) 2008, Aconex.  All Rights Reserved.
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
#include "gadget.h"

Gadget::Gadget(QWidget *widget)
{
    my.widget = widget;
}

QStringList Gadget::hosts()
{
    QStringList hosts;

    for (int m = 0; m < metricCount(); m++) {
	if (activeMetric(m) == false)
	    continue;
        QString host = metricContext(m)->source().host();
        if (!hosts.contains(host))
            hosts.append(host);
    }
    return hosts;
}

QString Gadget::pmloggerMetricSyntax(int m)
{
    QmcMetric *mp = metricPtr(m);
    QString config = mp->name();

    if (mp->numInst() == 1) {
	config.append(" [ \"");
	config.append(metricInstance(m));
	config.append("\" ]");
    }
    return config;
}

QString Gadget::pmloggerSyntax()
{
    QString config;
    bool beDiscrete = false;
    bool nonDiscrete = false;

    // discover whether we need separate log-once/log-every sections
    for (int m = 0; m < metricCount(); m++) {
	if (activeMetric(m) == false)
	    continue;
	if (metricDesc(m)->desc().sem == PM_SEM_DISCRETE)
	    beDiscrete = true;
	else
	    nonDiscrete = true;
    }

    if (beDiscrete) {
	config.append("log mandatory on once {\n");
	for (int m = 0; m < metricCount(); m++) {
	    if (activeMetric(m) == false)
	        continue;
	    if (metricDesc(m)->desc().sem != PM_SEM_DISCRETE)
		continue;
	    config.append('\t');
	    config.append(pmloggerMetricSyntax(m));
	    config.append('\n');
	}
	config.append("}\n");
    }
    if (nonDiscrete) {
	config.append("log mandatory on default {\n");
	for (int m = 0; m < metricCount(); m++) {
	    if (activeMetric(m) == false)
	        continue;
	    if (metricDesc(m)->desc().sem == PM_SEM_DISCRETE)
		continue;
	    config.append('\t');
	    config.append(pmloggerMetricSyntax(m));
	    config.append('\n');
	}
	config.append("}\n");
    }
    return config;
}
