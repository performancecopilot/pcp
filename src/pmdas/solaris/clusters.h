#ifndef __PMDA_SOLARIS_CLUSTERS_H
#define __PMDA_SOLARIS_CLUSTERS_H

/*
 * PMID cluster numbers
 *
 * Clusters are used to index method[] table and shall be contigious
 */
#define SCLR_SYSINFO		0
#define SCLR_DISK		1
#define SCLR_NETIF		2
#define SCLR_ZPOOL		3
#define SCLR_ZFS		4
#define SCLR_ZPOOL_PERDISK	5
#define SCLR_NETLINK		6
#define SCLR_FSFLUSH		7
#define SCLR_ARCSTATS		8
#define SCLR_FILESYS		9

#endif
