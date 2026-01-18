/*
 * NFS statistics
 * Copyright (c) 2026 Red Hat.
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
#include <sys/sysctl.h>
#include <sys/mount.h>
#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>
#include "pmapi.h"
#include "pmda.h"
#include "nfs.h"
#include "darwin.h"
#include "network.h"

int
refresh_nfs(struct nfsstats *stats)
{
	int	name[3];
	size_t length = sizeof(struct nfsclntstats);
	static int nfstype = -1;

	if (nfstype == -1) {
		struct vfsconf vfsconf;

		if (getvfsbyname("nfs", &vfsconf) == -1)
			return -oserror();
		nfstype = vfsconf.vfc_typenum;
	}

	name[0] = CTL_VFS;
	name[1] = nfstype;
	name[2] = NFS_NFSSTATS;
	if (sysctl(name, 3, &stats->client, &length, NULL, 0) == -1)
		return -oserror();
	return 0;
}

int
fetch_nfs(unsigned int item, unsigned int inst, pmAtomValue *atom)
{
	extern struct nfsstats mach_nfs;
	extern int mach_nfs_error;
	extern pmdaIndom indomtab[];

	if (mach_nfs_error)
		return mach_nfs_error;
	switch (item) {
	case 94: /* nfs3.client.calls */
		for (atom->ull = 0, inst = 0; inst < NFS3_RPC_COUNT; inst++)
			atom->ull += mach_nfs.client.rpccntv3[inst];
		return 1;
	case 95: /* nfs3.client.reqs */
		if (inst < 0 || inst >= NFS3_RPC_COUNT)
			return PM_ERR_INST;
		atom->ull = mach_nfs.client.rpccntv3[inst];
		return 1;
	case 96: /* nfs3.server.calls */
		for (atom->ull = 0, inst = 0; inst < NFS3_RPC_COUNT; inst++)
			atom->ull += mach_nfs.server.srvrpccntv3[inst];
		return 1;
	case 97: /* nfs3.server.reqs */
		if (inst < 0 || inst >= NFS3_RPC_COUNT)
			return PM_ERR_INST;
		atom->ull = mach_nfs.server.srvrpccntv3[inst];
		return 1;
	case 123:	/* rpc.server.nqnfs.leases    -- deprecated */
	case 124:	/* rpc.server.nqnfs.maxleases -- deprecated */
	case 125:	/* rpc.server.nqnfs.getleases -- deprecated */
		return PM_ERR_APPVERSION;
	}
	return PM_ERR_PMID;
}
