/*
 * Copyright (c) 2019,2021-2022 Red Hat.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */
#include "server.h"
#include <openssl/err.h>
#include <openssl/opensslv.h>
#include <openssl/ssl.h>

/* called with proxy->write_mutex locked */
static void
remove_connection_from_queue(struct client *client)
{
    struct proxy *proxy = client->proxy;

    if (client->secure.pending.writes_buffer != NULL)
	free(client->secure.pending.writes_buffer);
    if (client->secure.pending.prev == NULL) {
	/* next (if any) becomes first in pending_writes list */
    	proxy->pending_writes = client->secure.pending.next;
	if (proxy->pending_writes)
	    proxy->pending_writes->secure.pending.prev = NULL;
    }
    else {
	/* link next and prev */
	struct secure_client_pending *pending = &client->secure.pending;
	pending->prev->secure.pending.next = pending->next;
	pending->next->secure.pending.prev = pending->prev;
    }
    memset(&client->secure.pending, 0, sizeof(client->secure.pending));
}

void
on_secure_client_close(struct client *client)
{
    if (pmDebugOptions.auth || pmDebugOptions.http)
	fprintf(stderr, "%s: client %p\n", "on_secure_client_close", client);

    uv_mutex_lock(&client->proxy->write_mutex);
    remove_connection_from_queue(client);
    uv_mutex_unlock(&client->proxy->write_mutex);
    /* client->read and client->write freed by SSL_free */
    SSL_free(client->secure.ssl);
}

static void
maybe_flush_ssl(struct proxy *proxy, struct client *client)
{
    struct client *c;

    if (client->secure.pending.queued)
	return;

    if (BIO_pending(client->secure.write) == 0 &&
	client->secure.pending.writes_count > 0)
	return;

    uv_mutex_lock(&proxy->write_mutex);
    if (proxy->pending_writes == NULL) {
    	proxy->pending_writes = client;
	client->secure.pending.prev = client->secure.pending.next = NULL;
    }
    else {
    	for (c=proxy->pending_writes; c->secure.pending.next; c = c->secure.pending.next)
	    ; /**/
	c->secure.pending.next = client;
	client->secure.pending.prev = c;
    }
    client->secure.pending.queued = 1;
    uv_mutex_unlock(&proxy->write_mutex);
}

static void
setup_secure_client(struct proxy *proxy, struct client *client)
{
    if (pmDebugOptions.auth || pmDebugOptions.http)
	fprintf(stderr, "%s: SSL/TLS connection initiated by client %p\n",
		"setup_secure_client", client);

    client->secure.ssl = SSL_new(proxy->ssl);
    SSL_set_accept_state(client->secure.ssl);

    client->secure.read = BIO_new(BIO_s_mem());
    client->secure.write = BIO_new(BIO_s_mem());

    BIO_set_nbio(client->secure.read, 1);
    BIO_set_nbio(client->secure.write, 1);
    SSL_set_bio(client->secure.ssl, client->secure.read, client->secure.write);
}

void
on_secure_client_write(struct client *client)
{
    if (pmDebugOptions.auth || pmDebugOptions.http)
	fprintf(stderr, "%s: client %p\n", "on_secure_client_write", client);
}

void
on_secure_client_read(struct proxy *proxy, struct client *client,
			ssize_t nread, const uv_buf_t *buf)
{
    size_t		bytes = 0;
    int			sts;

    if (pmDebugOptions.auth || pmDebugOptions.http)
	fprintf(stderr, "%s: client %p\n", "on_secure_client_read", client);

    /* once-off per-client SSL setup first time through */
    if (client->stream.secure == 0) {
	setup_secure_client(proxy, client);
	client->stream.secure = 1;
    }

    do {
	sts = BIO_write_ex(client->secure.read, buf->base + bytes, nread - bytes, &bytes);
	if (sts <= 0)
	    break;
    } while (bytes);

    do {
	sts = SSL_read_ex(client->secure.ssl, buf->base, buf->len, &bytes);
	if (sts > 0)
	    on_protocol_read((uv_stream_t *)&client->stream, bytes, buf);
	else if (SSL_get_error(client->secure.ssl, sts) == SSL_ERROR_WANT_READ)
	    maybe_flush_ssl(proxy, client); /* defer to libuv if more to read */
	else
	    client_close(client);
	break;
    } while (1);
}

static void
flush_ssl_buffer(struct client *client)
{
    struct stream_write_baton	*request;
    ssize_t			bytes;

    if ((bytes = BIO_pending(client->secure.write)) > 0) {
	request = calloc(1, sizeof(struct stream_write_baton));
	request->buffer[0] = uv_buf_init(sdsnewlen(NULL, bytes), bytes);
	request->nbuffers = 1;
	request->writer.data = client;
	BIO_read(client->secure.write, request->buffer[0].base, bytes);
	uv_write(&request->writer, (uv_stream_t *)&client->stream,
			request->buffer, request->nbuffers, on_client_write);
    }
}

void
flush_secure_module(struct proxy *proxy)
{
    struct client	*client, **head;
    size_t		i, used;
    int			sts;

    uv_mutex_lock(&proxy->write_mutex);
    head = &proxy->pending_writes;
    while ((client = *head) != NULL) {
	flush_ssl_buffer(client);

	if (client->secure.pending.writes_count == 0) {
	    remove_connection_from_queue(client);
	    continue;
	}

	/* We have pending writes to deal with, add them into the SSL buffer */
	used = 0;
	for (i = 0; i < client->secure.pending.writes_count; i++) {
	    sts = SSL_write(client->secure.ssl,
			    client->secure.pending.writes_buffer[i].base,
			    client->secure.pending.writes_buffer[i].len);
	    if (sts > 0) {
		used++;
		continue;
	    }
	    sts = SSL_get_error(client->secure.ssl, sts);
	    if (sts == SSL_ERROR_WANT_WRITE) {
		flush_ssl_buffer(client);
		i--; /* retry */
		continue;
	    }
	    if (sts != SSL_ERROR_WANT_READ) {
		client->secure.pending.queued = 0;
		client_close(client);
		break;
	    }
	    /*
	     * Waiting for reads from the network.
	     * Cannot remove this instance yet, so instead adjust the
	     * pointer and start the scan/remove from this position.
	     */
	    head = &client->secure.pending.next;
	    break;
	}

	flush_ssl_buffer(client);

	if (used == client->secure.pending.writes_count) {
	    remove_connection_from_queue(client);
	} else {
	    client->secure.pending.writes_count -= used;
	    memmove(client->secure.pending.writes_buffer,
		    client->secure.pending.writes_buffer + used,
		    sizeof(uv_buf_t) * client->secure.pending.writes_count);
	}
    }
    uv_mutex_unlock(&proxy->write_mutex);
}

void
secure_client_write(struct client *client, struct stream_write_baton *request)
{
    struct proxy	*proxy = client->proxy;
    uv_buf_t		*dup;
    size_t		count, bytes;
    unsigned int	i;
    int			sts, defer = 0, maybe = 0;

    for (i = 0; i < request->nbuffers; i++) {
	if (defer == 0) {
	    if ((sts = SSL_write(client->secure.ssl,
			request->buffer[i].base, request->buffer[i].len)) > 0) {
		maybe = 1;
		continue;
	    }
	    sts = SSL_get_error(client->secure.ssl, sts);
	    if (sts == SSL_ERROR_WANT_WRITE) {
		flush_ssl_buffer(client);
		sts = SSL_write(client->secure.ssl,
			request->buffer[i].base, request->buffer[i].len);
		if (sts > 0)
		    continue;
	    }
	    if (sts != SSL_ERROR_WANT_READ) {
		on_client_write(&request->writer, 1);	/* fail client */
		return;
	    }
	}

	/*
	 * Need to re-negotiate with the client, so must defer this write -
	 * add it to the pending set for now and retry after the next read.
	 * Once we start deferring buffers within a request, all remaining
	 * buffers are deferred such that we do not send data out of order.
	 */
	defer = 1;
	count = client->secure.pending.writes_count++;
	bytes = sizeof(uv_buf_t) * count;
	if ((client->secure.pending.writes_buffer = realloc(
			client->secure.pending.writes_buffer, bytes)) == NULL) {
	    on_client_write(&request->writer, 1);	/* fail client */
	    return;
	}
	dup = &client->secure.pending.writes_buffer[count-1];
	dup->base = request->buffer[i].base;
	dup->len = request->buffer[i].len;
	maybe = 1;
    }
    on_client_write(&request->writer, 0);	/* successfully written */

    if (maybe)
	maybe_flush_ssl(proxy, client);
}

void
setup_secure_module(struct proxy *proxy)
{
    sds		option = pmIniFileLookup(config, "pmproxy", "secure.enabled");
    int		compat = 0;

    /* if explicitly disabled, we can leave here immediately */
    if (option && strncmp(option, "false", sdslen(option)) == 0)
	return;

    /* parse /etc/pcp/tls.conf (optional, preferred) */
    __pmSecureConfigInit();
    if (__pmGetSecureConfig(&proxy->tls) == -ENOENT)
	compat = 1;

    if (compat && option && strncmp(option, "true", sdslen(option)) != 0)
	return;

    /* backwards compatibility with original pmproxy.conf configuration */
    if ((option = pmIniFileLookup(config, "pmproxy", "certificates"))) {
	free(proxy->tls.certfile);
	proxy->tls.certfile = strdup(option);
	compat = 1;
    }
    if ((option = pmIniFileLookup(config, "pmproxy", "private_key"))) {
	free(proxy->tls.keyfile);
	proxy->tls.keyfile = strdup(option);
	compat = 1;
    }
    if ((option = pmIniFileLookup(config, "pmproxy", "authority"))) {
	free(proxy->tls.cacertfile);
	proxy->tls.cacertfile = strdup(option);
	compat = 1;
    }
    if ((option = pmIniFileLookup(config, "pmproxy", "cipher_list"))) {
	free(proxy->tls.ciphers);
	proxy->tls.ciphers = strdup(option);
	compat = 1;
    }
    if ((option = pmIniFileLookup(config, "pmproxy", "cipher_suites"))) {
	free(proxy->tls.ciphersuites);
	proxy->tls.ciphersuites = strdup(option);
	compat = 1;
    }

    /* default to enabling OpenSSL anytime a valid tls.conf exists */
    if (!compat && !proxy->tls.certfile) {
	__pmFreeSecureConfig(&proxy->tls);
	return;
    }

    /* OpenSSL initialization based on the configuration read */
    proxy->ssl = (SSL_CTX *)__pmSecureServerInit(&proxy->tls);
    if (proxy->ssl == NULL) {
	__pmFreeSecureConfig(&proxy->tls);
	return;
    }

    /* OpenSSL setup; log library version, ciphers in use */
#ifdef OPENSSL_VERSION_STR
    pmNotifyErr(LOG_INFO, "OpenSSL %s setup", OPENSSL_VERSION_STR);
#else /* back-compat and not ideal, includes date */
    pmNotifyErr(LOG_INFO, "%s setup", OPENSSL_VERSION_TEXT);
#endif
    if (proxy->tls.ciphersuites)
	pmNotifyErr(LOG_INFO, "Using cipher suites: %s\n",
				proxy->tls.ciphersuites);
    else if (proxy->tls.ciphers)
	pmNotifyErr(LOG_INFO, "Using cipher list: %s\n",
				proxy->tls.ciphers);

    if (compat) {
	char	*path = pmGetOptionalConfig("PCP_TLSCONF_PATH");

	pmNotifyErr(LOG_INFO,
		"TLS configured in pmproxy.conf, please switch to %s",
		path ? path : "/etc/pcp/tls.conf");
    }
}

void
reset_secure_module(struct proxy *proxy)
{
    /* SIGHUP: no-op */
    (void)proxy;
}

void
close_secure_module(struct proxy *proxy)
{
    if (proxy->ssl) {
	__pmSecureServerShutdown(proxy->ssl, &proxy->tls);
	proxy->ssl = NULL;
    }
}
