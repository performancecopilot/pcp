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
 * @file daemon.c
 * @brief  A minimal-HTTP server library
 * @author Daniel Pittman
 * @author Christian Grothoff
 */
#include "platform.h"
#include "internal.h"
#include "response.h"
#include "connection.h"
#include "memorypool.h"
#include <limits.h>

#if HAVE_SEARCH_H
#include <search.h>
#else
#include "tsearch.h"
#endif

#if HTTPS_SUPPORT
#include "connection_https.h"
#include <gnutls/gnutls.h>
#include <gcrypt.h>
#endif

#ifdef HAVE_POLL_H
#include <poll.h>
#endif

#ifdef LINUX
#include <sys/sendfile.h>
#endif

/**
 * Default connection limit.
 */
#ifndef WINDOWS
#define MHD_MAX_CONNECTIONS_DEFAULT FD_SETSIZE - 4
#else
#define MHD_MAX_CONNECTIONS_DEFAULT FD_SETSIZE
#endif

/**
 * Default memory allowed per connection.
 */
#define MHD_POOL_SIZE_DEFAULT (32 * 1024)

/**
 * Print extra messages with reasons for closing
 * sockets? (only adds non-error messages).
 */
#define DEBUG_CLOSE MHD_NO

/**
 * Print extra messages when establishing
 * connections? (only adds non-error messages).
 */
#define DEBUG_CONNECT MHD_NO

#ifndef LINUX
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
#endif

#ifndef SOCK_CLOEXEC
#define SOCK_CLOEXEC 0
#endif


/**
 * Default implementation of the panic function,
 * prints an error message and aborts.
 *
 * @param cls unused
 * @param file name of the file with the problem
 * @param line line number with the problem
 * @param msg error message with details
 */
static void 
mhd_panic_std (void *cls,
	       const char *file,
	       unsigned int line,
	       const char *reason)
{
#if HAVE_MESSAGES
  fprintf (stderr, "Fatal error in GNU libmicrohttpd %s:%u: %s\n",
	   file, line, reason);
#endif
  abort ();
}


/**
 * Handler for fatal errors.
 */
MHD_PanicCallback mhd_panic;

/**
 * Closure argument for "mhd_panic".
 */
void *mhd_panic_cls;


/**
 * Trace up to and return master daemon. If the supplied daemon
 * is a master, then return the daemon itself.
 *
 * @param daemon handle to a daemon 
 * @return master daemon handle
 */
static struct MHD_Daemon*
MHD_get_master (struct MHD_Daemon *daemon)
{
  while (NULL != daemon->master)
    daemon = daemon->master;
  return daemon;
}


/**
 * Maintain connection count for single address.
 */
struct MHD_IPCount
{
  /**
   * Address family. AF_INET or AF_INET6 for now.
   */
  int family;

  /**
   * Actual address.
   */
  union
  {
    /**
     * IPv4 address.
     */ 
    struct in_addr ipv4;
#if HAVE_IPV6
    /**
     * IPv6 address.
     */ 
    struct in6_addr ipv6;
#endif
  } addr;

  /**
   * Counter.
   */
  unsigned int count;
};


/**
 * Lock shared structure for IP connection counts and connection DLLs.
 *
 * @param daemon handle to daemon where lock is
 */
static void
MHD_ip_count_lock(struct MHD_Daemon *daemon)
{
  if (0 != pthread_mutex_lock(&daemon->per_ip_connection_mutex))
    {
      MHD_PANIC ("Failed to acquire IP connection limit mutex\n");
    }
}


/**
 * Unlock shared structure for IP connection counts and connection DLLs.
 *
 * @param daemon handle to daemon where lock is
 */
static void
MHD_ip_count_unlock(struct MHD_Daemon *daemon)
{
  if (0 != pthread_mutex_unlock(&daemon->per_ip_connection_mutex))
    {
      MHD_PANIC ("Failed to release IP connection limit mutex\n");
    }
}


/**
 * Tree comparison function for IP addresses (supplied to tsearch() family).
 * We compare everything in the struct up through the beginning of the
 * 'count' field.
 * 
 * @param a1 first address to compare
 * @param a2 second address to compare
 * @return -1, 0 or 1 depending on result of compare
 */
static int
MHD_ip_addr_compare(const void *a1, const void *a2)
{
  return memcmp (a1, a2, offsetof (struct MHD_IPCount, count));
}


/**
 * Parse address and initialize 'key' using the address.
 *
 * @param addr address to parse
 * @param addrlen number of bytes in addr
 * @param key where to store the parsed address
 * @return MHD_YES on success and MHD_NO otherwise (e.g., invalid address type)
 */
static int
MHD_ip_addr_to_key(const struct sockaddr *addr, 
		   socklen_t addrlen,
                   struct MHD_IPCount *key)
{
  memset(key, 0, sizeof(*key));

  /* IPv4 addresses */
  if (sizeof (struct sockaddr_in) == addrlen)
    {
      const struct sockaddr_in *addr4 = (const struct sockaddr_in*) addr;
      key->family = AF_INET;
      memcpy (&key->addr.ipv4, &addr4->sin_addr, sizeof(addr4->sin_addr));
      return MHD_YES;
    }

#if HAVE_IPV6
  /* IPv6 addresses */
  if (sizeof (struct sockaddr_in6) == addrlen)
    {
      const struct sockaddr_in6 *addr6 = (const struct sockaddr_in6*) addr;
      key->family = AF_INET6;
      memcpy (&key->addr.ipv6, &addr6->sin6_addr, sizeof(addr6->sin6_addr));
      return MHD_YES;
    }
#endif

  /* Some other address */
  return MHD_NO;
}


/**
 * Check if IP address is over its limit.
 *
 * @param daemon handle to daemon where connection counts are tracked
 * @param addr address to add (or increment counter)
 * @param addrlen number of bytes in addr
 * @return Return MHD_YES if IP below limit, MHD_NO if IP has surpassed limit.
 *   Also returns MHD_NO if fails to allocate memory.
 */
static int
MHD_ip_limit_add(struct MHD_Daemon *daemon,
                 const struct sockaddr *addr,
		 socklen_t addrlen)
{
  struct MHD_IPCount *key;
  void **nodep;
  void *node;
  int result;

  daemon = MHD_get_master (daemon);
  /* Ignore if no connection limit assigned */
  if (0 == daemon->per_ip_connection_limit)
    return MHD_YES;

  if (NULL == (key = malloc (sizeof(*key))))
    return MHD_NO;

  /* Initialize key */
  if (MHD_NO == MHD_ip_addr_to_key (addr, addrlen, key))
    {
      /* Allow unhandled address types through */
      free (key);
      return MHD_YES;
    }
  MHD_ip_count_lock (daemon);

  /* Search for the IP address */
  if (NULL == (nodep = TSEARCH (key, 
				&daemon->per_ip_connection_count, 
				&MHD_ip_addr_compare)))
    {
#if HAVE_MESSAGES
      MHD_DLOG (daemon,
		"Failed to add IP connection count node\n");
#endif      
      MHD_ip_count_unlock (daemon);
      free (key);
      return MHD_NO;
    }
  node = *nodep;
  /* If we got an existing node back, free the one we created */
  if (node != key)
    free(key);
  key = (struct MHD_IPCount *) node;
  /* Test if there is room for another connection; if so,
   * increment count */
  result = (key->count < daemon->per_ip_connection_limit);
  if (MHD_YES == result)
    ++key->count;

  MHD_ip_count_unlock (daemon);
  return result;
}


/**
 * Decrement connection count for IP address, removing from table
 * count reaches 0
 *
 * @param daemon handle to daemon where connection counts are tracked
 * @param addr address to remove (or decrement counter)
 * @param addrlen number of bytes in addr
 */
static void
MHD_ip_limit_del(struct MHD_Daemon *daemon,
                 const struct sockaddr *addr,
		 socklen_t addrlen)
{
  struct MHD_IPCount search_key;
  struct MHD_IPCount *found_key;
  void **nodep;

  daemon = MHD_get_master (daemon);
  /* Ignore if no connection limit assigned */
  if (0 == daemon->per_ip_connection_limit)
    return;
  /* Initialize search key */
  if (MHD_NO == MHD_ip_addr_to_key (addr, addrlen, &search_key))
    return;

  MHD_ip_count_lock (daemon);

  /* Search for the IP address */
  if (NULL == (nodep = TFIND (&search_key, 
			      &daemon->per_ip_connection_count, 
			      &MHD_ip_addr_compare)))
    {      
      /* Something's wrong if we couldn't find an IP address
       * that was previously added */
      MHD_PANIC ("Failed to find previously-added IP address\n");
    }
  found_key = (struct MHD_IPCount *) *nodep;
  /* Validate existing count for IP address */
  if (0 == found_key->count)
    {
      MHD_PANIC ("Previously-added IP address had 0 count\n");
    }
  /* Remove the node entirely if count reduces to 0 */
  if (0 == --found_key->count)
    {
      TDELETE (found_key, 
	       &daemon->per_ip_connection_count, 
	       &MHD_ip_addr_compare);
      free (found_key);
    }

  MHD_ip_count_unlock (daemon);
}


#if HTTPS_SUPPORT
/**
 * Callback for receiving data from the socket.
 *
 * @param connection the MHD connection structure
 * @param other where to write received data to
 * @param i maximum size of other (in bytes)
 * @return number of bytes actually received
 */
static ssize_t
recv_tls_adapter (struct MHD_Connection *connection, void *other, size_t i)
{
  int res;

  connection->tls_read_ready = MHD_NO;
  res = gnutls_record_recv (connection->tls_session, other, i);
  if ( (GNUTLS_E_AGAIN == res) ||
       (GNUTLS_E_INTERRUPTED == res) )
    {
      errno = EINTR;
      return -1;
    }
  if (res < 0)
    {
      /* Likely 'GNUTLS_E_INVALID_SESSION' (client communication
	 disrupted); set errno to something caller will interpret
	 correctly as a hard error*/
      errno = EPIPE;
      return res;
    }
  if (res == i)
    connection->tls_read_ready = MHD_YES;
  return res;
}


/**
 * Callback for writing data to the socket.
 *
 * @param connection the MHD connection structure
 * @param other data to write
 * @param i number of bytes to write
 * @return actual number of bytes written
 */
static ssize_t
send_tls_adapter (struct MHD_Connection *connection,
                  const void *other, size_t i)
{
  int res;

  res = gnutls_record_send (connection->tls_session, other, i);
  if ( (GNUTLS_E_AGAIN == res) ||
       (GNUTLS_E_INTERRUPTED == res) )
    {
      errno = EINTR;
      return -1;
    }
  return res;
}


/**
 * Read and setup our certificate and key.
 *
 * @param daemon handle to daemon to initialize
 * @return 0 on success
 */
static int
MHD_init_daemon_certificate (struct MHD_Daemon *daemon)
{
  gnutls_datum_t key;
  gnutls_datum_t cert;

  if (NULL != daemon->https_mem_trust) 
    {
      cert.data = (unsigned char *) daemon->https_mem_trust;
      cert.size = strlen (daemon->https_mem_trust);
      if (gnutls_certificate_set_x509_trust_mem (daemon->x509_cred, &cert,
						 GNUTLS_X509_FMT_PEM) < 0) 
	{
#if HAVE_MESSAGES
	  MHD_DLOG(daemon,
		   "Bad trust certificate format\n");
#endif
	  return -1;
	}
    }
  
  /* certificate & key loaded from memory */
  if ( (NULL != daemon->https_mem_cert) && 
       (NULL != daemon->https_mem_key) )
    {
      key.data = (unsigned char *) daemon->https_mem_key;
      key.size = strlen (daemon->https_mem_key);
      cert.data = (unsigned char *) daemon->https_mem_cert;
      cert.size = strlen (daemon->https_mem_cert);

      return gnutls_certificate_set_x509_key_mem (daemon->x509_cred,
						  &cert, &key,
						  GNUTLS_X509_FMT_PEM);
    }
#if HAVE_MESSAGES
  MHD_DLOG (daemon, "You need to specify a certificate and key location\n");
#endif
  return -1;
}


/**
 * Initialize security aspects of the HTTPS daemon
 *
 * @param daemon handle to daemon to initialize
 * @return 0 on success
 */
static int
MHD_TLS_init (struct MHD_Daemon *daemon)
{
  switch (daemon->cred_type)
    {
    case GNUTLS_CRD_CERTIFICATE:
      if (0 !=
          gnutls_certificate_allocate_credentials (&daemon->x509_cred))
        return GNUTLS_E_MEMORY_ERROR;
      return MHD_init_daemon_certificate (daemon);
    default:
#if HAVE_MESSAGES
      MHD_DLOG (daemon,
                "Error: invalid credentials type %d specified.\n",
                daemon->cred_type);
#endif
      return -1;
    }
}
#endif


/**
 * Obtain the select sets for this daemon.
 *
 * @param daemon daemon to get sets from
 * @param read_fd_set read set
 * @param write_fd_set write set
 * @param except_fd_set except set
 * @param max_fd increased to largest FD added (if larger
 *               than existing value); can be NULL
 * @return MHD_YES on success, MHD_NO if this
 *         daemon was not started with the right
 *         options for this call.
 */
int
MHD_get_fdset (struct MHD_Daemon *daemon,
               fd_set *read_fd_set,
               fd_set *write_fd_set, 
	       fd_set *except_fd_set,
	       int *max_fd)
{
  struct MHD_Connection *pos;
  int fd;

  if ( (NULL == daemon) 
       || (NULL == read_fd_set) 
       || (NULL == write_fd_set)
       || (NULL == except_fd_set) 
       || (NULL == max_fd)
       || (MHD_YES == daemon->shutdown)
       || (0 != (daemon->options & MHD_USE_THREAD_PER_CONNECTION))
       || (0 != (daemon->options & MHD_USE_POLL)))
    return MHD_NO;
  fd = daemon->socket_fd;
  if (-1 != fd)
  {
    FD_SET (fd, read_fd_set);
    /* update max file descriptor */
    if ((*max_fd) < fd) 
      *max_fd = fd;
  }
  for (pos = daemon->connections_head; NULL != pos; pos = pos->next)
    if (MHD_YES != MHD_connection_get_fdset (pos,
					     read_fd_set,
					     write_fd_set,
					     except_fd_set, max_fd))
      return MHD_NO;    
#if DEBUG_CONNECT
  MHD_DLOG (daemon, "Maximum socket in select set: %d\n", *max_fd);
#endif
  return MHD_YES;
}


/**
 * Main function of the thread that handles an individual
 * connection when MHD_USE_THREAD_PER_CONNECTION is set.
 * 
 * @param data the 'struct MHD_Connection' this thread will handle
 * @return always NULL
 */
static void *
MHD_handle_connection (void *data)
{
  struct MHD_Connection *con = data;
  int num_ready;
  fd_set rs;
  fd_set ws;
  fd_set es;
  int max;
  struct timeval tv;
  struct timeval *tvp;
  unsigned int timeout;
  time_t now;
#ifdef HAVE_POLL_H
  struct MHD_Pollfd mp;
  struct pollfd p[1];
#endif

  timeout = con->daemon->connection_timeout;
  while ( (MHD_YES != con->daemon->shutdown) && 
	  (MHD_CONNECTION_CLOSED != con->state) ) 
    {
      tvp = NULL;
      if (timeout > 0)
	{
	  now = MHD_monotonic_time();
	  if (now - con->last_activity > timeout)
	    tv.tv_sec = 0;
	  else
	    tv.tv_sec = timeout - (now - con->last_activity);
	  tv.tv_usec = 0;
	  tvp = &tv;
	}
      if ( (MHD_CONNECTION_NORMAL_BODY_UNREADY == con->state) ||
	   (MHD_CONNECTION_CHUNKED_BODY_UNREADY == con->state) )
	{
	  /* do not block (we're waiting for our callback to succeed) */
	  tv.tv_sec = 0;
	  tv.tv_usec = 0;
	  tvp = &tv;
	}
#if HTTPS_SUPPORT
      if (MHD_YES == con->tls_read_ready)
	{
	  /* do not block (more data may be inside of TLS buffers waiting for us) */
	  tv.tv_sec = 0;
	  tv.tv_usec = 0;
	  tvp = &tv;
	}
#endif
      if (0 == (con->daemon->options & MHD_USE_POLL))
	{
	  /* use select */
	  FD_ZERO (&rs);
	  FD_ZERO (&ws);
	  FD_ZERO (&es);
	  max = 0;
	  MHD_connection_get_fdset (con, &rs, &ws, &es, &max);
	  num_ready = SELECT (max + 1, &rs, &ws, &es, tvp);
	  if (num_ready < 0) 
	    {
	      if (EINTR == errno)
		continue;
#if HAVE_MESSAGES
	      MHD_DLOG (con->daemon,
			"Error during select (%d): `%s'\n", 
			max,
			STRERROR (errno));
#endif
	      break;
	    }
	  /* call appropriate connection handler if necessary */
	  if ( (FD_ISSET (con->socket_fd, &rs))
#if HTTPS_SUPPORT
		   || (MHD_YES == con->tls_read_ready) 
#endif
	       )
	    con->read_handler (con);
	  if (FD_ISSET (con->socket_fd, &ws))
	    con->write_handler (con);
	  if (MHD_NO == con->idle_handler (con))
	    goto exit;
	}
#ifdef HAVE_POLL_H
      else
	{
	    /* use poll */
	  memset(&mp, 0, sizeof (struct MHD_Pollfd));
	  MHD_connection_get_pollfd(con, &mp);
	  memset(&p, 0, sizeof (p));
	  p[0].fd = mp.fd;
	  if (mp.events & MHD_POLL_ACTION_IN) 
	    p[0].events |= POLLIN;        
	  if (mp.events & MHD_POLL_ACTION_OUT) 
	    p[0].events |= POLLOUT;
	  if (poll (p, 
		    1, 
		    (NULL == tvp) ? -1 : tv.tv_sec * 1000) < 0)
	    {
	      if (EINTR == errno)
		continue;
#if HAVE_MESSAGES
	      MHD_DLOG (con->daemon, "Error during poll: `%s'\n", 
			STRERROR (errno));
#endif
	      break;
	    }
	  if ( (0 != (p[0].revents & POLLIN)) 
#if HTTPS_SUPPORT
	       || (MHD_YES == con->tls_read_ready) 
#endif
	       )
	    con->read_handler (con);        
	  if (0 != (p[0].revents & POLLOUT)) 
	    con->write_handler (con);        
	  if (0 != (p[0].revents & (POLLERR | POLLHUP))) 
	    MHD_connection_close (con, MHD_REQUEST_TERMINATED_WITH_ERROR);      
	  if (MHD_NO == con->idle_handler (con))
	    goto exit;
	}
#endif
    }
  if (MHD_CONNECTION_IN_CLEANUP != con->state)
    {
#if DEBUG_CLOSE
#if HAVE_MESSAGES
      MHD_DLOG (con->daemon,
                "Processing thread terminating, closing connection\n");
#endif
#endif
      if (MHD_CONNECTION_CLOSED != con->state)
	MHD_connection_close (con, 
			      MHD_REQUEST_TERMINATED_DAEMON_SHUTDOWN);
      con->idle_handler (con);
    }
exit:
  if (NULL != con->response)
    {
      MHD_destroy_response (con->response);
      con->response = NULL;
    }
  return NULL;
}


/**
 * Callback for receiving data from the socket.
 *
 * @param conn the MHD connection structure
 * @param other where to write received data to
 * @param i maximum size of other (in bytes)
 * @return number of bytes actually received
 */
static ssize_t
recv_param_adapter (struct MHD_Connection *connection,
		    void *other, 
		    size_t i)
{
  if ( (-1 == connection->socket_fd) ||
       (MHD_CONNECTION_CLOSED == connection->state) )
    {
      errno = ENOTCONN;
      return -1;
    }
  if (0 != (connection->daemon->options & MHD_USE_SSL))
    return RECV (connection->socket_fd, other, i, MSG_NOSIGNAL);
  return RECV (connection->socket_fd, other, i, MSG_NOSIGNAL);
}


/**
 * Callback for writing data to the socket.
 *
 * @param conn the MHD connection structure
 * @param other data to write
 * @param i number of bytes to write
 * @return actual number of bytes written
 */
static ssize_t
send_param_adapter (struct MHD_Connection *connection,
                    const void *other,
		    size_t i)
{
#if LINUX
  int fd;
  off_t offset;
  off_t left;
  ssize_t ret;
#endif
  if ( (-1 == connection->socket_fd) ||
       (MHD_CONNECTION_CLOSED == connection->state) )
    {
      errno = ENOTCONN;
      return -1;
    }
  if (0 != (connection->daemon->options & MHD_USE_SSL))
    return SEND (connection->socket_fd, other, i, MSG_NOSIGNAL);
#if LINUX
  if ( (connection->write_buffer_append_offset ==
	connection->write_buffer_send_offset) &&
       (NULL != connection->response) &&
       (-1 != (fd = connection->response->fd)) )
    {
      /* can use sendfile */
      offset = (off_t) connection->response_write_position + connection->response->fd_off;
      left = connection->response->total_size - connection->response_write_position;
      if (left > SSIZE_MAX)
	left = SSIZE_MAX; /* cap at return value limit */
      if (-1 != (ret = sendfile (connection->socket_fd, 
				 fd,
				 &offset,
				 (size_t) left)))
	return ret;
      if ( (EINTR == errno) || (EAGAIN == errno) )
	return 0;
      if ( (EINVAL == errno) || (EBADF == errno) )
	return -1; 
      /* None of the 'usual' sendfile errors occurred, so we should try
	 to fall back to 'SEND'; see also this thread for info on
	 odd libc/Linux behavior with sendfile:
	 http://lists.gnu.org/archive/html/libmicrohttpd/2011-02/msg00015.html */
    }
#endif
  return SEND (connection->socket_fd, other, i, MSG_NOSIGNAL);
}


/**
 * Signature of main function for a thread.
 */
typedef void *(*ThreadStartRoutine)(void *cls);


/**
 * Create a thread and set the attributes according to our options.
 * 
 * @param thread handle to initialize
 * @param daemon daemon with options
 * @param start_routine main function of thread
 * @param arg argument for start_routine
 * @return 0 on success
 */
static int
create_thread (pthread_t * thread,
	       const struct MHD_Daemon *daemon,
	       ThreadStartRoutine start_routine,
	       void *arg)
{
  pthread_attr_t attr;
  pthread_attr_t *pattr;
  int ret;
  
  if (0 != daemon->thread_stack_size) 
    {
      if (0 != (ret = pthread_attr_init (&attr))) 
	goto ERR;
      if (0 != (ret = pthread_attr_setstacksize (&attr, daemon->thread_stack_size)))
	{
	  pthread_attr_destroy (&attr);
	  goto ERR;
	}
      pattr = &attr;
    }
  else
    {
      pattr = NULL;
    }
  ret = pthread_create (thread, pattr,
			start_routine, arg);
  if (0 != daemon->thread_stack_size) 
    pthread_attr_destroy (&attr);
  return ret;
 ERR:
#if HAVE_MESSAGES
  MHD_DLOG (daemon,
	    "Failed to set thread stack size\n");
#endif
  errno = EINVAL;
  return ret;
}


/**
 * Add another client connection to the set of connections 
 * managed by MHD.  This API is usually not needed (since
 * MHD will accept inbound connections on the server socket).
 * Use this API in special cases, for example if your HTTP
 * server is behind NAT and needs to connect out to the 
 * HTTP client.
 *
 * The given client socket will be managed (and closed!) by MHD after
 * this call and must no longer be used directly by the application
 * afterwards.
 *
 * Per-IP connection limits are ignored when using this API.
 *
 * @param daemon daemon that manages the connection
 * @param client_socket socket to manage (MHD will expect
 *        to receive an HTTP request from this socket next).
 * @param addr IP address of the client
 * @param addrlen number of bytes in addr
 * @return MHD_YES on success, MHD_NO if this daemon could
 *        not handle the connection (i.e. malloc failed, etc).
 *        The socket will be closed in any case.
 */
int 
MHD_add_connection (struct MHD_Daemon *daemon, 
		    int client_socket,
		    const struct sockaddr *addr,
		    socklen_t addrlen)
{
  struct MHD_Connection *connection;
  int res_thread_create;
#if OSX
  static int on = 1;
#endif

#ifndef WINDOWS
  if ( (client_socket >= FD_SETSIZE) &&
       (0 == (daemon->options & MHD_USE_POLL)) )
    {
#if HAVE_MESSAGES
      MHD_DLOG (daemon,
		"Socket descriptor larger than FD_SETSIZE: %d > %d\n",
		client_socket,
		FD_SETSIZE);
#endif
      SHUTDOWN (client_socket, SHUT_RDWR);
      CLOSE (client_socket);
      return MHD_NO;
    }
#endif


#if HAVE_MESSAGES
#if DEBUG_CONNECT
  MHD_DLOG (daemon, "Accepted connection on socket %d\n", s);
#endif
#endif
  if ( (0 == daemon->max_connections) ||
       (MHD_NO == MHD_ip_limit_add (daemon, addr, addrlen)) )
    {
      /* above connection limit - reject */
#if HAVE_MESSAGES
      MHD_DLOG (daemon,
                "Server reached connection limit (closing inbound connection)\n");
#endif
      SHUTDOWN (client_socket, SHUT_RDWR);
      CLOSE (client_socket);
      return MHD_NO;
    }

  /* apply connection acceptance policy if present */
  if ( (NULL != daemon->apc) && 
       (MHD_NO == daemon->apc (daemon->apc_cls, 
			       addr, addrlen)) )
    {
#if DEBUG_CLOSE
#if HAVE_MESSAGES
      MHD_DLOG (daemon, "Connection rejected, closing connection\n");
#endif
#endif
      SHUTDOWN (client_socket, SHUT_RDWR);
      CLOSE (client_socket);
      MHD_ip_limit_del (daemon, addr, addrlen);
      return MHD_YES;
    }

#if OSX
#ifdef SOL_SOCKET
#ifdef SO_NOSIGPIPE
  setsockopt (client_socket, 
	      SOL_SOCKET, SO_NOSIGPIPE, 
	      &on, sizeof (on));
#endif
#endif
#endif

  if (NULL == (connection = malloc (sizeof (struct MHD_Connection))))
    {
#if HAVE_MESSAGES
      MHD_DLOG (daemon, 
		"Error allocating memory: %s\n", 
		STRERROR (errno));
#endif
      SHUTDOWN (client_socket, SHUT_RDWR);
      CLOSE (client_socket);
      MHD_ip_limit_del (daemon, addr, addrlen);
      return MHD_NO;
    }
  memset (connection, 0, sizeof (struct MHD_Connection));
  connection->connection_timeout = daemon->connection_timeout;
  connection->pool = NULL;
  if (NULL == (connection->addr = malloc (addrlen)))
    {
#if HAVE_MESSAGES
      MHD_DLOG (daemon, 
		"Error allocating memory: %s\n", 
		STRERROR (errno));
#endif
      SHUTDOWN (client_socket, SHUT_RDWR);
      CLOSE (client_socket);
      MHD_ip_limit_del (daemon, addr, addrlen);
      free (connection);
      return MHD_NO;
    }
  memcpy (connection->addr, addr, addrlen);
  connection->addr_len = addrlen;
  connection->socket_fd = client_socket;
  connection->daemon = daemon;
  connection->last_activity = MHD_monotonic_time();

  /* set default connection handlers  */
  MHD_set_http_callbacks_ (connection);
  connection->recv_cls = &recv_param_adapter;
  connection->send_cls = &send_param_adapter;
  /* non-blocking sockets are required on most systems and for GNUtls;
     however, they somehow cause serious problems on CYGWIN (#1824) */
#ifdef CYGWIN
  if (0 != (daemon->options & MHD_USE_SSL))
#endif
  {
    /* make socket non-blocking */
#ifndef MINGW
    int flags = fcntl (connection->socket_fd, F_GETFL);
    if ( (-1 == flags) ||
	 (0 != fcntl (connection->socket_fd, F_SETFL, flags | O_NONBLOCK)) )
      {
#if HAVE_MESSAGES
	MHD_DLOG (daemon,
		  "Failed to make socket %d non-blocking: %s\n", 
		  connection->socket_fd,
		  STRERROR (errno));
#endif
      }
#else
    unsigned long flags = 1;
    if (0 != ioctlsocket (connection->socket_fd, FIONBIO, &flags))
      {
#if HAVE_MESSAGES
	MHD_DLOG (daemon, 
		  "Failed to make socket non-blocking: %s\n", 
		  STRERROR (errno));
#endif
      }
#endif
  }

#if HTTPS_SUPPORT
  if (0 != (daemon->options & MHD_USE_SSL))
    {
      connection->recv_cls = &recv_tls_adapter;
      connection->send_cls = &send_tls_adapter;
      connection->state = MHD_TLS_CONNECTION_INIT;
      MHD_set_https_callbacks (connection);
      gnutls_init (&connection->tls_session, GNUTLS_SERVER);
      gnutls_priority_set (connection->tls_session,
			   daemon->priority_cache);
      switch (daemon->cred_type)
        {
          /* set needed credentials for certificate authentication. */
        case GNUTLS_CRD_CERTIFICATE:
          gnutls_credentials_set (connection->tls_session,
				  GNUTLS_CRD_CERTIFICATE,
				  daemon->x509_cred);
          break;
        default:
#if HAVE_MESSAGES
          MHD_DLOG (connection->daemon,
                    "Failed to setup TLS credentials: unknown credential type %d\n",
                    daemon->cred_type);
#endif
          SHUTDOWN (client_socket, SHUT_RDWR);
          CLOSE (client_socket);
          MHD_ip_limit_del (daemon, addr, addrlen);
          free (connection->addr);
          free (connection);
          MHD_PANIC ("Unknown credential type");
 	  return MHD_NO;
        }
      gnutls_transport_set_ptr (connection->tls_session,
				(gnutls_transport_ptr_t) connection);
      gnutls_transport_set_pull_function (connection->tls_session,
					  (gnutls_pull_func) &
                                               recv_param_adapter);
      gnutls_transport_set_push_function (connection->tls_session,
					  (gnutls_push_func) &
                                               send_param_adapter);

      if (daemon->https_mem_trust){
      gnutls_certificate_server_set_request(connection->tls_session, GNUTLS_CERT_REQUEST);
      }
    }
#endif

  if (0 != pthread_mutex_lock(&daemon->cleanup_connection_mutex))
    {
      MHD_PANIC ("Failed to acquire cleanup mutex\n");
    }
  DLL_insert (daemon->connections_head,
	      daemon->connections_tail,
	      connection);
  if (0 != pthread_mutex_unlock(&daemon->cleanup_connection_mutex))
    {
      MHD_PANIC ("Failed to release cleanup mutex\n");
    }

  /* attempt to create handler thread */
  if (0 != (daemon->options & MHD_USE_THREAD_PER_CONNECTION))
    {
      res_thread_create = create_thread (&connection->pid, daemon,
					 &MHD_handle_connection, connection);
      if (0 != res_thread_create)
        {
#if HAVE_MESSAGES
          MHD_DLOG (daemon, "Failed to create a thread: %s\n",
                    STRERROR (res_thread_create));
#endif
          SHUTDOWN (client_socket, SHUT_RDWR);
          CLOSE (client_socket);
          MHD_ip_limit_del (daemon, addr, addrlen);
	  if (0 != pthread_mutex_lock(&daemon->cleanup_connection_mutex))
	    {
	      MHD_PANIC ("Failed to acquire cleanup mutex\n");
	    }
	  DLL_remove (daemon->connections_head,
		      daemon->connections_tail,
		      connection);
	  if (0 != pthread_mutex_unlock(&daemon->cleanup_connection_mutex))
	    {
	      MHD_PANIC ("Failed to release cleanup mutex\n");
	    }
          free (connection->addr);
          free (connection);
          return MHD_NO;
        }
    }
  daemon->max_connections--;
  return MHD_YES;  
}


/**
 * Accept an incoming connection and create the MHD_Connection object for
 * it.  This function also enforces policy by way of checking with the
 * accept policy callback.
 * 
 * @param daemon handle with the listen socket
 * @return MHD_YES on success
 */
static int
MHD_accept_connection (struct MHD_Daemon *daemon)
{
#if HAVE_INET6
  struct sockaddr_in6 addrstorage;
#else
  struct sockaddr_in addrstorage;
#endif
  struct sockaddr *addr = (struct sockaddr *) &addrstorage;
  socklen_t addrlen;
  int s;
  int flags;
  int need_fcntl;

  addrlen = sizeof (addrstorage);
  memset (addr, 0, sizeof (addrstorage));
#if HAVE_ACCEPT4
  s = accept4 (daemon->socket_fd, addr, &addrlen, SOCK_CLOEXEC);
  need_fcntl = MHD_NO;
#else
  s = -1;
  need_fcntl = MHD_YES;
#endif
  if (-1 == s)
  {
    s = ACCEPT (daemon->socket_fd, addr, &addrlen);
    need_fcntl = MHD_YES;
  }
  if ((-1 == s) || (addrlen <= 0))
    {
#if HAVE_MESSAGES
      /* This could be a common occurance with multiple worker threads */
      if ((EAGAIN != errno) && (EWOULDBLOCK != errno))
        MHD_DLOG (daemon, 
		  "Error accepting connection: %s\n", 
		  STRERROR (errno));
#endif
      if (-1 != s)
        {
          SHUTDOWN (s, SHUT_RDWR);
          CLOSE (s);
          /* just in case */
        }
      return MHD_NO;
    }
  if (MHD_YES == need_fcntl)
  {
    /* make socket non-inheritable */
#ifdef WINDOWS
    DWORD dwFlags;
    if (!GetHandleInformation ((HANDLE) s, &dwFlags) ||
        ((dwFlags != dwFlags & ~HANDLE_FLAG_INHERIT) &&
        !SetHandleInformation ((HANDLE) s, HANDLE_FLAG_INHERIT, 0)))
      {
#if HAVE_MESSAGES
        SetErrnoFromWinError (GetLastError ());
	MHD_DLOG (daemon,
		  "Failed to make socket non-inheritable: %s\n", 
		  STRERROR (errno));
#endif
      }
#else
    flags = fcntl (s, F_GETFD);
    if ( ( (-1 == flags) ||
	   ( (flags != (flags | FD_CLOEXEC)) &&
	     (0 != fcntl (s, F_SETFD, flags | FD_CLOEXEC)) ) ) )
      {
#if HAVE_MESSAGES
	MHD_DLOG (daemon,
		  "Failed to make socket non-inheritable: %s\n", 
		  STRERROR (errno));
#endif
      }
#endif
  }
#if HAVE_MESSAGES
#if DEBUG_CONNECT
  MHD_DLOG (daemon, "Accepted connection on socket %d\n", s);
#endif
#endif
  return MHD_add_connection (daemon, s,
			     addr, addrlen);
}


/**
 * Free resources associated with all closed connections.
 * (destroy responses, free buffers, etc.).  All closed
 * connections are kept in the "cleanup" doubly-linked list.
 *
 * @param daemon daemon to clean up
 */
static void
MHD_cleanup_connections (struct MHD_Daemon *daemon)
{
  struct MHD_Connection *pos;
  void *unused;
  int rc;

  if (0 != pthread_mutex_lock(&daemon->cleanup_connection_mutex))
    {
      MHD_PANIC ("Failed to acquire cleanup mutex\n");
    }
  while (NULL != (pos = daemon->cleanup_head))
    {
      DLL_remove (daemon->cleanup_head,
		  daemon->cleanup_tail,
		  pos);
      if ( (0 != (daemon->options & MHD_USE_THREAD_PER_CONNECTION)) &&
	   (MHD_NO == pos->thread_joined) )
	{ 
	  if (0 != (rc = pthread_join (pos->pid, &unused)))
	    {
	      MHD_PANIC ("Failed to join a thread\n");
	    }
	}
      MHD_pool_destroy (pos->pool);
#if HTTPS_SUPPORT
      if (pos->tls_session != NULL)
	gnutls_deinit (pos->tls_session);
#endif
      MHD_ip_limit_del (daemon, (struct sockaddr*)pos->addr, pos->addr_len);
      if (NULL != pos->response)
	{
	  MHD_destroy_response (pos->response);
	  pos->response = NULL;
	}
      if (-1 != pos->socket_fd)
	CLOSE (pos->socket_fd);
      if (NULL != pos->addr)
	free (pos->addr);
      free (pos);
      daemon->max_connections++;
    }
  if (0 != pthread_mutex_unlock(&daemon->cleanup_connection_mutex))
    {
      MHD_PANIC ("Failed to release cleanup mutex\n");
    }
}


/**
 * Obtain timeout value for select for this daemon
 * (only needed if connection timeout is used).  The
 * returned value is how long select should at most
 * block, not the timeout value set for connections.
 *
 * @param daemon daemon to query for timeout
 * @param timeout set to the timeout (in milliseconds)
 * @return MHD_YES on success, MHD_NO if timeouts are
 *        not used (or no connections exist that would
 *        necessiate the use of a timeout right now).
 */
int
MHD_get_timeout (struct MHD_Daemon *daemon,
		 MHD_UNSIGNED_LONG_LONG *timeout)
{
  time_t earliest_deadline;
  time_t now;
  struct MHD_Connection *pos;
  int have_timeout;

  if (0 != (daemon->options & MHD_USE_THREAD_PER_CONNECTION))
    {
#if HAVE_MESSAGES
      MHD_DLOG (daemon, "Illegal call to MHD_get_timeout\n");
#endif  
      return MHD_NO;
    }
  have_timeout = MHD_NO;
  for (pos = daemon->connections_head; NULL != pos; pos = pos->next)
    {
#if HTTPS_SUPPORT
      if (MHD_YES == pos->tls_read_ready)
	{
	  earliest_deadline = 0;
	  have_timeout = MHD_YES;
	  break;
	}
#endif
      if (0 != pos->connection_timeout) 
	{
	  if ( (! have_timeout) ||
	       (earliest_deadline > pos->last_activity + pos->connection_timeout) )
	    earliest_deadline = pos->last_activity + pos->connection_timeout;
#if HTTPS_SUPPORT
	  if (  (0 != (daemon->options & MHD_USE_SSL)) &&
		(0 != gnutls_record_check_pending (pos->tls_session)) )
	    earliest_deadline = 0;
#endif
	  have_timeout = MHD_YES;
	}
    }
  if (MHD_NO == have_timeout)
    return MHD_NO;
  now = MHD_monotonic_time();
  if (earliest_deadline < now)
    *timeout = 0;
  else
    *timeout = 1000 * (1 + earliest_deadline - now);
  return MHD_YES;
}


/**
 * Main select call.
 *
 * @param daemon daemon to run select loop for
 * @param may_block YES if blocking, NO if non-blocking
 * @return MHD_NO on serious errors, MHD_YES on success
 */
static int
MHD_select (struct MHD_Daemon *daemon, 
	    int may_block)
{
  struct MHD_Connection *pos;
  struct MHD_Connection *next;
  int num_ready;
  fd_set rs;
  fd_set ws;
  fd_set es;
  int max;
  struct timeval timeout;
  struct timeval *tv;
  MHD_UNSIGNED_LONG_LONG ltimeout;
  int ds;

  timeout.tv_sec = 0;
  timeout.tv_usec = 0;
  if (MHD_YES == daemon->shutdown)
    return MHD_NO;
  FD_ZERO (&rs);
  FD_ZERO (&ws);
  FD_ZERO (&es);
  max = -1;
  if (0 == (daemon->options & MHD_USE_THREAD_PER_CONNECTION))
    {
      /* single-threaded, go over everything */
      if (MHD_NO == MHD_get_fdset (daemon, &rs, &ws, &es, &max))
        return MHD_NO;

      /* If we're at the connection limit, no need to
         accept new connections. */
      if ( (0 == daemon->max_connections) && 
	   (-1 != daemon->socket_fd) )
        FD_CLR (daemon->socket_fd, &rs);
    }
  else
    {
      /* accept only, have one thread per connection */
      if (-1 != daemon->socket_fd) 
	{
	  max = daemon->socket_fd;
	  FD_SET (daemon->socket_fd, &rs);
	}
    }
  if (-1 != daemon->wpipe[0])
    {
      FD_SET (daemon->wpipe[0], &rs);
      /* update max file descriptor */
      if (max < daemon->wpipe[0])
	max = daemon->wpipe[0];
    }

  tv = NULL;
  if (MHD_NO == may_block)
    {
      timeout.tv_usec = 0;
      timeout.tv_sec = 0;
      tv = &timeout;
    }
  else if ( (0 == (daemon->options & MHD_USE_THREAD_PER_CONNECTION)) &&
	    (MHD_YES == MHD_get_timeout (daemon, &ltimeout)) )
    {
      /* ltimeout is in ms */
      timeout.tv_usec = (ltimeout % 1000) * 1000;
      timeout.tv_sec = ltimeout / 1000;
      tv = &timeout;
    }
  num_ready = SELECT (max + 1, &rs, &ws, &es, tv);

  if (MHD_YES == daemon->shutdown)
    return MHD_NO;
  if (num_ready < 0)
    {
      if (EINTR == errno)
        return MHD_YES;
#if HAVE_MESSAGES
      MHD_DLOG (daemon, "select failed: %s\n", STRERROR (errno));
#endif
      return MHD_NO;
    }
  /* select connection thread handling type */
  if ( (-1 != (ds = daemon->socket_fd)) &&
       (FD_ISSET (ds, &rs)) )
    MHD_accept_connection (daemon);
  if (0 == (daemon->options & MHD_USE_THREAD_PER_CONNECTION))
    {
      /* do not have a thread per connection, process all connections now */
      next = daemon->connections_head;
      while (NULL != (pos = next))
        {
	  next = pos->next;
          ds = pos->socket_fd;
          if (ds != -1)
            {
              if ( (FD_ISSET (ds, &rs))
#if HTTPS_SUPPORT
		   || (MHD_YES == pos->tls_read_ready) 
#endif
		   )
                pos->read_handler (pos);
              if (FD_ISSET (ds, &ws))
                pos->write_handler (pos);
	      pos->idle_handler (pos);
            }
        }
    }
  return MHD_YES;
}


#ifdef HAVE_POLL_H
/**
 * Process all of our connections and possibly the server
 * socket using 'poll'.
 *
 * @param daemon daemon to run poll loop for
 * @param may_block YES if blocking, NO if non-blocking
 * @return MHD_NO on serious errors, MHD_YES on success
 */
static int
MHD_poll_all (struct MHD_Daemon *daemon,
	      int may_block)
{
  unsigned int num_connections;
  struct MHD_Connection *pos;
  struct MHD_Connection *next;

  /* count number of connections and thus determine poll set size */
  num_connections = 0;
  for (pos = daemon->connections_head; NULL != pos; pos = pos->next)
    num_connections++;

  {
    struct pollfd p[2 + num_connections];
    struct MHD_Pollfd mp;
    MHD_UNSIGNED_LONG_LONG ltimeout;
    unsigned int i;
    int timeout;
    unsigned int poll_server;
    int poll_listen;
    
    memset (p, 0, sizeof (p));
    poll_server = 0;
    poll_listen = -1;
    if ( (-1 != daemon->socket_fd) &&
	 (0 != daemon->max_connections) )
      {
	/* only listen if we are not at the connection limit */
	p[poll_server].fd = daemon->socket_fd;
	p[poll_server].events = POLLIN;
	p[poll_server].revents = 0;
	poll_listen = (int) poll_server;
	poll_server++;
      }
    if (-1 != daemon->wpipe[0]) 
      {
	p[poll_server].fd = daemon->wpipe[0];
	p[poll_server].events = POLLIN;
	p[poll_server].revents = 0;
	poll_server++;
      }
    if (may_block == MHD_NO)
      timeout = 0;
    else if ( (0 != (daemon->options & MHD_USE_THREAD_PER_CONNECTION)) ||
	      (MHD_YES != MHD_get_timeout (daemon, &ltimeout)) )
      timeout = -1;
    else
      timeout = (ltimeout > INT_MAX) ? INT_MAX : (int) ltimeout;
    
    i = 0;
    for (pos = daemon->connections_head; NULL != pos; pos = pos->next)
      {
	memset(&mp, 0, sizeof (struct MHD_Pollfd));
	MHD_connection_get_pollfd (pos, &mp);
	p[poll_server+i].fd = mp.fd;
	if (mp.events & MHD_POLL_ACTION_IN) 
	  p[poll_server+i].events |= POLLIN;        
	if (mp.events & MHD_POLL_ACTION_OUT) 
	  p[poll_server+i].events |= POLLOUT;
	i++;
      }
    if (poll (p, poll_server + num_connections, timeout) < 0) 
      {
	if (EINTR == errno)
	  return MHD_YES;
#if HAVE_MESSAGES
	MHD_DLOG (daemon, 
		  "poll failed: %s\n", 
		  STRERROR (errno));
#endif
	return MHD_NO;
      }
    /* handle shutdown */
    if (MHD_YES == daemon->shutdown)
      return MHD_NO;  
    i = 0;
    next = daemon->connections_head;
    while (NULL != (pos = next))
      {
	next = pos->next;
	/* first, sanity checks */
	if (i >= num_connections)
	  break; /* connection list changed somehow, retry later ... */
	MHD_connection_get_pollfd (pos, &mp);
	if (p[poll_server+i].fd != mp.fd)
	  break; /* fd mismatch, something else happened, retry later ... */

	/* normal handling */
	if (0 != (p[poll_server+i].revents & POLLIN)) 
	  pos->read_handler (pos);
	if (0 != (p[poll_server+i].revents & POLLOUT)) 
	  pos->write_handler (pos);	
	pos->idle_handler (pos);
	i++;
      }
    if ( (-1 != poll_listen) &&
	 (0 != (p[poll_listen].revents & POLLIN)) )
      MHD_accept_connection (daemon);
  }
  return MHD_YES;
}


/**
 * Process only the listen socket using 'poll'.
 *
 * @param daemon daemon to run poll loop for
 * @param may_block YES if blocking, NO if non-blocking
 * @return MHD_NO on serious errors, MHD_YES on success
 */
static int
MHD_poll_listen_socket (struct MHD_Daemon *daemon,
			int may_block)
{
  struct pollfd p[2];
  int timeout;
  unsigned int poll_count;
  int poll_listen;
    
  memset (&p, 0, sizeof (p));
  poll_count = 0;
  poll_listen = -1;
  if (-1 != daemon->socket_fd)
    {
      p[poll_count].fd = daemon->socket_fd;
      p[poll_count].events = POLLIN;
      p[poll_count].revents = 0;
      poll_listen = poll_count;
      poll_count++;
    }
  if (-1 != daemon->wpipe[0])
    {
      p[poll_count].fd = daemon->wpipe[0];
      p[poll_count].events = POLLIN;
      p[poll_count].revents = 0;
      poll_count++;
    }
  if (MHD_NO == may_block)
    timeout = 0;
  else
    timeout = -1;
  if (poll (p, poll_count, timeout) < 0)
    {
      if (EINTR == errno)
	return MHD_YES;
#if HAVE_MESSAGES
      MHD_DLOG (daemon, "poll failed: %s\n", STRERROR (errno));
#endif
      return MHD_NO;
    }
  /* handle shutdown */
  if (MHD_YES == daemon->shutdown)
    return MHD_NO;  
  if ( (-1 != poll_listen) &&
       (0 != (p[poll_listen].revents & POLLIN)) )
    MHD_accept_connection (daemon);  
  return MHD_YES;
}
#endif


/**
 * Do 'poll'-based processing.
 *
 * @param daemon daemon to run poll loop for
 * @param may_block YES if blocking, NO if non-blocking
 * @return MHD_NO on serious errors, MHD_YES on success
 */
static int
MHD_poll (struct MHD_Daemon *daemon,
	  int may_block)
{
#ifdef HAVE_POLL_H
  if (MHD_YES == daemon->shutdown)
    return MHD_NO;
  if (0 == (daemon->options & MHD_USE_THREAD_PER_CONNECTION))
    return MHD_poll_all (daemon, may_block);
  else
    return MHD_poll_listen_socket (daemon, may_block);
#else
  return MHD_NO;
#endif
}


/**
 * Run webserver operations (without blocking unless
 * in client callbacks).  This method should be called
 * by clients in combination with MHD_get_fdset
 * if the client-controlled select method is used.
 *
 * @return MHD_YES on success, MHD_NO if this
 *         daemon was not started with the right
 *         options for this call.
 */
int
MHD_run (struct MHD_Daemon *daemon)
{
  if ( (MHD_YES == daemon->shutdown) || 
       (0 != (daemon->options & MHD_USE_THREAD_PER_CONNECTION)) ||
       (0 != (daemon->options & MHD_USE_SELECT_INTERNALLY)) )
    return MHD_NO;
  if (0 == (daemon->options & MHD_USE_POLL)) 
    MHD_select (daemon, MHD_NO);    
  else    
    MHD_poll (daemon, MHD_NO);    
  MHD_cleanup_connections (daemon);
  return MHD_YES;
}


/**
 * Thread that runs the select loop until the daemon
 * is explicitly shut down.
 *
 * @param cls 'struct MHD_Deamon' to run select loop in a thread for
 * @return always NULL (on shutdown)
 */
static void *
MHD_select_thread (void *cls)
{
  struct MHD_Daemon *daemon = cls;

  while (MHD_YES != daemon->shutdown)
    {
      if (0 == (daemon->options & MHD_USE_POLL)) 
	MHD_select (daemon, MHD_YES);
      else 
	MHD_poll (daemon, MHD_YES);      
      MHD_cleanup_connections (daemon);
    }
  return NULL;
}


/**
 * Start a webserver on the given port.
 *
 * @param port port to bind to
 * @param apc callback to call to check which clients
 *        will be allowed to connect
 * @param apc_cls extra argument to apc
 * @param dh default handler for all URIs
 * @param dh_cls extra argument to dh
 * @return NULL on error, handle to daemon on success
 */
struct MHD_Daemon *
MHD_start_daemon (unsigned int options,
                  uint16_t port,
                  MHD_AcceptPolicyCallback apc,
                  void *apc_cls,
                  MHD_AccessHandlerCallback dh, void *dh_cls, ...)
{
  struct MHD_Daemon *daemon;
  va_list ap;

  va_start (ap, dh_cls);
  daemon = MHD_start_daemon_va (options, port, apc, apc_cls, dh, dh_cls, ap);
  va_end (ap);
  return daemon;
}


/**
 * Signature of the MHD custom logger function.
 *
 * @param cls closure
 * @param format format string
 * @param va arguments to the format string (fprintf-style)
 */
typedef void (*VfprintfFunctionPointerType)(void *cls,
					    const char *format, 
					    va_list va);


/**
 * Parse a list of options given as varargs.
 * 
 * @param daemon the daemon to initialize
 * @param servaddr where to store the server's listen address
 * @param ap the options
 * @return MHD_YES on success, MHD_NO on error
 */
static int
parse_options_va (struct MHD_Daemon *daemon,
		  const struct sockaddr **servaddr,
		  va_list ap);


/**
 * Parse a list of options given as varargs.
 * 
 * @param daemon the daemon to initialize
 * @param servaddr where to store the server's listen address
 * @param ... the options
 * @return MHD_YES on success, MHD_NO on error
 */
static int
parse_options (struct MHD_Daemon *daemon,
	       const struct sockaddr **servaddr,
	       ...)
{
  va_list ap;
  int ret;

  va_start (ap, servaddr);
  ret = parse_options_va (daemon, servaddr, ap);
  va_end (ap);
  return ret;
}


/**
 * Parse a list of options given as varargs.
 * 
 * @param daemon the daemon to initialize
 * @param servaddr where to store the server's listen address
 * @param ap the options
 * @return MHD_YES on success, MHD_NO on error
 */
static int
parse_options_va (struct MHD_Daemon *daemon,
		  const struct sockaddr **servaddr,
		  va_list ap)
{
  enum MHD_OPTION opt;
  struct MHD_OptionItem *oa;
  unsigned int i;
#if HTTPS_SUPPORT
  int ret;
  const char *pstr;
#endif
  
  while (MHD_OPTION_END != (opt = (enum MHD_OPTION) va_arg (ap, int)))
    {
      switch (opt)
        {
        case MHD_OPTION_CONNECTION_MEMORY_LIMIT:
          daemon->pool_size = va_arg (ap, size_t);
          break;
        case MHD_OPTION_CONNECTION_LIMIT:
          daemon->max_connections = va_arg (ap, unsigned int);
          break;
        case MHD_OPTION_CONNECTION_TIMEOUT:
          daemon->connection_timeout = va_arg (ap, unsigned int);
          break;
        case MHD_OPTION_NOTIFY_COMPLETED:
          daemon->notify_completed =
            va_arg (ap, MHD_RequestCompletedCallback);
          daemon->notify_completed_cls = va_arg (ap, void *);
          break;
        case MHD_OPTION_PER_IP_CONNECTION_LIMIT:
          daemon->per_ip_connection_limit = va_arg (ap, unsigned int);
          break;
        case MHD_OPTION_SOCK_ADDR:
          *servaddr = va_arg (ap, const struct sockaddr *);
          break;
        case MHD_OPTION_URI_LOG_CALLBACK:
          daemon->uri_log_callback =
            va_arg (ap, LogCallback);
          daemon->uri_log_callback_cls = va_arg (ap, void *);
          break;
        case MHD_OPTION_THREAD_POOL_SIZE:
          daemon->worker_pool_size = va_arg (ap, unsigned int);
	  if (daemon->worker_pool_size >= (SIZE_MAX / sizeof (struct MHD_Daemon)))
	    {
#if HAVE_MESSAGES
	      MHD_DLOG (daemon,
			"Specified thread pool size (%u) too big\n",
			daemon->worker_pool_size);
#endif
	      return MHD_NO;
	    }
          break;
#if HTTPS_SUPPORT
        case MHD_OPTION_HTTPS_MEM_KEY:
	  if (0 != (daemon->options & MHD_USE_SSL))
	    daemon->https_mem_key = va_arg (ap, const char *);
#if HAVE_MESSAGES
	  else
	    MHD_DLOG (daemon,
		      "MHD HTTPS option %d passed to MHD but MHD_USE_SSL not set\n",
		      opt);	  
#endif
          break;
        case MHD_OPTION_HTTPS_MEM_CERT:
	  if (0 != (daemon->options & MHD_USE_SSL))
	    daemon->https_mem_cert = va_arg (ap, const char *);
#if HAVE_MESSAGES
	  else
	    MHD_DLOG (daemon,
		      "MHD HTTPS option %d passed to MHD but MHD_USE_SSL not set\n",
		      opt);	  
#endif
          break;
        case MHD_OPTION_HTTPS_MEM_TRUST:
	  if (0 != (daemon->options & MHD_USE_SSL))
	    daemon->https_mem_trust = va_arg (ap, const char *);
#if HAVE_MESSAGES
	  else
	    MHD_DLOG (daemon,
		      "MHD HTTPS option %d passed to MHD but MHD_USE_SSL not set\n",
		      opt);
#endif
          break;
	case MHD_OPTION_HTTPS_CRED_TYPE:
	  daemon->cred_type = (gnutls_credentials_type_t) va_arg (ap, int);
	  break;
        case MHD_OPTION_HTTPS_PRIORITIES:
	  if (0 != (daemon->options & MHD_USE_SSL))
	    {
	      gnutls_priority_deinit (daemon->priority_cache);
	      ret = gnutls_priority_init (&daemon->priority_cache,
					  pstr = va_arg (ap, const char*),
					  NULL);
	      if (ret != GNUTLS_E_SUCCESS)
	      {
#if HAVE_MESSAGES
		MHD_DLOG (daemon,
			  "Setting priorities to `%s' failed: %s\n",
			  pstr,
			  gnutls_strerror (ret));
#endif	  
		daemon->priority_cache = NULL;
		return MHD_NO;
	      }
	    }
          break;
#endif
#ifdef DAUTH_SUPPORT
	case MHD_OPTION_DIGEST_AUTH_RANDOM:
	  daemon->digest_auth_rand_size = va_arg (ap, size_t);
	  daemon->digest_auth_random = va_arg (ap, const char *);
	  break;
	case MHD_OPTION_NONCE_NC_SIZE:
	  daemon->nonce_nc_size = va_arg (ap, unsigned int);
	  break;
#endif
	case MHD_OPTION_LISTEN_SOCKET:
	  daemon->socket_fd = va_arg (ap, int);	  
	  break;
        case MHD_OPTION_EXTERNAL_LOGGER:
#if HAVE_MESSAGES
          daemon->custom_error_log =
            va_arg (ap, VfprintfFunctionPointerType);
          daemon->custom_error_log_cls = va_arg (ap, void *);
#else
          va_arg (ap, VfprintfFunctionPointerType);
          va_arg (ap, void *);
#endif
          break;
        case MHD_OPTION_THREAD_STACK_SIZE:
          daemon->thread_stack_size = va_arg (ap, size_t);
          break;
	case MHD_OPTION_ARRAY:
	  oa = va_arg (ap, struct MHD_OptionItem*);
	  i = 0;
	  while (MHD_OPTION_END != (opt = oa[i].option))
	    {
	      switch (opt)
		{
		  /* all options taking 'size_t' */
		case MHD_OPTION_CONNECTION_MEMORY_LIMIT:
		case MHD_OPTION_THREAD_STACK_SIZE:
		  if (MHD_YES != parse_options (daemon,
						servaddr,
						opt,
						(size_t) oa[i].value,
						MHD_OPTION_END))
		    return MHD_NO;
		  break;
		  /* all options taking 'unsigned int' */
		case MHD_OPTION_NONCE_NC_SIZE:
		case MHD_OPTION_CONNECTION_LIMIT:
		case MHD_OPTION_CONNECTION_TIMEOUT:
		case MHD_OPTION_PER_IP_CONNECTION_LIMIT:
		case MHD_OPTION_THREAD_POOL_SIZE:
		  if (MHD_YES != parse_options (daemon,
						servaddr,
						opt,
						(unsigned int) oa[i].value,
						MHD_OPTION_END))
		    return MHD_NO;
		  break;
		  /* all options taking 'int' or 'enum' */
		case MHD_OPTION_HTTPS_CRED_TYPE:
		case MHD_OPTION_LISTEN_SOCKET:
		  if (MHD_YES != parse_options (daemon,
						servaddr,
						opt,
						(int) oa[i].value,
						MHD_OPTION_END))
		    return MHD_NO;
		  break;
		  /* all options taking one pointer */
		case MHD_OPTION_SOCK_ADDR:
		case MHD_OPTION_HTTPS_MEM_KEY:
		case MHD_OPTION_HTTPS_MEM_CERT:
		case MHD_OPTION_HTTPS_MEM_TRUST:
		case MHD_OPTION_HTTPS_PRIORITIES:
		case MHD_OPTION_ARRAY:
		  if (MHD_YES != parse_options (daemon,
						servaddr,
						opt,
						oa[i].ptr_value,
						MHD_OPTION_END))
		    return MHD_NO;
		  break;
		  /* all options taking two pointers */
		case MHD_OPTION_NOTIFY_COMPLETED:
		case MHD_OPTION_URI_LOG_CALLBACK:
		case MHD_OPTION_EXTERNAL_LOGGER:
		case MHD_OPTION_UNESCAPE_CALLBACK:
		  if (MHD_YES != parse_options (daemon,
						servaddr,
						opt,
						(void *) oa[i].value,
						oa[i].ptr_value,
						MHD_OPTION_END))
		    return MHD_NO;
		  break;
		  /* options taking size_t-number followed by pointer */
		case MHD_OPTION_DIGEST_AUTH_RANDOM:
		  if (MHD_YES != parse_options (daemon,
						servaddr,
						opt,
						(size_t) oa[i].value,
						oa[i].ptr_value,
						MHD_OPTION_END))
		    return MHD_NO;
		  break;
		default:
		  return MHD_NO;
		}
	      i++;
	    }
	  break;
        case MHD_OPTION_UNESCAPE_CALLBACK:
          daemon->unescape_callback =
            va_arg (ap, UnescapeCallback);
          daemon->unescape_callback_cls = va_arg (ap, void *);
          break;
        default:
#if HAVE_MESSAGES
          if (((opt >= MHD_OPTION_HTTPS_MEM_KEY) &&
              (opt <= MHD_OPTION_HTTPS_PRIORITIES)) || (opt == MHD_OPTION_HTTPS_MEM_TRUST))
            {
              MHD_DLOG (daemon,
			"MHD HTTPS option %d passed to MHD compiled without HTTPS support\n",
			opt);
            }
          else
            {
              MHD_DLOG (daemon,
			"Invalid option %d! (Did you terminate the list with MHD_OPTION_END?)\n",
			opt);
            }
#endif
	  return MHD_NO;
        }
    }  
  return MHD_YES;
}


/**
 * Create a listen socket, if possible with CLOEXEC flag set.
 *
 * @param domain socket domain (i.e. PF_INET)
 * @param type socket type (usually SOCK_STREAM)
 * @param protocol desired protocol, 0 for default
 */
static int
create_socket (int domain, int type, int protocol)
{
  static int sock_cloexec = SOCK_CLOEXEC;
  int ctype = SOCK_STREAM | sock_cloexec;
  int fd;
  int flags;
#ifdef WINDOWS
  DWORD dwFlags;
#endif
 
  /* use SOCK_STREAM rather than ai_socktype: some getaddrinfo
   * implementations do not set ai_socktype, e.g. RHL6.2. */
  fd = SOCKET (domain, ctype, protocol);
  if ( (-1 == fd) && (EINVAL == errno) && (0 != sock_cloexec) )
  {
    sock_cloexec = 0;
    fd = SOCKET(domain, type, protocol);
  }
  if (-1 == fd)
    return -1;
  if (0 != sock_cloexec)
    return fd; /* this is it */  
  /* flag was not set during 'socket' call, let's try setting it manually */
#ifndef WINDOWS
  flags = fcntl (fd, F_GETFD);
  if (flags < 0)
#else
  if (!GetHandleInformation ((HANDLE) fd, &dwFlags))
#endif
  {
#ifdef WINDOWS
    SetErrnoFromWinError (GetLastError ());
#endif
    return fd; /* good luck */
  }
#ifndef WINDOWS
  if (flags == (flags | FD_CLOEXEC))
    return fd; /* already set */
  flags |= FD_CLOEXEC;
  if (0 != fcntl (fd, F_SETFD, flags))
#else
  if (dwFlags != (dwFlags | HANDLE_FLAG_INHERIT))
    return fd; /* already unset */
  if (!SetHandleInformation ((HANDLE) fd, HANDLE_FLAG_INHERIT, 0))
#endif
  {
#ifdef WINDOWS
    SetErrnoFromWinError (GetLastError ());
#endif
    return fd; /* good luck */
  }
  return fd;
}


/**
 * Start a webserver on the given port.
 *
 * @param port port to bind to
 * @param apc callback to call to check which clients
 *        will be allowed to connect
 * @param apc_cls extra argument to apc
 * @param dh default handler for all URIs
 * @param dh_cls extra argument to dh
 * @return NULL on error, handle to daemon on success
 */
struct MHD_Daemon *
MHD_start_daemon_va (unsigned int options,
                     uint16_t port,
                     MHD_AcceptPolicyCallback apc,
                     void *apc_cls,
                     MHD_AccessHandlerCallback dh, void *dh_cls,
		     va_list ap)
{
  const int on = 1;
  struct MHD_Daemon *daemon;
  int socket_fd;
  struct sockaddr_in servaddr4;
#if HAVE_INET6
  struct sockaddr_in6 servaddr6;
#endif
  const struct sockaddr *servaddr = NULL;
  socklen_t addrlen;
  unsigned int i;
  int res_thread_create;
  int use_pipe;

#ifndef HAVE_INET6
  if (0 != (options & MHD_USE_IPv6))
    return NULL;    
#endif
#ifndef HAVE_POLL_H
  if (0 != (options & MHD_USE_POLL))
    return NULL;    
#endif
#if ! HTTPS_SUPPORT
  if (0 != (options & MHD_USE_SSL))
    return NULL;    
#endif
  if (NULL == dh)
    return NULL;
  if (NULL == (daemon = malloc (sizeof (struct MHD_Daemon))))
    return NULL;
  memset (daemon, 0, sizeof (struct MHD_Daemon));
  /* try to open listen socket */
#if HTTPS_SUPPORT
  if (0 != (options & MHD_USE_SSL))
    {
      gnutls_priority_init (&daemon->priority_cache,
			    "NORMAL",
			    NULL);
    }
#endif
  daemon->socket_fd = -1;
  daemon->options = (enum MHD_OPTION) options;
  daemon->port = port;
  daemon->apc = apc;
  daemon->apc_cls = apc_cls;
  daemon->default_handler = dh;
  daemon->default_handler_cls = dh_cls;
  daemon->max_connections = MHD_MAX_CONNECTIONS_DEFAULT;
  daemon->pool_size = MHD_POOL_SIZE_DEFAULT;
  daemon->unescape_callback = &MHD_http_unescape;
  daemon->connection_timeout = 0;       /* no timeout */
  daemon->wpipe[0] = -1;
  daemon->wpipe[1] = -1;
#if HAVE_MESSAGES
  daemon->custom_error_log = (MHD_LogCallback) &vfprintf;
  daemon->custom_error_log_cls = stderr;
#endif
#ifdef HAVE_LISTEN_SHUTDOWN
  use_pipe = (0 != (daemon->options & MHD_USE_NO_LISTEN_SOCKET));
#else
  use_pipe = 1; /* yes, must use pipe to signal shutdown */
#endif
  if ( (use_pipe) &&
       (0 != PIPE (daemon->wpipe)) )
    {
#if HAVE_MESSAGES
      MHD_DLOG (daemon, 
		"Failed to create control pipe: %s\n",
		STRERROR (errno));
#endif
      free (daemon);
      return NULL;
    }
#ifndef WINDOWS
  if ( (0 == (options & MHD_USE_POLL)) &&
       (daemon->wpipe[0] >= FD_SETSIZE) )
    {
#if HAVE_MESSAGES
      MHD_DLOG (daemon, 
		"file descriptor for control pipe exceeds maximum value\n");
#endif
      CLOSE (daemon->wpipe[0]);
      CLOSE (daemon->wpipe[1]);
      free (daemon);
      return NULL;
    }
#endif
#ifdef DAUTH_SUPPORT
  daemon->digest_auth_rand_size = 0;
  daemon->digest_auth_random = NULL;
  daemon->nonce_nc_size = 4; /* tiny */
#endif
#if HTTPS_SUPPORT
  if (0 != (options & MHD_USE_SSL))
    {
      daemon->cred_type = GNUTLS_CRD_CERTIFICATE;
    }
#endif


  if (MHD_YES != parse_options_va (daemon, &servaddr, ap))
    {
#if HTTPS_SUPPORT
      if ( (0 != (options & MHD_USE_SSL)) &&
	   (NULL != daemon->priority_cache) )
	gnutls_priority_deinit (daemon->priority_cache);
#endif
      free (daemon);
      return NULL;
    }
#ifdef DAUTH_SUPPORT
  if (daemon->nonce_nc_size > 0) 
    {
      if ( ( (size_t) (daemon->nonce_nc_size * sizeof(struct MHD_NonceNc))) / 
	   sizeof(struct MHD_NonceNc) != daemon->nonce_nc_size)
	{
#if HAVE_MESSAGES
	  MHD_DLOG (daemon,
		    "Specified value for NC_SIZE too large\n");
#endif
#if HTTPS_SUPPORT
	  if (0 != (options & MHD_USE_SSL))
	    gnutls_priority_deinit (daemon->priority_cache);
#endif
	  free (daemon);
	  return NULL;	  
	}
      daemon->nnc = malloc (daemon->nonce_nc_size * sizeof(struct MHD_NonceNc));
      if (NULL == daemon->nnc)
	{
#if HAVE_MESSAGES
	  MHD_DLOG (daemon,
		    "Failed to allocate memory for nonce-nc map: %s\n",
		    STRERROR (errno));
#endif
#if HTTPS_SUPPORT
	  if (0 != (options & MHD_USE_SSL))
	    gnutls_priority_deinit (daemon->priority_cache);
#endif
	  free (daemon);
	  return NULL;
	}
    }
  
  if (0 != pthread_mutex_init (&daemon->nnc_lock, NULL))
    {
#if HAVE_MESSAGES
      MHD_DLOG (daemon,
		"MHD failed to initialize nonce-nc mutex\n");
#endif
#if HTTPS_SUPPORT
      if (0 != (options & MHD_USE_SSL))
	gnutls_priority_deinit (daemon->priority_cache);
#endif
      free (daemon->nnc);
      free (daemon);
      return NULL;
    }
#endif

  /* Thread pooling currently works only with internal select thread model */
  if ( (0 == (options & MHD_USE_SELECT_INTERNALLY)) && 
       (daemon->worker_pool_size > 0) )
    {
#if HAVE_MESSAGES
      MHD_DLOG (daemon,
		"MHD thread pooling only works with MHD_USE_SELECT_INTERNALLY\n");
#endif
      goto free_and_fail;
    }

#ifdef __SYMBIAN32__
  if (0 != (options & (MHD_USE_SELECT_INTERNALLY | MHD_USE_THREAD_PER_CONNECTION)))
    {
#if HAVE_MESSAGES
      MHD_DLOG (daemon,
		"Threaded operations are not supported on Symbian.\n");
#endif
      goto free_and_fail;
    }
#endif
  if ( (-1 == daemon->socket_fd) &&
       (0 == (daemon->options & MHD_USE_NO_LISTEN_SOCKET)) )
    {
      /* try to open listen socket */
      if ((options & MHD_USE_IPv6) != 0)
	socket_fd = create_socket (PF_INET6, SOCK_STREAM, 0);
      else
	socket_fd = create_socket (PF_INET, SOCK_STREAM, 0);
      if (-1 == socket_fd)
	{
#if HAVE_MESSAGES
	  if (0 != (options & MHD_USE_DEBUG))
	    MHD_DLOG (daemon, 
		      "Call to socket failed: %s\n", 
		      STRERROR (errno));
#endif
	  goto free_and_fail;
	}
      if ((SETSOCKOPT (socket_fd,
		       SOL_SOCKET,
		       SO_REUSEADDR,
		       &on, sizeof (on)) < 0) && ((options & MHD_USE_DEBUG) != 0))
	{
#if HAVE_MESSAGES
	  MHD_DLOG (daemon, 
		    "setsockopt failed: %s\n", 
		    STRERROR (errno));
#endif
	}
      
      /* check for user supplied sockaddr */
#if HAVE_INET6
      if (0 != (options & MHD_USE_IPv6))
	addrlen = sizeof (struct sockaddr_in6);
      else
#endif
	addrlen = sizeof (struct sockaddr_in);
      if (NULL == servaddr)
	{
#if HAVE_INET6
	  if (0 != (options & MHD_USE_IPv6))
	    {
	      memset (&servaddr6, 0, sizeof (struct sockaddr_in6));
	      servaddr6.sin6_family = AF_INET6;
	      servaddr6.sin6_port = htons (port);
#if HAVE_SOCKADDR_IN_SIN_LEN
	      servaddr6.sin6_len = sizeof (struct sockaddr_in6);
#endif
	      servaddr = (struct sockaddr *) &servaddr6;
	    }
	  else
#endif
	    {
	      memset (&servaddr4, 0, sizeof (struct sockaddr_in));
	      servaddr4.sin_family = AF_INET;
	      servaddr4.sin_port = htons (port);
#if HAVE_SOCKADDR_IN_SIN_LEN
	      servaddr4.sin_len = sizeof (struct sockaddr_in);
#endif
	      servaddr = (struct sockaddr *) &servaddr4;
	    }
	}
      daemon->socket_fd = socket_fd;

      if (0 != (options & MHD_USE_IPv6))
	{
#ifdef IPPROTO_IPV6
#ifdef IPV6_V6ONLY
	  /* Note: "IPV6_V6ONLY" is declared by Windows Vista ff., see "IPPROTO_IPV6 Socket Options" 
	     (http://msdn.microsoft.com/en-us/library/ms738574%28v=VS.85%29.aspx); 
	     and may also be missing on older POSIX systems; good luck if you have any of those,
	     your IPv6 socket may then also bind against IPv4... */
#ifndef WINDOWS
	  const int on = 1;
	  setsockopt (socket_fd, 
		      IPPROTO_IPV6, IPV6_V6ONLY, 
		      &on, sizeof (on));
#else
	  const char on = 1;
	  setsockopt (socket_fd, 
		      IPPROTO_IPV6, IPV6_V6ONLY, 
		      &on, sizeof (on));
#endif
#endif
#endif
	}
      if (-1 == BIND (socket_fd, servaddr, addrlen))
	{
#if HAVE_MESSAGES
	  if (0 != (options & MHD_USE_DEBUG))
	    MHD_DLOG (daemon,
		      "Failed to bind to port %u: %s\n", 
		      (unsigned int) port, 
		      STRERROR (errno));
#endif
	  CLOSE (socket_fd);
	  goto free_and_fail;
	}
      
      if (LISTEN (socket_fd, 20) < 0)
	{
#if HAVE_MESSAGES
	  if (0 != (options & MHD_USE_DEBUG))
	    MHD_DLOG (daemon,
		      "Failed to listen for connections: %s\n", 
		      STRERROR (errno));
#endif
	  CLOSE (socket_fd);
	  goto free_and_fail;
	}      
    }
  else
    {
      socket_fd = daemon->socket_fd;
    }
#ifndef WINDOWS
  if ( (socket_fd >= FD_SETSIZE) &&
       (0 == (options & MHD_USE_POLL)) )
    {
#if HAVE_MESSAGES
      if ((options & MHD_USE_DEBUG) != 0)
        MHD_DLOG (daemon,
		  "Socket descriptor larger than FD_SETSIZE: %d > %d\n",
		  socket_fd,
		  FD_SETSIZE);
#endif
      CLOSE (socket_fd);
      goto free_and_fail;
    }
#endif

  if (0 != pthread_mutex_init (&daemon->per_ip_connection_mutex, NULL))
    {
#if HAVE_MESSAGES
      MHD_DLOG (daemon,
               "MHD failed to initialize IP connection limit mutex\n");
#endif
      if (-1 != socket_fd)
	CLOSE (socket_fd);
      goto free_and_fail;
    }
  if (0 != pthread_mutex_init (&daemon->cleanup_connection_mutex, NULL))
    {
#if HAVE_MESSAGES
      MHD_DLOG (daemon,
               "MHD failed to initialize IP connection limit mutex\n");
#endif
      pthread_mutex_destroy (&daemon->cleanup_connection_mutex);
      if (-1 != socket_fd)
	CLOSE (socket_fd);
      goto free_and_fail;
    }

#if HTTPS_SUPPORT
  /* initialize HTTPS daemon certificate aspects & send / recv functions */
  if ((0 != (options & MHD_USE_SSL)) && (0 != MHD_TLS_init (daemon)))
    {
#if HAVE_MESSAGES
      MHD_DLOG (daemon, 
		"Failed to initialize TLS support\n");
#endif
      if (-1 != socket_fd)
	CLOSE (socket_fd);
      pthread_mutex_destroy (&daemon->cleanup_connection_mutex);
      pthread_mutex_destroy (&daemon->per_ip_connection_mutex);
      goto free_and_fail;
    }
#endif
  if ( ( (0 != (options & MHD_USE_THREAD_PER_CONNECTION)) ||
	 ( (0 != (options & MHD_USE_SELECT_INTERNALLY)) &&
	   (0 == daemon->worker_pool_size)) ) && 
       (0 == (daemon->options & MHD_USE_NO_LISTEN_SOCKET)) &&
       (0 != (res_thread_create =
	      create_thread (&daemon->pid, daemon, &MHD_select_thread, daemon))))
    {
#if HAVE_MESSAGES
      MHD_DLOG (daemon,
                "Failed to create listen thread: %s\n", 
		STRERROR (res_thread_create));
#endif
      pthread_mutex_destroy (&daemon->cleanup_connection_mutex);
      pthread_mutex_destroy (&daemon->per_ip_connection_mutex);
      if (-1 != socket_fd)
	CLOSE (socket_fd);
      goto free_and_fail;
    }
  if ( (daemon->worker_pool_size > 0) &&
       (0 == (daemon->options & MHD_USE_NO_LISTEN_SOCKET)) )
    {
#ifndef MINGW
      int sk_flags;
#else
      unsigned long sk_flags;
#endif

      /* Coarse-grained count of connections per thread (note error
       * due to integer division). Also keep track of how many
       * connections are leftover after an equal split. */
      unsigned int conns_per_thread = daemon->max_connections
                                      / daemon->worker_pool_size;
      unsigned int leftover_conns = daemon->max_connections
                                    % daemon->worker_pool_size;

      i = 0; /* we need this in case fcntl or malloc fails */

      /* Accept must be non-blocking. Multiple children may wake up
       * to handle a new connection, but only one will win the race.
       * The others must immediately return. */
#ifndef MINGW
      sk_flags = fcntl (socket_fd, F_GETFL);
      if (sk_flags < 0)
        goto thread_failed;
      if (0 != fcntl (socket_fd, F_SETFL, sk_flags | O_NONBLOCK))
        goto thread_failed;
#else
      sk_flags = 1;
#if HAVE_PLIBC_FD
      if (SOCKET_ERROR ==
	  ioctlsocket (plibc_fd_get_handle (socket_fd), FIONBIO, &sk_flags))
        goto thread_failed;
#else
      if (ioctlsocket (socket_fd, FIONBIO, &sk_flags) == SOCKET_ERROR)
        goto thread_failed;
#endif // PLIBC_FD
#endif // MINGW

      /* Allocate memory for pooled objects */
      daemon->worker_pool = malloc (sizeof (struct MHD_Daemon)
                                    * daemon->worker_pool_size);
      if (NULL == daemon->worker_pool)
        goto thread_failed;

      /* Start the workers in the pool */
      for (i = 0; i < daemon->worker_pool_size; ++i)
        {
          /* Create copy of the Daemon object for each worker */
          struct MHD_Daemon *d = &daemon->worker_pool[i];
          memcpy (d, daemon, sizeof (struct MHD_Daemon));

          /* Adjust pooling params for worker daemons; note that memcpy()
             has already copied MHD_USE_SELECT_INTERNALLY thread model into
             the worker threads. */
          d->master = daemon;
          d->worker_pool_size = 0;
          d->worker_pool = NULL;

          /* Divide available connections evenly amongst the threads.
           * Thread indexes in [0, leftover_conns) each get one of the
           * leftover connections. */
          d->max_connections = conns_per_thread;
          if (i < leftover_conns)
            ++d->max_connections;

          /* Spawn the worker thread */
          if (0 != (res_thread_create = create_thread (&d->pid, daemon, &MHD_select_thread, d)))
            {
#if HAVE_MESSAGES
              MHD_DLOG (daemon,
                        "Failed to create pool thread: %s\n", 
			STRERROR (res_thread_create));
#endif
              /* Free memory for this worker; cleanup below handles
               * all previously-created workers. */
              goto thread_failed;
            }
        }
    }
  return daemon;

thread_failed:
  /* If no worker threads created, then shut down normally. Calling
     MHD_stop_daemon (as we do below) doesn't work here since it
     assumes a 0-sized thread pool means we had been in the default
     MHD_USE_SELECT_INTERNALLY mode. */
  if (0 == i)
    {
      if (-1 != socket_fd)
	CLOSE (socket_fd);
      pthread_mutex_destroy (&daemon->cleanup_connection_mutex);
      pthread_mutex_destroy (&daemon->per_ip_connection_mutex);
      if (NULL != daemon->worker_pool)
        free (daemon->worker_pool);
      goto free_and_fail;
    }

  /* Shutdown worker threads we've already created. Pretend
     as though we had fully initialized our daemon, but
     with a smaller number of threads than had been
     requested. */
  daemon->worker_pool_size = i - 1;
  MHD_stop_daemon (daemon);
  return NULL;

 free_and_fail:
  /* clean up basic memory state in 'daemon' and return NULL to 
     indicate failure */
#ifdef DAUTH_SUPPORT
  free (daemon->nnc);
  pthread_mutex_destroy (&daemon->nnc_lock);
#endif
#if HTTPS_SUPPORT
  if (0 != (options & MHD_USE_SSL))
    gnutls_priority_deinit (daemon->priority_cache);
#endif
  free (daemon);
  return NULL;
}


/**
 * Close all connections for the daemon; must only be called after
 * all of the threads have been joined and there is no more concurrent
 * activity on the connection lists.
 *
 * @param daemon daemon to close down
 */
static void
close_all_connections (struct MHD_Daemon *daemon)
{
  struct MHD_Connection *pos;
  void *unused;
  int rc;
  
  /* first, make sure all threads are aware of shutdown; need to
     traverse DLLs in peace... */
  if (0 != pthread_mutex_lock(&daemon->cleanup_connection_mutex))
    {
      MHD_PANIC ("Failed to acquire cleanup mutex\n");
    }
  for (pos = daemon->connections_head; NULL != pos; pos = pos->next)    
    SHUTDOWN (pos->socket_fd, 
	      (pos->read_closed == MHD_YES) ? SHUT_WR : SHUT_RDWR);    
  if (0 != pthread_mutex_unlock(&daemon->cleanup_connection_mutex))
    {
      MHD_PANIC ("Failed to release cleanup mutex\n");
    }

  /* now, collect threads */
  if (0 != (daemon->options & MHD_USE_THREAD_PER_CONNECTION))
    {
      while (NULL != (pos = daemon->connections_head))
	{
	  if (0 != (rc = pthread_join (pos->pid, &unused)))
	    {
	      MHD_PANIC ("Failed to join a thread\n");
	    }
	  pos->thread_joined = MHD_YES;
	}
    }

  /* now that we're alone, move everyone to cleanup */
  while (NULL != (pos = daemon->connections_head))
    {
      MHD_connection_close (pos,
			    MHD_REQUEST_TERMINATED_DAEMON_SHUTDOWN);
      DLL_remove (daemon->connections_head,
		  daemon->connections_tail,
		  pos);
      DLL_insert (daemon->cleanup_head,
		  daemon->cleanup_tail,
		  pos);
    }
  MHD_cleanup_connections (daemon);
}


/**
 * Shutdown an http daemon
 *
 * @param daemon daemon to stop
 */
void
MHD_stop_daemon (struct MHD_Daemon *daemon)
{
  void *unused;
  int fd;
  unsigned int i;
  int rc;

  if (NULL == daemon)
    return;
  daemon->shutdown = MHD_YES;
  fd = daemon->socket_fd;
  daemon->socket_fd = -1;
  /* Prepare workers for shutdown */
  if (NULL != daemon->worker_pool)
    {
      /* MHD_USE_NO_LISTEN_SOCKET disables thread pools, hence we need to check */
      for (i = 0; i < daemon->worker_pool_size; ++i)
	{
	  daemon->worker_pool[i].shutdown = MHD_YES;
	  daemon->worker_pool[i].socket_fd = -1;
	}
    }
  if (-1 != daemon->wpipe[1])
    {
      if (1 != WRITE (daemon->wpipe[1], "e", 1))
	MHD_PANIC ("failed to signal shutdownn via pipe");
    }
#ifdef HAVE_LISTEN_SHUTDOWN
  else
    {
      /* fd must not be -1 here, otherwise we'd have used the wpipe */
      SHUTDOWN (fd, SHUT_RDWR);
    }
#endif
#if DEBUG_CLOSE
#if HAVE_MESSAGES
  MHD_DLOG (daemon, "MHD listen socket shutdown\n");
#endif
#endif


  /* Signal workers to stop and clean them up */
  if (NULL != daemon->worker_pool)
    {
      /* MHD_USE_NO_LISTEN_SOCKET disables thread pools, hence we need to check */
      for (i = 0; i < daemon->worker_pool_size; ++i)
	{
	  if (0 != (rc = pthread_join (daemon->worker_pool[i].pid, &unused)))
	    {
	      MHD_PANIC ("Failed to join a thread\n");
	    }
	  close_all_connections (&daemon->worker_pool[i]);
	}
      free (daemon->worker_pool);
    }
  else
    {
      /* clean up master threads */
      if ((0 != (daemon->options & MHD_USE_THREAD_PER_CONNECTION)) ||
	  ((0 != (daemon->options & MHD_USE_SELECT_INTERNALLY))
	   && (0 == daemon->worker_pool_size)))
	{
	  if (0 != (rc = pthread_join (daemon->pid, &unused)))
	    {
	      MHD_PANIC ("Failed to join a thread\n");
	    }
	}
    }
  close_all_connections (daemon);
  if (-1 != fd)
    CLOSE (fd);

  /* TLS clean up */
#if HTTPS_SUPPORT
  if (0 != (daemon->options & MHD_USE_SSL))
    {
      gnutls_priority_deinit (daemon->priority_cache);
      if (daemon->x509_cred)
        gnutls_certificate_free_credentials (daemon->x509_cred);
    }
#endif

#ifdef DAUTH_SUPPORT
  free (daemon->nnc);
  pthread_mutex_destroy (&daemon->nnc_lock);
#endif
  pthread_mutex_destroy (&daemon->per_ip_connection_mutex);
  pthread_mutex_destroy (&daemon->cleanup_connection_mutex);

  if (-1 != daemon->wpipe[1])
    {
      CLOSE (daemon->wpipe[0]);
      CLOSE (daemon->wpipe[1]);
    }

  free (daemon);
}


/**
 * Obtain information about the given daemon
 * (not fully implemented!).
 *
 * @param daemon what daemon to get information about
 * @param infoType what information is desired?
 * @param ... depends on infoType
 * @return NULL if this information is not available
 *         (or if the infoType is unknown)
 */
const union MHD_DaemonInfo *
MHD_get_daemon_info (struct MHD_Daemon *daemon,
                     enum MHD_DaemonInfoType infoType, ...)
{
  switch (infoType)
    {
    case MHD_DAEMON_INFO_LISTEN_FD:
      return (const union MHD_DaemonInfo *) &daemon->socket_fd;
    default:
      return NULL;
    };
}


/**
 * Sets the global error handler to a different implementation.  "cb"
 * will only be called in the case of typically fatal, serious
 * internal consistency issues.  These issues should only arise in the
 * case of serious memory corruption or similar problems with the
 * architecture.  While "cb" is allowed to return and MHD will then
 * try to continue, this is never safe.
 *
 * The default implementation that is used if no panic function is set
 * simply prints an error message and calls "abort".  Alternative
 * implementations might call "exit" or other similar functions.
 *
 * @param cb new error handler
 * @param cls passed to error handler
 */
void 
MHD_set_panic_func (MHD_PanicCallback cb, void *cls)
{
  mhd_panic = cb;
  mhd_panic_cls = cls;
}


/**
 * Obtain the version of this library
 *
 * @return static version string, e.g. "0.4.1"
 */
const char *
MHD_get_version (void)
{
  return PACKAGE_VERSION;
}


#ifdef __GNUC__
#define ATTRIBUTE_CONSTRUCTOR __attribute__ ((constructor))
#define ATTRIBUTE_DESTRUCTOR __attribute__ ((destructor))
#else  // !__GNUC__
#define ATTRIBUTE_CONSTRUCTOR
#define ATTRIBUTE_DESTRUCTOR
#endif  // __GNUC__

#if HTTPS_SUPPORT
GCRY_THREAD_OPTION_PTHREAD_IMPL;
#endif


/**
 * Initialize do setup work.
 */
void ATTRIBUTE_CONSTRUCTOR 
MHD_init ()
{
  mhd_panic = &mhd_panic_std;
  mhd_panic_cls = NULL;

#ifdef WINDOWS
  plibc_init_utf8 ("GNU", "libmicrohttpd", 1);
#endif
#if HTTPS_SUPPORT
  gcry_control (GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread);
  gnutls_global_init ();
#endif
}


void ATTRIBUTE_DESTRUCTOR 
MHD_fini ()
{
#if HTTPS_SUPPORT
  gnutls_global_deinit ();
#endif
#ifdef WINDOWS
  plibc_shutdown ();
#endif
}

/* end of daemon.c */
