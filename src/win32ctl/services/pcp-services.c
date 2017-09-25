/*
 * Copyright (C) 2008-2009 Aconex.  All Rights Reserved.
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
#include "pmapi.h"
#include "impl.h"
#include <wtypes.h>
#include <winnt.h>
#include <winsvc.h>
#include <winuser.h>

typedef enum {
    PCP_SERVICE_COLLECTORS 		= 0,
    PCP_SERVICE_INFERENCE		= 1,
    PCP_SERVICE_PROXY			= 2,
    NUM_SERVICES
} PCPSERVICE;

VOID  WINAPI pcpCollectorsSetup(DWORD, LPTSTR *);
VOID  WINAPI pcpInferenceSetup(DWORD, LPTSTR *);
VOID  WINAPI pcpProxySetup(DWORD, LPTSTR *);
DWORD WINAPI pcpCollectorsDispatch(DWORD, DWORD, LPVOID, LPVOID);
DWORD WINAPI pcpInferenceDispatch(DWORD, DWORD, LPVOID, LPVOID);
DWORD WINAPI pcpProxyDispatch(DWORD, DWORD, LPVOID, LPVOID);

typedef VOID WINAPI (*SETUPFUNC)(DWORD, LPTSTR *);
typedef DWORD WINAPI (*DISPATCHFUNC)(DWORD, DWORD, LPVOID, LPVOID);

struct {
    TCHAR *			name;
    TCHAR *			script;
    HANDLE			stopEvent;
    SERVICE_STATUS		status;
    SERVICE_STATUS_HANDLE	statusHandle;
    SETUPFUNC			setup;
    DISPATCHFUNC		dispatch;
} services[3] = {
    {	.name		= "PCP Collector Processes",
	.script		= "pcp",
	.setup		= pcpCollectorsSetup,
	.dispatch	= pcpCollectorsDispatch,
    },
    {	.name		= "PCP Inference Engines",
	.script		= "pmie",
	.setup		= pcpInferenceSetup,
	.dispatch	= pcpInferenceDispatch,
    },
    {	.name		= "PCP Collector Proxy",
	.script		= "pmproxy",
	.setup		= pcpProxySetup,
	.dispatch	= pcpProxyDispatch,
    },
};

static char pcpdir[MAXPATHLEN+16];	/* PCP_DIR environment variable */
static char pcpconf[MAXPATHLEN+16];	/* PCP_CONF string for putenv */
static char pcpdirenv[MAXPATHLEN+16];	/* PCP_DIR string for putenv */

int
pcpScript(const char *name, const char *action)
{
    char s[MAXPATHLEN];
    pmsprintf(s, sizeof(s), "%s\\bin\\sh.exe /etc/%s %s", pcpdir, name, action);
    return system(s);
}

VOID
pcpSetServiceState(PCPSERVICE s, DWORD state, DWORD code, DWORD waitHint)
{
    services[s].status.dwServiceType = SERVICE_WIN32_SHARE_PROCESS;
    services[s].status.dwCurrentState = state;
    services[s].status.dwWin32ExitCode = code;
    services[s].status.dwWaitHint = waitHint;

    if (state == SERVICE_START_PENDING)
	services[s].status.dwControlsAccepted = 0;
    else
	services[s].status.dwControlsAccepted = SERVICE_ACCEPT_STOP;

    if ((state == SERVICE_RUNNING) || (state == SERVICE_STOPPED))
	services[s].status.dwCheckPoint = 0;
    else
	services[s].status.dwCheckPoint++;

    /* Report the status of the service to the SCM. */
    SetServiceStatus(services[s].statusHandle, &services[s].status);
}

VOID
pcpServiceMain(DWORD argc, LPTSTR *argv, PCPSERVICE s)
{
    char *default_basedirs[] = { "C:\\Glider", "C:\\MSYS" };
    char *default_service = "pcp";
    char *service = NULL, *basedir = NULL;
    int i;

    if (argc > 1) {	/* first argument is service name */
	service = argv[1];
	for (i = 0; i < NUM_SERVICES; i++)
	    if (strcmp(service, services[i].name) == 0)
		break;
	if (i == NUM_SERVICES)
	    return;	/* unknown service requested - bail out */
    }
    if (service == NULL)
	service = default_service;

    if (argc > 2)	/* second argument is PCP_DIR */
	basedir = argv[2];
    else if ((basedir = getenv("PCP_DIR")) == NULL) {
	for (i = 0; i < sizeof(default_basedirs)/sizeof(default_basedirs[0]); i++)
	    if (access(default_basedirs[i], R_OK) == 0) {
		basedir = default_basedirs[i];
		break;
	    }
    }
    if (!basedir || access(basedir, R_OK) != 0)
	return;	/* stuffed up if we have no PCP_DIR - bail out */

    pmsprintf(pcpdir, sizeof(pcpdir), "%s", basedir);
    pmsprintf(pcpconf, sizeof(pcpconf), "PCP_CONF=%s\\etc\\pcp.conf", pcpdir);
    pmsprintf(pcpdirenv, sizeof(pcpdirenv), "PCP_DIR=%s", pcpdir);
    putenv(pcpconf);
    putenv(pcpdirenv);

    services[s].statusHandle = RegisterServiceCtrlHandlerEx(
			services[s].name, services[s].dispatch, NULL);
    if (!services[s].statusHandle)
	return;

    pcpSetServiceState(s, SERVICE_START_PENDING, NO_ERROR, 0);

    /*
     * Create an event. The control handler function signals
     * this event when it receives the stop control code.
     */
    services[s].stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!services[s].stopEvent) {
	pcpSetServiceState(s, SERVICE_STOPPED, NO_ERROR, 0);
	return;
    }

    /*
     * Go, go, go... run the start script for this service.
     */
    if (pcpScript(services[s].script, "start") != 0) {
	pcpSetServiceState(s, SERVICE_STOPPED, NO_ERROR, 0);
	return;
    }

    pcpSetServiceState(s, SERVICE_RUNNING, NO_ERROR, 0);

    /* Check whether to stop the service. */
    WaitForSingleObject(services[s].stopEvent, INFINITE);

    /* ServiceState already set to SERVICE_STOP_PENDING */
    pcpScript(services[s].script, "stop");

    pcpSetServiceState(s, SERVICE_STOPPED, NO_ERROR, 0);
}

VOID WINAPI
pcpCollectorsSetup(DWORD argc, LPTSTR *argv)
{
    pcpServiceMain(argc, argv, PCP_SERVICE_COLLECTORS);
}

VOID WINAPI
pcpInferenceSetup(DWORD argc, LPTSTR *argv)
{
    pcpServiceMain(argc, argv, PCP_SERVICE_INFERENCE);
}

VOID WINAPI
pcpProxySetup(DWORD argc, LPTSTR *argv)
{
    pcpServiceMain(argc, argv, PCP_SERVICE_PROXY);
}

DWORD
pcpServiceHandler(DWORD dwControl, DWORD dwEventType,
		 LPVOID lpEventData, LPVOID lpContext, PCPSERVICE s)
{
    switch(dwControl) {
    case SERVICE_CONTROL_STOP:
	services[s].status.dwCurrentState = SERVICE_STOP_PENDING;
	SetServiceStatus(services[s].statusHandle, &services[s].status);
	SetEvent(services[s].stopEvent);
	return NO_ERROR;

    default:
	break;
    }

    /* Send current status (done for most request types) */
    SetServiceStatus(services[s].statusHandle, &services[s].status);
    return NO_ERROR;
}

DWORD WINAPI
pcpCollectorsDispatch(DWORD ctrl, DWORD type, LPVOID data, LPVOID ctxt)
{
    return pcpServiceHandler(ctrl, type, data, ctxt, PCP_SERVICE_COLLECTORS);
}

DWORD WINAPI
pcpInferenceDispatch(DWORD ctrl, DWORD type, LPVOID data, LPVOID ctxt)
{
    return pcpServiceHandler(ctrl, type, data, ctxt, PCP_SERVICE_INFERENCE);
}

DWORD WINAPI
pcpProxyDispatch(DWORD ctrl, DWORD type, LPVOID data, LPVOID ctxt)
{
    return pcpServiceHandler(ctrl, type, data, ctxt, PCP_SERVICE_PROXY);
}

int
main(int argc, char **argv)
{
    SERVICE_TABLE_ENTRY dispatchTable[2];

    __pmSetProgname(argv[0]);

    /* setup dispatch table and sentinel */
    dispatchTable[0].lpServiceName = services[0].name;
    dispatchTable[0].lpServiceProc = services[0].setup;
    dispatchTable[1].lpServiceName = NULL;
    dispatchTable[1].lpServiceProc = NULL;

    if (!StartServiceCtrlDispatcher(dispatchTable)) {
	DWORD c = GetLastError();
	fprintf(stderr, "%s: cannot dispatch services (%ld)\n", pmProgname, c);
	if (c == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT)
	    fprintf(stderr, "%s: run as service, not on console\n", pmProgname);
	return 1;
    }
    return 0;
}
