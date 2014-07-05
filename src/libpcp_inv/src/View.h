/* -*- C++ -*- */

#ifndef _INV_VIEW_H_
#define _INV_VIEW_H_

/*
 * Copyright (c) 1995 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
 * 
 */


#include <stdio.h>
#include <unistd.h>

#include <Inventor/Xt/SoXt.h>
#include <Inventor/nodes/SoDrawStyle.h>

#include "pmapi.h"
#include "pmapi_mon.h"
#include "impl.h"
#include "impl_mon.h"
#include "Args.h"
#include "Bool.h"
#include "Record.h"
#include "Window.h"

class INV_ModList;
class SoEventCallback;
class SoXtViewer;

class INV_View : public INV_Window
{
 public:

    enum RenderOptions { 
	nothing = 0, fetch = 1, metrics = 2, inventor = 4, metricLabel = 8, 
	timeLabel = 16, all = 31
    };

    typedef int (*SetupCB)();

 private:

    int				_sts;
    int				_argc;
    char			**_argv;
    OMC_String			_argFlags;
    OMC_Bool			_sourceFlag;
    OMC_Bool			_hostFlag;

    OMC_String			_launchVersion;
    OMC_Bool			_checkConfigFlag;
    OMC_String			_pmnsFile;

    SoSeparator			*_root;
    SoDrawStyle			*_drawStyle;

    OMC_String			_text;
    OMC_String			_prevText;

    int				_timeMode;
    int				_timeFD;
    OMC_String			_timePort;
    pmTime			_timeState;
    OMC_String			_timeZone;
    struct timeval		_interval;

    INV_Record			_record;

public:

    ~INV_View();
	      
    INV_View(int argc, char **argv,
	     // getopt flags based on theDefaultFlags
	     const OMC_String &flags,
	     // generate tool specific config for pmafm launch
	     INV_Record::ToolConfigCB toolCB = NULL,
	     // generate additional pmlogger configs
	     INV_Record::LogConfigCB logCB = NULL,
	     // monitoring from more than one source
	     OMC_Bool sourceFlag = OMC_true,
	     // monitor from more than one live host
	     OMC_Bool hostFlag = OMC_false,
	     // generate additional pmlogger config from modualted objects
	     OMC_Bool modConfig = OMC_true);

    int status() const
	{ return _sts; }

    const OMC_String &pmnsFile()
	{ return _pmnsFile; }
    OMC_Bool checkConfigOnly() const
	{ return _checkConfigFlag; }

    SoSeparator* root()
	{ return _root; }
    SoXtViewer *viewer();

//
// Manage and parse command line args
//

    int parseArgs();
    int parseConfig(SetupCB appCB);

//
// Launching other tools
//

    int launch(const OMC_Args& args);

//
// Show and update the scene
//

    OMC_Bool view(OMC_Bool showAxis = OMC_false,
		  float xAxis = 0.0, float yAxis = 0.0, float zAxis = 0.0, 
		  float angle = 0.0, float scale = 0.0);

    void render(RenderOptions options, time_t);

//
// view changes when dragging
//

    void hideView()
	{ _drawStyle->style.setValue(SoDrawStyle::LINES); }
    void showView()
	{ _drawStyle->style.setValue(SoDrawStyle::FILLED); }

//
// pmtime state information
//

    pmTime& timeState()
	{ return _timeState; }
    int timeFD() const
	{ return _timeFD; }
    int timeConnect(OMC_Bool reconnect = OMC_false);
    
//
// notifier when selection has changed
//

    static void selectionCB(INV_ModList *, OMC_Bool);

//
// Recording
//

    void record();
    void setRecordConfig(const char *config)
	{ _record.setRecordConfig(config); }
    void setRecAddConfig(const char *config)
	{ _record.setRecAddConfig(config); }
    
private:

    void createTextArea(Widget parent);
    void attachWidgets();

    static void updateCB(void *, SoSensor *);
    static void timeCommandCB(VkCallbackObject *obj, void *clientData, 
			      void *callData);
    void changeDir();

    static void recordStateCB(void *);
};

#endif
