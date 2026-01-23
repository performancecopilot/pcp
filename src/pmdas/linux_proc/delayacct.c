/*
 * Copyright (c) 2026 Red Hat.
 * Copyright (c) 2024 htop dev team.  (linux/LibNl.c)
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
#include "delayacct.h"

#ifdef HAVE_DELAYACCT
#include <dlfcn.h>
#include <linux/netlink.h>
#include <linux/taskstats.h>
#include <netlink/attr.h>
#include <netlink/handlers.h>
#include <netlink/msg.h>

static struct nl_sock *netlink_socket;
static int netlink_family;

static void *libnlHandle;
static void *libnlGenlHandle;

static void (*sym_nl_close)(struct nl_sock*);
static int (*sym_nl_connect)(struct nl_sock*, int);
static int (*sym_nl_recvmsgs_default)(struct nl_sock*);
static int (*sym_nl_send_sync)(struct nl_sock*, struct nl_msg*);
static struct nl_sock* (*sym_nl_socket_alloc)(void);
static void (*sym_nl_socket_free)(struct nl_sock*);
static int (*sym_nl_socket_modify_cb)(struct nl_sock*, enum nl_cb_type, enum nl_cb_kind, nl_recvmsg_msg_cb_t, void*);
static void* (*sym_nla_data)(const struct nlattr*);
static struct nlattr* (*sym_nla_next)(const struct nlattr*, int*);
static int (*sym_nla_put_u32)(struct nl_msg*, int, uint32_t);
static struct nl_msg* (*sym_nlmsg_alloc)(void);
static struct nlmsghdr* (*sym_nlmsg_hdr)(struct nl_msg*);
static void (*sym_nlmsg_free)(struct nl_msg*);

static int (*sym_genl_ctrl_resolve)(struct nl_sock*, const char*);
static int (*sym_genlmsg_parse)(struct nlmsghdr*, int, struct nlattr**, int, const struct nla_policy*);
static void* (*sym_genlmsg_put)(struct nl_msg*, uint32_t, uint32_t, int, int, int, uint8_t, uint8_t);

static void
unload_libnl(void)
{
    sym_nl_close = NULL;
    sym_nl_connect = NULL;
    sym_nl_recvmsgs_default = NULL;
    sym_nl_send_sync = NULL;
    sym_nl_socket_alloc = NULL;
    sym_nl_socket_free = NULL;
    sym_nl_socket_modify_cb = NULL;
    sym_nla_data = NULL;
    sym_nla_next = NULL;
    sym_nla_put_u32 = NULL;
    sym_nlmsg_alloc = NULL;
    sym_nlmsg_free = NULL;
    sym_nlmsg_hdr = NULL;

    sym_genl_ctrl_resolve = NULL;
    sym_genlmsg_parse = NULL;
    sym_genlmsg_put = NULL;

    if (libnlGenlHandle) {
       dlclose(libnlGenlHandle);
       libnlGenlHandle = NULL;
    }
    if (libnlHandle) {
       dlclose(libnlHandle);
       libnlHandle = NULL;
    }
}

static int
load_libnl(void)
{
    if (libnlHandle && libnlGenlHandle)
        return 0;

    libnlHandle = dlopen("libnl-3.so", RTLD_LAZY);
    if (!libnlHandle) {
        libnlHandle = dlopen("libnl-3.so.200", RTLD_LAZY);
        if (!libnlHandle) {
            goto dlfailure;
        }
    }

    libnlGenlHandle = dlopen("libnl-genl-3.so", RTLD_LAZY);
    if (!libnlGenlHandle) {
        libnlGenlHandle = dlopen("libnl-genl-3.so.200", RTLD_LAZY);
        if (!libnlGenlHandle) {
            goto dlfailure;
        }
    }

    /* Clear any errors */
    dlerror();
 
    #define resolve(handle, symbolname) do {                         \
        *(void **)(&sym_##symbolname) = dlsym(handle, #symbolname);  \
        if (!sym_##symbolname || dlerror() != NULL) {                \
            goto dlfailure;                                          \
        }                                                            \
    } while(0)

    resolve(libnlHandle, nl_close);
    resolve(libnlHandle, nl_connect);
    resolve(libnlHandle, nl_recvmsgs_default);
    resolve(libnlHandle, nl_send_sync);
    resolve(libnlHandle, nl_socket_alloc);
    resolve(libnlHandle, nl_socket_free);
    resolve(libnlHandle, nl_socket_modify_cb);
    resolve(libnlHandle, nla_data);
    resolve(libnlHandle, nla_next);
    resolve(libnlHandle, nla_put_u32);
    resolve(libnlHandle, nlmsg_alloc);
    resolve(libnlHandle, nlmsg_free);
    resolve(libnlHandle, nlmsg_hdr);

    resolve(libnlGenlHandle, genl_ctrl_resolve);
    resolve(libnlGenlHandle, genlmsg_parse);
    resolve(libnlGenlHandle, genlmsg_put);

    #undef resolve

    return 0;

dlfailure:
    unload_libnl();
    return -1;
}

static void
initNetlinkSocket(void)
{
    if (load_libnl() < 0)
        return;

    netlink_socket = sym_nl_socket_alloc();
    if (netlink_socket == NULL)
        return;
    if (sym_nl_connect(netlink_socket, NETLINK_GENERIC) < 0)
        return;
    netlink_family = sym_genl_ctrl_resolve(netlink_socket, TASKSTATS_GENL_NAME);
}

void
delayacct_stop(void)
{
    if (netlink_socket) {
        sym_nl_close(netlink_socket);
        sym_nl_socket_free(netlink_socket);
        netlink_socket = NULL;
    }
    unload_libnl();
}

void
delayacct_init(void)
{
    initNetlinkSocket();
    atexit(delayacct_stop);
}

static int
handleNetlinkMsg(struct nl_msg *nlmsg, void *arg)
{
    struct delayacct *dap = (struct delayacct *) arg;
    struct nlmsghdr *nlhdr;
    struct nlattr *nlattrs[TASKSTATS_TYPE_MAX + 1];
    const struct nlattr *nlattr;
    struct taskstats stats;
    void *nladata;
    int rem;

    nlhdr = sym_nlmsg_hdr(nlmsg);

    if (sym_genlmsg_parse(nlhdr, 0, nlattrs, TASKSTATS_TYPE_MAX, NULL) < 0)
        return NL_SKIP;

    if ((nlattr = nlattrs[TASKSTATS_TYPE_AGGR_PID]) ||
	(nlattr = nlattrs[TASKSTATS_TYPE_NULL])) {
        nladata = sym_nla_data(sym_nla_next(sym_nla_data(nlattr), &rem));
        memcpy(&stats, nladata, sizeof(stats));
        /* Linux Kernel "Documentation/accounting/taskstats-struct.rst" */
        dap->swapin_delay_total = stats.swapin_delay_total;
        dap->blkio_delay_total = stats.blkio_delay_total;
        dap->cpu_delay_total = stats.cpu_delay_total;
    }
    return NL_OK;
}

/*
 * Gather delay-accounting information (thread-specific data)
 */
int
delayacct_info(int pid, delayacct_t *dap)
{
    struct nl_msg* msg;

    if (!netlink_socket) {
	initNetlinkSocket();
	if (!netlink_socket)
	    goto delayacct_failure;
    }

    if (sym_nl_socket_modify_cb(netlink_socket, NL_CB_VALID, NL_CB_CUSTOM, handleNetlinkMsg, dap) < 0)
	goto delayacct_failure;

    if (!(msg = sym_nlmsg_alloc()))
	goto delayacct_failure;

    if (!sym_genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, netlink_family, 0, NLM_F_REQUEST, TASKSTATS_CMD_GET, TASKSTATS_VERSION))
	sym_nlmsg_free(msg);

    if (sym_nla_put_u32(msg, TASKSTATS_CMD_ATTR_PID, pid) < 0)
	sym_nlmsg_free(msg);

    if (sym_nl_send_sync(netlink_socket, msg) < 0)
	goto delayacct_failure;

    if (sym_nl_recvmsgs_default(netlink_socket) < 0)
	goto delayacct_failure;

    return 0;

delayacct_failure:
    memset(dap, 0, sizeof(*dap));
    return PM_ERR_VALUE;
}

#else
int
delayacct_info(int pid, delayacct_t *dap)
{
    (void)pid; (void)dap;
    return -EOPNOTSUPP;
}

void
delayacct_init(void)
{
    /* nothing to do here */
}
#endif /* HAVE_DELAYACCT */
