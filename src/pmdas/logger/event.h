/*
 * event support for the Logger PMDA
 *
 * Copyright (c) 2011 Red Hat Inc.
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
 */

#ifndef _EVENT_H
#define _EVENT_H

#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/pmda.h>
#include <sys/queue.h>

#define BUF_SIZE 1024

struct event {
    TAILQ_ENTRY(event) events;
    int clients;
    char buffer[BUF_SIZE];
};

TAILQ_HEAD(tailhead, event);

struct EventFileData {
    int			fd;
    pid_t	        pid;
    int			numclients;
    pmID		pmid;
    __uint32_t		count;
    __uint32_t		pad;
    __uint64_t		bytes;
    struct stat		pathstat;
    char		pmnsname[MAXPATHLEN];
    char		pathname[MAXPATHLEN];
    struct tailhead	head;
};

extern struct EventFileData *logfiles;
extern int numlogfiles;

extern int maxfd;
extern fd_set fds;

extern void event_init(void);
extern int event_create(unsigned int logfile);
extern int event_fetch(pmValueBlock **vbpp, unsigned int logfile);
extern int event_get_clients_per_logfile(unsigned int logfile);
extern void event_shutdown(void);

#endif /* _EVENT_H */
