
/*
 * Copyright (c) 2000 Silicon Graphics, Inc.  All Rights Reserved.
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
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#ident "$Id: weblog.h,v 2.14 2004/06/15 09:39:44 kenmcd Exp $"

#ifndef _WEBLOG_H
#define _WEBLOG_H

/*
 * As this is multi-threaded code, use the multi-threaded errno
 */
#define _SGI_MP_SOURCE

#include <unistd.h>
#include <syslog.h>
/*
 * POSIX regular expression library has unacceptably poor
 * performance on Irix forcing us to revert to the old 
 * regcmp/regex routines in libgen for Irix. Linux has
 * the POSIX regex library but not regcmp/regex. The
 * performance of the Linux POSIX library is perfectly 
 * acceptable.
 *
 * For Backwards Compatibility, we include the old stuff
 * whenever it is available (Irix). We always include the
 * Posix code.
 */
#ifdef __sgi
#define  NON_POSIX_REGEX
#endif

#include <regex.h>

#include <sys/types.h>
#include <sys/stat.h>
#include "pmapi.h"
#include "impl.h"
#include "pmda.h"

enum HTTP_Methods {
    wl_httpGet, wl_httpHead, wl_httpPost, wl_httpOther, wl_numMethods
};

enum HistSizes {
    wl_zero, wl_le3k, wl_le10k, wl_le30k, wl_le100k, wl_le300k, wl_le1m,
    wl_le3m, wl_gt3m, wl_unknownSize, wl_numSizes
};

#define FIBUFSIZE	16*1024
#define DORMANT_WARN	86400

typedef struct {
    char*		fileName;
    int			filePtr;
    struct stat		fileStat;
    char		buf[FIBUFSIZE];
    char		*bp;
    char		*bend;
    u_int		format;		/* index into regex for parsing file */
    time_t		lastActive;	/* time in sec when last active */
} FileInfo;

typedef struct {
    __uint32_t		methodReq[wl_numMethods];
    __uint64_t		methodBytes[wl_numMethods];
    __uint32_t		sizeReq[wl_numSizes];
    __uint64_t		sizeBytes[wl_numSizes];
    __uint32_t		cached_sizeReq[wl_numSizes];
    __uint64_t		cached_sizeBytes[wl_numSizes];
    __uint32_t		uncached_sizeReq[wl_numSizes];
    __uint64_t		uncached_sizeBytes[wl_numSizes];
    __uint32_t		sumReq;
    __uint32_t		client_sumReq;
    __uint32_t		cached_sumReq;
    __uint32_t		uncached_sumReq;
    __uint64_t		sumBytes;
    __uint64_t		cached_sumBytes;
    __uint64_t		uncached_sumBytes;
    __uint32_t		errors;
    __uint32_t		active;
    __uint32_t		numLogs;
    __uint32_t		modTime;
    __uint32_t		extendedp;
} WebCount;

typedef struct {
    int			update;		/* flag for updating this server */
    WebCount		counts;
    FileInfo		access;
    FileInfo		error;
} WebServer;

typedef struct {
    int		id;
    pid_t	pid;
    int         firstServer;
    int         lastServer;
    int         inFD[2];
    int         outFD[2];
    char	*methodStr;
    char	*sizeStr;
    char        *c_statusStr;
    char        *s_statusStr;
    int		strLength;
} WebSproc;

typedef struct {
    char*	name;
#ifdef NON_POSIX_REGEX
    char        *np_regex;
#endif
    regex_t* 	regex;
    int		methodPos;
    int		sizePos;
    int         c_statusPos;
    int         s_statusPos;
    int         posix_regexp;
} WebRegex;

extern WebServer	*wl_servers;
extern WebSproc		*wl_sproc;
extern WebRegex		*wl_regexTable;
extern pmdaInstid	*wl_serverInst;
extern pmdaIndom	wl_indomTable[];

extern __uint32_t	wl_numServers;
extern __uint32_t	wl_numActive;
extern __uint32_t	wl_refreshDelay;
extern __uint32_t	wl_chkDelay;
extern __uint32_t	wl_sprocThresh;
extern __uint32_t	wl_numSprocs;
extern __uint32_t	wl_catchupTime;
extern __uint32_t	wl_numRegex;

extern int		wl_updateAll;
extern time_t		wl_timeOfProbe;
extern char		*wl_logFile;
extern char		wl_helpFile[];
extern int		wl_isDSO;

int openLogFile(FileInfo*);
void probe(void);
void refresh(WebSproc*);
void refreshAll(void);
void sprocMain(void*);
void web_init(pmdaInterface*);
void logmessage(int, const char *, ...);

#define wl_close(fd) close(fd); fd = -1;

#endif /* _WEBLOG_H */
