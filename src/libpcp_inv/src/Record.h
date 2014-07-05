/* -*- C++ -*- */

#ifndef _INV_RECORD_H_
#define _INV_RECORD_H_

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
#include <X11/Intrinsic.h>
#include "Bool.h"
#include "String.h"
#include "List.h"
#include "pmapi.h"
#include "pmapi_mon.h"
#include "impl.h"
#include "impl_mon.h"

class VkCallbackObject;

class INV_Record;

struct INV_RecordHost {
public:

    OMC_String		_host;
    OMC_StrList		_metrics;
    OMC_StrList		_once;
    pmRecordHost	*_rhp;
    INV_Record		*_record;
    XtInputId		_id;

    ~INV_RecordHost();
    INV_RecordHost(const char *host);

    friend ostream& operator<<(ostream &os, const INV_RecordHost &host);

private:

    INV_RecordHost(const INV_RecordHost &);
    const INV_RecordHost &operator=(const INV_RecordHost &);
};

typedef OMC_List<INV_RecordHost *> INV_RecordList;

class INV_Record {
public:

    friend struct INV_RecordHost;

    typedef OMC_Bool	(*ToolConfigCB)(FILE *);
    typedef void	(*LogConfigCB)(INV_Record &);
    typedef void	(*StateCB)(void *);
    
private:

    OMC_Bool		_active;
    OMC_Bool		_modConfig;
    ToolConfigCB	_toolConfigCB;	// Does the tool need a config file?
    LogConfigCB		_logConfigCB;	// App can add metrics to log config
    StateCB		_stateCB;	// CB if all loggers have terminated
    void		*_stateData;	// State data to be passed back
    INV_RecordList	_list;
    uint_t		_lastHost;
    uint_t		_numActive;
    OMC_String		_allConfig;	// Use this file as the config
    OMC_String		_addConfig;	// Add this file to the config

public:

    ~INV_Record();

    INV_Record(ToolConfigCB toolCB = NULL,
	       OMC_Bool modConfig = OMC_true,
	       LogConfigCB logCB = NULL,
	       StateCB stateCB = NULL,
	       void *stateData = NULL,
	       const char *allConfig = NULL,
	       const char *addConfig = NULL);

    OMC_Bool active() const
	{ return _active; }
    uint_t numHosts() const
	{ return _list.length(); }

    void add(const char *host, const char *metric);
    void addOnce(const char *host, const char *metric);

    // Note delta is in milliseconds
    void changeState(uint_t delta = 10);

    int dumpConfig(FILE *fp, const INV_RecordHost &host, uint_t delta);
    int dumpFile(FILE *fp);

    // Use this config only
    void setRecordConfig(const char *config)
	{ _allConfig = config; }

    // Add this config to the metrics in the scene
    void setRecAddConfig(const char *config)
	{ _addConfig = config; }

    friend ostream &operator<<(ostream &os, const INV_Record &rhs);

private:

    static void logFailCB(XtPointer clientData, int *, XtInputId *id);

    int checkspecial(char* p);
};

#endif
