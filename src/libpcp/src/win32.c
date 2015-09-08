/*
 * Copyright (c) 2013-2015 Red Hat.
 * Copyright (c) 2008-2010 Aconex.  All Rights Reserved.
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
 */

/*
 * For the MinGW headers and library to work correctly, we need
 * something newer than the default Windows 95 versions of the Win32
 * APIs - we select Windows7 below as that provides minimum version
 * with modern IPv6, as well as symlink support.
 *
 * WINVER needs to be set before any of the MinGW headers are processed
 * and we include <windows.h> from pmapi.h via platform_defs.h.
 *
 * Thanks to "Earnie" on the mingw-users@lists.sourceforge.net mailing
 * list for this tip.
 */
#define WINVER Windows7
#define _WIN32_WINNT _WIN32_WINNT_WIN7

#include "pmapi.h"
#include "impl.h"
#include <winbase.h>
#include <psapi.h>

#define FILETIME_1970		116444736000000000ull	/* 1/1/1601-1/1/1970 */
#define HECTONANOSEC_PER_SEC	10000000ull
#define MILLISEC_PER_SEC	1000
#define NANOSEC_PER_MILLISEC	1000000ull
#define NANOSEC_BOUND		(1000000000ull - 1)
#define MAX_SIGNALS		3	/* HUP, USR1, TERM */

static struct {
    int			signal;
    HANDLE		eventhandle;
    HANDLE		waithandle;
    __pmSignalHandler	callback;
} signals[MAX_SIGNALS];

VOID CALLBACK
SignalCallback(PVOID param, BOOLEAN timerorwait)
{
    int index = (int)param;

    if (index >= 0 && index < MAX_SIGNALS)
	signals[index].callback(signals[index].signal);
    else
	fprintf(stderr, "SignalCallback: bad signal index (%d)\n", index);
}

static char *
MapSignals(int sig, int *index)
{
    static char name[8];

    switch (sig) {
    case SIGHUP:
	*index = 0;
	strcpy(name, "SIGHUP");
	break;
    case SIGUSR1:
	*index = 1;
	strcpy(name, "SIGUSR1");
	break;
    case SIGTERM:
	*index = 2;
	strcpy(name, "SIGTERM");
	break;
    default:
	return NULL;
    }
    return name;
}

int
__pmSetSignalHandler(int sig, __pmSignalHandler func)
{
    int sts, index = 0;
    char *signame, evname[64];
    HANDLE eventhdl, waithdl;

    if ((signame = MapSignals(sig, &index)) == NULL)
	return index;

    if (signals[index].callback) {	/* remove old handler */
	UnregisterWait(signals[index].waithandle);
	CloseHandle(signals[index].eventhandle);
	signals[index].callback = NULL;
	signals[index].signal = -1;
    }

    if (func == SIG_IGN)
	return 0;

    sts = 0;
    snprintf(evname, sizeof(evname), "PCP/%" FMT_PID "/%s", getpid(), signame);
    if (!(eventhdl = CreateEvent(NULL, FALSE, FALSE, TEXT(evname)))) {
	sts = GetLastError();
	fprintf(stderr, "CreateEvent::%s failed (%d)\n", signame, sts);
    }
    else if (!RegisterWaitForSingleObject(&waithdl, eventhdl,
		SignalCallback, (PVOID)index, INFINITE, 0)) {
	sts = GetLastError();
	fprintf(stderr, "RegisterWait::%s failed (%d)\n", signame, sts);
    }
    else {
	signals[index].eventhandle = eventhdl;
	signals[index].waithandle = waithdl;
	signals[index].callback = func;
	signals[index].signal = sig;
    }
    return sts;
}

static void
sigterm_callback(int sig)
{
    exit(0);	/* give atexit(3) handlers a look-in */
}

int
__pmSetProcessIdentity(const char *username)
{
    (void)username;
    return 0;	/* Not Yet Implemented */
}

int
__pmSetProgname(const char *program)
{
    int	sts1, sts2;
    char *p, *suffix = NULL;
    WORD wVersionRequested = MAKEWORD(2, 2);
    WSADATA wsaData;

    /* Trim command name of leading directory components and ".exe" suffix */
    if (program)
	pmProgname = (char *)program;
    for (p = pmProgname; pmProgname && *p; p++) {
	if (*p == '\\' || *p == '/')
	    pmProgname = p + 1;
	if (*p == '.')
	    suffix = p;
    }
    if (suffix && strcmp(suffix, ".exe") == 0)
	*suffix = '\0';

    /* Deal with all files in binary mode - no EOL futzing */
    _fmode = O_BINARY;

    /*
     * If Windows networking is not setup, all networking calls fail;
     * this even includes gethostname(2), if you can believe that. :[
     */
    sts1 = WSAStartup(wVersionRequested, &wsaData);

    /*
     * Here we are emulating POSIX signals using Event objects.
     * For all processes we want a SIGTERM handler, which allows
     * us an opportunity to cleanly shutdown: atexit(1) handlers
     * get a look-in, IOW.  Other signals (HUP/USR1) are handled
     * in a similar way, but only by processes that need them.
     */
    sts2 = __pmSetSignalHandler(SIGTERM, sigterm_callback);

    return sts1 | sts2;
}

void *
__pmMemoryMap(int fd, size_t sz, int writable)
{
    void *addr = NULL;
    int cflags = writable ? PAGE_READWRITE : PAGE_READONLY;

    HANDLE handle = CreateFileMapping((HANDLE)_get_osfhandle(fd),
					NULL, cflags, 0, sz, NULL);
    if (handle != NULL) {
	int mflags = writable ? FILE_MAP_ALL_ACCESS : FILE_MAP_READ;
	addr = MapViewOfFile(handle, mflags, 0, 0, sz);
	CloseHandle(handle);
	if (addr == MAP_FAILED)
	    return NULL;
    }
    return addr;
}

void
__pmMemoryUnmap(void *addr, size_t sz)
{
    (void)sz;
    UnmapViewOfFile(addr);
}

int
__pmProcessExists(pid_t pid)
{
    HANDLE ph = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (ph == NULL)
	return 0;
    CloseHandle(ph);
    return 1;
}

int
__pmProcessTerminate(pid_t pid, int force)
{
    HANDLE ph = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (ph != NULL) {
	TerminateProcess(ph, 0);
	CloseHandle(ph);
	return 0;
    }
    return -ESRCH;
}

pid_t
__pmProcessCreate(char **argv, int *infd, int *outfd)
{
    HANDLE hChildStdinRd, hChildStdinWr, hChildStdoutRd, hChildStdoutWr;
    PROCESS_INFORMATION piProcInfo; 
    SECURITY_ATTRIBUTES saAttr; 
    STARTUPINFO siStartInfo;
    LPTSTR cmdline = NULL;
    char *command;
    int i, sz = 0;
 
    ZeroMemory(&saAttr, sizeof(SECURITY_ATTRIBUTES));
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
    saAttr.bInheritHandle = TRUE;	/* pipe handles are inherited. */
    saAttr.lpSecurityDescriptor = NULL; 

    /*
     * Create a pipe for communication with the child process.
     * Ensure that the read handle to the child process's pipe for
     * STDOUT is not inherited.
     */
    if (!CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &saAttr, 0))
	return -1;
    SetHandleInformation(hChildStdoutRd, HANDLE_FLAG_INHERIT, 0);

    /*
     * Create a pipe for the child process's STDIN.
     * Ensure that the write handle to the child process's pipe for
     * STDIN is not inherited.
     */
    if (!CreatePipe(&hChildStdinRd, &hChildStdinWr, &saAttr, 0)) {
	CloseHandle(hChildStdoutRd);
	CloseHandle(hChildStdoutWr);
	return -1;
    }
    SetHandleInformation(hChildStdinWr, HANDLE_FLAG_INHERIT, 0);

    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
    siStartInfo.cb = sizeof(STARTUPINFO); 
    siStartInfo.hStdOutput = hChildStdoutWr;
    siStartInfo.hStdInput = hChildStdinRd;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    /* Flatten the argv array for the Windows CreateProcess API */
 
    for (command = argv[0], i = 0; command && *command; command = argv[++i]) {
	int length = strlen(command);
	cmdline = realloc(cmdline, sz + length + 1); /* 1space or 1null */
	strcpy(&cmdline[sz], command);
	cmdline[sz + length] = ' ';
	sz += length + 1;
    }
    cmdline[sz - 1] = '\0';

    if (0 == CreateProcess(NULL, 
	cmdline,       /* command line */
	NULL,          /* process security attributes */
	NULL,          /* primary thread security attributes */
	TRUE,          /* handles are inherited */
	0,             /* creation flags */
	NULL,          /* use parent's environment */
	NULL,          /* use parent's current directory */
	&siStartInfo,  /* STARTUPINFO pointer */
	&piProcInfo))  /* receives PROCESS_INFORMATION */
    {
	CloseHandle(hChildStdinRd);
	CloseHandle(hChildStdinWr);
	CloseHandle(hChildStdoutRd);
	CloseHandle(hChildStdoutWr);
	return -1;
    }
    else {
	CloseHandle(piProcInfo.hProcess);
	CloseHandle(piProcInfo.hThread);
    }

    *infd = _open_osfhandle((intptr_t)hChildStdoutRd, _O_RDONLY);
    *outfd = _open_osfhandle((intptr_t)hChildStdinWr, _O_WRONLY);
    return piProcInfo.dwProcessId;
}

pid_t
__pmProcessWait(pid_t pid, int nowait, int *code, int *signal)
{
    HANDLE ph;
    DWORD status;

    if (pid == (pid_t)-1 || pid == (pid_t)-2)
	return -1;
    if ((ph = OpenProcess(SYNCHRONIZE, FALSE, pid)) == NULL)
	return -1;
    if (WaitForSingleObject(ph, (DWORD)(-1L)) == WAIT_FAILED) {
	CloseHandle(ph);
	return -1;
    }
    if (GetExitCodeProcess(ph, &status)) {
	CloseHandle(ph);
	return -1;
    }
    if (code)
	*code = status;
    CloseHandle(ph);
    *signal = -1;
    return pid;
}

int
__pmProcessDataSize(unsigned long *datasize)
{
    PROCESS_MEMORY_COUNTERS pmc;
    HANDLE ph;
    int sts = -1;

    if (!datasize)
	return 0;
    *datasize = 0UL;
    ph = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, GetCurrentProcessId());
    if (ph == NULL)
	return sts;
    else if (GetProcessMemoryInfo(ph, &pmc, sizeof(pmc))) {
	*datasize = pmc.WorkingSetSize / 1024;
	sts = 0;
    }
    CloseHandle(ph);
    return sts;
}

int
__pmProcessRunTimes(double *usr, double *sys)
{
    ULARGE_INTEGER ul;
    FILETIME times[4];
    HANDLE ph;
    int sts = -1;

    *usr = *sys = 0.0;
    ph = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, GetCurrentProcessId());
    if (ph == NULL)
	return sts;
    else if (GetProcessTimes(ph, &times[0], &times[1], &times[2], &times[3])) {
	ul.LowPart = times[2].dwLowDateTime;
	ul.HighPart = times[2].dwHighDateTime;
	*sys = ul.QuadPart / 10000000.0;
	ul.LowPart = times[3].dwLowDateTime;
	ul.HighPart = times[3].dwHighDateTime;
	*usr = ul.QuadPart / 10000000.0;
	sts = 0;
    }
    CloseHandle(ph);
    return sts;
}

void
__pmDumpStack(FILE *f)
{
   /* TODO: StackWalk64 API */
}

void
__pmtimevalNow(struct timeval *tv)
{
    struct timespec ts;
    union {
	unsigned long long ns100; /*time since 1 Jan 1601 in 100ns units */
	FILETIME ft;
    } now;

    GetSystemTimeAsFileTime(&now.ft);
    now.ns100 -= FILETIME_1970;
    ts.tv_sec = now.ns100 / HECTONANOSEC_PER_SEC;
    ts.tv_nsec = (long)(now.ns100 % HECTONANOSEC_PER_SEC) * 100;
    tv->tv_sec = ts.tv_sec;
    tv->tv_usec = (ts.tv_nsec / 1000);
}

int
nanosleep(const struct timespec *req, struct timespec *rem)
{
    DWORD milliseconds;

    if (req->tv_sec < 0 || req->tv_nsec < 0 || req->tv_nsec > NANOSEC_BOUND) {
	SetLastError(EINVAL);
	return -1;
    }
    milliseconds = req->tv_sec * MILLISEC_PER_SEC
			+ req->tv_nsec / NANOSEC_PER_MILLISEC;
    SleepEx(milliseconds, TRUE);
    if (rem)
	memset(rem, 0, sizeof(*rem));
    return 0;
}

unsigned int
sleep(unsigned int seconds)
{
    SleepEx(seconds * 1000, TRUE);
    return 0;
}

void
setlinebuf(FILE *stream)
{
    setvbuf(stream, NULL, _IONBF, 0);	/* no line buffering in Win32 */
}

int
fsync(int fd)
{
    return FlushFileBuffers((HANDLE)_get_osfhandle(fd)) ? 0 : -1;
}

int
symlink(const char *oldpath, const char *newpath)
{
    return CreateSymbolicLink(newpath, oldpath, 0) ? 0 : -1;
}

int
readlink(const char *path, char *buf, size_t bufsiz)
{
    return -ENOTSUP;	/* NYI */
}

long int
random(void)
{
    return rand();
}

void
srandom(unsigned int seed)
{
    srandom(seed);
}

long int
lrand48(void)
{
    return rand();
}

void
srand48(long int seed)
{
    srand(seed);
}

char *
index(const char *string, int marker)
{
    char *p;
    for (p = (char *)string; *p != '\0'; p++)
	if (*p == marker)
	    return p;
    return NULL;
}

char *
rindex(const char *string, int marker)
{
    char *p;
    for (p = (char *)string; *p != '\0'; p++)
	;
    if (p == string)
	return NULL;
    for (--p; p != string; p--)
	if (*p == marker)
	    return p;
    return NULL;
}

char *
strcasestr(const char *string, const char *substr)
{
    int i, j;
    int sublen = strlen(substr);
    int length = strlen(string) - sublen + 1;

    for (i = 0; i < length; i++) {
	for (j = 0; j < sublen; j++)
	    if (toupper(string[i+j]) != toupper(substr[j]))
		goto outerloop;
	return (char *) substr + i;
    outerloop:
	continue;
    }
    return NULL;
}

void *
dlopen(const char *filename, int flag)
{
    return LoadLibrary(filename);
}

void *
dlsym(void *handle, const char *symbol)
{
    return GetProcAddress(handle, symbol);
}

int
dlclose(void *handle)
{
    return FreeLibrary(handle);
}

char *
dlerror(void)
{
    return strerror(GetLastError());
}

static HANDLE eventlog;
static char *eventlogPrefix;

void
openlog(const char *ident, int option, int facility)
{
    if (eventlog)
	closelog();
    eventlog = RegisterEventSource(NULL, "Application");
    if (ident)
	eventlogPrefix = strdup(ident);
}

void
syslog(int priority, const char *format, ...)
{
    va_list	arg;
    LPCSTR	msgptr;
    char	logmsg[2048];
    char	*p = logmsg;
    int		offset = 0;
    DWORD	eventlogPriority;

    va_start(arg, format);

    if (!eventlog)
	openlog(NULL, 0, 0);

    if (eventlogPrefix)
	offset = snprintf(p, sizeof(logmsg), "%s: ", eventlogPrefix);

    switch (priority) {
    case LOG_EMERG:
    case LOG_CRIT:
    case LOG_ERR:
	eventlogPriority = EVENTLOG_ERROR_TYPE;
	break;
    case LOG_WARNING:
    case LOG_ALERT:
	eventlogPriority = EVENTLOG_WARNING_TYPE;
	break;
    case LOG_NOTICE:
    case LOG_DEBUG:
    case LOG_INFO:
    default:
	eventlogPriority = EVENTLOG_INFORMATION_TYPE;
	break;
    }
    msgptr = logmsg;
    snprintf(p + offset, sizeof(logmsg) - offset, format, arg);
    ReportEvent(eventlog, eventlogPriority, 0, 0, NULL, 1, 0, &msgptr, NULL);
    va_end(arg);
}

void
closelog(void)
{
    if (eventlog) {
	DeregisterEventSource(eventlog);
	if (eventlogPrefix)
	    free(eventlogPrefix);
	eventlogPrefix = NULL;
    }
    eventlog = NULL;
}

const char *
strerror_r(int errnum, char *buf, size_t buflen)
{
    strerror_s(buf, buflen, errnum);
    return (const char *)buf;
}

/* Windows socket error codes - what a nightmare! */
static const struct {
    int  	err;
    char	*errmess;
} wsatab[] = {
/*10004*/ { WSAEINTR, "Interrupted function call" },
/*10009*/ { WSAEBADF, "File handle is not valid" },
/*10013*/ { WSAEACCES, "Permission denied" },
/*10014*/ { WSAEFAULT, "Bad address" },
/*10022*/ { WSAEINVAL, "Invalid argument" },
/*10024*/ { WSAEMFILE, "Too many open files" },
/*10035*/ { WSAEWOULDBLOCK, "Resource temporarily unavailable" },
/*10036*/ { WSAEINPROGRESS, "Operation now in progress" },
/*10037*/ { WSAEALREADY, "Operation already in progress" },
/*10038*/ { WSAENOTSOCK, "Socket operation on nonsocket" },
/*10039*/ { WSAEDESTADDRREQ, "Destination address required" },
/*10040*/ { WSAEMSGSIZE, "Message too long" },
/*10041*/ { WSAEPROTOTYPE, "Protocol wrong type for socket" },
/*10042*/ { WSAENOPROTOOPT, "Bad protocol option" },
/*10043*/ { WSAEPROTONOSUPPORT, "Protocol not supported" },
/*10044*/ { WSAESOCKTNOSUPPORT, "Socket type not supported" },
/*10045*/ { WSAEOPNOTSUPP, "Operation not supported" },
/*10046*/ { WSAEPFNOSUPPORT, "Protocol family not supported" },
/*10047*/ { WSAEAFNOSUPPORT, "Address family not supported by protocol family"},
/*10048*/ { WSAEADDRINUSE, "Address already in use" },
/*10049*/ { WSAEADDRNOTAVAIL, "Cannot assign requested address" },
/*10050*/ { WSAENETDOWN, "Network is down" },
/*10051*/ { WSAENETUNREACH, "Network is unreachable" },
/*10052*/ { WSAENETRESET, "Network dropped connection on reset" },
/*10053*/ { WSAECONNABORTED, "Software caused connection abort" },
/*10054*/ { WSAECONNRESET, "Connection reset by peer" },
/*10055*/ { WSAENOBUFS, "No buffer space available" },
/*10056*/ { WSAEISCONN, "Socket is already connected" },
/*10057*/ { WSAENOTCONN, "Socket is not connected" },
/*10058*/ { WSAESHUTDOWN, "Cannot send after socket shutdown" },
/*10059*/ { WSAETOOMANYREFS, "Too many references" },
/*10060*/ { WSAETIMEDOUT, "Connection timed out" },
/*10061*/ { WSAECONNREFUSED, "Connection refused" },
/*10062*/ { WSAELOOP, "Cannot translate name" },
/*10063*/ { WSAENAMETOOLONG, "Name too long" },
/*10064*/ { WSAEHOSTDOWN, "Host is down" },
/*10065*/ { WSAEHOSTUNREACH, "No route to host" },
/*10066*/ { WSAENOTEMPTY, "Directory not empty" },
/*10067*/ { WSAEPROCLIM, "Too many processes" },
/*10070*/ { WSAESTALE, "Stale file handle reference" },
/*10091*/ { WSASYSNOTREADY, "Network subsystem is unavailable" },
/*10092*/ { WSAVERNOTSUPPORTED, "Winsock.dll version out of range" },
/*10093*/ { WSANOTINITIALISED, "Successful WSAStartup not yet performed" },
/*10101*/ { WSAEDISCON, "Graceful shutdown in progress" },
/*10102*/ { WSAENOMORE, "No more results" },
/*10103*/ { WSAECANCELLED, "Call has been canceled" },
/*10104*/ { WSAEINVALIDPROCTABLE, "Procedure call table is invalid" },
/*10105*/ { WSAEINVALIDPROVIDER, "Service provider is invalid" },
/*10106*/ { WSAEPROVIDERFAILEDINIT, "Service provider failed to initialize" },
/*10107*/ { WSASYSCALLFAILURE, "System call failure" },
/*10108*/ { WSASERVICE_NOT_FOUND, "Service not found" },
/*10109*/ { WSATYPE_NOT_FOUND, "Class type not found" },
/*10110*/ { WSA_E_NO_MORE, "No more results" },
/*10111*/ { WSA_E_CANCELLED, "Call was canceled" },
/*10112*/ { WSAEREFUSED, "Database query was refused" },
/*11001*/ { WSAHOST_NOT_FOUND, "Host not found" },
/*11002*/ { WSATRY_AGAIN, "Nonauthoritative host not found" },
/*11003*/ { WSANO_RECOVERY, "This is a nonrecoverable error" },
/*11004*/ { WSANO_DATA, "Valid name, no data record of requested type" },
          { 0,"" }
};

const char *
wsastrerror(int code)
{
    int i;

    for (i = 0; wsatab[i].err; i++)
	if (wsatab[i].err == code)
	    return wsatab[i].errmess;
    return NULL;
}

/*
 * User and group account management using Security IDs (SIDs)
 */
int
__pmValidUserID(__pmUserID sid)
{
    return -ENOTSUP;	/* NYI */
}

int
__pmValidGroupID(__pmGroupID sid)
{
    return -ENOTSUP;	/* NYI */
}

int
__pmEqualUserIDs(__pmUserID sid1, __pmUserID sid2)
{
    return -ENOTSUP;	/* NYI */
}

int
__pmEqualGroupIDs(__pmGroupID sid1, __pmGroupID sid2)
{
    return -ENOTSUP;	/* NYI */
}

void
__pmUserIDFromString(const char *username, __pmUserID *sid)
{
    /* NYI */
}

void
__pmGroupIDFromString(const char *groupname, __pmGroupID *sid)
{
    /* NYI */
}

char *
__pmUserIDToString(__pmUserID sid, char *buffer, size_t size)
{
    return NULL;	/* NYI */
}

char *
__pmGroupIDToString(__pmGroupID gid, char *buffer, size_t size)
{
    return NULL;	/* NYI */
}

int
__pmUsernameToID(const char *username, __pmUserID *uidp)
{
    return -ENOTSUP;	/* NYI */
}

int
__pmGroupnameToID(const char *groupname, __pmGroupID *gidp)
{
    return -ENOTSUP;	/* NYI */
}

char *
__pmGroupnameFromID(__pmGroupID gid, char *buf, size_t size)
{
    return NULL;	/* NYI */
}

char *
__pmUsernameFromID(__pmUserID uid, char *buf, size_t size)
{
    return NULL;	/* NYI */
}

int
__pmUsersGroupIDs(const char *username, __pmGroupID **groupids, unsigned int *ngroups)
{
    return -ENOTSUP;	/* NYI */
}

int
__pmGroupsUserIDs(const char *groupname, __pmUserID **userids, unsigned int *nusers)
{
    return -ENOTSUP;	/* NYI */
}

int
__pmGetUserIdentity(const char *username, __pmUserID *uid, __pmGroupID *gid, int mode)
{
    return -ENOTSUP;	/* NYI */
}
