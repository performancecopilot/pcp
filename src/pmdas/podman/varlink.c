/*
 * Copyright (c) 2018 Red Hat.
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

#include "podman.h"
#include <varlink.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>

typedef struct varlink {
    int			epoll_fd;
    int			signal_fd;
    VarlinkConnection	*connection;
} varlink_t;

typedef struct varlink_reply {
    char		*error;
    VarlinkObject	*parameters;
} varlink_reply_t;

static inline int
epoll_control(int epfd, int op, int fd, uint32_t events, void *ptr)
{
    struct epoll_event	event = {.events = events, .data = {.ptr = ptr}};

    return epoll_ctl(epfd, op, fd, &event);
}

static varlink_t *
varlink_connect(void)
{
    static varlink_t	varlink;
    static int		setup;
    sigset_t		mask;
    int			sts;

    if (setup == 0) {
	if ((varlink.epoll_fd = epoll_create1(EPOLL_CLOEXEC)) < 0)
	    return NULL;
	sigemptyset(&mask);
	sigaddset(&mask, SIGTERM);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGPIPE);
	sigprocmask(SIG_BLOCK, &mask, NULL);
	varlink.signal_fd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
	if (varlink.signal_fd < 0)
	    return NULL;
	epoll_control(varlink.epoll_fd, EPOLL_CTL_ADD, varlink.signal_fd, EPOLLIN, NULL);
	setup = 1;
    }

    sts = varlink_connection_new(&varlink.connection, "unix:/run/podman/io.podman");
    if (sts != 0)
	return NULL;

    return &varlink;
}

static void
varlink_disconnect(varlink_t *link)
{
    epoll_control(link->epoll_fd, EPOLL_CTL_DEL,
		  varlink_connection_get_fd(link->connection), 0, NULL);
    varlink_connection_close(link->connection);
    varlink_connection_free(link->connection);
    link->connection = NULL;
}

static long
varlink_reply_callback(VarlinkConnection *connection, const char *error,
                VarlinkObject *parameters, uint64_t flags, void *arg)
{
    varlink_reply_t	*reply = (varlink_reply_t *)arg;

    if (error != NULL) {
	if ((reply->error = strdup(error)) == NULL)
	    return -ENOMEM;
    }
    reply->parameters = varlink_object_ref(parameters);
    return 0;
}

static long
varlink_connection_wait(varlink_t *link)
{
    struct epoll_event	event;
    int			sts, timeout = -1;

    if ((sts = varlink_connection_get_events(link->connection)) < 0)
	return sts;

    if ((sts = epoll_control(link->epoll_fd, EPOLL_CTL_ADD,
			varlink_connection_get_fd(link->connection),
			varlink_connection_get_events(link->connection),
			link->connection)) < 0 && errno != EEXIST)
	return sts;

    for (;;) {
	sts = epoll_wait(link->epoll_fd, &event, 1, timeout);
	if (sts < 0) {
	    if (errno == EINTR)
		continue;
	    return -errno;
	}
	if (sts == 0)
	    return -ETIMEDOUT;
	if (event.data.ptr == link->connection) {
	    epoll_control(link->epoll_fd, EPOLL_CTL_MOD,
			varlink_connection_get_fd(link->connection),
			varlink_connection_get_events(link->connection),
			link->connection);
	    break;	/* ready */
	}
	if (event.data.ptr == NULL) {
	    struct signalfd_siginfo	info;
	    long			size;

	    size = read(link->signal_fd, &info, sizeof(info));
	    if (size != sizeof(info))
		continue;
	    pmNotifyErr(LOG_ERR, "podman command interrupted\n");
	    return -EINTR;
	}
    }

    if ((sts = varlink_connection_process_events(link->connection, 0)) != 0)
	return sts;

    return 0;
}

static void
refresh_container_info(VarlinkObject *info, container_info_t *ip)
{
    VarlinkArray	*args;
    const char		*temp;
    size_t		bytes, length = 0;
    char		cmd[BUFSIZ] = {0};
    int			i, sts, count;

    temp = NULL;
    varlink_object_get_string(info, "names", &temp);
    ip->name = temp? podman_strings_insert(temp) : -1;

    /* extract the command, stored as an array of strings */
    sts = varlink_object_get_array(info, "command", &args);
    count = sts < 0? 0 : varlink_array_get_n_elements(args);
    for (i = 0; i < count; i++) {
	temp = NULL;
	varlink_array_get_string(args, i, &temp);
	bytes = temp? strlen(temp) : 0;
	if (bytes > 0 && bytes < sizeof(cmd) - length - 1) {
	    strcat(cmd, temp);
	    strcat(cmd, " ");
	}
	length += bytes + 1;
    }
    if (length > 0) {
	cmd[length-1] = '\0';
	ip->command = podman_strings_insert(cmd);
    } else {
	ip->command = -1;
    }

    temp = NULL;
    varlink_object_get_string(info, "status", &temp);
    ip->status = temp? podman_strings_insert(temp) : -1;
    varlink_object_get_int(info, "rootfssize", &ip->rootfssize);
    varlink_object_get_int(info, "rwsize", &ip->rwsize);
    varlink_object_get_bool(info, "running", &ip->running);
}

static int
varlink_container_info(varlink_t *link, char *name, container_info_t *ip)
{
    varlink_reply_t	reply = {0};
    VarlinkObject	*info;
    int			sts;

    if (pmDebugOptions.attr)
	fprintf(stderr, "refresh container info for %s\n", name);

    varlink_object_new(&reply.parameters);
    varlink_object_set_string(reply.parameters, "name", name);

    sts = varlink_connection_call(link->connection,
			"io.podman.GetContainer",
			reply.parameters, 0, varlink_reply_callback, &reply);
    varlink_object_unref(reply.parameters);
    if (sts != 0)
	return sts;

    if ((sts = varlink_connection_wait(link)) < 0)
	goto done;

    if (reply.error) {
	if (strcmp(reply.error, "io.podman.NoContainerRunning") != 0)
	    fprintf(stderr, "Error: %s %s - %s\n", "io.podman.GetContainer",
			    name, reply.error);
	free(reply.error);
	goto done;
    }

    sts = varlink_object_get_object(reply.parameters, "container", &info);
    if (sts != 0)
	goto done;

    refresh_container_info(info, ip);

done:
    varlink_object_unref(reply.parameters);
    return sts;
}

static int
varlink_container_stats(varlink_t *link, char *name, container_stats_t *cp)
{
    varlink_reply_t	reply = {0};
    VarlinkObject	*stats;
    const char		*temp;
    int			sts;

    if (pmDebugOptions.attr)
	fprintf(stderr, "refresh container stats for %s\n", name);

    varlink_object_new(&reply.parameters);
    varlink_object_set_string(reply.parameters, "name", name);

    sts = varlink_connection_call(link->connection,
			"io.podman.GetContainerStats",
			reply.parameters, 0, varlink_reply_callback, &reply);
    varlink_object_unref(reply.parameters);
    if (sts != 0)
	return sts;

    if ((sts = varlink_connection_wait(link)) < 0)
	goto done;

    if (reply.error) {
	if (strcmp(reply.error, "io.podman.NoContainerRunning") != 0)
	    fprintf(stderr, "Error: %s %s - %s\n", "io.podman.GetContainerStats",
			    name, reply.error);
	free(reply.error);
	goto done;
    }

    sts = varlink_object_get_object(reply.parameters, "container", &stats);
    if (sts != 0)
	goto done;
    varlink_object_get_int(stats, "net_input", &cp->net_input);
    varlink_object_get_int(stats, "net_output", &cp->net_output);
    varlink_object_get_int(stats, "block_input", &cp->block_input);
    varlink_object_get_int(stats, "block_output", &cp->block_output);
    varlink_object_get_float(stats, "cpu", &cp->cpu);
    varlink_object_get_int(stats, "cpu_nano", &cp->cpu_nano);
    varlink_object_get_int(stats, "system_nano", &cp->system_nano);
    varlink_object_get_int(stats, "mem_usage", &cp->mem_usage);
    varlink_object_get_int(stats, "mem_limit", &cp->mem_limit);
    varlink_object_get_float(stats, "mem_perc", &cp->mem_perc);
    varlink_object_get_int(stats, "pids", &cp->nprocesses);
    varlink_object_get_string(stats, "name", &temp);
    cp->name = podman_strings_insert(temp);

done:
    varlink_object_unref(reply.parameters);
    return sts;
}

void
refresh_podman_container(pmInDom indom, char *name, state_flags_t flags)
{
    container_t		*cp;
    varlink_t		*link;
    int			sts;

    if (pmDebugOptions.attr)
	fprintf(stderr, "refresh podman container %s\n", name);

    if ((link = varlink_connect()) == NULL)
	return;

    if ((sts = pmdaCacheLookupName(indom, name, NULL, (void **)&cp)) < 0) {
	if ((cp = calloc(1, sizeof(container_t))) == NULL)
	    return;
	cp->id = podman_strings_insert(name);
	if (pmDebugOptions.attr)
	    fprintf(stderr, "adding container %s (%u)\n", name, cp->id);
    }
    pmdaCacheStore(indom, PMDA_CACHE_ADD, name, (void *)cp);

    if (flags & STATE_INFO) {
	if ((sts = varlink_container_info(link, name, &cp->info)) == 0)
	    cp->flags |= STATE_INFO;
    }
    if (flags & STATE_STATS) {
	if ((sts = varlink_container_stats(link, name, &cp->stats)) == 0)
	    cp->flags |= STATE_STATS;
    }
}

static int
varlink_container_list(varlink_t *link, pmInDom indom)
{
    container_t		*cp;
    varlink_reply_t	reply = {0};
    VarlinkObject	*state;
    VarlinkArray	*list;
    const char		*id;
    int			i, sts, count = 0;

    if (pmDebugOptions.attr)
	fprintf(stderr, "list containers\n");

    sts = varlink_connection_call(link->connection, "io.podman.ListContainers",
			reply.parameters, 0, varlink_reply_callback, &reply);
    if (sts != 0)
	return sts;

    if ((sts = varlink_connection_wait(link)) < 0)
	goto done;

    if (reply.error) {
	fprintf(stderr, "Error: %s - %s\n", "io.podman.ListContainers",
			reply.error);
	free(reply.error);
	goto done;
    }

    sts = varlink_object_get_array(reply.parameters, "containers", &list);
    if (sts < 0)
	goto done;

    count = varlink_array_get_n_elements(list);
    for (i = 0; i < count; i++) {
	varlink_array_get_object(list, i, &state);
	varlink_object_get_string(state, "id", &id);
	if ((sts = pmdaCacheLookupName(indom, id, NULL, (void **)&cp)) < 0) {
	    if ((cp = calloc(1, sizeof(container_t))) == NULL)
		continue;
	    cp->id = podman_strings_insert(id);
	    if (pmDebugOptions.attr)
		fprintf(stderr, "adding container %s (%u)\n", id, cp->id);
	}
	pmdaCacheStore(indom, PMDA_CACHE_ADD, id, (void *)cp);
	refresh_container_info(state, &cp->info);
	cp->flags |= STATE_INFO;
    }

done:
    varlink_object_unref(reply.parameters);
    return count;
}

static int
varlink_pod_list(varlink_t *link, pmInDom indom)
{
    pod_info_t		*pp;
    varlink_reply_t	reply = {0};
    VarlinkObject	*state;
    VarlinkArray	*list;
    const char		*id, *temp;
    int			i, sts, count = 0;

    if (pmDebugOptions.attr)
	fprintf(stderr, "list pods\n");

    sts = varlink_connection_call(link->connection, "io.podman.ListPods",
			reply.parameters, 0, varlink_reply_callback, &reply);
    if (sts != 0)
	return sts;

    if ((sts = varlink_connection_wait(link)) < 0)
	goto done;

    if (reply.error) {
	fprintf(stderr, "Error: %s - %s\n", "io.podman.ListPods", reply.error);
	free(reply.error);
	goto done;
    }

    sts = varlink_object_get_array(reply.parameters, "pods", &list);
    if (sts < 0)
	goto done;

    count = varlink_array_get_n_elements(list);
    for (i = 0; i < count; i++) {
	varlink_array_get_object(list, i, &state);
	varlink_object_get_string(state, "id", &id);
	if ((sts = pmdaCacheLookupName(indom, id, NULL, (void **)&pp)) < 0) {
	    if ((pp = calloc(1, sizeof(pod_info_t))) == NULL)
		continue;
	    pp->id = podman_strings_insert(id);
	    if (pmDebugOptions.attr)
		fprintf(stderr, "adding pod %s (%u)\n", id, pp->id);
	}
	pmdaCacheStore(indom, PMDA_CACHE_ADD, id, (void *)pp);

	temp = NULL;
	varlink_object_get_string(state, "name", &temp);
	pp->name = temp? podman_strings_insert(temp) : -1;
	temp = NULL;
	varlink_object_get_string(state, "cgroup", &temp);
	pp->cgroup = temp? podman_strings_insert(temp) : -1;
	temp = NULL;
	varlink_object_get_string(state, "status", &temp);
	pp->status = temp? podman_strings_insert(temp) : -1;
	temp = NULL;
	varlink_object_get_string(state, "numberofcontainers", &temp);
	pp->ncontainers = temp ? atoi(temp) : 0;

	pp->flags |= STATE_INFO;
    }

done:
    varlink_object_unref(reply.parameters);
    return count;
}

void
refresh_podman_containers(pmInDom indom, state_flags_t flags)
{
    container_t		*cp;
    varlink_t		*varlink;
    char		*name;
    int			inst, sts;

    pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

    if ((varlink = varlink_connect()) == NULL)
	return;

    /* extract list of containers - this also provides all container 'info' */
    sts = varlink_container_list(varlink, indom);
    if ((sts == 0) || (!(flags & STATE_STATS)))
	goto done;

    /* walk active entries, refreshing container 'stats' */
    for (pmdaCacheOp(indom, PMDA_CACHE_WALK_REWIND);;) {
	if ((inst = pmdaCacheOp(indom, PMDA_CACHE_WALK_NEXT)) < 0)
	    break;
	if (!pmdaCacheLookup(indom, inst, &name, (void **)&cp) || !cp)
	    continue;
	if ((sts = varlink_container_stats(varlink, name, &cp->stats)) == 0)
	    cp->flags |= STATE_STATS;
    }

done:
    varlink_disconnect(varlink);
}

static int
varlink_pod_info(varlink_t *link, char *name, pod_info_t *pp)
{
    varlink_reply_t	reply = {0};
    VarlinkObject	*info;
    const char		*temp;
    int			sts;

    if (pmDebugOptions.attr)
	fprintf(stderr, "refresh pod info for %s\n", name);

    varlink_object_new(&reply.parameters);
    varlink_object_set_string(reply.parameters, "name", name);

    sts = varlink_connection_call(link->connection,
			"io.podman.GetPod",
			reply.parameters, 0, varlink_reply_callback, &reply);
    varlink_object_unref(reply.parameters);
    if (sts != 0)
	return sts;

    if ((sts = varlink_connection_wait(link)) < 0)
	goto done;

    if (reply.error) {
	if (strcmp(reply.error, "io.podman.NoPodRunning") != 0)
	    fprintf(stderr, "Error: %s %s - %s\n", "io.podman.GetPod",
			    name, reply.error);
	free(reply.error);
	goto done;
    }

    sts = varlink_object_get_object(reply.parameters, "container", &info);
    if (sts != 0)
	goto done;
    temp = NULL;
    varlink_object_get_string(info, "name", &temp);
    pp->name = temp? podman_strings_insert(temp) : -1;
    temp = NULL;
    varlink_object_get_string(info, "cgroup", &temp);
    pp->cgroup = temp? podman_strings_insert(temp) : -1;
    temp = NULL;
    varlink_object_get_string(info, "status", &temp);
    pp->status = temp? podman_strings_insert(temp) : -1;
    temp = NULL;
    varlink_object_get_string(info, "numberofcontainers", &temp);
    pp->ncontainers = temp? atoi(temp) : 0;

done:
    varlink_object_unref(reply.parameters);
    return sts;
}

void
refresh_podman_pod_info(pmInDom indom, char *name)
{
    pod_info_t		*pp;
    varlink_t		*link;
    int			sts;

    if (pmDebugOptions.attr)
	fprintf(stderr, "refresh podman pod %s\n", name);

    if ((link = varlink_connect()) == NULL)
	return;

    if ((sts = pmdaCacheLookupName(indom, name, NULL, (void **)&pp)) < 0) {
	if ((pp = calloc(1, sizeof(pod_info_t))) == NULL)
	    return;
	pp->id = podman_strings_insert(name);
	if (pmDebugOptions.attr)
	    fprintf(stderr, "adding pod %s (%u)\n", name, pp->id);
    }
    pmdaCacheStore(indom, PMDA_CACHE_ADD, name, (void *)pp);

    if ((sts = varlink_pod_info(link, name, pp)) == 0)
	pp->flags |= STATE_INFO;
}

void
refresh_podman_pods_info(pmInDom indom)
{
    varlink_t		*varlink;

    pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

    if ((varlink = varlink_connect()) == NULL)
	return;

    varlink_pod_list(varlink, indom);

    varlink_disconnect(varlink);
}
