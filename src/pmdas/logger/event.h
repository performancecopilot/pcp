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

struct LogfileData {
    pmID	pmid;
    __uint32_t	count;
    __uint64_t	bytes;
    char	pmns_name[MAXPATHLEN];
    char	pathname[MAXPATHLEN];
    struct stat	pathstat;
};

extern void event_init(pmdaInterface *dispatch, struct LogfileData *logfiles,
		       int numlogfiles);
extern int event_fetch(pmValueBlock **vbpp, unsigned int logfile);
extern int event_get_clients_per_logfile(unsigned int logfile);
extern void event_shutdown(void);

#endif /* _EVENT_H */
