/*
 * Copyright (c) 2014-2017 Red Hat.
 * Copyright (c) 2010 Aconex.  All Rights Reserved.
 * Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
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
#include <sys/stat.h>
#include <sys/dir.h>
#include <ctype.h>
#include <fcntl.h>
#include "linux.h"

char *
get_distro_info(void)
{
    /*
     * Heuristic guesswork ... add code here as we learn
     * more about how to identify each Linux distribution.
     */
    static char		*distro_name;
    struct stat		sbuf;
    int			r, sts, fd = -1, len = 0;
    char		path[MAXPATHLEN];
    char		prefix[16];
    enum {	/* rfiles array offsets */
	DEB_VERSION	= 0,
	LSB_RELEASE	= 6,
    };
    char *rfiles[] = { "debian_version", "oracle-release", "fedora-release",
	"redhat-release", "slackware-version", "SuSE-release", "lsb-release",
	/* insert any new distribution release variants here */
	NULL
    };

    if (distro_name)
	return distro_name;

    for (r = 0; rfiles[r] != NULL; r++) {
	pmsprintf(path, sizeof(path), "%s/etc/%s", linux_statspath, rfiles[r]);
	if ((fd = open(path, O_RDONLY)) == -1)
	    continue;
	if (fstat(fd, &sbuf) == -1) {
	    close(fd);
	    fd = -1;
	    continue;
	}
	break;
    }
    if (fd != -1) {
	if (r == DEB_VERSION) {	/* Debian, needs a prefix */
	    strncpy(prefix, "Debian ", sizeof(prefix));
	    len = 7;
	}
	/*
	 * at this point, assume sbuf is good and file contains
	 * the string we want, probably with a \n terminator
	 */
	distro_name = (char *)malloc(len + (int)sbuf.st_size + 1);
	if (distro_name != NULL) {
	    if (len)
		strncpy(distro_name, prefix, len);
	    sts = read(fd, distro_name + len, (int)sbuf.st_size);
	    if (sts <= 0) {
		free(distro_name);
		distro_name = NULL;
	    } else {
		char *nl;

		if (r == LSB_RELEASE) {	/* may be Ubuntu */
		    if (!strncmp(distro_name, "DISTRIB_ID = ", 13))
			distro_name += 13;	/* ick */
		    if (!strncmp(distro_name, "DISTRIB_ID=", 11))
			distro_name += 11;	/* more ick */
		}
		distro_name[sts + len] = '\0';
		if ((nl = strchr(distro_name, '\n')) != NULL)
		    *nl = '\0';
	    }
	}
	close(fd);
    }
    if (distro_name == NULL) 
	distro_name = "?";
    return distro_name;
}

char *
get_machine_info(char *fallback)
{
    static char	*machine_name;
    char	*p, name[1024];
    FILE	*f;

    if (machine_name)
	return machine_name;

    /* vendor-specific hardware information - Silicon Graphics machines */
    f = linux_statsfile("/proc/sgi_prominfo/node0/version", name, sizeof(name));
    if (f != NULL) {
	while (fgets(name, sizeof(name), f)) {
	    if (strncmp(name, "SGI", 3) == 0) {
		if ((p = strstr(name, " IP")) != NULL)
		    machine_name = strndup(p+1, 4);
		break;
	    }
	}
	fclose(f);
    }
    if (machine_name == NULL)
	machine_name = fallback;
    return machine_name;
}
