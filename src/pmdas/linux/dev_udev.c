/*
 * Linux dev_udev cluster
 *
 * Copyright (c) 2009, Red Hat, Inc.  All Rights Reserved.
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
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include "dev_udev.h"

int
refresh_dev_udev(dev_udev_t *ud)
{
    char buf[64];
    int fd;

    if ((fd = open("/dev/.udev/uevent_seqnum", O_RDONLY)) < 0) {
    	ud->valid = 0;
	return -errno;
    }

    if (read(fd, buf, sizeof(buf)) <= 0)
    	ud->valid = 0;
    else {
    	sscanf(buf, "%llu", (long long unsigned int *)&ud->seqnum);
	ud->valid = 1;
    }
    close(fd);

    return 0;
}
