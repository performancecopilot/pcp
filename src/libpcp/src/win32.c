/*
 * Copyright (c) 2008 Aconex.  All Rights Reserved.
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
 */
#include "pmapi.h"
#include "impl.h"
#include <winbase.h>

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

unsigned long __pmProcessDataSize()
{
    return 0L;
}

int
nanosleep(const struct timespec *req, struct timespec *rem)
{
    Sleep((req->tv_sec * 1000) + (req->tv_nsec / 1000));
    memset(rem, 0, sizeof(*rem));
    return 0;
}

unsigned int
sleep(unsigned int seconds)
{
    Sleep(seconds * 1000);
    return 0;
}

void setlinebuf(FILE *stream)
{
    setvbuf(stream, NULL, _IONBF, 0);	/* no line buffering in Win32 */
}

const char *
hstrerror(int error)
{
    return strerror(error);
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

const char *
index(const char *string, int marker)
{
    const char *p;
    for (p = string; *p != '\0'; p++)
	if (*p == marker)
	    return p;
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

void
openlog(const char *ident, int option, int facility)
{
    /* TODO */
}

void
syslog(int priority, const char *format, ...)
{
    /* TODO */
}

void
closelog(void)
{
    /* TODO */
}


/* loop.c */

int
pmLoopRegisterInput(
    int fd,
    int flags,
    int (*callback)(int fd, int flags, void *closure),
    void *closure,
    int priority)
{
    return -1;
}

void
pmLoopUnregisterInput(int tag)
{
}

int
pmLoopRegisterSignal(
    int sig,
    int (*callback)(int sig, void *closure),
    void *closure)
{
    return -1;
}

void
pmLoopUnregisterSignal(int tag)
{
}

int
pmLoopRegisterTimeout(
    int tout_msec,
    int (*callback)(void *closure),
    void *closure)
{
    return -1;
}

void
pmLoopUnregisterTimeout(int tag)
{
}

int
pmLoopRegisterChild(
    pid_t pid,
    int (*callback)(pid_t pid, int status, const struct rusage *, void *closure),
    void *closure)
{
    return -1;
}

void
pmLoopUnregisterChild(int tag)
{
}

int
pmLoopRegisterIdle(
    int (*callback)(void *closure),
    void *closure)
{
    return -1;
}

void
pmLoopUnregisterIdle(int tag)
{
}

void
pmLoopStop(void)
{
}

int
pmLoopMain(void)
{
    return -1;
}


/* AF.c */

int
__pmAFregister(const struct timeval *delta, void *data, void (*func)(int, void *))
{
    /* TODO */
    return -1;
}

int
__pmAFunregister(int afid)
{
    /* TODO */
    return -1;
}

void
__pmAFblock(void)
{
    /* TODO */
}

void
__pmAFunblock(void)
{
    /* TODO */
}

int
__pmAFisempty(void)
{
    /* TODO */
    return -1;
}
