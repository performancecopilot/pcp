/*
 * Copyright (c) 2019 Red Hat.
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

static void
remove_connection_from_queue(struct client *client)
{
    if (client->secure.pending.writes_buffer != NULL)
	free(client->secure.pending.writes_buffer);
    if (client->secure.pending.prev != NULL)
	*client->secure.pending.prev = client->secure.pending.next;
    memset(&client->secure.pending, 0, sizeof(client->secure.pending));
}

void
on_secure_client_close(struct client *client)
{
    remove_connection_from_queue(client);
    /* client->read and client->write freed by SSL_free */
    SSL_free(client->secure.ssl);
}

static void
maybe_flush_ssl(struct proxy *proxy, struct client *client)
{
    if (client->secure.pending.queued)
	return;

    if (BIO_pending(client->secure.write) == 0 &&
	client->secure.pending.writes_count > 0)
	return;

    client->secure.pending.next = proxy->pending_writes;
    if (client->secure.pending.next != NULL)
	client->secure.pending.next->secure.pending.prev = &client->secure.pending.next;
    client->secure.pending.prev = &proxy->pending_writes;
    client->secure.pending.queued = 1;

    proxy->pending_writes = client;
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
on_secure_client_read(struct proxy *proxy, struct client *client,
			ssize_t nread, const uv_buf_t *buf)
{
    size_t		bytes = 0;
    int			sts;

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
    stream_write_baton	*request = calloc(1, sizeof(stream_write_baton));
    struct proxy	*proxy = client->proxy;
    ssize_t		bytes;

    if ((bytes = BIO_pending(client->secure.write)) > 0) {
	request->buffer[0] = uv_buf_init(sdsnewlen(SDS_NOINIT, bytes), bytes);
	request->nbuffers = 1;
	BIO_read(client->secure.write, request->buffer[0].base, bytes);
	uv_callback_fire(&proxy->write_callbacks, request, NULL);
    }
}

void
flush_secure_module(struct proxy *proxy)
{
    struct client	*client, **head = &proxy->pending_writes;
    size_t		i;
    int			used, sts;

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
		    client->secure.pending.writes_buffer + sizeof(uv_buf_t) * used,
		    sizeof(uv_buf_t) * client->secure.pending.writes_count);
	}
    }
}

void
secure_client_write(struct client *client, stream_write_baton *request)
{
    struct proxy	*proxy = client->proxy;
    uv_buf_t		*dup;
    size_t		count, bytes;
    int			i, sts, defer = 0, maybe = 0;

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
    on_client_write((uv_write_t *)request, 0);	/* successfully written */

    if (maybe)
	maybe_flush_ssl(proxy, client);
}

static void
abort_secure_module_setup(void)
{
    ERR_print_errors_fp(stderr);
    exit(1);
}

void
setup_secure_module(struct proxy *proxy)
{
    const char		*certificates = NULL, *private_key = NULL;
    const char		*cipher_list = NULL, *cipher_suites = NULL;
    const char		*authority = NULL;
    SSL_CTX		*context;
    char		version[] = OPENSSL_VERSION_TEXT;
    sds			option;
    int			verify_mode = SSL_VERIFY_PEER, length;
    int			flags = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 |
				SSL_OP_NO_TLSv1 |SSL_OP_NO_TLSv1_1;

    if ((option = pmIniFileLookup(config, "pmproxy", "certificates")))
	certificates = option;
    if ((option = pmIniFileLookup(config, "pmproxy", "private_key")))
	private_key = option;
    if ((option = pmIniFileLookup(config, "pmproxy", "authority")))
	authority = option;
    if ((option = pmIniFileLookup(config, "pmproxy", "cipher_list")))
	cipher_list = option;
    if ((option = pmIniFileLookup(config, "pmproxy", "cipher_suites")))
	cipher_suites = option;

    if (!certificates || !private_key) {
	pmNotifyErr(LOG_INFO, "%s - no %s found\n", OPENSSL_VERSION_TEXT,
			certificates ? "private_key" : "certificates");
	return;
    }

    SSL_library_init();
    SSL_load_error_strings();

    if ((context = SSL_CTX_new(TLS_server_method())) == NULL) {
	pmNotifyErr(LOG_ERR, "Error creating initial secure server context\n");
	abort_secure_module_setup();
    }
    /* all secure client connections must use at least TLSv1.2 */
    SSL_CTX_set_options(context, flags);
    /* verification mode of client certificate, default is SSL_VERIFY_PEER */
    SSL_CTX_set_verify(context, verify_mode, NULL);

    if (authority) {
	SSL_CTX_set_client_CA_list(context, SSL_load_client_CA_file(authority));
	if (!SSL_CTX_load_verify_locations(context, authority, NULL)) {
	    pmNotifyErr(LOG_ERR, "Error loading the client CA list (%s)\n",
			authority);
	    abort_secure_module_setup();
	}
    }

    if (!SSL_CTX_use_certificate_chain_file(context, certificates)) {
	pmNotifyErr(LOG_ERR, "Error loading certificate chain: %s\n",
			certificates);
	abort_secure_module_setup();
    }
    if (!SSL_CTX_use_PrivateKey_file(context, private_key, SSL_FILETYPE_PEM)) {
	pmNotifyErr(LOG_ERR, "Error loading private key: %s\n", private_key);
	abort_secure_module_setup();
    }
    if (!SSL_CTX_check_private_key(context)) {
	pmNotifyErr(LOG_ERR, "Error validating the certificate\n");
	abort_secure_module_setup();
    }

    /* optional list of ciphers (TLSv1.2 and earlier) */
    if (cipher_list && !SSL_CTX_set_cipher_list(context, cipher_list)) {
	pmNotifyErr(LOG_ERR, "Error setting the cipher_list: %s\n",
			cipher_list);
	abort_secure_module_setup();
    }

    /* optional suites of ciphers (TLSv1.3 and later) */
    if (cipher_suites && !SSL_CTX_set_ciphersuites(context, cipher_suites)) {
	pmNotifyErr(LOG_ERR, "Error setting the cipher_suites: %s\n",
			cipher_suites);
	abort_secure_module_setup();
    }

    /*
     * OpenSSL setup complete - log openssl version and ciphers in use
     * Version format expected: "OpenSSL 1.1.1b FIPS  26 Feb 2019".
     */
    if ((length = strlen(version)) > 20)
	version[length - 13] = '\0';
    pmNotifyErr(LOG_INFO, "%s setup\n", version);
    if (cipher_suites)
	pmNotifyErr(LOG_INFO, "Using cipher suites: %s\n", cipher_suites);
    if (cipher_list)
	pmNotifyErr(LOG_INFO, "Using cipher list: %s\n", cipher_list);
    proxy->ssl = context;
}

void
close_secure_module(struct proxy *proxy)
{
    if (proxy->ssl) {
	SSL_CTX_free(proxy->ssl);
	proxy->ssl = NULL;
    }
}
