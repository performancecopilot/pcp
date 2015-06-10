/*
** Copyright (C) 2015 Red Hat.
**
** This program is free software; you can redistribute it and/or modify it
** under the terms of the GNU General Public License as published by the
** Free Software Foundation; either version 2, or (at your option) any
** later version.
**
** This program is distributed in the hope that it will be useful, but
** WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
** See the GNU General Public License for more details.
*/
#include <pcp/pmapi.h>
#include <pcp/impl.h>

char *
getstrvers(void)
{
	static char vers[64];

	snprintf(vers, sizeof vers,
		"%s version: %s <pcp@oss.sgi.com>",
		pmProgname, PCP_VERSION);

	return vers;
}
