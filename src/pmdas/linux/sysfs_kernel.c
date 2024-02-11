/*
 * Linux sysfs_kernel cluster
 *
 * Copyright (c) 2009,2014,2016,2023-2024 Red Hat.
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
#include <ctype.h>
#include "linux.h"
#include "sysfs_kernel.h"

int
refresh_sysfs_kernel(sysfs_kernel_t *sk, int *need_refresh)
{
    char buf[MAXPATHLEN];
    int n, i;

    memset(sk, 0, sizeof(*sk));

    if (need_refresh[REFRESH_SYSFS_KERNEL_UEVENTSEQ]) {
	int	fd;

	pmsprintf(buf, sizeof(buf), "%s/%s/uevent_seqnum",
				    linux_statspath, "sys/kernel");
	if ((fd = open(buf, O_RDONLY)) >= 0) {
	    if ((n = read(fd, buf, sizeof(buf))) > 0) {
		buf[n-1] = '\0';
		sscanf(buf, "%llu", (long long unsigned int *)&sk->uevent_seqnum);
		sk->valid_uevent_seqnum = 1;
	    }
	    close(fd);
	}
    }

    if (need_refresh[REFRESH_SYSFS_KERNEL_EXTFRAG]) {
	unsigned long node;
	pernode_t *np;
	pmInDom nodes = INDOM(NODE_INDOM);
	float frag[16];
	char name[64], tmp[64];
	FILE *fp;

	pmsprintf(buf, sizeof(buf), "%s/%s/debug/extfrag/unusable_index",
				    linux_statspath, "sys/kernel");
	if ((fp = fopen(buf, "r")) != NULL) {

	    while (fgets(buf, sizeof(buf), fp) != NULL) {
		if (strlen(buf) < 6)
		    continue;
		n = sscanf(&buf[5], "%lu, %s %s "
			"%f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f",
			&node, tmp, name,
			&frag[0], &frag[1], &frag[2], &frag[3], &frag[4],
			&frag[5], &frag[6], &frag[7], &frag[8], &frag[9],
			&frag[10], &frag[11], &frag[12], &frag[13],
			&frag[14], &frag[15]);
		if (n < 4 || strncmp(name, "Normal", 7) != 0)
		    continue;

		np = NULL;
		pmsprintf(name, sizeof(name), "node%lu", node);
		if (!pmdaCacheLookupName(nodes, name, NULL, (void **)&np) || !np) {
		    fprintf(stderr, "Unknown node '%s' in sysfs file", name);
		} else {
		    np->extfrag_unusable = 0;
		    np->num_extfrag_index = 0;
		    n -= 3;
		    for (i = 0; i < n; i++)
			np->extfrag_unusable += frag[i];
		    np->num_extfrag_index = n;
		}
	    }
	    fclose(fp);
	}
    }

    if (need_refresh[REFRESH_SYSFS_MODULE_ZSWAP]) {
	int	fd;

	memset(sk->zswap_enabled, 0, sizeof(sk->zswap_enabled));
	pmsprintf(buf, sizeof(buf), "%s/%s/zswap/parameters/enabled",
				    linux_statspath, "sys/module");
	if ((fd = open(buf, O_RDONLY)) >= 0) {
	    if ((read(fd, buf, sizeof(buf))) > 0)
		sscanf(buf, "%c", &sk->zswap_enabled[0]);
	    close(fd);
	}

	pmsprintf(buf, sizeof(buf), "%s/%s/zswap/parameters/max_pool_percent",
				    linux_statspath, "sys/module");
	if ((fd = open(buf, O_RDONLY)) >= 0) {
	    if ((n = read(fd, buf, sizeof(buf))) > 0) {
		buf[n-1] = '\0';
		sscanf(buf, "%u", (unsigned int *)&sk->zswap_max_pool_percent);
	    }
	    close(fd);
	}
    }

    return 0;
}
