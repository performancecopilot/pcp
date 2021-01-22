/*
 * Copyright (c) 2021 Red Hat.
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

#include <pcp/pmapi.h>
#include <pcp/pmda.h>
#include "ss_stats.h"

static ss_stats_t ss_p;

/* boolean value with no separate value, default 0 */
#define PM_TYPE_BOOL (PM_TYPE_UNKNOWN-1)

static struct {
    char *field;
    int len;
    int type;
    void *addr;
    int found;
} parse_table[] = {
    { "timer:", 6, PM_TYPE_STRING, &ss_p.timer_str },
    { "uid:", 4, PM_TYPE_U32, &ss_p.uid },
    { "ino:", 4, PM_TYPE_64, &ss_p.inode },
    { "sk:", 3, PM_TYPE_U64, &ss_p.sk },
    { "cgroup:", 7, PM_TYPE_STRING, &ss_p.cgroup },
    { "v6only:", 7, PM_TYPE_32, &ss_p.v6only },
    { "--- ", 4, PM_TYPE_UNKNOWN, NULL  },
    { "<-> ", 4, PM_TYPE_UNKNOWN, NULL  },
    { "--> ", 4, PM_TYPE_UNKNOWN, NULL  },
    { "skmem:", 6, PM_TYPE_STRING, &ss_p.skmem_str,  },
    { "ts ", 3, PM_TYPE_BOOL, &ss_p.ts },
    { "sack ", 5, PM_TYPE_BOOL, &ss_p.sack },
    { "cubic ", 6, PM_TYPE_BOOL, &ss_p.cubic },
    { "wscale:", 7, PM_TYPE_STRING, &ss_p.wscale_str },
    { "rto:", 4, PM_TYPE_DOUBLE, &ss_p.rto },
    { "rtt:", 4, PM_TYPE_STRING, &ss_p.round_trip_str  },
    { "ato:", 4, PM_TYPE_DOUBLE, &ss_p.ato },
    { "backoff:", 8, PM_TYPE_32, &ss_p.backoff },
    { "mss:", 4, PM_TYPE_U32, &ss_p.mss },
    { "pmtu:", 5, PM_TYPE_U32, &ss_p.pmtu },
    { "rcvmss:", 7, PM_TYPE_U32, &ss_p.rcvmss },
    { "advmss:", 7, PM_TYPE_U32, &ss_p.advmss },
    { "cwnd:", 5, PM_TYPE_U32, &ss_p.cwnd },
    { "lost:", 5, PM_TYPE_32, &ss_p.lost },
    { "ssthresh:", 9, PM_TYPE_U32, &ss_p.ssthresh },
    { "bytes_sent:", 11, PM_TYPE_U64, &ss_p.bytes_sent },
    { "bytes_retrans:", 14, PM_TYPE_U64, &ss_p.bytes_retrans },
    { "bytes_acked:", 12, PM_TYPE_U64, &ss_p.bytes_acked },
    { "bytes_received:", 15, PM_TYPE_U64, &ss_p.bytes_received },
    { "segs_out:", 9, PM_TYPE_U32, &ss_p.segs_out },
    { "segs_in:", 8, PM_TYPE_U32, &ss_p.segs_in },
    { "data_segs_out:", 14, PM_TYPE_U32, &ss_p.data_segs_out },
    { "data_segs_in:", 13, PM_TYPE_U32, &ss_p.data_segs_in },
    { "send ", 5, PM_TYPE_U32, &ss_p.send }, /* no ':' */
    { "lastsnd:", 8, PM_TYPE_U32, &ss_p.lastsnd },
    { "lastrcv:", 8, PM_TYPE_U32, &ss_p.lastrcv },
    { "lastack:", 8, PM_TYPE_U32, &ss_p.lastack },
    { "pacing_rate ", 12, PM_TYPE_DOUBLE, &ss_p.pacing_rate }, /* no ':' */
    { "delivery_rate ", 14, PM_TYPE_DOUBLE, &ss_p.delivery_rate }, /* no ':' */
    { "delivered:", 10, PM_TYPE_U32, &ss_p.delivered },
    { "app_limited ", 12, PM_TYPE_BOOL, &ss_p.app_limited },
    { "reord_seen:", 11, PM_TYPE_32, &ss_p.reord_seen },
    { "busy:", 5, PM_TYPE_U64, &ss_p.busy },
    { "unacked:", 8, PM_TYPE_32, &ss_p.unacked },
    { "rwnd_limited:", 13, PM_TYPE_U64, &ss_p.rwnd_limited },
    { "retrans:", 8, PM_TYPE_STRING, &ss_p.retrans_str },
    { "dsack_dups:", 11, PM_TYPE_U32, &ss_p.dsack_dups },
    { "rcv_rtt:", 8, PM_TYPE_DOUBLE, &ss_p.rcv_rtt },
    { "rcv_space:", 10, PM_TYPE_32, &ss_p.rcv_space },
    { "rcv_ssthresh:", 13, PM_TYPE_32, &ss_p.rcv_ssthresh },
    { "minrtt:", 7, PM_TYPE_DOUBLE, &ss_p.minrtt },
    { "notsent:", 8, PM_TYPE_U32, &ss_p.notsent },

    { NULL }
};

static char *
skip(char *p, char sep)
{
    while (*p) {
	for(p++; *p && *p != sep; p++);
	for(p++; *p && *p == sep; p++);
	if (*p)
	    return p;
    }
    return NULL;
}

static int
strcpy_to_comma(char *dst, char *src, int maxlen)
{
    int i;

    for (i=0; i < maxlen; i++) {
	if (src[i] == ',' || src[i] == '\0')
	    break;
	dst[i] = src[i];
    }
    dst[i] = '\0';

    return i;
}

/*
 * Parse subfields
 * skmem:(...)
 * timer:(...)
 * wscale:8,7
 * rtt:0.231/0.266
 */
static void
extract_subfields(ss_stats_t *s)
{
    char *p;

    /* skmem:(r0,rb2358342,t0,tb2626560,f0,w0,o0,bl0,d0) */
    for (p = s->skmem_str; p && *p;) {
	if (strncmp(p, "rb", 2) == 0)
	    s->skmem_rcv_buf = strtoul(p+2, NULL,10);
	else if (strncmp(p, "tb", 2) == 0)
	    s->skmem_snd_buf = strtoul(p+2, NULL, 10);
	else if (strncmp(p, "bl", 2) == 0)
	    s->skmem_back_log = strtoul(p+2, NULL, 10);
	else {
	    switch(*p) {
	    case 'r':
		s->skmem_rmem_alloc = strtoul(p+1, NULL, 10);
		break;
	    case 't':
		s->skmem_wmem_alloc = strtoul(p+1, NULL, 10);
		break;
	    case 'f':
		s->skmem_fwd_alloc = strtoul(p+1, NULL, 10);
		break;
	    case 'w':
		s->skmem_wmem_queued = strtoul(p+1, NULL, 10);
		break;
	    case 'o':
		s->skmem_ropt_mem = strtoul(p+1, NULL, 10);
		break;
	    case 'd':
		s->skmem_sock_drop = strtoul(p+1, NULL, 10);
		break;
	    default:
	    	break;
	    }
	}
	p = skip(p, ',');
    }

    /* timer:(keepalive,3min57sec,0) */
    p = s->timer_str;
    if (*p) {
	strcpy_to_comma(s->timer_name, p, sizeof(s->timer_name));
	p = skip(p, ',');
	strcpy_to_comma(s->timer_expire_str, p, sizeof(s->timer_expire_str));
	p = skip(p, ',');
	if (p && *p)
	    s->timer_retrans = strtol(p, NULL, 10);
    }

    /* wscale:8,7 */
    p = s->wscale_str;
    if (*p)
    	sscanf(p, "%d,%d", &s->wscale_snd, &s->wscale_rcv);

    /* rtt:0.231/0.266 */
    p = s->round_trip_str;
    if (*p)
    	sscanf(p, "%lf/%lf", &s->round_trip_rtt, &s->round_trip_rttvar);
}

int
ss_parse(char *line, ss_stats_t *ss)
{
    int i;
    char *r, *s, *p = line;
    int sts = 0;

    memset(&ss_p, 0, sizeof(ss_p));
    sscanf(line, "%s %s %u %u %s %s",
    	ss_p.af, ss_p.state, &ss_p.sendq, &ss_p.recvq, ss_p.src, ss_p.dst);

    /* skip first 6 fields, already scanned (above) */
    for (i=0; i < 6; i++)
    	p = skip(p, ' ');

    for (i=0; parse_table[i].field != NULL; i++)
    	parse_table[i].found = 0;

    while (p && *p && *p != '\n') {
	if (*p == ' ' || *p == '(' || *p == ')') {
	    p++;
	    continue;
	}
    	for (i=0; parse_table[i].field != NULL; i++) {
	    if (parse_table[i].found)
	    	continue;
	    if (strncmp(parse_table[i].field, p, parse_table[i].len) == 0) {
		parse_table[i].found = 1;
		switch (parse_table[i].type) {
		case PM_TYPE_STRING:
		    p += parse_table[i].len;
		    if (*p == '(')
		    	p++;
		    r = (char *)parse_table[i].addr;
		    for (s=p; *s && *s != ' ' && *s != '\n' && *s != ')'; s++)
			*r++ = *s; /* TODO check r len */
		    *r = '\0';
		    break;
		case PM_TYPE_32:
		    p += parse_table[i].len;
		    *(__int32_t *)(parse_table[i].addr) = strtol(p, NULL, 10);
		    break;
		case PM_TYPE_U32:
		    p += parse_table[i].len;
		    *(__uint32_t *)(parse_table[i].addr) = strtoul(p, NULL, 10);
		    break;
		case PM_TYPE_64:
		    p += parse_table[i].len;
		    *(__int64_t *)(parse_table[i].addr) = strtoll(p, NULL, 10);
		    break;
		case PM_TYPE_U64:
		    p += parse_table[i].len;
		    *(__uint64_t *)(parse_table[i].addr) = strtoull(p, NULL, 10);
		    break;
		case PM_TYPE_FLOAT:
		    p += parse_table[i].len;
		    *(float *)(parse_table[i].addr) = strtof(p, NULL);
		    break;
		case PM_TYPE_DOUBLE:
		    p += parse_table[i].len;
		    *(double *)(parse_table[i].addr) = strtod(p, NULL);
		    break;
		case PM_TYPE_UNKNOWN:
		case PM_TYPE_BOOL:
		    /* no separate value. ignore if NULL addr */
		    if (parse_table[i].addr)
			*(int *)(parse_table[i].addr) = 1;
		    break;
		}

	    	break;
	    }
	}
	p = skip(p, ' ');
    }

#if DEBUG
	fprintf(stderr, "\nLINE:%s", line);
	for (i=0; parse_table[i].field != NULL; i++)
	    if (parse_table[i].found)
		fprintf(stderr, "Found %s\n", parse_table[i].field);
#endif

    extract_subfields(&ss_p);
    *ss = ss_p; /* struct assign */

    return sts;
}

