/*
 * Copyright (c) 1997,2004 Silicon Graphics, Inc.  All Rights Reserved.
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


#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <locale.h>
#include <Vk/VkApp.h>
#include <Vk/VkWarningDialog.h>
#include <Vk/VkErrorDialog.h>
#include <Vk/VkFatalErrorDialog.h>
#include <Inventor/Xt/SoXt.h>

#include "pmapi.h"
#include "impl.h"
#include "Inv.h"
#include "String.h"
#include "App.h"
#ifdef HAVE_IOSTREAM
#include <iostream>
using namespace std;
#else
#include <iostream.h>
#endif

//
// Globals
//

const uint_t		theBufferLen = 2048;
char			theBuffer[theBufferLen];
float			theScale = 1.0;
OMC_String		theAppName = "Undefined";
const OMC_String	theDefaultFlags = "A:a:CD:h:l:n:O:p:S:t:T:x:Z:z?";
OMC_Bool		theStderrFlag = OMC_false;
INV_App			*theApp = NULL;

int
INV_setup(const char *appname, int *argc, char **argv, 
	  XrmOptionDescRec *cmdopts, int numopts, INV_TermCB termCB)
{
    char	*env;
    int		dbg;
    int		i;

    pmProgname = basename(argv[0]);
    theAppName = appname;

    // Find if pmDebug should be set
    for (i = 1; i < *argc; i++) {
	if (strcmp(argv[i], "-D") == 0)
	    if (i < *argc - 1)
		env = argv[i+1];
	    else {
	    	pmprintf("%s: Error: -D requires an argument\n", pmProgname);
		return -1;
		/*NOTREACHED*/
	    }
	else if (strncmp(argv[i], "-D", 2) == 0)
	    env = argv[i] + 2;
	else
	    continue;

	dbg = __pmParseDebug(env);
	if (dbg < 0) {
	    pmprintf("%s: Error: Unrecognized debug flag specification \"%s\"\n",
		     pmProgname, env);
	    return -1;
	    /*NOTREACHED*/
	}
	
	pmDebug |= dbg;
    }

    // I18N
    setlocale(LC_ALL, "");
    XtSetLanguageProc(NULL, NULL, NULL);

    theApp = new INV_App((char *)appname, argc, argv, cmdopts, numopts, termCB);
    SoXt::init(theApp->baseWidget());

    env = getenv("PCP_STDERR");
    if (env == NULL) {
    	env = getenv("PCP_USE_STDERR");
	if (env == NULL)
	    theStderrFlag = OMC_false;
       	else
    	    theStderrFlag = OMC_true;
    }
    else if (strcmp(env, "DISPLAY") == 0)
	theStderrFlag = OMC_false;
    else {
	theStderrFlag = OMC_true;
    	if (strlen(env))
	    cerr << pmProgname 
	    	 << ": Warning: PCP_STDERR is set to a file" << endl
		 << pmProgname
	         << ": Error messages after initial setup will go to stderr"
		 << endl;
    }

    return 0;
}

int
INV_warningMsg(const char *file, int line, const char *msg, ...)
{
    int sts = VkDialogManager::OK;

    va_list arg;
    va_start(arg, msg);

    int pos = sprintf(theBuffer, "%s: Warning: ", pmProgname);
    pos += vsprintf(theBuffer+pos, msg, arg);
    sprintf(theBuffer+pos, "\n");

#ifdef PCP_DEBUG
    if (pmDebug && !theStderrFlag)
    	cerr << file << ":" << line << ": " << theBuffer << endl;
#endif

    if (theStderrFlag)
    	cerr << theBuffer;
    else
	sts = theWarningDialog->postAndWait(theBuffer, TRUE);
    va_end(arg);

    return sts;
}

int
INV_errorMsg(const char *file, int line, const char *msg, ...)
{
    int sts = VkDialogManager::OK;

    va_list arg;
    va_start(arg, msg);

    int pos = sprintf(theBuffer, "%s: Error: ", pmProgname);
    pos += vsprintf(theBuffer+pos, msg, arg);
    sprintf(theBuffer+pos, "\n");

#ifdef PCP_DEBUG
    if (pmDebug && !theStderrFlag)
    	cerr << file << ":" << line << ": " << theBuffer << endl;
#endif

    if (theStderrFlag)
    	cerr << theBuffer;
    else
	sts = theErrorDialog->postAndWait(theBuffer, TRUE);
    va_end(arg);

    return sts;
}

int
INV_fatalMsg(const char *file, int line, const char *msg, ...)
{
    int sts = VkDialogManager::OK;

    va_list arg;
    va_start(arg, msg);

    int pos = sprintf(theBuffer, "%s: Fatal: ", pmProgname);
    pos += vsprintf(theBuffer+pos, msg, arg);
    sprintf(theBuffer+pos, "\n");

#ifdef PCP_DEBUG
    if (pmDebug && !theStderrFlag)
    	cerr << file << ":" << line << ": " << theBuffer << endl;
#endif

    if (theStderrFlag) {
    	cerr << theBuffer;
	exit(1);
    }
    else
	sts = theFatalErrorDialog->postAndWait(theBuffer, TRUE);
    va_end(arg);

    return sts;
}
