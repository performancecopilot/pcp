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
#include <avahi-client/publish.h>
#include <avahi-common/alternative.h>
#include <avahi-common/thread-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>

typedef struct __pmServerAvahiPresence __pmServerAvahiPresence;

__pmServerAvahiPresence *__pmServerAvahiAdvertisePresence(const char *, int);
void __pmServerAvahiUnadvertisePresence(__pmServerAvahiPresence *);

#endif /* AVAHI_H */
