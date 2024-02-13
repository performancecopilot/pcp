/*
 * SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)
 * Copyright (c) 2021 Bytedance
 * https://github.com/bytedance/netatop-bpf
 */
#ifndef __NETATOP__
#define __NETATOP__

struct taskcount {
	unsigned long long	tcpsndpacks;
	unsigned long long	tcpsndbytes;
	unsigned long long	tcprcvpacks;
	unsigned long long	tcprcvbytes;

	unsigned long long	udpsndpacks;
	unsigned long long	udpsndbytes;
	unsigned long long	udprcvpacks;
	unsigned long long	udprcvbytes;

	/* space for future extensions */
};

extern struct netatop_bpf *skel;
extern int tgid_map_fd;
extern int nr_cpus;

void bpf_attach(struct netatop_bpf *);
void bpf_destroy(struct netatop_bpf *);
void cleanup(struct netatop_bpf *);
#endif
