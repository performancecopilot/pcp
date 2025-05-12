/*
 * Copyright (c) 2021-2022 Red Hat.
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
#define SZ_ADDR_PORT	64	/* max size of an address + port */
typedef struct ss_stats {
    int			instid;
    char		netid[16];	/* protocol identifer */
    char		state[16];	/* socket state */
    __int32_t		recvq;
    __int32_t		sendq;
    char		src[SZ_ADDR_PORT];	/* source address and port */
    char		dst[SZ_ADDR_PORT];	/* dest address and port */
    __int64_t 		inode;		/* socket inode in VFS */
    char		timer_str[64];
    char		timer_name[16];	/* e.g. "keepalive" */
    char		timer_expire_str[16];
    __int32_t		timer_retrans;
    __uint32_t		uid;
    __uint64_t		sk;
    char		cgroup[128];
    __int32_t		v6only;
    char		skmem_str[64];
    __int32_t		skmem_rmem_alloc;
    __int32_t		skmem_wmem_alloc;
    __int32_t		skmem_rcv_buf;
    __int32_t		skmem_snd_buf;
    __int32_t		skmem_fwd_alloc;
    __int32_t		skmem_wmem_queued;
    __int32_t		skmem_ropt_mem;
    __int32_t		skmem_back_log;
    __int32_t		skmem_sock_drop;
    __int32_t		ts;
    __int32_t		sack;
    __int32_t		cubic;
    char		wscale_str[16];
    __int32_t		wscale_snd;
    __int32_t		wscale_rcv;
    double		rto;
    char		round_trip_str[16];
    double		round_trip_rtt;
    double		round_trip_rttvar;
    double		ato;
    __int32_t		backoff;
    __int32_t		mss;
    __uint32_t		pmtu;
    __int32_t		rcvmss;
    __int32_t		advmss;
    __uint32_t		cwnd;
    __int32_t		ssthresh;
    __uint64_t		bytes_sent;
    __uint64_t		bytes_retrans;
    __uint64_t		bytes_acked;
    __uint64_t		bytes_received;
    __uint32_t		segs_out;
    __uint32_t		segs_in;
    __uint32_t		data_segs_out;
    __uint32_t		data_segs_in;
    double		send;
    __uint32_t		lastsnd;
    __uint32_t		lastrcv;
    __uint32_t		lastack;
    double		pacing_rate;
    double		delivery_rate;
    __uint32_t		delivered;
    __int32_t		app_limited;
    __int32_t		reord_seen;
    __uint64_t		busy;
    __int32_t		unacked;
    __uint64_t		rwnd_limited;
    char		retrans_str[8];
    __int32_t		dsack_dups;
    double		rcv_rtt;
    __int32_t		rcv_space;
    __int32_t		lost;
    __int32_t		rcv_ssthresh;
    double		minrtt;
    __uint32_t		notsent;
} ss_stats_t;

extern int ss_refresh(int);
extern int ss_parse(char *, int, ss_stats_t *);
extern FILE *ss_open_stream(void);
extern void ss_close_stream(FILE *);
extern char *ss_filter; /* current string value of network.persocket.filter */
