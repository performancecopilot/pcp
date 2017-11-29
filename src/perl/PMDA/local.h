/*
 * Copyright (c) 2008-2010 Aconex.  All Rights Reserved.
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
#ifndef LOCAL_H
#define LOCAL_H

#include "pmapi.h"
#include "libpcp.h"
#include "pmda.h"

typedef struct sv scalar_t;
typedef struct timeval delta_t;

typedef struct {
    int		id;
    delta_t	delta;
    int		cookie;
    scalar_t	*callback;
} timers_t;

typedef enum { FILE_PIPE, FILE_SOCK, FILE_TAIL } file_type_t;

typedef struct {
    FILE	*file;
} pipe_data_t;

typedef struct {
    char	*path;
    dev_t	dev;
    ino_t	ino;
} tail_data_t;

typedef struct {
    char	*host;
    int		port;
} sock_data_t;

typedef struct {
    int		fd;
    int		type;
    int		cookie;
    scalar_t	*callback;
    union {
	pipe_data_t pipe;
	tail_data_t tail;
	sock_data_t sock;
    } me;
} files_t;

extern void timer_callback(int, void *);
extern void input_callback(scalar_t *, int, char *);

extern int local_install(void);

extern char *local_strdup_suffix(const char *string, const char *suffix);
extern char *local_strdup_prefix(const char *prefix, const char *string);

extern int local_user(const char *username);

extern int local_timer(double timeout, scalar_t *callback, int cookie);
extern int local_timer_get_cookie(int id);
extern scalar_t *local_timer_get_callback(int id);

extern int local_pipe(char *pipe, scalar_t *callback, int cookie);
extern int local_tail(char *file, scalar_t *callback, int cookie);
extern int local_sock(char *host, int port, scalar_t *callback, int cookie);

extern void local_atexit(void);
extern int local_files_get_descriptor(int id);
extern void local_pmdaMain(pmdaInterface *self);

#endif /* LOCAL_H */
