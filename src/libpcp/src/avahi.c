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

#if HAVE_AVAHI
#include <avahi-client/publish.h>
#include <avahi-common/alternative.h>
#include <avahi-common/thread-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>

struct __pmServerPresence {
    char *avahi_service_name;
    const char *avahi_service_tag;
    int port;
    AvahiThreadedPoll *avahi_threaded_poll;
    AvahiClient *avahi_client;
    AvahiEntryGroup *avahi_group;
};

static void entryGroupCallback(
    AvahiEntryGroup *g,
    AvahiEntryGroupState state,
    void *userdata
);

static void
createServices(AvahiClient *c, __pmServerPresence *s)
{
    char *n;
    int ret;
    assert (c);

    /*
     * If this is the first time we're called, create a new
     * entry group if necessary.
     */
    if (! s->avahi_group) {
	if (! (s->avahi_group = avahi_entry_group_new(c, entryGroupCallback, s))) {
	    __pmNotifyErr(LOG_ERR, "avahi_entry_group_new () failed: %s",
			  avahi_strerror (avahi_client_errno(c)));
	    goto fail;
	}
    }

    /*
     * If the group is empty (either because it was just created, or
     * because it was reset previously, add our entries.
     */
    if (avahi_entry_group_is_empty(s->avahi_group)) {
	__pmNotifyErr(LOG_INFO, "Adding Avahi service '%s'", s->avahi_service_name);

	/*
	 * We will now add our service to the entry group. Only services with the
	 * same name should be put in the same entry group.
	 */
	if ((ret = avahi_entry_group_add_service(s->avahi_group, AVAHI_IF_UNSPEC,
						 AVAHI_PROTO_UNSPEC,
						 (AvahiPublishFlags)0,
						 s->avahi_service_name,
						 s->avahi_service_tag,
						 NULL, NULL, s->port, NULL))
	    < 0) {
	    if (ret == AVAHI_ERR_COLLISION)
		goto collision;

	    __pmNotifyErr(LOG_ERR, "Failed to add %s service: %s",
			  s->avahi_service_tag, avahi_strerror(ret));
	    goto fail;
	}

	/* Tell the server to register the service. */
	if ((ret = avahi_entry_group_commit(s->avahi_group)) < 0) {
	    __pmNotifyErr(LOG_ERR, "Failed to commit avahi entry group: %s",
			  avahi_strerror(ret));
	    goto fail;
	}
    }
    return;

 collision:
    /*
     * A service name collision with a local service happened.
     * pick a new name.
     */
    n = avahi_alternative_service_name(s->avahi_service_name);
    avahi_free(s->avahi_service_name);
    s->avahi_service_name = n;
    __pmNotifyErr(LOG_WARNING, "Avahi service name collision, renaming service to '%s'",
		  s->avahi_service_name);
    avahi_entry_group_reset(s->avahi_group);
    createServices (c, s);
    return;

 fail:
    avahi_entry_group_reset(s->avahi_group);
}

static void
entryGroupCallback(
    AvahiEntryGroup *g,
    AvahiEntryGroupState state,
    void *userdata
)
{
    __pmServerPresence *s = (__pmServerPresence *)userdata;
    assert(g == s->avahi_group || s->avahi_group == NULL);
    s->avahi_group = g;

    /* Called whenever the entry group state changes. */
    switch (state) {
	case AVAHI_ENTRY_GROUP_ESTABLISHED:
	    /* The entry group has been established successfully. */
	    __pmNotifyErr(LOG_INFO, "Avahi service '%s' successfully established.",
			  s->avahi_service_name);
	    break;

	case AVAHI_ENTRY_GROUP_COLLISION: {
	    char *n;
	    /* A service name collision with a remote service happened.
	     * Let's pick a new name ...
	     */
	    n = avahi_alternative_service_name(s->avahi_service_name);
	    avahi_free(s->avahi_service_name);
	    s->avahi_service_name = n;
	    __pmNotifyErr(LOG_WARNING, "Avahi service name collision, renaming service to '%s'",
			  s->avahi_service_name);

	    /* ... and recreate the services. */
	    createServices(avahi_entry_group_get_client(g), s);
	    break;
	}

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

static void cleanupClient(__pmServerPresence *s) {
    /* This also frees the entry group, if any. */
    if (s->avahi_client) {
	avahi_client_free(s->avahi_client);
	s->avahi_client = 0;
	s->avahi_group = 0;
    }
}
 
static void
clientCallback(AvahiClient *c, AvahiClientState state, void *userdata) {
    assert(c);
    __pmServerPresence *s = (__pmServerPresence *)userdata;

    /* Called whenever the client or server state changes. */
    switch (state) {
	case AVAHI_CLIENT_S_RUNNING:
	    /*
	     * The server has started successfully and registered its host
	     * name on the network, so it's time to create our services.
	     */
	    createServices(c, s);
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
		cleanupClient(s);
		s->avahi_client =
		    avahi_client_new(
			avahi_threaded_poll_get(s->avahi_threaded_poll),
			(AvahiClientFlags)AVAHI_CLIENT_NO_FAIL,
			clientCallback, s, & error);
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
	    if (s->avahi_group)
		avahi_entry_group_reset (s->avahi_group);
	    break;

	case AVAHI_CLIENT_CONNECTING:
	    /*
	     * The avahi-daemon is not currently running. Our service will be
	     * advertised if/when the deamon is started.
	     */
	    __pmNotifyErr(LOG_WARNING,
			  "The Avahi daemon is not running. Avahi service '%s' will be established when the deamon is started",
			  s->avahi_service_name);
	    break;
    }
}

static void
cleanup(__pmServerPresence *s) {
    if (s == NULL)
	return;

    if (s->avahi_service_name)
	__pmNotifyErr(LOG_INFO, "Removing Avahi service '%s'",
		      s->avahi_service_name);

    /* Stop the avahi client, if it's running. */
    if (s->avahi_threaded_poll)
	avahi_threaded_poll_stop(s->avahi_threaded_poll);

    /* Clean up the avahi objects. The order of freeing these is significant. */
    cleanupClient(s);
    if (s->avahi_threaded_poll) {
	avahi_threaded_poll_free(s->avahi_threaded_poll);
	s->avahi_threaded_poll = 0;
    }
    if (s->avahi_service_name) {
	avahi_free(s->avahi_service_name);
	s->avahi_service_name = 0;
    }
}

/* The entry point for the avahi client thread. */
static __pmServerPresence *
publishService(const char *serviceName, const char *serviceTag, int port)
{
  int error;
  __pmServerPresence *s = calloc(1, sizeof(*s));

  if (s) {
      /* Save the given parameters. */
      s->avahi_service_name = avahi_strdup(serviceName); /* may get reallocated */
      s->avahi_service_tag = serviceTag;
      s->port = port;

      /* Allocate main loop object. */
      if (! (s->avahi_threaded_poll = avahi_threaded_poll_new())) {
	  __pmNotifyErr(LOG_ERR, "Failed to create avahi threaded poll object.");
	  goto fail;
      }

      /*
       * Always allocate a new client. Passing AVAHI_CLIENT_NO_FAIL allows the client to be
       * created, even if the avahi daemon is not running. Our service will be advertised
       * if/when the daemon is started.
       */
      s->avahi_client =
	  avahi_client_new(avahi_threaded_poll_get(s->avahi_threaded_poll),
			   (AvahiClientFlags)AVAHI_CLIENT_NO_FAIL,
			   clientCallback, s, &error);

      /* Check whether creating the client object succeeded. */
      if (! s->avahi_client) {
	  __pmNotifyErr(LOG_ERR, "Failed to create avahi client: %s",
			avahi_strerror(error));
	  goto fail;
      }

      /* Run the main loop. */
      avahi_threaded_poll_start(s->avahi_threaded_poll);
  }
      
  return s;

 fail:
  cleanup(s);
  free(s);
  return NULL;
}
#endif // HAVE_AVAHI

__pmServerPresence *
__pmServerAdvertisePresence(
    const char *serviceName __attribute__ ((unused)),
    const char *serviceTag __attribute__ ((unused)),
    int port __attribute__ ((unused))
) {
#if HAVE_AVAHI
    return publishService (serviceName, serviceTag, port);
#else
    __pmNotifyErr(LOG_ERR, "Unable to advertise presence on the network. Avahi is not available");
    return NULL;
#endif
}

void
__pmServerUnadvertisePresence(__pmServerPresence *s __attribute__ ((unused))) {
#if HAVE_AVAHI
    cleanup(s);
#endif
}
