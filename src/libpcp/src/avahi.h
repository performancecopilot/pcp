/*
 * Copyright (c) 2013-2014 Red Hat.
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
 */
#ifndef AVAHI_H
#define AVAHI_H

#ifdef HAVE_AVAHI
void __pmServerAvahiAdvertisePresence(__pmServerPresence *) _PCP_HIDDEN;
void __pmServerAvahiUnadvertisePresence(__pmServerPresence *) _PCP_HIDDEN;
int __pmAvahiDiscoverServices(const char *,
			      const char *,
			      const __pmServiceDiscoveryOptions *,
			      int,
			      char ***) _PCP_HIDDEN;
#else
#define __pmServerAvahiAdvertisePresence(p)		do { } while (0)
#define __pmServerAvahiUnadvertisePresence(p)		do { } while (0)
#define __pmAvahiDiscoverServices(s, m, o, n, u)	0
#endif

#endif /* AVAHI_H */
