/*
 * Copyright (c) 2004,2006 Silicon Graphics, Inc.  All Rights Reserved.
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
#include <fcntl.h>
#include <limits.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <mach/mach.h>
#include "pmapi.h"
#include "pmda.h"
#include "network.h"

extern mach_port_t mach_master_port;

void
init_network(void)
{

}

/*
 * Ensure we have space for the next interface in our pre-allocated
 * interface stats pool.  If not, make some or pass on the error.
 */
static int
check_stats_size(struct netstats *stats, int count)
{
    if (count > stats->highwater) {
	stats->highwater++;
	stats->interfaces = realloc(stats->interfaces,
				stats->highwater * sizeof(struct ifacestat));
	if (!stats->interfaces) {
	    stats->highwater = 0;
	    return -ENOMEM;
	}
    }
    return 0;
}

/*
 * Insert all interfaces into the global network instance domain.
 */
static int
update_network_indom(struct netstats *all, int count, pmdaIndom *indom)
{
    int	i;

    if (count > 0 && count != indom->it_numinst) {
	i = sizeof(pmdaInstid) * count;
	if ((indom->it_set = realloc(indom->it_set, i)) == NULL) {
	    indom->it_numinst = 0;
	    return -ENOMEM;
	}
    }
    for (i = 0; i < count; i++) {
	indom->it_set[i].i_name = all->interfaces[i].name;
	indom->it_set[i].i_inst = i;
    }
    indom->it_numinst = count;
    return 0;
}

int
refresh_network(struct netstats *stats, pmdaIndom *indom)
{
    int			i = 0, status = 0;

   size_t n;
   char *new_buf, *next, *end;
   struct sockaddr_dl *sdl;

   static char *buf=NULL;
   static size_t buf_len=0;

#ifdef RTM_IFINFO2
   struct if_msghdr2 *ifm;
   int mib[6] = {CTL_NET, PF_ROUTE, 0, 0, NET_RT_IFLIST2, 0};
#else
   struct if_msghdr *ifm;
   int mib[6] = {CTL_NET, PF_ROUTE, 0, 0, NET_RT_IFLIST, 0};
#endif

   if( sysctl( mib, 6, NULL, &n, NULL, 0 ) < 0 ) {
      /* unable to query buffer size */
      fprintf( stderr, "%s: get net mib buf len failed\n", pmGetProgname() );
      return -ENXIO;
   }
   if( n > buf_len ) {
      if( (new_buf = malloc(n)) == NULL ) {
         /* unable to malloc buf */
         fprintf( stderr, "%s: net mib buf malloc failed\n", pmGetProgname() );
         return -ENXIO;
      } else {
         if( buf != NULL ) free( buf );
         buf = new_buf;
         buf_len = n;
      }
   }
   if( sysctl( mib, 6, buf, &n, NULL, 0 ) < 0 ) {
      /* unable to copy-in buffer */
      fprintf( stderr, "%s: net mib buf read failed\n", pmGetProgname() );
      return -ENXIO;
   }

   for( next = buf, i=0, end = buf + n; next < end; ) {

#ifdef RTM_IFINFO2
      ifm = (struct if_msghdr2 *)next;
      next += ifm->ifm_msglen;
      if( ifm->ifm_type == RTM_IFINFO2 ) {
#else
      ifm = (struct if_msghdr *)next;
      next += ifm->ifm_msglen;
      if( ifm->ifm_type == RTM_IFINFO ) {
#endif

         status = check_stats_size(stats, i + 1);
         if (status < 0) break;

         sdl = (struct sockaddr_dl *)(ifm + 1);
         n = sdl->sdl_nlen < IFNAMEMAX ? sdl->sdl_nlen : IFNAMEMAX;
         strncpy( stats->interfaces[i].name, sdl->sdl_data, n );
         stats->interfaces[i].name[n] = 0;

         stats->interfaces[i].mtu = ifm->ifm_data.ifi_mtu;
         stats->interfaces[i].baudrate = ifm->ifm_data.ifi_baudrate;
         stats->interfaces[i].ipackets = ifm->ifm_data.ifi_ipackets;
         stats->interfaces[i].ierrors = ifm->ifm_data.ifi_ierrors;
         stats->interfaces[i].opackets = ifm->ifm_data.ifi_opackets;
         stats->interfaces[i].oerrors = ifm->ifm_data.ifi_oerrors;
         stats->interfaces[i].collisions = ifm->ifm_data.ifi_collisions;
         stats->interfaces[i].ibytes = ifm->ifm_data.ifi_ibytes;
         stats->interfaces[i].obytes = ifm->ifm_data.ifi_obytes;
         stats->interfaces[i].imcasts = ifm->ifm_data.ifi_imcasts;
         stats->interfaces[i].omcasts = ifm->ifm_data.ifi_omcasts;
         stats->interfaces[i].iqdrops = ifm->ifm_data.ifi_iqdrops;
         i++;
      }
   }
   if (!status) update_network_indom(stats, i, indom);
   return status;
}

int
refresh_nfs(struct nfsstats *stats)
{
    int	name[3];
    size_t length = sizeof(struct nfsstats);
    static int nfstype = -1;

    if (nfstype == -1) {
	struct vfsconf vfsconf;

	if (getvfsbyname("nfs", &vfsconf) == -1)
	    return -oserror();
	nfstype = vfsconf.vfc_typenum;
    }

    name[0] = CTL_VFS;
    name[1] = nfstype;
    name[0] = NFS_NFSSTATS;
    if (sysctl(name, 3, stats, &length, NULL, 0) == -1)
	return -oserror();
    stats->biocache_reads -= stats->read_bios;
    stats->biocache_writes -= stats->write_bios;
    stats->biocache_readlinks -= stats->readlink_bios;
    stats->biocache_readdirs -= stats->readdir_bios;
    return 0;
}
