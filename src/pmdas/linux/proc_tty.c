/*
 * Copyright (c) 2017-2018 Red Hat.
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
#include "pmda.h"
#include "linux.h"
#include "proc_tty.h"
#include <sys/stat.h>

int
refresh_tty(pmInDom tty_indom)
{
    char buf[MAXPATHLEN];
    char port[64];
    char uart[64];
    char *p;
    char *u;
    FILE *fp;
    int sts = 0;
    int port_result = -1;
    ttydev_t *device;

    if ((fp = linux_statsfile("/proc/tty/driver/serial", buf, sizeof(buf))) == NULL)
	return -oserror();

    pmdaCacheOp(tty_indom, PMDA_CACHE_INACTIVE);
    while (fgets(buf, sizeof(buf), fp) != NULL) {
	if (((p = strstr(buf, ":")) != NULL) && ((u = strstr(buf, "uart:")) != NULL)) {
	    strncpy(port, buf, (int)(p - buf));
	    port[(int)(p - buf)] = '\0';
	    errno = 0;
	    port_result = (int)strtol(port, NULL, 10);
	    if (errno != 0 || port_result < 0) {
		pmNotifyErr(LOG_DEBUG, "Invalid tty number: %d %s (%d)\n", errno, strerror(errno), port_result);
		goto done;
	    }
	    sscanf(u+5, "%s", uart);
	    if (strcmp(uart, "unknown") && strcmp(port, "serinfo")) {
		sts = pmdaCacheLookupName(tty_indom, port, NULL, (void **)&device);
		if (sts < 0) {
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
	done:
	    memset(&port, 0, sizeof(port));
	    memset(&uart, 0, sizeof(uart));
	}
    }
    fclose(fp);
    return 0;
}
