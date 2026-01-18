/*
 * Darwin PMDA uname cluster
 *
 * Copyright (c) 2025 Red Hat.
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

#include <sys/utsname.h>
#include "pmapi.h"
#include "pmda.h"
#include "darwin.h"
#include "uname.h"

extern int mach_uname_error;
extern struct utsname mach_uname;
extern char *macos_version(void);

int
refresh_uname(struct utsname *utsname)
{
	return (uname(utsname) == -1) ? -oserror() : 0;
}

int
fetch_uname(unsigned int item, pmAtomValue *atom)
{
	static char mach_uname_all[(_SYS_NAMELEN*5)+8];

	if (mach_uname_error)
		return mach_uname_error;
	switch (item) {
	case 28: /* pmda.uname */
		pmsprintf(mach_uname_all, sizeof(mach_uname_all), "%s %s %s %s %s",
			mach_uname.sysname, mach_uname.nodename,
			mach_uname.release, mach_uname.version,
			mach_uname.machine);
		atom->cp = mach_uname_all;
		return 1;
	case 29: /* pmda.version */
		atom->cp = pmGetConfig("PCP_VERSION");
		return 1;
	case 30: /* kernel.all.distro */
		atom->cp = macos_version();
		return 1;
	}
	return PM_ERR_PMID;
}
