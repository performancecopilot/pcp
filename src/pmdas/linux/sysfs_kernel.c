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

	pmsprintf(buf, sizeof(buf), "%s/%s/zswap/parameters/enabled",
				    linux_statspath, "sys/module");
	if ((fd = open(buf, O_RDONLY)) >= 0) {
	    if ((n = read(fd, buf, sizeof(buf))) > 0) {
		buf[n-1] = '\0';
		sscanf(buf, "%c", &sk->zswap_enabled[0]);
	    }
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

    if (need_refresh[REFRESH_SYSFS_KERNEL_VMMEMCTL]) {
	unsigned long long value, failed;
	char name[64];
	FILE *fp;

	pmsprintf(buf, sizeof(buf), "%s/%s/debug/vmmemctl",
				    linux_statspath, "sys/kernel");
	if ((fp = fopen(buf, "r")) != NULL) {

	    while (fgets(buf, sizeof(buf), fp) != NULL) {
		/*
		 * Care is needed here: this sysfs file has conflicting
		 * entries like the following (we want the first case):
		 * target     :     1023777
		 * target     :         768 (0 failed)
		 *
		 * To tackle this, provide matching on the failed part,
		 * and discard any line that *does* match there.
		 */
		n = sscanf(buf, "%s : %llu (%llu failed)", name, &value, &failed);
		if (n != 2)
		    continue;
		else if (strcmp(name, "current") == 0)
		    sk->vmmemctl_current = value << _pm_pageshift;
		else if (strcmp(name, "target") == 0)
		    sk->vmmemctl_target = value << _pm_pageshift;
	    }
	    fclose(fp);
	}
    }

    if (need_refresh[REFRESH_SYSFS_KERNEL_HVBALLOON]) {
	unsigned long long value;
	char name[64];
	FILE *fp;

	pmsprintf(buf, sizeof(buf), "%s/%s/debug/hv-balloon",
				    linux_statspath, "sys/kernel");
	if ((fp = fopen(buf, "r")) != NULL) {

	    while (fgets(buf, sizeof(buf), fp) != NULL) {
		n = sscanf(buf, "%s : %llu", name, &value);
		if (n != 2)
		    continue;
		else if (strcmp(name, "state") == 0)
		    sk->hv_balloon_state = value;
		else if (strcmp(name, "page_size") == 0)
		    sk->hv_balloon_pagesize = value;
		else if (strcmp(name, "pages_added") == 0)
		    sk->hv_balloon_added = value;
		else if (strcmp(name, "pages_onlined") == 0)
		    sk->hv_balloon_onlined = value;
		else if (strcmp(name, "pages_ballooned") == 0)
		    sk->hv_balloon_ballooned = value;
		else if (strcmp(name, "total_pages_committed") == 0)
		    sk->hv_balloon_total_committed = value;
	    }
	    value = sk->hv_balloon_pagesize ? /* fallback to kernel pagesize */
		    sk->hv_balloon_pagesize : (1 << _pm_pageshift);
	    sk->hv_balloon_added *= value;
	    sk->hv_balloon_onlined *= value;
	    sk->hv_balloon_ballooned *= value;
	    sk->hv_balloon_total_committed *= value;
	    fclose(fp);
	}
    }

    return 0;
}
