/*
 * Copyright (c) 2024 Red Hat.
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

#include "pmapi.h"
#include "pmda.h"
#include "tcpconn.h"

#include <sys/sysctl.h>
#include <netinet/in_pcb.h>
#include <netinet/tcp_var.h>

/*
 * Soft limit for TCP connection buffer allocation (bytes)
 * Log a debug message if buffer exceeds this size
 * Default: 10MB (handles ~20,000 connections)
 */
#define TCPCONN_BUFFER_SOFT_LIMIT (10 * 1024 * 1024)

/*
 * Refresh TCP connection state statistics
 * Parses net.inet.tcp.pcblist64 sysctl to count connections by state
 */
int
refresh_tcpconn(tcpconn_stats_t *stats)
{
    char *buf = NULL;
    size_t len = 0;
    struct xinpgen *xig, *oxig;
    struct xtcpcb64 *tp;

    /* Get required buffer size */
    if (sysctlbyname("net.inet.tcp.pcblist64", NULL, &len, NULL, 0) == -1)
        return -oserror();

    /* Log warning if buffer size exceeds soft limit (debug only) */
    if (len > TCPCONN_BUFFER_SOFT_LIMIT) {
        pmNotifyErr(LOG_DEBUG,
            "refresh_tcpconn: large buffer allocation (%zu bytes, ~%zu connections)",
            len, len / 500);
    }

    /* Allocate buffer */
    buf = malloc(len);
    if (!buf)
        return -ENOMEM;

    /* Fetch PCB list */
    if (sysctlbyname("net.inet.tcp.pcblist64", buf, &len, NULL, 0) == -1) {
        free(buf);
        return -oserror();
    }

    /* Zero state counters */
    memset(stats, 0, sizeof(*stats));

    /* Parse PCB list and count connections by state */
    oxig = xig = (struct xinpgen *)buf;

    for (xig = (struct xinpgen *)((char *)xig + xig->xig_len);
         xig->xig_len > sizeof(struct xinpgen);
         xig = (struct xinpgen *)((char *)xig + xig->xig_len))
    {
        tp = (struct xtcpcb64 *)xig;

        /* Validate state and increment counter */
        if (tp->t_state >= 0 && tp->t_state < TCP_NSTATES)
            stats->state[tp->t_state]++;
    }

    free(buf);
    return 0;
}
