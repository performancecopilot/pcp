/*
  This file is part of libmicrohttpd
  (C) 2007, 2008, 2009, 2010, 2011, 2012 Daniel Pittman and Christian Grothoff
  
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
  
  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
  
  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

/**
 * @file internal.h
 * @brief  internal shared structures
 * @author Daniel Pittman
 * @author Christian Grothoff
 */

#ifndef INTERNAL_H
#define INTERNAL_H

#include "platform.h"
#include "microhttpd.h"
#if HTTPS_SUPPORT
#include <gnutls/gnutls.h>
#endif

/**
 * Should we perform additional sanity checks at runtime (on our internal
 * invariants)?  This may lead to aborts, but can be useful for debugging.
 */
#define EXTRA_CHECKS MHD_NO

#define MHD_MAX(a,b) ((a)<(b)) ? (b) : (a)
#define MHD_MIN(a,b) ((a)<(b)) ? (a) : (b)


/**
 * Size by which MHD usually tries to increment read/write buffers.
 * TODO: we should probably get rid of this magic constant and
 * put in code to automatically determine a good value.
 */
#define MHD_BUF_INC_SIZE 2048

/**
 * Handler for fatal errors.
 */
extern MHD_PanicCallback mhd_panic;

/**
 * Closure argument for "mhd_panic".
 */
extern void *mhd_panic_cls;

#if HAVE_MESSAGES
/**
 * Trigger 'panic' action based on fatal errors.
 * 
 * @param error message (const char *)
 */
#define MHD_PANIC(msg) mhd_panic (mhd_panic_cls, __FILE__, __LINE__, msg)
#else
/**
 * Trigger 'panic' action based on fatal errors.
 * 
 * @param error message (const char *)
 */
#define MHD_PANIC(msg) mhd_panic (mhd_panic_cls, __FILE__, __LINE__, NULL)
#endif

/**
 * Events we care about with respect to poll/select
 * for file descriptors.
 */
enum MHD_PollActions
  {
    /**
     * No event interests us.
     */
    MHD_POLL_ACTION_NOTHING = 0,

    /**
     * We would like to read.
     */
    MHD_POLL_ACTION_IN = 1,

    /**
     * We would like to write.
     */ 
    MHD_POLL_ACTION_OUT = 2
  };


/**
 * Socket descriptor and events we care about.
 */
struct MHD_Pollfd 
{
  /**
   * Socket descriptor.
   */
  int fd;

  /**
   * Which events do we care about for this socket?
   */
  enum MHD_PollActions events;
};


/**
 * Maximum length of a nonce in digest authentication.  32(MD5 Hex) +
 * 8(Timestamp Hex) + 1(NULL); hence 41 should suffice, but Opera
 * (already) takes more (see Mantis #1633), so we've increased the
 * value to support something longer...
 */
#define MAX_NONCE_LENGTH 129


/**
 * A structure representing the internal holder of the
 * nonce-nc map.
 */
struct MHD_NonceNc 
{
  
  /**
   * Nonce counter, a value that increases for each subsequent
   * request for the same nonce.
   */
  unsigned long int nc;

  /**
   * Nonce value: 
   */
  char nonce[MAX_NONCE_LENGTH];

};

#if HAVE_MESSAGES
/**
 * fprintf-like helper function for logging debug
 * messages.
 */
void MHD_DLOG (const struct MHD_Daemon *daemon, const char *format, ...);

#endif

/**
 * Process escape sequences ('+'=space, %HH) Updates val in place; the
 * result should be UTF-8 encoded and cannot be larger than the input.
 * The result must also still be 0-terminated.
 *
 * @param cls closure (use NULL)
 * @param connection handle to connection, not used
 * @return length of the resulting val (strlen(val) maybe
 *  shorter afterwards due to elimination of escape sequences)
 */
size_t MHD_http_unescape (void *cls,
			  struct MHD_Connection *connection,
			  char *val);

/**
 * Header or cookie in HTTP request or response.
 */
struct MHD_HTTP_Header
{
  /**
   * Headers are kept in a linked list.
   */
  struct MHD_HTTP_Header *next;

  /**
   * The name of the header (key), without
   * the colon.
   */
  char *header;

  /**
   * The value of the header.
   */
  char *value;

  /**
   * Type of the header (where in the HTTP
   * protocol is this header from).
   */
  enum MHD_ValueKind kind;

};


/**
 * Representation of a response.
 */
struct MHD_Response
{

  /**
   * Headers to send for the response.  Initially
   * the linked list is created in inverse order;
   * the order should be inverted before sending!
   */
  struct MHD_HTTP_Header *first_header;

  /**
   * Buffer pointing to data that we are supposed
   * to send as a response.
   */
  char *data;

  /**
   * Closure to give to the content reader
   * free callback.
   */
  void *crc_cls;

  /**
   * How do we get more data?  NULL if we are
   * given all of the data up front.
   */
  MHD_ContentReaderCallback crc;

  /**
   * NULL if data must not be freed, otherwise
   * either user-specified callback or "&free".
   */
  MHD_ContentReaderFreeCallback crfc;

  /**
   * Mutex to synchronize access to data/size and
   * reference counts.
   */
  pthread_mutex_t mutex;

  /**
   * Set to MHD_SIZE_UNKNOWN if size is not known.
   */
  uint64_t total_size;

  /**
   * At what offset in the stream is the
   * beginning of data located?
   */
  uint64_t data_start;

  /**
   * Offset to start reading from when using 'fd'.
   */
  off_t fd_off;

  /**
   * Size of data.
   */
  size_t data_size;

  /**
   * Size of the data buffer.
   */
  size_t data_buffer_size;

  /**
   * Reference count for this response.  Free
   * once the counter hits zero.
   */
  unsigned int reference_count;

  /**
   * File-descriptor if this response is FD-backed.
   */
  int fd;

};


/**
 * States in a state machine for a connection.
 *
 * Transitions are any-state to CLOSED, any state to state+1,
 * FOOTERS_SENT to INIT.  CLOSED is the terminal state and
 * INIT the initial state.
 *
 * Note that transitions for *reading* happen only after
 * the input has been processed; transitions for
 * *writing* happen after the respective data has been
 * put into the write buffer (the write does not have
 * to be completed yet).  A transition to CLOSED or INIT
 * requires the write to be complete.
 */
enum MHD_CONNECTION_STATE
{
  /**
   * Connection just started (no headers received).
   * Waiting for the line with the request type, URL and version.
   */
  MHD_CONNECTION_INIT = 0,

  /**
   * 1: We got the URL (and request type and version).  Wait for a header line.
   */
  MHD_CONNECTION_URL_RECEIVED = MHD_CONNECTION_INIT + 1,

  /**
   * 2: We got part of a multi-line request header.  Wait for the rest.
   */
  MHD_CONNECTION_HEADER_PART_RECEIVED = MHD_CONNECTION_URL_RECEIVED + 1,

  /**
   * 3: We got the request headers.  Process them.
   */
  MHD_CONNECTION_HEADERS_RECEIVED = MHD_CONNECTION_HEADER_PART_RECEIVED + 1,

  /**
   * 4: We have processed the request headers.  Send 100 continue.
   */
  MHD_CONNECTION_HEADERS_PROCESSED = MHD_CONNECTION_HEADERS_RECEIVED + 1,

  /**
   * 5: We have processed the headers and need to send 100 CONTINUE.
   */
  MHD_CONNECTION_CONTINUE_SENDING = MHD_CONNECTION_HEADERS_PROCESSED + 1,

  /**
   * 6: We have sent 100 CONTINUE (or do not need to).  Read the message body.
   */
  MHD_CONNECTION_CONTINUE_SENT = MHD_CONNECTION_CONTINUE_SENDING + 1,

  /**
   * 7: We got the request body.  Wait for a line of the footer.
   */
  MHD_CONNECTION_BODY_RECEIVED = MHD_CONNECTION_CONTINUE_SENT + 1,

  /**
   * 8: We got part of a line of the footer.  Wait for the
   * rest.
   */
  MHD_CONNECTION_FOOTER_PART_RECEIVED = MHD_CONNECTION_BODY_RECEIVED + 1,

  /**
   * 9: We received the entire footer.  Wait for a response to be queued
   * and prepare the response headers.
   */
  MHD_CONNECTION_FOOTERS_RECEIVED = MHD_CONNECTION_FOOTER_PART_RECEIVED + 1,

  /**
   * 10: We have prepared the response headers in the writ buffer.
   * Send the response headers.
   */
  MHD_CONNECTION_HEADERS_SENDING = MHD_CONNECTION_FOOTERS_RECEIVED + 1,

  /**
   * 11: We have sent the response headers.  Get ready to send the body.
   */
  MHD_CONNECTION_HEADERS_SENT = MHD_CONNECTION_HEADERS_SENDING + 1,

  /**
   * 12: We are ready to send a part of a non-chunked body.  Send it.
   */
  MHD_CONNECTION_NORMAL_BODY_READY = MHD_CONNECTION_HEADERS_SENT + 1,

  /**
   * 13: We are waiting for the client to provide more
   * data of a non-chunked body.
   */
  MHD_CONNECTION_NORMAL_BODY_UNREADY = MHD_CONNECTION_NORMAL_BODY_READY + 1,

  /**
   * 14: We are ready to send a chunk.
   */
  MHD_CONNECTION_CHUNKED_BODY_READY = MHD_CONNECTION_NORMAL_BODY_UNREADY + 1,

  /**
   * 15: We are waiting for the client to provide a chunk of the body.
   */
  MHD_CONNECTION_CHUNKED_BODY_UNREADY = MHD_CONNECTION_CHUNKED_BODY_READY + 1,

  /**
   * 16: We have sent the response body. Prepare the footers.
   */
  MHD_CONNECTION_BODY_SENT = MHD_CONNECTION_CHUNKED_BODY_UNREADY + 1,

  /**
   * 17: We have prepared the response footer.  Send it.
   */
  MHD_CONNECTION_FOOTERS_SENDING = MHD_CONNECTION_BODY_SENT + 1,

  /**
   * 18: We have sent the response footer.  Shutdown or restart.
   */
  MHD_CONNECTION_FOOTERS_SENT = MHD_CONNECTION_FOOTERS_SENDING + 1,

  /**
   * 19: This connection is to be closed.
   */
  MHD_CONNECTION_CLOSED = MHD_CONNECTION_FOOTERS_SENT + 1,

  /**
   * 20: This connection is finished (only to be freed)
   */
  MHD_CONNECTION_IN_CLEANUP = MHD_CONNECTION_CLOSED + 1,

  /*
   *  SSL/TLS connection states
   */

  /**
   * The initial connection state for all secure connectoins
   * Handshake messages will be processed in this state & while
   * in the 'MHD_TLS_HELLO_REQUEST' state
   */
  MHD_TLS_CONNECTION_INIT = MHD_CONNECTION_CLOSED + 1

};

/**
 * Should all state transitions be printed to stderr?
 */
#define DEBUG_STATES MHD_NO


#if HAVE_MESSAGES
#if DEBUG_STATES
const char *
MHD_state_to_string (enum MHD_CONNECTION_STATE state);
#endif
#endif

/**
 * Function to receive plaintext data.
 *
 * @param conn the connection struct
 * @param write_to where to write received data
 * @param max_bytes maximum number of bytes to receive
 * @return number of bytes written to write_to
 */
typedef ssize_t (*ReceiveCallback) (struct MHD_Connection * conn,
                                    void *write_to, size_t max_bytes);


/**
 * Function to transmit plaintext data.
 *
 * @param conn the connection struct
 * @param read_from where to read data to transmit
 * @param max_bytes maximum number of bytes to transmit
 * @return number of bytes transmitted
 */
typedef ssize_t (*TransmitCallback) (struct MHD_Connection * conn,
                                     const void *write_to, size_t max_bytes);


/**
 * State kept for each HTTP request.
 */
struct MHD_Connection
{

  /**
   * This is a doubly-linked list.
   */
  struct MHD_Connection *next;

  /**
   * This is a doubly-linked list.
   */
  struct MHD_Connection *prev;

  /**
   * Reference to the MHD_Daemon struct.
   */
  struct MHD_Daemon *daemon;

  /**
   * Linked list of parsed headers.
   */
  struct MHD_HTTP_Header *headers_received;

  /**
   * Tail of linked list of parsed headers.
   */
  struct MHD_HTTP_Header *headers_received_tail;

  /**
   * Response to transmit (initially NULL).
   */
  struct MHD_Response *response;

  /**
   * The memory pool is created whenever we first read
   * from the TCP stream and destroyed at the end of
   * each request (and re-created for the next request).
   * In the meantime, this pointer is NULL.  The
   * pool is used for all connection-related data
   * except for the response (which maybe shared between
   * connections) and the IP address (which persists
   * across individual requests).
   */
  struct MemoryPool *pool;

  /**
   * We allow the main application to associate some
   * pointer with the connection.  Here is where we
   * store it.  (MHD does not know or care what it
   * is).
   */
  void *client_context;

  /**
   * Request method.  Should be GET/POST/etc.  Allocated
   * in pool.
   */
  char *method;

  /**
   * Requested URL (everything after "GET" only).  Allocated
   * in pool.
   */
  char *url;

  /**
   * HTTP version string (i.e. http/1.1).  Allocated
   * in pool.
   */
  char *version;

  /**
   * Buffer for reading requests.   Allocated
   * in pool.  Actually one byte larger than
   * read_buffer_size (if non-NULL) to allow for
   * 0-termination.
   */
  char *read_buffer;

  /**
   * Buffer for writing response (headers only).  Allocated
   * in pool.
   */
  char *write_buffer;

  /**
   * Last incomplete header line during parsing of headers.
   * Allocated in pool.  Only valid if state is
   * either HEADER_PART_RECEIVED or FOOTER_PART_RECEIVED.
   */
  char *last;

  /**
   * Position after the colon on the last incomplete header
   * line during parsing of headers.
   * Allocated in pool.  Only valid if state is
   * either HEADER_PART_RECEIVED or FOOTER_PART_RECEIVED.
   */
  char *colon;

  /**
   * Foreign address (of length addr_len).  MALLOCED (not
   * in pool!).
   */
  struct sockaddr *addr;

  /**
   * Thread for this connection (if we are using
   * one thread per connection).
   */
  pthread_t pid;

  /**
   * Size of read_buffer (in bytes).  This value indicates
   * how many bytes we're willing to read into the buffer;
   * the real buffer is one byte longer to allow for
   * adding zero-termination (when needed).
   */
  size_t read_buffer_size;

  /**
   * Position where we currently append data in
   * read_buffer (last valid position).
   */
  size_t read_buffer_offset;

  /**
   * Size of write_buffer (in bytes).
   */
  size_t write_buffer_size;

  /**
   * Offset where we are with sending from write_buffer.
   */
  size_t write_buffer_send_offset;

  /**
   * Last valid location in write_buffer (where do we
   * append and up to where is it safe to send?)
   */
  size_t write_buffer_append_offset;

  /**
   * How many more bytes of the body do we expect
   * to read? "-1" for unknown.
   */
  uint64_t remaining_upload_size;

  /**
   * Current write position in the actual response
   * (excluding headers, content only; should be 0
   * while sending headers).
   */
  uint64_t response_write_position;

  /**
   * Position in the 100 CONTINUE message that
   * we need to send when receiving http 1.1 requests.
   */
  size_t continue_message_write_offset;

  /**
   * Length of the foreign address.
   */
  socklen_t addr_len;

  /**
   * Last time this connection had any activity
   * (reading or writing).
   */
  time_t last_activity;

  /**
   * After how many seconds of inactivity should
   * this connection time out?  Zero for no timeout.
   */
  unsigned int connection_timeout;

  /**
   * Did we ever call the "default_handler" on this connection?
   * (this flag will determine if we call the 'notify_completed'
   * handler when the connection closes down).
   */
  int client_aware;

  /**
   * Socket for this connection.  Set to -1 if
   * this connection has died (daemon should clean
   * up in that case).
   */
  int socket_fd;

  /**
   * Has this socket been closed for reading (i.e.
   * other side closed the connection)?  If so,
   * we must completely close the connection once
   * we are done sending our response (and stop
   * trying to read from this socket).
   */
  int read_closed;

  /**
   * Set to MHD_YES if the thread has been joined.
   */
  int thread_joined;

  /**
   * State in the FSM for this connection.
   */
  enum MHD_CONNECTION_STATE state;

  /**
   * HTTP response code.  Only valid if response object
   * is already set.
   */
  unsigned int responseCode;

  /**
   * Set to MHD_YES if the response's content reader
   * callback failed to provide data the last time
   * we tried to read from it.  In that case, the
   * write socket should be marked as unready until
   * the CRC call succeeds.
   */
  int response_unready;

  /**
   * Are we receiving with chunked encoding?  This will be set to
   * MHD_YES after we parse the headers and are processing the body
   * with chunks.  After we are done with the body and we are
   * processing the footers; once the footers are also done, this will
   * be set to MHD_NO again (before the final call to the handler).
   */
  int have_chunked_upload;

  /**
   * If we are receiving with chunked encoding, where are we right
   * now?  Set to 0 if we are waiting to receive the chunk size;
   * otherwise, this is the size of the current chunk.  A value of
   * zero is also used when we're at the end of the chunks.
   */
  unsigned int current_chunk_size;

  /**
   * If we are receiving with chunked encoding, where are we currently
   * with respect to the current chunk (at what offset / position)?
   */
  unsigned int current_chunk_offset;

  /**
   * Handler used for processing read connection operations
   */
  int (*read_handler) (struct MHD_Connection * connection);

  /**
   * Handler used for processing write connection operations
   */
  int (*write_handler) (struct MHD_Connection * connection);

  /**
   * Handler used for processing idle connection operations
   */
  int (*idle_handler) (struct MHD_Connection * connection);

  /**
   * Function used for reading HTTP request stream.
   */
  ReceiveCallback recv_cls;

  /**
   * Function used for writing HTTP response stream.
   */
  TransmitCallback send_cls;

#if HTTPS_SUPPORT
  /**
   * State required for HTTPS/SSL/TLS support.
   */
  gnutls_session_t tls_session;

  /**
   * Memory location to return for protocol session info.
   */
  int protocol;

  /**
   * Memory location to return for protocol session info.
   */
  int cipher;

  /**
   * Could it be that we are ready to read due to TLS buffers
   * even though the socket is not?
   */
  int tls_read_ready;

#endif
};

/**
 * Signature of function called to log URI accesses.
 *
 * @param cls closure
 * @param uri uri being accessed
 * @return new closure
 */
typedef void * (*LogCallback)(void * cls, const char * uri);

/**
 * Signature of function called to unescape URIs.  See also
 * MHD_http_unescape.
 *
 * @param cls closure
 * @param conn connection handle
 * @param uri 0-terminated string to unescape (should be updated)
 * @return length of the resulting string
 */
typedef size_t (*UnescapeCallback)(void *cls,
				   struct MHD_Connection *conn,
				   char *uri);


/**
 * State kept for each MHD daemon.
 */
struct MHD_Daemon
{

  /**
   * Callback function for all requests.
   */
  MHD_AccessHandlerCallback default_handler;

  /**
   * Closure argument to default_handler.
   */
  void *default_handler_cls;

  /**
   * Tail of doubly-linked list of our current, active connections.
   */
  struct MHD_Connection *connections_head;

  /**
   * Tail of doubly-linked list of our current, active connections.
   */
  struct MHD_Connection *connections_tail;

  /**
   * Tail of doubly-linked list of connections to clean up.
   */
  struct MHD_Connection *cleanup_head;

  /**
   * Tail of doubly-linked list of connections to clean up.
   */
  struct MHD_Connection *cleanup_tail;

  /**
   * Function to call to check if we should
   * accept or reject an incoming request.
   * May be NULL.
   */
  MHD_AcceptPolicyCallback apc;

  /**
   * Closure argument to apc.
   */
  void *apc_cls;

  /**
   * Function to call when we are done processing
   * a particular request.  May be NULL.
   */
  MHD_RequestCompletedCallback notify_completed;

  /**
   * Closure argument to notify_completed.
   */
  void *notify_completed_cls;

  /**
   * Function to call with the full URI at the
   * beginning of request processing.  May be NULL.
   * <p>
   * Returns the initial pointer to internal state
   * kept by the client for the request.
   */
  LogCallback uri_log_callback;

  /**
   * Closure argument to uri_log_callback.
   */
  void *uri_log_callback_cls;

  /**
   * Function to call when we unescape escape sequences.
   */
  UnescapeCallback unescape_callback;

  /**
   * Closure for unescape callback.
   */
  void *unescape_callback_cls;

#if HAVE_MESSAGES
  /**
   * Function for logging error messages (if we
   * support error reporting).
   */
  void (*custom_error_log) (void *cls, const char *fmt, va_list va);

  /**
   * Closure argument to custom_error_log.
   */
  void *custom_error_log_cls;
#endif

  /**
   * Pointer to master daemon (NULL if this is the master)
   */
  struct MHD_Daemon *master;

  /**
   * Worker daemons (one per thread)
   */
  struct MHD_Daemon *worker_pool;

  /**
   * Table storing number of connections per IP
   */
  void *per_ip_connection_count;

  /**
   * Size of the per-connection memory pools.
   */
  size_t pool_size;

  /**
   * Size of threads created by MHD.
   */
  size_t thread_stack_size;

  /**
   * Number of worker daemons
   */
  unsigned int worker_pool_size;

  /**
   * PID of the select thread (if we have internal select)
   */
  pthread_t pid;

  /**
   * Mutex for per-IP connection counts.
   */
  pthread_mutex_t per_ip_connection_mutex;

  /**
   * Mutex for (modifying) access to the "cleanup" connection DLL.
   */
  pthread_mutex_t cleanup_connection_mutex;

  /**
   * Listen socket.
   */
  int socket_fd;

  /**
   * Pipe we use to signal shutdown, unless
   * 'HAVE_LISTEN_SHUTDOWN' is defined AND we have a listen
   * socket (which we can then 'shutdown' to stop listening).
   */
  int wpipe[2];

  /**
   * Are we shutting down?
   */
  int shutdown;

  /**
   * Limit on the number of parallel connections.
   */
  unsigned int max_connections;

  /**
   * After how many seconds of inactivity should
   * connections time out?  Zero for no timeout.
   */
  unsigned int connection_timeout;

  /**
   * Maximum number of connections per IP, or 0 for
   * unlimited.
   */
  unsigned int per_ip_connection_limit;

  /**
   * Daemon's options.
   */
  enum MHD_OPTION options;

  /**
   * Listen port.
   */
  uint16_t port;

#if HTTPS_SUPPORT
  /**
   * Desired cipher algorithms.
   */
  gnutls_priority_t priority_cache;

  /**
   * What kind of credentials are we offering
   * for SSL/TLS?
   */
  gnutls_credentials_type_t cred_type;

  /**
   * Server x509 credentials
   */
  gnutls_certificate_credentials_t x509_cred;

  /**
   * Diffie-Hellman parameters
   */
  gnutls_dh_params_t dh_params;

  /**
   * Pointer to our SSL/TLS key (in ASCII) in memory.
   */
  const char *https_mem_key;

  /**
   * Pointer to our SSL/TLS certificate (in ASCII) in memory.
   */
  const char *https_mem_cert;

  /**
   * Pointer to our SSL/TLS certificate authority (in ASCII) in memory.
   */
  const char *https_mem_trust;

#endif

#ifdef DAUTH_SUPPORT

  /**
   * Character array of random values.
   */
  const char *digest_auth_random;

  /**
   * An array that contains the map nonce-nc.
   */
  struct MHD_NonceNc *nnc;

  /**
   * A rw-lock for synchronizing access to `nnc'.
   */
  pthread_mutex_t nnc_lock;

  /**
   * Size of `digest_auth_random.
   */
  unsigned int digest_auth_rand_size;

  /**
   * Size of the nonce-nc array.
   */
  unsigned int nonce_nc_size;

#endif

};


#if EXTRA_CHECKS
#define EXTRA_CHECK(a) if (!(a)) abort();
#else
#define EXTRA_CHECK(a)
#endif


/**
 * Insert an element at the head of a DLL. Assumes that head, tail and
 * element are structs with prev and next fields.
 *
 * @param head pointer to the head of the DLL
 * @param tail pointer to the tail of the DLL
 * @param element element to insert
 */
#define DLL_insert(head,tail,element) do { \
  (element)->next = (head); \
  (element)->prev = NULL; \
  if ((tail) == NULL) \
    (tail) = element; \
  else \
    (head)->prev = element; \
  (head) = (element); } while (0)


/**
 * Remove an element from a DLL. Assumes
 * that head, tail and element are structs
 * with prev and next fields.
 *
 * @param head pointer to the head of the DLL
 * @param tail pointer to the tail of the DLL
 * @param element element to remove
 */
#define DLL_remove(head,tail,element) do { \
  if ((element)->prev == NULL) \
    (head) = (element)->next;  \
  else \
    (element)->prev->next = (element)->next; \
  if ((element)->next == NULL) \
    (tail) = (element)->prev;  \
  else \
    (element)->next->prev = (element)->prev; \
  (element)->next = NULL; \
  (element)->prev = NULL; } while (0)


/**
 * Equivalent to time(NULL) but tries to use some sort of monotonic
 * clock that isn't affected by someone setting the system real time
 * clock.
 */
time_t MHD_monotonic_time(void);

#endif
