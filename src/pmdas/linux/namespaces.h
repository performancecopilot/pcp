/*
 * Copyright (c) 2014-2015 Red Hat.
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

enum {
    LINUX_NAMESPACE_IPC_INDEX = 0,
    LINUX_NAMESPACE_UTS_INDEX,
    LINUX_NAMESPACE_NET_INDEX,
    LINUX_NAMESPACE_MNT_INDEX,
    LINUX_NAMESPACE_USER_INDEX,

    LINUX_NAMESPACE_COUNT
};

#define LINUX_NAMESPACE_IPC      (1<<LINUX_NAMESPACE_IPC_INDEX)
#define LINUX_NAMESPACE_UTS      (1<<LINUX_NAMESPACE_UTS_INDEX)
#define LINUX_NAMESPACE_NET      (1<<LINUX_NAMESPACE_NET_INDEX)
#define LINUX_NAMESPACE_MNT      (1<<LINUX_NAMESPACE_MNT_INDEX)
#define LINUX_NAMESPACE_USER     (1<<LINUX_NAMESPACE_USER_INDEX)

typedef struct linux_container {
    int		pid;
    int		netfd;
    int		length;
    char	*name;
} linux_container_t;

extern int container_enter_namespaces(int, linux_container_t *, int);
extern int container_leave_namespaces(int, int);

extern int container_open_network(linux_container_t *);
extern void container_close_network(linux_container_t *);

