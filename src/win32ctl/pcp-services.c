/*
 * Copyright (C) 2008 Aconex.  All Rights Reserved.
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
#include <getopt.h>
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
    int				isManual;
    HANDLE			stopEvent;
    SERVICE_STATUS		status;
    SERVICE_STATUS_HANDLE	statusHandle;
    SETUPFUNC			setup;
    DISPATCHFUNC		dispatch;
    TCHAR *			description;
} services[3] = {
    {	.name		= "PCP Collector Processes",
	.script		= "pcp",
	.isManual	= 0,
	.setup		= pcpCollectorsSetup,
	.dispatch	= pcpCollectorsDispatch,
	.description	= \
"Provides metric collection and logging services for Performance Co-Pilot."
    },
    {	.name		= "PCP Inference Engines",
	.script		= "pmie",
	.isManual	= 1,
	.setup		= pcpInferenceSetup,
	.dispatch	= pcpInferenceDispatch,
	.description	= \
"Provides performance rule evaluation services for Performance Co-Pilot."
    },
    {	.name		= "PCP Collector Proxy",
	.script		= "pmproxy",
	.isManual	= 1,
	.setup		= pcpProxySetup,
	.dispatch	= pcpProxyDispatch,
	.description	= \
"Provides proxied access to Performance Co-Pilot collector processes."
    },
};

char *
statusString(DWORD state)
{
    static char buffer[32];

    switch (state) {
    case SERVICE_CONTINUE_PENDING:
	return "Continue Pending";
    case SERVICE_PAUSE_PENDING:
	return "Pause Pending";
    case SERVICE_PAUSED:
	return "Paused";
    case SERVICE_RUNNING:
	return "Running";
    case SERVICE_START_PENDING:
	return "Start Pending";
    case SERVICE_STOP_PENDING:
	return "Stop Pending";
    case SERVICE_STOPPED:
	return "Stopped";
    }
    snprintf(buffer, sizeof(buffer), "Unknown(%lu)", state);
    return buffer;
}

int
pcpQueryScript(char *name)
{
    char cmd[MAXPATHLEN];
    snprintf(cmd, sizeof(cmd), "sh %s/etc/%s status", getenv("PCP_DIR"), name);
    return system(cmd);
}

int
pcpQueryService(char *name)
{
    SERVICE_STATUS status;
    SC_HANDLE manager;
    SC_HANDLE service;
    int c;

    manager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!manager) {
	fprintf(stderr, "%s: cannot open service manager (%ld)\n",
			pmProgname, GetLastError());
	return 1;
    }

    for (c = 0; c < NUM_SERVICES; c++) {
	if (strcmp(services[c].name, name) == 0 ||
	    strcmp(services[c].script, name) == 0)
	    break;
    }
    if (c == NUM_SERVICES) {
	fprintf(stderr, "%s: cannot find service \"%s\"\n", pmProgname, name);
	return 1;
    }

    service = OpenService(manager, services[c].name, SERVICE_QUERY_STATUS);
    if (!service)
	fprintf(stderr, "%s: OpenService failed on \"%s\" (%ld)\n",
			pmProgname, services[c].name, GetLastError());
    if (!QueryServiceStatus(service, &status))
	fprintf(stderr, "%s: QueryServiceStatus failed for \"%s\" (%ld)\n",
			pmProgname, services[c].name, GetLastError());
    else
	printf("Service status: %s\n", statusString(status.dwCurrentState));
    if (service)
	CloseServiceHandle(service);
    CloseServiceHandle(manager);

    return pcpQueryScript(services[c].script);
}

int
pcpStartScript(char *name)
{
    char cmd[MAXPATHLEN];
    snprintf(cmd, sizeof(cmd), "sh %s/etc/%s start", getenv("PCP_DIR"), name);
    return system(cmd);
}

int
pcpStartService(char *name)
{
    SC_HANDLE manager;
    SC_HANDLE service;
    int c;

    manager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!manager) {
	fprintf(stderr, "%s: cannot open service manager (%ld)\n",
			pmProgname, GetLastError());
	return 1;
    }

    for (c = 0; c < NUM_SERVICES; c++) {
	if (strcmp(services[c].name, name) == 0 ||
	    strcmp(services[c].script, name) == 0)
	    break;
    }
    if (c == NUM_SERVICES) {
	fprintf(stderr, "%s: cannot find service \"%s\"\n", pmProgname, name);
	return 1;
    }

    service = OpenService(manager, services[c].name, SERVICE_ALL_ACCESS);
    if (!service)
	fprintf(stderr, "%s: OpenService failed on \"%s\" (%ld)\n",
			pmProgname, services[c].name, GetLastError());
    if (!StartService(service, 0, NULL)) {
	fprintf(stderr, "%s: QueryServiceStatus failed for \"%s\" (%ld)\n",
			pmProgname, services[c].name, GetLastError());
	CloseServiceHandle(service);
    }
    CloseServiceHandle(manager);
    return 0;
}

int
pcpStopScript(char *name)
{
    char cmd[MAXPATHLEN];
    snprintf(cmd, sizeof(cmd), "sh %s/etc/%s stop", getenv("PCP_DIR"), name);
    return system(cmd);
}

int
pcpStopService(char *name)
{
    SERVICE_STATUS ss;
    SC_HANDLE manager;
    SC_HANDLE service;
    int c;

    manager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!manager) {
	fprintf(stderr, "%s: cannot open service manager (%ld)\n",
			pmProgname, GetLastError());
	return 1;
    }

    for (c = 0; c < NUM_SERVICES; c++) {
	if (strcmp(services[c].name, name) == 0 ||
	    strcmp(services[c].script, name) == 0)
	    break;
    }
    if (c == NUM_SERVICES) {
	fprintf(stderr, "%s: cannot find service \"%s\"\n", pmProgname, name);
	return 1;
    }

    service = OpenService(manager, services[c].name, SERVICE_ALL_ACCESS);
    if (!service)
	fprintf(stderr, "%s: OpenService failed on \"%s\" (%ld)\n",
			pmProgname, services[c].name, GetLastError());
    if (!ControlService(service, SERVICE_CONTROL_STOP, &ss)) {
	fprintf(stderr, "%s: ControlService (stop) failed for \"%s\" (%ld)\n",
			pmProgname, services[c].name, GetLastError());
	CloseServiceHandle(service);
    }
    CloseServiceHandle(manager);
    return 0;
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

int
pcpInstallServices(void)
{
    SC_HANDLE manager;
    SC_HANDLE service;
    TCHAR path[MAX_PATH];
    int c;

    if (!GetModuleFileName(NULL, path, MAX_PATH)) {
	fprintf(stderr, "%s: cannot install service (%ld)\n",
			pmProgname, GetLastError());
	return 1;
    }

    manager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!manager) {
	fprintf(stderr, "%s: cannot open service manager (%ld)\n",
			pmProgname, GetLastError());
	return 1;
    }

    for (c = 0; c < NUM_SERVICES; c++) {
	service = CreateService(manager,
				services[c].name,
				services[c].name,
				SERVICE_ALL_ACCESS,
				SERVICE_WIN32_SHARE_PROCESS,
				services[c].isManual ?
				SERVICE_DEMAND_START : SERVICE_AUTO_START,
				SERVICE_ERROR_NORMAL,
				path, NULL, NULL, NULL, NULL, NULL);
	if (!service)
	    fprintf(stderr, "%s: CreateService failed for \"%s\" (%ld)\n",
			pmProgname, services[c].name, GetLastError());
	else {
	    SERVICE_DESCRIPTION sd;
	    sd.lpDescription = services[c].description;
	    ChangeServiceConfig2(service, SERVICE_CONFIG_DESCRIPTION, &sd);
	    CloseServiceHandle(service);
	}
    }

    CloseServiceHandle(manager);
    return 0;
}

int
pcpRemoveServices(void)
{
    SC_HANDLE manager;
    SC_HANDLE service;
    int c;

    for (c = 0; c < NUM_SERVICES; c++) {
	pcpSetServiceState(c, SERVICE_STOP_PENDING, NO_ERROR, 0);
	pcpStopService(services[c].script);
    }

    manager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!manager) {
	fprintf(stderr, "%s: cannot open service manager (%ld)\n",
			pmProgname, GetLastError());
	return 1;
    }

    for (c = 0; c < NUM_SERVICES; c++) {
	service = OpenService(manager, services[c].name, SERVICE_ALL_ACCESS);
	if (!service)
	    fprintf(stderr, "%s: OpenService failed on \"%s\" (%ld)\n",
			pmProgname, services[c].name, GetLastError());
	else if (!DeleteService(service))
	    fprintf(stderr, "%s: DeleteService failed on \"%s\" (%ld)\n",
			pmProgname, services[c].name, GetLastError());
	else
	    CloseServiceHandle(service);
    }

    CloseServiceHandle(manager);
    return 0;
}

VOID
pcpServiceMain(DWORD argc, LPTSTR *argv, PCPSERVICE s)
{
    services[s].statusHandle = RegisterServiceCtrlHandlerEx(
			services[s].name, services[s].dispatch, NULL);
    if (!services[s].statusHandle) {
	fprintf(stderr, "%s: RegisterServiceCtrlHandlerEx() failed"
			" for \"%s\" %ld\n",
		pmProgname, services[s].name, GetLastError());
	return;
    }

    pcpSetServiceState(s, SERVICE_START_PENDING, NO_ERROR, 0);

    /*
     * Create an event. The control handler function
     * signals this event when it receives the stop
     * control code.
     */
    services[s].stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!services[s].stopEvent) {
	pcpSetServiceState(s, SERVICE_STOPPED, NO_ERROR, 0);
	return;
    }

    /*
     * Go, go, go... run the start script for this service.
     */
    if (pcpStartScript(services[s].script) != 0) {
	pcpSetServiceState(s, SERVICE_STOPPED, NO_ERROR, 0);
	return;
    }

    pcpSetServiceState(s, SERVICE_RUNNING, NO_ERROR, 0);

    /* Check whether to stop the service. */
    WaitForSingleObject(services[s].stopEvent, INFINITE);

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
    fprintf(stderr, "%s: in to %s service control... (%ld)\n",
			pmProgname, services[s].name, dwControl);

    switch(dwControl) {
    case SERVICE_CONTROL_PAUSE:
	services[s].status.dwCurrentState = SERVICE_PAUSED;
	break;

    case SERVICE_CONTROL_CONTINUE:
	services[s].status.dwCurrentState = SERVICE_RUNNING;
	break;

    case SERVICE_CONTROL_STOP:
	services[s].status.dwCurrentState = SERVICE_STOP_PENDING;
	SetServiceStatus(services[s].statusHandle, &services[s].status);
	SetEvent(services[s].stopEvent);
	return NO_ERROR;

    case SERVICE_CONTROL_INTERROGATE:
	break;

    default:
	fprintf(stderr, "%s: unrecognised control code=%ld on \"%s\"\n",
			pmProgname, dwControl, services[s].name);
    }

    /* Send current status (done for most request types) */
    if (!SetServiceStatus(services[s].statusHandle, &services[s].status)) {
	fprintf(stderr, "%s: SetServiceStatus on %s failed (%ld)\n",
			pmProgname, services[s].name, GetLastError());
    }
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
    int error = 0, c;
    SERVICE_TABLE_ENTRY dispatchTable[NUM_SERVICES+1];

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "iurq:s:S:?")) != EOF) {
	switch (c) {
	case 'i':
	    return pcpInstallServices();
	case 'u':
	case 'r':
	    return pcpRemoveServices();
	case 'q':
	    return pcpQueryService(optarg);
	case 's':
	    return pcpStartService(optarg);
	case 'S':
	    return pcpStopService(optarg);
	default:
	    error++;
	}
    }

    if (error) {
	fprintf(stderr, "Usage: %s [ options ]\n\n"
			"Options:\n"
			"  -i		install PCP services\n"
			"  -u (or -r)	uninstall PCP services\n"
			"  -q service	query status (pcp/pmie/pmproxy)\n"
			"  -s service	start (pcp/pmie/pmproxy)\n"
			"  -S service	stop (pcp/pmie/pmproxy)\n",
		pmProgname);
	return 2;
    }

    /* setup dispatch table and sentinal */
    for (c = 0; c < NUM_SERVICES; c++) {
	dispatchTable[c].lpServiceName = services[c].name;
	dispatchTable[c].lpServiceProc = services[c].setup;
    }
    dispatchTable[c].lpServiceName = NULL;
    dispatchTable[c].lpServiceProc = NULL;

    if (!StartServiceCtrlDispatcher(dispatchTable)) {
	fprintf(stderr, "%s: cannot dispatch services (%ld)\n",
			pmProgname, GetLastError());
	return 1;
    }
    return 0;
}
