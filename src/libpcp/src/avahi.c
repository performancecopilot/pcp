/*
 * Copyright (c) 2013 Red Hat.
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

#include <assert.h>

#include "pmapi.h"
#include "impl.h"
#include "internal.h"
#include "avahi.h"

static AvahiThreadedPoll	*threadedPoll;
static AvahiClient		*client;
static AvahiEntryGroup		*group;

static __pmServerPresence	**activeServices;
static int			nActiveServices;
static int			szActiveServices;

struct __pmServerAvahiPresence {
    char	*serviceName;
    char	*serviceTag;
    int		collisions;
};

static void entryGroupCallback(AvahiEntryGroup *, AvahiEntryGroupState, void *);

static int
renameService(__pmServerPresence *s)
{
    /*
     * Each service must have a unique name on the local network.
     * When there is a collision, we try to rename the service.
     * However, we need to limit the number of attempts, since the
     * service namespace could be maliciously flooded with service
     * names designed to maximize collisions.
     * Arbitrarily choose a limit of 65535, which is the number of
     * TCP ports.
     */
    ++s->avahi->collisions;
    if (s->avahi->collisions >= 65535) {
	__pmNotifyErr(LOG_ERR, "Too many service name collisions for Avahi service %s",
		      s->avahi->serviceTag);
	return -EBUSY;
    }

    /*
     * Use the avahi-supplied function to generate a new service name.
     */
    char *n = avahi_alternative_service_name(s->avahi->serviceName);

    if (pmDebug & DBG_TRACE_DISCOVERY)
	__pmNotifyErr(LOG_INFO, "Avahi service name collision, renaming service '%s' to '%s'",
		      s->avahi->serviceName, n);
    avahi_free(s->avahi->serviceName);
    s->avahi->serviceName = n;

    return 0;
}

static int
renameServices(void)
{
    int i;
    int rc = 0;
    __pmServerPresence *s;

    for (i = 0; i < szActiveServices; ++i) {
	s = activeServices[i];
	if (s == NULL)
	    continue; /* empty entry */
	if ((rc = renameService(s)) < 0)
	    break;
    }

    return rc;
}

static void
createServices(AvahiClient *c)
{
    __pmServerPresence *s;
    int ret;
    int i;

    assert (c);

     /*
      * Create a new entry group, if necessary, or reset the existing one.
      */
    if (group == NULL) {
	if ((group = avahi_entry_group_new(c, entryGroupCallback, NULL)) == NULL) {
 	    __pmNotifyErr(LOG_ERR, "avahi_entry_group_new () failed: %s",
 			  avahi_strerror (avahi_client_errno(c)));
	    return;
 	}
    }
    else
	avahi_entry_group_reset(group);

    /*
     * We will now add our services to the entry group.
     */
    for (i = 0; i < szActiveServices; ++i) {
	s = activeServices[i];
	if (s == NULL)
	    continue; /* empty table entry */

	if (pmDebug & DBG_TRACE_DISCOVERY)
	    __pmNotifyErr(LOG_INFO, "Adding %s Avahi service on port %d",
			  s->avahi->serviceName, s->port);

	/* Loop until no collisions */
	for (;;) {
	    ret = avahi_entry_group_add_service(group, AVAHI_IF_UNSPEC,
						AVAHI_PROTO_UNSPEC,
						(AvahiPublishFlags)0,
						s->avahi->serviceName,
						s->avahi->serviceTag,
						NULL, NULL, s->port, NULL);
	    if (ret == AVAHI_OK)
		break; /* success! */
	    if (ret == AVAHI_ERR_COLLISION) {
		/*
		 * A service name collision with a local service happened.
		 * Pick a new name.  Since a service may be listening on
		 * multiple ports, this is expected to happen sometimes -
		 * do not issue warnings here.
		 */
		if (renameService(s) < 0) {
		    /* Too many collisions. Message already issued */
		    goto fail;
		}
		continue; /* try again */
	    }

	    __pmNotifyErr(LOG_ERR, "Failed to add %s Avahi service on port %d: %s",
			  s->avahi->serviceName, s->port, avahi_strerror(ret));
	    goto fail;
	}
    }

    /* Tell the server to register the services. */
    if ((ret = avahi_entry_group_commit(group)) < 0) {
	__pmNotifyErr(LOG_ERR, "Failed to commit avahi entry group: %s",
		      avahi_strerror(ret));
	goto fail;
    }

    return;

 fail:
    avahi_entry_group_reset(group);
}

static void
entryGroupCallback(AvahiEntryGroup *g, AvahiEntryGroupState state, void *data)
{
    (void)data;

    assert(g != NULL);
    assert(group == NULL || group == g);
    group = g;

    /* Called whenever the entry group state changes. */
    switch (state) {
	case AVAHI_ENTRY_GROUP_ESTABLISHED:
	    /* The entry group has been established successfully. */
	    if (pmDebug & DBG_TRACE_DISCOVERY)
		__pmNotifyErr(LOG_INFO, "Avahi services successfully established.");
	    break;

	case AVAHI_ENTRY_GROUP_COLLISION:
	    /*
	     * A service name collision with a remote service happened.
	     * Unfortunately, we don't know which entry collided.
	     * We need to rename them all and recreate the services.
	     */
	    if (renameServices() == 0)
		createServices(avahi_entry_group_get_client(g));
	    break;

	case AVAHI_ENTRY_GROUP_FAILURE:
	    /* Some kind of failure happened. */
	    __pmNotifyErr(LOG_ERR, "Avahi entry group failure: %s",
			  avahi_strerror(avahi_client_errno(avahi_entry_group_get_client(g))));
	    break;

	case AVAHI_ENTRY_GROUP_UNCOMMITED:
	case AVAHI_ENTRY_GROUP_REGISTERING:
	    break;
    }
}

static void
cleanupClient(void)
{
    /* This also frees the entry group, if any. */
    if (client) {
	avahi_client_free(client);
	client = NULL;
	group = NULL;
    }
}
 
static void
clientCallback(AvahiClient *c, AvahiClientState state, void *userData)
{
    assert(c);
    (void)userData;

    /* Called whenever the client or server state changes. */
    switch (state) {
	case AVAHI_CLIENT_S_RUNNING:
	    /*
	     * The server has started successfully and registered its host
	     * name on the network, so it's time to create our services.
	     */
	    createServices(c);
	    break;

	case AVAHI_CLIENT_FAILURE:
	    __pmNotifyErr(LOG_ERR, "Avahi client failure: %s",
			  avahi_strerror(avahi_client_errno(c)));
	    if (avahi_client_errno (c) == AVAHI_ERR_DISCONNECTED) {
		int error;
		/*
		 * The client has been disconnected; probably because the
		 * avahi daemon has been restarted. We can free the client
		 * here and try to reconnect using a new one.
		 * Passing AVAHI_CLIENT_NO_FAIL allows the new client to be
		 * created, even if the avahi daemon is not running. Our
		 * service will be advertised if/when the daemon is started.
		 */
		cleanupClient();
		client = avahi_client_new(avahi_threaded_poll_get(threadedPoll),
					  (AvahiClientFlags)AVAHI_CLIENT_NO_FAIL,
					  clientCallback, NULL, & error);
	    }
	    break;

	case AVAHI_CLIENT_S_COLLISION:
	    /*
	     * Drop our registered services. When the server is back
	     * in AVAHI_SERVER_RUNNING state we will register them
	     * again with the new host name.
	     * Fall through ...
	     */
	case AVAHI_CLIENT_S_REGISTERING:
	    /*
	     * The server records are now being established. This
	     * might be caused by a host name change. We need to wait
	     * for our own records to register until the host name is
	     * properly esatblished.
	     */
	    if (group)
		avahi_entry_group_reset (group);
	    break;

	case AVAHI_CLIENT_CONNECTING:
	    /*
	     * The avahi-daemon is not currently running. Our service will be
	     * advertised if/when the daemon is started.
	     */
	    if (pmDebug & DBG_TRACE_DISCOVERY)
		__pmNotifyErr(LOG_INFO,
			      "The Avahi daemon is not running. "
			      "Avahi services will be established when the daemon is started");
	    break;
    }
}

static void
cleanup(__pmServerAvahiPresence *s)
{
    if (s == NULL)
	return;

    if (s->serviceName) {
	avahi_free(s->serviceName);
	s->serviceName = NULL;
    }
    if (s->serviceTag) {
	avahi_free(s->serviceTag);
	s->serviceTag = NULL;
    }
}

/* Publish a new service. */
static void
addService(__pmServerPresence *s)
{
    int i;
    size_t size;

    /* Find an empty slot in the table of active services. */
    for (i = 0; i < szActiveServices; ++i) {
	if (activeServices[i] == NULL)
	    break;
    }

    /* Do we need to grow the table? */
    if (i >= szActiveServices) {
	++szActiveServices;
	size = szActiveServices * sizeof(*activeServices);
	activeServices = realloc(activeServices, size);
	if (activeServices == NULL) {
	    __pmNoMem("__pmServerAvahiAdvertisePresence: can't allocate service table",
		  size, PM_FATAL_ERR);
	}
    }

    /* Add the service to the table. */
    activeServices[i] = s;
    ++nActiveServices;
}

/* Publish a new service. */
static void
removeService(__pmServerPresence *s)
{
    int i;

    /*
     * Find the service in the table of active services.
     * We can do this by comparing the pointers directly, since
     * that's how the services were added.
     */
    for (i = 0; i < szActiveServices; ++i) {
	/*
	 * Did we find it? If so, clear the entry.
	 * We don't free it here.
	 */
	if (activeServices[i] == s) {
	    activeServices[i] = NULL;
	    --nActiveServices;
	    return;
	}
    }
}

/* Publish a new service. */
static void
publishService(__pmServerPresence *s)
{
    int error;

    /* Add the service to our list of active services. */
    addService(s);

    /* Is this the first service to be added? */
    if (threadedPoll == NULL) {
	/* Allocate main loop object. */
	if ((threadedPoll = avahi_threaded_poll_new()) == NULL) {
	    __pmNotifyErr(LOG_ERR, "Failed to create avahi threaded poll object.");
	    goto fail;
	}

	/*
	 * Always allocate a new client. Passing AVAHI_CLIENT_NO_FAIL allows
	 * the client to be created, even if the avahi daemon is not running.
	 * Our service will be advertised if/when the daemon is started.
	 */
	client = avahi_client_new(avahi_threaded_poll_get(threadedPoll),
				  (AvahiClientFlags)AVAHI_CLIENT_NO_FAIL,
				  clientCallback, NULL, &error);

	/* Check whether creating the client object succeeded. */
	if (! client) {
	    __pmNotifyErr(LOG_ERR, "Failed to create avahi client: %s",
			  avahi_strerror(error));
	    goto fail;
	}

	/* Start the main loop. */
	avahi_threaded_poll_start(threadedPoll);
    }
    else {
	/* Stop the main loop while we recreate the services. */
	avahi_threaded_poll_lock(threadedPoll);
	createServices(client);
	avahi_threaded_poll_unlock(threadedPoll);
    }

    return;

 fail:
    cleanup(s->avahi);
    free(s->avahi);
}

void
__pmServerAvahiAdvertisePresence(__pmServerPresence *s)
{
    size_t size;

    /* Allocate the avahi server presence. */
    s->avahi = malloc(sizeof(*s->avahi));
    if (s->avahi == NULL) {
	__pmNoMem("__pmServerAvahiAdvertisePresence: can't allocate avahi service data",
		  sizeof(*s->avahi), PM_FATAL_ERR);
    }

    /*
     * The service spec is simply the name of the server. Use it to
     * construct the avahi service name and service tag.
     */
    size = sizeof("PCP ") + strlen(s->serviceSpec); /* includes room for the nul */
    if ((s->avahi->serviceName = avahi_malloc(size)) == NULL) {
	__pmNoMem("__pmServerAvahiAdvertisePresence: can't allocate service name",
		  size, PM_FATAL_ERR);
    }
    sprintf(s->avahi->serviceName, "PCP %s", s->serviceSpec);

    size = sizeof("_._tcp") + strlen(s->serviceSpec); /* includes room for the nul */
    if ((s->avahi->serviceTag = avahi_malloc(size)) == NULL) {
	__pmNoMem("__pmServerAvahiAdvertisePresence: can't allocate service tag",
		  size, PM_FATAL_ERR);
    }
    sprintf(s->avahi->serviceTag, "_%s._tcp", s->serviceSpec);
    s->avahi->collisions = 0;
    
    /* Now publish the avahi service. */
    publishService(s);
}

void
__pmServerAvahiUnadvertisePresence(__pmServerPresence *s)
{
    /* Not an avahi service? */
    if (s->avahi == NULL)
	return;

    if (pmDebug & DBG_TRACE_DISCOVERY)
	__pmNotifyErr(LOG_INFO, "Removing Avahi service '%s' on port %d",
		      s->avahi->serviceName , s->port);

    /* Remove and cleanup the service. */
    removeService(s);
    cleanup(s->avahi);
    free(s->avahi);
    s->avahi = NULL;

    /* Nothing to do if the client is not running. */
    if (threadedPoll == NULL)
	return;

    /* Stop the main loop. */

    /* If no services remain, then shut down the avahi client. */
    if (nActiveServices == 0) {
	/* Clean up the avahi objects. The order of freeing these is significant. */
	avahi_threaded_poll_stop(threadedPoll);
	cleanupClient();
	avahi_threaded_poll_free(threadedPoll);
	threadedPoll = NULL;
	return;
    }

    /* Otherwise, stop the main loop while we recreate the services. */
    avahi_threaded_poll_lock(threadedPoll);
    createServices(client);
    avahi_threaded_poll_unlock(threadedPoll);
}
