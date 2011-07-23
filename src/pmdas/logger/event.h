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

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include <sys/queue.h>

/*
 * Queue access methods that are missing on glibc <= 2.3, e.g. RHEL4
 */ 
#ifndef TAILQ_NEXT	
#define	TAILQ_NEXT(elm, field)	((elm)->field.tqe_next)
#endif

#ifndef TAILQ_FIRST
#define TAILQ_FIRST(head)	((head)->tqh_first)
#endif

#ifndef TAILQ_EMPTY
#define TAILQ_EMPTY(head)               ((head)->tqh_first == NULL)
#endif

#ifndef TAILQ_LAST
#define TAILQ_LAST(head, headname) \
        (*(((struct headname *)((head)->tqh_last))->tqh_last))
#endif

#ifndef TAILQ_PREV
#define TAILQ_PREV(elm, headname, field) \
        (*(((struct headname *)((elm)->field.tqe_prev))->tqh_last))
#endif

struct event {
    TAILQ_ENTRY(event)	events;
    int			clients;
    int			size;		/* buffer size in bytes */
    char		buffer[];
};

TAILQ_HEAD(tailqueue, event);

struct EventFileData {
    int			fd;
    pid_t	        pid;
    pmID		pmid;
    int			numclients;
    int			restricted;	/* access controlled via pmstore */
    __uint32_t		count;		/* exported metric: event counter */
    __uint64_t		bytes;		/* exported metric: data throughput */
    struct stat		pathstat;
    char		pmnsname[MAXPATHLEN];
    char		pathname[MAXPATHLEN];
    struct tailqueue	queue;		/* queue of events for clients */
    long		queuesize;	/* total data in the queue (< maxmem) */
};

extern struct EventFileData *logfiles;
extern int numlogfiles;

extern int maxfd;
extern fd_set fds;
extern long maxmem;

extern void event_init(void);
extern int event_create(unsigned int logfile);
extern int event_fetch(pmValueBlock **vbpp, unsigned int logfile);
extern int event_get_clients_per_logfile(unsigned int logfile);
extern void event_shutdown(void);

#endif /* _EVENT_H */
