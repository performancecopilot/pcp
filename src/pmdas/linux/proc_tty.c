/*
 * Copyright (c) 2017 Red Hat.
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

#include "pmapi.h"
#include "libpcp.h"
#include "pmda.h"
#include "linux.h"
#include "proc_tty.h"
#include <sys/stat.h>

int
refresh_tty(pmInDom tty_indom)
{
    char buf[MAXPATHLEN];
    char port[64];
    char *p;
    FILE *fp;
    int sts = 0;
    ttydev_t *device;

    if ((fp = linux_statsfile("/proc/tty/driver/serial", buf, sizeof(buf))) == NULL)
	return -oserror();

    pmdaCacheOp(tty_indom, PMDA_CACHE_INACTIVE);
    while (fgets(buf, sizeof(buf), fp) != NULL) {
	if ((p = strstr(buf, "port:")) != NULL) {
	    sscanf(p+5, "%s", port);
	    if (strncmp(port, "00000000", 8)){
		sts = pmdaCacheLookupName(tty_indom, port, NULL, (void **)&device);
		if (sts < 0){
		    device = (ttydev_t*)malloc(sizeof(ttydev_t));
		    memset(device, 0, sizeof(ttydev_t));
		}
		if ((p = strstr(buf, "irq:")) != NULL)
		    sscanf(p+4, "%u", &device->irq);
		if ((p = strstr(buf, "tx:")) != NULL)
		    sscanf(p+3, "%u", &device->tx);
		if ((p = strstr(buf, "rx:")) != NULL)
		    sscanf(p+3, "%u", &device->rx);
		if ((p = strstr(buf, "fe:")) != NULL)
		    sscanf(p+3, "%u", &device->frame);
		if ((p = strstr(buf, "pe:")) != NULL)
		    sscanf(p+3, "%u", &device->parity);
		if ((p = strstr(buf, "brk:")) != NULL)
		    sscanf(p+4, "%u", &device->brk);
		if ((p = strstr(buf, "oe:")) != NULL)
		    sscanf(p+3, "%u", &device->overrun);
		pmdaCacheStore(tty_indom, PMDA_CACHE_ADD, port, (void*) device);
	    }
	}
    }
    fclose(fp);
    return 0;
}
