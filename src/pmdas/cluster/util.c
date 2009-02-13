/*
 * Copyright (c) 2008-2009 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <unistd.h>
#include <stdio.h>

static size_t
cluster_node_rw(int do_read, int fd, void *buf, size_t len)
{
    size_t off = 0;
    int n;
    int k = 0;

    if (len > 1048576)
	fprintf(stderr, "cluster_node_rw: warning: unlikely len=%lu\n", len);
    while (off < len) {
	if (do_read)
	    n = read(fd, buf + off, len - off);
	else
	    n = write(fd, buf + off, len - off);
	if (n < 0)
	    return n;
	off += n;
	if (k++ > 1000) {
	    fprintf(stderr, "cluster_node_rw: spinning\n");
	    break;
	}
    }

    return off;
}

size_t
cluster_node_read(int fd, void *buf, size_t len)
{
    return cluster_node_rw(1, fd, buf, len);
}

size_t
cluster_node_write(int fd, void *buf, size_t len)
{
    return cluster_node_rw(0, fd, buf, len);
}
