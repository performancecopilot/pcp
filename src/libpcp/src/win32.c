/*
 * Copyright (c) 2013-2018 Red Hat.
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
 *
 *
 * Thread-safe notes
 *
 * rand() et al are not thread-safe, but we don't really care here.
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
#include "libpcp.h"
#include "internal.h"
#include "deprecated.h"
#include <winbase.h>
#include <psapi.h>

static int	pcp_dir_init = 1;
static char	*pcp_dir;

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
    __psint_t index = (__psint_t)param;

    if (index >= 0 && index < MAX_SIGNALS)
	signals[index].callback(signals[index].signal);
    else {
	fprintf(stderr, "SignalCallback: bad signal index (%" FMT_INT64 ")\n", (__int64_t)index);
    }
}

static char *
MapSignals(int sig, __psint_t *index)
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
    int sts;
    __psint_t index = 0;
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
    pmsprintf(evname, sizeof(evname), "PCP/%" FMT_PID "/%s", (pid_t)getpid(), signame);
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
pmSetProcessIdentity(const char *username)
{
    (void)username;
    return 0;	/* Not Yet Implemented */
}

void
pmSetProgname(const char *program)
{
    char	*p, *suffix = NULL;
    static int	setup;
    WORD	wVersionRequested = MAKEWORD(2, 2);
    WSADATA	wsaData;

    if (program == NULL) {
	/* Restore the default application name */
	pmProgname = "pcp";
    } else {
	/* Trim command name of leading directory components */
	pmProgname = (char *)program;
	for (p = pmProgname; *p; p++) {
	    if (*p == '\\' || *p == '/') {
		pmProgname = p + 1;
		suffix = NULL;
	    }
	    else if (*p == '.')
		suffix = p;
	}
	/* Drop the .exe suffix from the name if we found it */
	if (suffix && strcmp(suffix, ".exe") == 0)
	    *suffix = '\0';
    }

    if (setup)
	return;
    setup = 1;

    /* Deal with all files in binary mode - no EOL futzing */
    _fmode = O_BINARY;

    /*
     * If Windows networking is not setup, all networking calls fail;
     * this even includes gethostname(2), if you can believe that. :[
     */
    WSAStartup(wVersionRequested, &wsaData);

    /*
     * Here we are emulating POSIX signals using Event objects.
     * For all processes we want a SIGTERM handler, which allows
     * us an opportunity to cleanly shutdown: atexit(1) handlers
     * get a look-in, IOW.  Other signals (HUP/USR1) are handled
     * in a similar way, but only by processes that need them.
     */
    __pmSetSignalHandler(SIGTERM, sigterm_callback);
}

static LPTSTR
append_option(LPTSTR cmdline, int *size, const char *option)
{
    int sz = *size, length = strlen(command);

    /* add 1space or 1null */
    if ((cmdline = realloc(cmdline, sz + length + 1)) == NULL) {
	pmNoMem("__pmServerStart", sz + length + 1, PM_FATAL_ERR);
	/* NOTREACHED */
    }
    strcpy(&cmdline[sz], command);
    cmdline[sz + length] = ' ';
    sz += length + 1;

    *size = sz;
    return cmdline;
}

void
__pmServerStart(int argc, char **argv, int flags)
{
    PROCESS_INFORMATION piProcInfo;
    STARTUPINFO siStartInfo;
    LPTSTR cmdline = NULL;
    char *command;
    int i, sz = 0;
    int	sts;

    fflush(stdout);
    fflush(stderr);

    if (pcp_dir_init) {
	/* one-trip initialize to get possible $PCP_DIR */
	pcp_dir = getenv("PCP_DIR");
	pcp_dir_init = 0;
    }

    /* Flatten the argv array for the Windows CreateProcess API */
    if (pcp_dir != NULL) {
	if (argv[0][0] == '/') {
	    /*
	     * if argv[0] starts with a / no $PATH searching happens,
	     * so we need to prefix argv[0] with $PCP_DIR
	     */
	    cmdline = strdup(pcp_dir);
	    sz = strlen(cmdline);
	    /* append argv[0] (with leading slash) at cmdline[sz] */
	}
    }
    for (command = argv[0], i = 0;
	 i < argc && command && *command;
	 command = argv[++i]) {
	cmdline = append_option(cmdline, &sz, command)
    }
    if (flags & 0x1) {
	/*
	 * force append -f option - no exec() so for traditional PCP
	 * daemons, this is added to argv to prevent infinite loop.
	 */
	cmdline = append_option(cmdline, &sz, "-f");
    }
    cmdline[sz - 1] = '\0';

    if (pmDebugOptions.exec)
	fprintf(stderr, "__pmServerStart: cmdline=%s\n", cmdline);

    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
    siStartInfo.cb = sizeof(STARTUPINFO);
    siStartInfo.hStdInput = (HANDLE)_get_osfhandle(fileno(stdin));
    siStartInfo.hStdOutput = (HANDLE)_get_osfhandle(fileno(stdout));
    siStartInfo.hStdError = (HANDLE)_get_osfhandle(fileno(stderr));
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    if ((sts = CreateProcess( NULL,
		    cmdline,
		    NULL,          /* process security attributes */
		    NULL,          /* primary thread security attributes */
		    TRUE,          /* inherit handles */
		    CREATE_NEW_PROCESS_GROUP | CREATE_NO_WINDOW | DETACHED_PROCESS,	/* creation flags */
		    NULL,          /* environment (from parent) */
		    NULL,          /* current directory */
		    &siStartInfo,  /* STARTUPINFO pointer */
				   /* receives PROCESS_INFORMATION */
		    &piProcInfo)) == 0) {
	/* failed */
	DWORD	lasterror;
	char	errmsg[PM_MAXERRMSGLEN];
	lasterror = GetLastError();
	fprintf(stderr, "__pmServerStart: CreateProcess(NULL, \"%s\", ...) failed, lasterror=%ld %s\n", cmdline, lasterror, osstrerror_r(errmsg, sizeof(errmsg)));
	/* but keep going */
    }
    else {
	/* parent, let her exit, but avoid ugly "Log finished" messages */
	if (pmDebugOptions.exec)
	    fprintf(stderr, "__pmServerStart: background PID=%" FMT_PID "\n", (pid_t)piProcInfo.dwProcessId);
	fclose(stderr);
	exit(0);
    }
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

/*
 * fromChild - pipe used for reading from the caller, connected to the
 * standard output of the created process
 * toChild - pipe used for writing from the caller, connected to the
 * std input of the created process
 * If either is NULL, no pipe is created and created process
 * inherits stdio streams from the parent
 */
pid_t
__pmProcessCreate(char **argv, int *fromChild, int *toChild)
{
    HANDLE hChildStdinRd, hChildStdinWr, hChildStdoutRd, hChildStdoutWr;
    PROCESS_INFORMATION piProcInfo; 
    SECURITY_ATTRIBUTES saAttr; 
    STARTUPINFO siStartInfo;
    LPTSTR cmdline = NULL;
    char *command;
    int i, sz = 0;
    int	sts;

    if (pcp_dir_init) {
	/* one-trip initialize to get possible $PCP_DIR */
	pcp_dir = getenv("PCP_DIR");
	pcp_dir_init = 0;
    }
 
    ZeroMemory(&saAttr, sizeof(SECURITY_ATTRIBUTES));
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
    saAttr.bInheritHandle = TRUE;	/* pipe handles are inherited. */
    saAttr.lpSecurityDescriptor = NULL; 

    if (fromChild != NULL && toChild != NULL) {
	*fromChild = *toChild = -1;		/* in case of errors */
	/*
	 * Create a pipe for stdout of the child process.
	 * Ensure that the read handle for the pipe is not inherited.
	 */
	if ((sts = CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &saAttr, 0)) != 1) {
	    if (pmDebugOptions.exec)
		fprintf(stderr, "__pmProcessCreate: CreatePipe() failed for stdout sts=%d\n", sts);
	    return -1;
	}
	if ((sts = SetHandleInformation(hChildStdoutRd, HANDLE_FLAG_INHERIT, 0)) != 1) {
	    if (pmDebugOptions.exec)
		fprintf(stderr, "__pmProcessCreate: set handle inheritance failed for stdout sts=%d\n", sts);
	    CloseHandle(hChildStdoutRd);
	    CloseHandle(hChildStdoutWr);
	    return -1;
	}

	/*
	 * Create a pipe for stdin of the child process.
	 * Ensure that the write handle to the pipe is not inherited.
	 */
	if ((sts = CreatePipe(&hChildStdinRd, &hChildStdinWr, &saAttr, 0)) != 1) {
	    if (pmDebugOptions.exec)
		fprintf(stderr, "__pmProcessCreate: CreatePipe() failed for stdiin sts=%d\n", sts);
	    CloseHandle(hChildStdoutRd);
	    CloseHandle(hChildStdoutWr);
	    return -1;
	}
	if ((sts = SetHandleInformation(hChildStdinWr, HANDLE_FLAG_INHERIT, 0)) != 1) {
	    if (pmDebugOptions.exec)
		fprintf(stderr, "__pmProcessCreate: set handle inheritance failed for stdin sts=%d\n", sts);
	    CloseHandle(hChildStdoutRd);
	    CloseHandle(hChildStdoutWr);
	    CloseHandle(hChildStdinRd);
	    CloseHandle(hChildStdinWr);
	    return -1;
	}
    }

    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
    siStartInfo.cb = sizeof(STARTUPINFO); 
    if (fromChild != NULL && toChild != NULL) {
	siStartInfo.hStdError = (HANDLE)_get_osfhandle(fileno(stderr));
	siStartInfo.hStdOutput = hChildStdoutWr;
	siStartInfo.hStdInput = hChildStdinRd;
	siStartInfo.dwFlags |= STARTF_USESTDHANDLES;
    }

    /* Flatten the argv array for the Windows CreateProcess API */
    if (pcp_dir != NULL) {
	if (argv[0][0] == '/') {
	    /*
	     * if argv[0] starts with a / no $PATH searching happens,
	     * so we need to prefix argv[0] with $PCP_DIR
	     */
	    cmdline = strdup(pcp_dir);
	    sz = strlen(cmdline);
	    /* append argv[0] (with leading slash) at cmdline[sz] */
	}
    }

 
    for (command = argv[0], i = 0; command && *command; command = argv[++i]) {
	int length = strlen(command);
	/* add 1space or 1null */
	if ((cmdline = realloc(cmdline, sz + length + 1)) == NULL) {
	    pmNoMem("__pmProcessCreate", sz + length + 1, PM_FATAL_ERR);
	    /* NOTREACHED */
	}
	strcpy(&cmdline[sz], command);
	cmdline[sz + length] = ' ';
	sz += length + 1;
    }
    cmdline[sz - 1] = '\0';
    if (pmDebugOptions.exec)
	fprintf(stderr, "__pmProcessCreate: cmdline=%s\n", cmdline);

    if ((sts = CreateProcess(NULL, 
		    cmdline,       /* command line */
		    NULL,          /* process security attributes */
		    NULL,          /* primary thread security attributes */
		    TRUE,          /* handles are inherited */
		    0,             /* creation flags */
		    NULL,          /* use parent's environment */
		    NULL,          /* use parent's current directory */
		    &siStartInfo,  /* STARTUPINFO pointer */
				   /* receives PROCESS_INFORMATION */
		    &piProcInfo)) == 0) {
	/* failed */
	DWORD	lasterror;
	char	errmsg[PM_MAXERRMSGLEN];
	lasterror = GetLastError();
	fprintf(stderr, "__pmProcessCreate: CreateProcess(NULL, \"%s\", ...) failed, lasterror=%ld %s\n", cmdline, lasterror, osstrerror_r(errmsg, sizeof(errmsg)));
	if (fromChild != NULL && toChild != NULL) {
	    CloseHandle(hChildStdinRd);
	    CloseHandle(hChildStdinWr);
	    CloseHandle(hChildStdoutRd);
	    CloseHandle(hChildStdoutWr);
	}
	return -1;
    }
    else {
	if ((sts = CloseHandle(piProcInfo.hProcess)) != 1) {
	    if (pmDebugOptions.exec)
		fprintf(stderr, "Warning: CloseHandle() failed for child process sts=%d\n", sts);
	}
	if ((sts = CloseHandle(piProcInfo.hThread)) != 1) {
	    if (pmDebugOptions.exec)
		fprintf(stderr, "Warning: CloseHandle() failed for child's primary thread sts=%d\n", sts);
	}
    }

    if (fromChild != NULL && toChild != NULL) {
	*fromChild = _open_osfhandle((intptr_t)hChildStdoutRd, _O_RDONLY);
	CloseHandle(hChildStdoutWr);
	CloseHandle(hChildStdinRd);
	*toChild = _open_osfhandle((intptr_t)hChildStdinWr, _O_WRONLY);
	if (pmDebugOptions.exec && pmDebugOptions.desperate)
	    fprintf(stderr, "__pmProcessCreate: fromChild=%d toChild=%d\n", *fromChild, *toChild);
    }

    return piProcInfo.dwProcessId;
}

pid_t
__pmProcessWait(pid_t pid, int nowait, int *code, int *signal)
{
    HANDLE ph;
    DWORD status;

    if (pid == (pid_t)-1 || pid == (pid_t)-2) {
	if (pmDebugOptions.exec)
	    fprintf(stderr, "__pmProcessWait: pid=%" FMT_PID " is unexpected\n", pid);
	return -1;
    }
    if ((ph = OpenProcess(SYNCHRONIZE, FALSE, pid)) == NULL) {
	if (pmDebugOptions.exec)
	    fprintf(stderr, "__pmProcessWait: OpenProcess pid=%" FMT_PID " failed\n", pid);
	return -1;
    }
    if (WaitForSingleObject(ph, (DWORD)(-1L)) == WAIT_FAILED) {
	CloseHandle(ph);
	if (pmDebugOptions.exec)
	    fprintf(stderr, "__pmProcessWait: WaitForSingleObject pid=%" FMT_PID " failed\n", pid);
	return -1;
    }
    if (GetExitCodeProcess(ph, &status)) {
	CloseHandle(ph);
	if (pmDebugOptions.exec)
	    fprintf(stderr, "__pmProcessWait: GetExitCodeProcess pid=%" FMT_PID " failed\n", pid);
	return -1;
    }
    if (pmDebugOptions.exec) {
	fprintf(stderr, "__pmProcessWait: pid=%" FMT_PID " exit status=%" FMT_UINT64, pid, (__uint64_t)status);
	if (status != 0) {
	    char	errmsg[PM_MAXERRMSGLEN];
	    pmErrStr_r(-status, errmsg, sizeof(errmsg));
	    fprintf(stderr, ": %s", errmsg);
	}
	fputc('\n', stderr);
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
pmtimevalNow(struct timeval *tv)
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

char *
strsep(char **stringp, const char *delim)
{
    char	*ss, *se;
    const char	*dp;

    if ((ss = *stringp) == NULL)
	return NULL;

    for (se = ss; *se; se++) {
	for (dp = delim; *dp; dp++) {
	    if (*se == *dp)
		break;
	}
    }

    if (*se != '\0') {
	/* match: terminate and update stringp to point past match */
	*se++ = '\0';
	*stringp = se;
    }
    else
	*stringp = NULL;

    return ss;
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
    PM_LOCK(__pmLock_extcall);
    if (eventlog)
	closelog();
    eventlog = RegisterEventSource(NULL, "Application");	/* THREADSAFE */
    if (ident)
	eventlogPrefix = strdup(ident);
    PM_UNLOCK(__pmLock_extcall);
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
	offset = pmsprintf(p, sizeof(logmsg), "%s: ", eventlogPrefix);

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
    pmsprintf(p + offset, sizeof(logmsg) - offset, format, arg);
    ReportEvent(eventlog, eventlogPriority, 0, 0, NULL, 1, 0, &msgptr, NULL);
    va_end(arg);
}

void
closelog(void)
{
    PM_LOCK(__pmLock_extcall);
    if (eventlog) {
	DeregisterEventSource(eventlog);		/* THREADSAFE */
	if (eventlogPrefix)
	    free(eventlogPrefix);
	eventlogPrefix = NULL;
    }
    eventlog = NULL;
    PM_UNLOCK(__pmLock_extcall);
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

/*
 * always called with __pmLock_extcall already held, so THREADSAFE
 */
int
setenv(const char *name, const char *value, int overwrite)
{
    char	*ebuf;

    if (getenv(name) != NULL) {		/* THREADSAFE */
	/* already in the environment */
	if (!overwrite)
	    return(0);
    }

    if ((ebuf = (char *)malloc(strlen(name) + strlen(value) + 2)) == NULL)
	return -1;

    strncpy(ebuf, name, strlen(name)+1);
    strncat(ebuf, "=", 1);
    strncat(ebuf, value, strlen(value));

    return _putenv(ebuf);		/* THREADSAFE */
}

/*
 * always called with __pmLock_extcall already held, so THREADSAFE
 */
int
unsetenv(const char *name)
{
    char	*ebuf;
    int		sts;

    if ((ebuf = (char *)malloc(strlen(name) + 2)) == NULL)
	return -1;

    /* strange but true */
    strncpy(ebuf, name, strlen(name)+1);
    strncat(ebuf, "=", 1);
    sts = _putenv(ebuf);		/* THREADSAFE */
    free(ebuf);
    return sts;
}

int
win32_inet_pton(int af, const char *src, void *dst)
{
    return InetPton(af, src, dst);
}

const char *
win32_inet_ntop(int af, void *src, char *dst, socklen_t size)
{
    return InetNtop(af, src, dst, size);
}
