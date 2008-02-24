/*
 * Copyright (c) 2008 Aconex.  All Rights Reserved.
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
#ifndef LOCAL_H
#define LOCAL_H

extern int	pmDebug;
extern char *	pmProgname;

typedef struct timeval delta_t;

typedef struct {
    int		id;
    delta_t	delta;
    SV		*cookie;
    SV		*callback;
} timers_t;

typedef enum { FILE_PIPE, FILE_SOCK, FILE_TAIL } file_type_t;

typedef struct {
    FILE	*file;
} pipe_data_t;

typedef struct {
    FILE	*file;
    dev_t	dev;
    ino_t	ino;
} tail_data_t;

typedef struct {
    FILE	*file;
    char	*host;
    int		port;
} sock_data_t;

typedef struct {
    int		fd;
    int		type;
    SV		*cookie;
    SV		*callback;
    union {
	pipe_data_t pipe;
	tail_data_t tail;
	sock_data_t sock;
    } me;
} files_t;

#endif /* LOCAL_H */
