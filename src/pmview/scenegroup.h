/*
 * Copyright (c) 2009, Aconex.  All Rights Reserved.
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
#ifndef SCENEGROUP_H
#define SCENEGROUP_H

#include <QtCore/QList>
#include <QtGui/QColor>
#include <QtGui/QPainter>
#include <QtGui/QDockWidget>
#include <QtGui/QAbstractButton>
#include "groupcontrol.h"
#include "qmc_metric.h"
#include "qmc_group.h"
#include "pmtime.h"

class SceneGroup : public GroupControl
{
    Q_OBJECT

public:
    SceneGroup();
    virtual ~SceneGroup();
    void init(struct timeval *, struct timeval *);

    bool isArchiveSource();
    bool isActive(PmTime::Packet *);
    bool isRecording(PmTime::Packet *);

    void updateTimeAxis();
    void updateTimeButton();

    void setupWorldView();
    void step(PmTime::Packet *);
    void setTimezone(PmTime::Packet *, char *);

protected:
    void adjustLiveWorldViewForward(PmTime::Packet *);
    void adjustArchiveWorldViewForward(PmTime::Packet *, bool);
    void adjustArchiveWorldViewBackward(PmTime::Packet *, bool);

    void adjustStep(PmTime::Packet *);
    void setButtonState(TimeButton::State);

private:
    void refreshScenes(bool);

//  struct {
//  } my;
};

#endif	// SCENEGROUP_H
