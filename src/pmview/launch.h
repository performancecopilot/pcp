/*
 * Copyright (c) 1997 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2009 Aconex.  All Rights Reserved.
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
#ifndef _LAUNCH_H_
#define _LAUNCH_H_

#include "main.h"
#include <QtCore/QStringList>

class SbColor;
class ColorScale;

class Launch
{
private:

    static const QString	theVersion1Str;
    static const QString	theVersion2Str;
    
    QStringList		_strings;
    int			_groupMetric;
    int			_groupCount;
    QString		_groupHint;
    int			_metricCount;
    int			_version;

public:

    ~Launch();

    Launch(const QString &version = "");	// defaults to version 2.0
    Launch(const Launch &rhs);
    const Launch &operator=(const Launch &rhs);

    static const char *launchPath();

    void setDefaultOptions(int interval = 5,
			   int debug = 0,
			   const char *timeport = NULL,
			   const char *starttime = NULL,
			   const char *endtime = NULL,
			   const char *offset = NULL,
			   const char *timezone = NULL,
			   const char *defsourcetype = NULL,
			   const char *defsourcename = NULL,
			   bool selected = false);

    void addOption(const char *name, const char *value);
    void addOption(const char *name, int value);

    void addMetric(const QmcMetric &metric, 
		   const SbColor &color,
		   int instance,
		   bool useSocks = false);

    void addMetric(const QmcMetric &metric, 
		   const ColorScale &scale,
		   int instance,
		   bool useSocks = false);

    void addMetric(int context,
		   const QString &source,
		   const QString &host,
		   const QString &metric,
		   const char *instance,
		   const QmcDesc &desc,
		   const SbColor &color,
		   double scale,
		   bool useSocks = false);
    // Add metric with static color and scale

    void addMetric(int context,
		   const QString &source,
		   const QString &host,
		   const QString &metric,
		   const char *instance,
		   const QmcDesc &desc,
		   const ColorScale &scale,
		   bool useSocks = false);
    // Add metric with dynamic color range

    void startGroup(const char *hint = "none");
    void endGroup();

    void append(Launch const& rhs);

    void output(int fd) const;

    friend QTextStream& operator<<(QTextStream& os, Launch const& rhs);

private:

    void preColor(int context, 
    		  const QString &source, 
		  const QString &host,
		  const QString &metric,
		  bool useSocks,
		  QString &str);
    void postColor(const QmcDesc &desc, const char *instance, QString &str);
};

#endif /* _LAUNCH_H_ */
