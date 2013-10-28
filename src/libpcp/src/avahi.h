#ifndef AVAHI_H
#define AVAHI_H 1
/*
 * Copyright (c) 2013 Red Hat.
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
#if HAVE_AVAHI
#include <avahi-client/publish.h>
#include <avahi-common/alternative.h>
#include <avahi-common/thread-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>
#endif

typedef struct __pmServerAvahiPresence {
#if HAVE_AVAHI
    char		*avahi_service_name;
    const char		*avahi_service_tag;
    int			port;
    AvahiThreadedPoll	*avahi_threaded_poll;
    AvahiClient		*avahi_client;
    AvahiEntryGroup	*avahi_group;
#else
    char unused; /* we need something */
#endif
} __pmServerAvahiPresence;

__pmServerAvahiPresence *__pmServerAvahiAdvertisePresence(const char *, int);
void __pmServerAvahiUnadvertisePresence(__pmServerAvahiPresence *);

#endif /* AVAHI_H */
