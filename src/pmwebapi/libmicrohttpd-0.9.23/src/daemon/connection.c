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
 * @file connection.c
 * @brief  Methods for managing connections
 * @author Daniel Pittman
 * @author Christian Grothoff
 */

#include "internal.h"
#include <limits.h>
#include "connection.h"
#include "memorypool.h"
#include "response.h"
#include "reason_phrase.h"

#if HAVE_NETINET_TCP_H
/* for TCP_CORK */
#include <netinet/tcp.h>
#endif

/**
 * Message to transmit when http 1.1 request is received
 */
#define HTTP_100_CONTINUE "HTTP/1.1 100 Continue\r\n\r\n"

/**
 * Response text used when the request (http header) is too big to
 * be processed.
 *
 * Intentionally empty here to keep our memory footprint
 * minimal.
 */
#if HAVE_MESSAGES
#define REQUEST_TOO_BIG "<html><head><title>Request too big</title></head><body>Your HTTP header was too big for the memory constraints of this webserver.</body></html>"
#else
#define REQUEST_TOO_BIG ""
#endif

/**
 * Response text used when the request (http header) does not
 * contain a "Host:" header and still claims to be HTTP 1.1.
 *
 * Intentionally empty here to keep our memory footprint
 * minimal.
 */
#if HAVE_MESSAGES
#define REQUEST_LACKS_HOST "<html><head><title>&quot;Host:&quot; header required</title></head><body>In HTTP 1.1, requests must include a &quot;Host:&quot; header, and your HTTP 1.1 request lacked such a header.</body></html>"
#else
#define REQUEST_LACKS_HOST ""
#endif

/**
 * Response text used when the request (http header) is
 * malformed.
 *
 * Intentionally empty here to keep our memory footprint
 * minimal.
 */
#if HAVE_MESSAGES
#define REQUEST_MALFORMED "<html><head><title>Request malformed</title></head><body>Your HTTP request was syntactically incorrect.</body></html>"
#else
#define REQUEST_MALFORMED ""
#endif

/**
 * Response text used when there is an internal server error.
 *
 * Intentionally empty here to keep our memory footprint
 * minimal.
 */
#if HAVE_MESSAGES
#define INTERNAL_ERROR "<html><head><title>Internal server error</title></head><body>Some programmer needs to study the manual more carefully.</body></html>"
#else
#define INTERNAL_ERROR ""
#endif

/**
 * Add extra debug messages with reasons for closing connections
 * (non-error reasons).
 */
#define DEBUG_CLOSE MHD_NO

/**
 * Should all data send be printed to stderr?
 */
#define DEBUG_SEND_DATA MHD_NO


/**
 * Get all of the headers from the request.
 *
 * @param connection connection to get values from
 * @param kind types of values to iterate over
 * @param iterator callback to call on each header;
 *        maybe NULL (then just count headers)
 * @param iterator_cls extra argument to iterator
 * @return number of entries iterated over
 */
int
MHD_get_connection_values (struct MHD_Connection *connection,
                           enum MHD_ValueKind kind,
                           MHD_KeyValueIterator iterator, void *iterator_cls)
{
  int ret;
  struct MHD_HTTP_Header *pos;

  if (NULL == connection)
    return -1;
  ret = 0;
  for (pos = connection->headers_received; NULL != pos; pos = pos->next)
    if (0 != (pos->kind & kind))
      {
	ret++;
	if ((NULL != iterator) &&
	    (MHD_YES != iterator (iterator_cls,
				  kind, pos->header, pos->value)))
	  return ret;
      }
  return ret;
}


/**
 * This function can be used to append an entry to
 * the list of HTTP headers of a connection (so that the
 * MHD_get_connection_values function will return
 * them -- and the MHD PostProcessor will also
 * see them).  This maybe required in certain
 * situations (see Mantis #1399) where (broken)
 * HTTP implementations fail to supply values needed
 * by the post processor (or other parts of the
 * application).
 * <p>
 * This function MUST only be called from within
 * the MHD_AccessHandlerCallback (otherwise, access
 * maybe improperly synchronized).  Furthermore,
 * the client must guarantee that the key and
 * value arguments are 0-terminated strings that
 * are NOT freed until the connection is closed.
 * (The easiest way to do this is by passing only
 * arguments to permanently allocated strings.).
 *
 * @param connection the connection for which a
 *  value should be set
 * @param kind kind of the value
 * @param key key for the value
 * @param value the value itself
 * @return MHD_NO if the operation could not be
 *         performed due to insufficient memory;
 *         MHD_YES on success
 */
int
MHD_set_connection_value (struct MHD_Connection *connection,
                          enum MHD_ValueKind kind,
                          const char *key, const char *value)
{
  struct MHD_HTTP_Header *pos;

  pos = MHD_pool_allocate (connection->pool,
                           sizeof (struct MHD_HTTP_Header), MHD_NO);
  if (NULL == pos)
    return MHD_NO;
  pos->header = (char *) key;
  pos->value = (char *) value;
  pos->kind = kind;
  pos->next = NULL;
  /* append 'pos' to the linked list of headers */
  if (NULL == connection->headers_received_tail)
  {
    connection->headers_received = pos;
    connection->headers_received_tail = pos;
  }
  else
  {
    connection->headers_received_tail->next = pos;
    connection->headers_received_tail = pos;
  }
  return MHD_YES;
}


/**
 * Get a particular header value.  If multiple
 * values match the kind, return any one of them.
 *
 * @param connection connection to get values from
 * @param kind what kind of value are we looking for
 * @param key the header to look for, NULL to lookup 'trailing' value without a key
 * @return NULL if no such item was found
 */
const char *
MHD_lookup_connection_value (struct MHD_Connection *connection,
                             enum MHD_ValueKind kind, const char *key)
{
  struct MHD_HTTP_Header *pos;

  if (NULL == connection)
    return NULL;
  for (pos = connection->headers_received; NULL != pos; pos = pos->next)
    if ((0 != (pos->kind & kind)) && 
	( (key == pos->header) ||
	  ( (NULL != pos->header) &&
	    (NULL != key) &&
	    (0 == strcasecmp (key, pos->header))) ))
      return pos->value;    
  return NULL;
}


/**
 * Queue a response to be transmitted to the client (as soon as
 * possible but after MHD_AccessHandlerCallback returns).
 *
 * @param connection the connection identifying the client
 * @param status_code HTTP status code (i.e. 200 for OK)
 * @param response response to transmit
 * @return MHD_NO on error (i.e. reply already sent),
 *         MHD_YES on success or if message has been queued
 */
int
MHD_queue_response (struct MHD_Connection *connection,
                    unsigned int status_code, struct MHD_Response *response)
{
  if ( (NULL == connection) ||
       (NULL == response) ||
       (NULL != connection->response) ||
       ( (MHD_CONNECTION_HEADERS_PROCESSED != connection->state) &&
	 (MHD_CONNECTION_FOOTERS_RECEIVED != connection->state) ) )
    return MHD_NO;
  MHD_increment_response_rc (response);
  connection->response = response;
  connection->responseCode = status_code;
  if ( (NULL != connection->method) &&
       (0 == strcasecmp (connection->method, MHD_HTTP_METHOD_HEAD)) )
    {
      /* if this is a "HEAD" request, pretend that we
         have already sent the full message body */
      connection->response_write_position = response->total_size;
    }
  if (MHD_CONNECTION_HEADERS_PROCESSED == connection->state)
    {
      /* response was queued "early",
         refuse to read body / footers or further
         requests! */
      (void) SHUTDOWN (connection->socket_fd, SHUT_RD);
      connection->read_closed = MHD_YES;
      connection->state = MHD_CONNECTION_FOOTERS_RECEIVED;
    }
  return MHD_YES;
}


/**
 * Do we (still) need to send a 100 continue
 * message for this connection?
 *
 * @param connection connection to test
 * @return 0 if we don't need 100 CONTINUE, 1 if we do
 */
static int
need_100_continue (struct MHD_Connection *connection)
{
  const char *expect;

  return ( (NULL == connection->response) &&
	   (NULL != connection->version) &&
	   (0 == strcasecmp (connection->version,
			     MHD_HTTP_VERSION_1_1)) &&
	   (NULL != (expect = MHD_lookup_connection_value (connection,
							   MHD_HEADER_KIND,
							   MHD_HTTP_HEADER_EXPECT))) &&
	   (0 == strcasecmp (expect, "100-continue")) &&
	   (connection->continue_message_write_offset <
	    strlen (HTTP_100_CONTINUE)) );
}


/**
 * Close the given connection and give the
 * specified termination code to the user.
 *
 * @param connection connection to close
 * @param termination_code termination reason to give
 */
void
MHD_connection_close (struct MHD_Connection *connection,
                      enum MHD_RequestTerminationCode termination_code)
{
  struct MHD_Daemon *daemon;

  daemon = connection->daemon;
  SHUTDOWN (connection->socket_fd, 
	    (connection->read_closed == MHD_YES) ? SHUT_WR : SHUT_RDWR);
  connection->state = MHD_CONNECTION_CLOSED;
  if ( (NULL != daemon->notify_completed) &&
       (MHD_YES == connection->client_aware) )
    daemon->notify_completed (daemon->notify_completed_cls, 
			      connection,
			      &connection->client_context,
			      termination_code);
  connection->client_aware = MHD_NO;
}


/**
 * A serious error occured, close the
 * connection (and notify the application).
 *
 * @param connection connection to close with error
 * @param emsg error message (can be NULL)
 */
static void
connection_close_error (struct MHD_Connection *connection,
			const char *emsg)
{
#if HAVE_MESSAGES
  if (NULL != emsg)
    MHD_DLOG (connection->daemon, emsg);
#endif
  MHD_connection_close (connection, MHD_REQUEST_TERMINATED_WITH_ERROR);
}


/**
 * Macro to only include error message in call to
 * "connection_close_error" if we have HAVE_MESSAGES.
 */
#if HAVE_MESSAGES
#define CONNECTION_CLOSE_ERROR(c, emsg) connection_close_error (c, emsg)
#else
#define CONNECTION_CLOSE_ERROR(c, emsg) connection_close_error (c, NULL)
#endif


/**
 * Prepare the response buffer of this connection for
 * sending.  Assumes that the response mutex is
 * already held.  If the transmission is complete,
 * this function may close the socket (and return
 * MHD_NO).
 *
 * @param connection the connection
 * @return MHD_NO if readying the response failed
 */
static int
try_ready_normal_body (struct MHD_Connection *connection)
{
  ssize_t ret;
  struct MHD_Response *response;

  response = connection->response;
  if (NULL == response->crc)
    return MHD_YES;
  if ( (response->data_start <=
	connection->response_write_position) &&
       (response->data_size + response->data_start >
	connection->response_write_position) )
    return MHD_YES; /* response already ready */
#if LINUX
  if ( (-1 != response->fd) &&
       (0 == (connection->daemon->options & MHD_USE_SSL)) )
    {
      /* will use sendfile, no need to bother response crc */
      return MHD_YES; 
    }
#endif
  
  ret = response->crc (response->crc_cls,
                       connection->response_write_position,
                       response->data,
                       MHD_MIN (response->data_buffer_size,
                                response->total_size -
                                connection->response_write_position));
  if ((0 == ret) &&
      (0 != (connection->daemon->options & MHD_USE_SELECT_INTERNALLY)))
    mhd_panic (mhd_panic_cls, __FILE__, __LINE__
#if HAVE_MESSAGES
	       , "API violation"
#else
	       , NULL
#endif
	       );
  if ( (MHD_CONTENT_READER_END_OF_STREAM == ret) ||
       (MHD_CONTENT_READER_END_WITH_ERROR == ret) )
    {
      /* either error or http 1.0 transfer, close socket! */
      response->total_size = connection->response_write_position;
      CONNECTION_CLOSE_ERROR (connection,
			      (ret == MHD_CONTENT_READER_END_OF_STREAM) 
			      ? "Closing connection (end of response)\n"
			      : "Closing connection (stream error)\n");
      return MHD_NO;
    }
  response->data_start = connection->response_write_position;
  response->data_size = ret;
  if (0 == ret)
    {
      connection->state = MHD_CONNECTION_NORMAL_BODY_UNREADY;
      return MHD_NO;
    }
  return MHD_YES;
}


/**
 * Prepare the response buffer of this connection for sending.
 * Assumes that the response mutex is already held.  If the
 * transmission is complete, this function may close the socket (and
 * return MHD_NO).
 *
 * @param connection the connection
 * @return MHD_NO if readying the response failed
 */
static int
try_ready_chunked_body (struct MHD_Connection *connection)
{
  ssize_t ret;
  char *buf;
  struct MHD_Response *response;
  size_t size;
  char cbuf[10];                /* 10: max strlen of "%x\r\n" */
  int cblen;

  response = connection->response;
  if (0 == connection->write_buffer_size)
    {
      size = connection->daemon->pool_size;
      do
        {
          size /= 2;
          if (size < 128)
            {
              /* not enough memory */
              CONNECTION_CLOSE_ERROR (connection,
				      "Closing connection (out of memory)\n");
              return MHD_NO;
            }
          buf = MHD_pool_allocate (connection->pool, size, MHD_NO);
        }
      while (NULL == buf);
      connection->write_buffer_size = size;
      connection->write_buffer = buf;
    }

  if ( (response->data_start <=
	connection->response_write_position) &&
       (response->data_size + response->data_start >
	connection->response_write_position) )
    {
      /* buffer already ready, use what is there for the chunk */
      ret = response->data_size + response->data_start - connection->response_write_position;
      if (ret > connection->write_buffer_size - sizeof (cbuf) - 2)
	ret = connection->write_buffer_size - sizeof (cbuf) - 2;
      memcpy (&connection->write_buffer[sizeof (cbuf)],
	      &response->data[connection->response_write_position - response->data_start],
	      ret);
    }
  else
    {
      /* buffer not in range, try to fill it */
      ret = response->crc (response->crc_cls,
			   connection->response_write_position,
			   &connection->write_buffer[sizeof (cbuf)],
			   connection->write_buffer_size - sizeof (cbuf) - 2);
    }
  if (MHD_CONTENT_READER_END_WITH_ERROR == ret)
    {
      /* error, close socket! */
      response->total_size = connection->response_write_position;
      CONNECTION_CLOSE_ERROR (connection,
			      "Closing connection (error generating response)\n");
      return MHD_NO;
    }
  if (MHD_CONTENT_READER_END_OF_STREAM == ret) 
    {
      /* end of message, signal other side! */
      strcpy (connection->write_buffer, "0\r\n");
      connection->write_buffer_append_offset = 3;
      connection->write_buffer_send_offset = 0;
      response->total_size = connection->response_write_position;
      return MHD_YES;
    }
  if (0 == ret)
    {
      connection->state = MHD_CONNECTION_CHUNKED_BODY_UNREADY;
      return MHD_NO;
    }
  if (ret > 0xFFFFFF)
    ret = 0xFFFFFF;
  snprintf (cbuf, 
	    sizeof (cbuf),
	    "%X\r\n", (unsigned int) ret);
  cblen = strlen (cbuf);
  EXTRA_CHECK (cblen <= sizeof (cbuf));
  memcpy (&connection->write_buffer[sizeof (cbuf) - cblen], cbuf, cblen);
  memcpy (&connection->write_buffer[sizeof (cbuf) + ret], "\r\n", 2);
  connection->response_write_position += ret;
  connection->write_buffer_send_offset = sizeof (cbuf) - cblen;
  connection->write_buffer_append_offset = sizeof (cbuf) + ret + 2;
  return MHD_YES;
}


/**
 * Check if we need to set some additional headers
 * for http-compiliance.
 *
 * @param connection connection to check (and possibly modify)
 */
static void
add_extra_headers (struct MHD_Connection *connection)
{
  const char *have;
  char buf[128];

  connection->have_chunked_upload = MHD_NO;
  if (MHD_SIZE_UNKNOWN == connection->response->total_size)
    {
      have = MHD_get_response_header (connection->response,
                                      MHD_HTTP_HEADER_CONNECTION);
      if ((NULL == have) || (0 != strcasecmp (have, "close")))
        {
          if ((NULL != connection->version) &&
              (0 == strcasecmp (connection->version, MHD_HTTP_VERSION_1_1)))
            {
              connection->have_chunked_upload = MHD_YES;
              have = MHD_get_response_header (connection->response,
                                              MHD_HTTP_HEADER_TRANSFER_ENCODING);
              if (NULL == have)
                MHD_add_response_header (connection->response,
                                         MHD_HTTP_HEADER_TRANSFER_ENCODING,
                                         "chunked");
            }
          else
            {
              MHD_add_response_header (connection->response,
                                       MHD_HTTP_HEADER_CONNECTION, "close");
            }
        }
    }
  else if (NULL == MHD_get_response_header (connection->response,
                                            MHD_HTTP_HEADER_CONTENT_LENGTH))
    {
      SPRINTF (buf,
               MHD_UNSIGNED_LONG_LONG_PRINTF,
	       (MHD_UNSIGNED_LONG_LONG) connection->response->total_size);
      MHD_add_response_header (connection->response,
                               MHD_HTTP_HEADER_CONTENT_LENGTH, buf);
    }
}


/**
 * Produce HTTP "Date:" header.
 *
 * @param date where to write the header, with
 *        at least 128 bytes available space.
 */
static void
get_date_string (char *date)
{
  static const char *days[] =
    { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
  static const char *mons[] =
    { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct",
    "Nov", "Dec"
  };
  struct tm now;
  time_t t;

  time (&t);
  gmtime_r (&t, &now);
  SPRINTF (date,
           "Date: %3s, %02u %3s %04u %02u:%02u:%02u GMT\r\n",
           days[now.tm_wday % 7],
           (unsigned int) now.tm_mday,
           mons[now.tm_mon % 12],
           (unsigned int) (1900 + now.tm_year),
	   (unsigned int) now.tm_hour, 
	   (unsigned int) now.tm_min, 
	   (unsigned int) now.tm_sec);
}


/**
 * Try growing the read buffer
 *
 * @param connection the connection
 * @return MHD_YES on success, MHD_NO on failure
 */
static int
try_grow_read_buffer (struct MHD_Connection *connection)
{
  void *buf;

  buf = MHD_pool_reallocate (connection->pool,
                             connection->read_buffer,
                             connection->read_buffer_size,
                             connection->read_buffer_size * 2 +
                             MHD_BUF_INC_SIZE + 1);
  if (NULL == buf)
    return MHD_NO;
  /* we can actually grow the buffer, do it! */
  connection->read_buffer = buf;
  connection->read_buffer_size =
    connection->read_buffer_size * 2 + MHD_BUF_INC_SIZE;
  return MHD_YES;
}


/**
 * Allocate the connection's write buffer and fill it with all of the
 * headers (or footers, if we have already sent the body) from the
 * HTTPd's response.
 *
 * @param connection the connection
 */
static int
build_header_response (struct MHD_Connection *connection)
{
  size_t size;
  size_t off;
  struct MHD_HTTP_Header *pos;
  char code[256];
  char date[128];
  char *data;
  enum MHD_ValueKind kind;
  const char *reason_phrase;
  uint32_t rc;
  int must_add_close;

  EXTRA_CHECK (NULL != connection->version);
  if (0 == strlen(connection->version))
    {
      data = MHD_pool_allocate (connection->pool, 0, MHD_YES);
      connection->write_buffer = data;
      connection->write_buffer_append_offset = 0;
      connection->write_buffer_send_offset = 0;
      connection->write_buffer_size = 0;
      return MHD_YES;
    }
  if (MHD_CONNECTION_FOOTERS_RECEIVED == connection->state)
    {
      add_extra_headers (connection);
      rc = connection->responseCode & (~MHD_ICY_FLAG);
      reason_phrase = MHD_get_reason_phrase_for (rc);
      SPRINTF (code,
               "%s %u %s\r\n",
	       (0 != (connection->responseCode & MHD_ICY_FLAG))
	       ? "ICY" 
	       : ( (0 == strcasecmp (MHD_HTTP_VERSION_1_0,
				     connection->version)) 
		   ? MHD_HTTP_VERSION_1_0 
		   : MHD_HTTP_VERSION_1_1),
	       rc, 
	       reason_phrase);
      off = strlen (code);
      /* estimate size */
      size = off + 2;           /* extra \r\n at the end */
      kind = MHD_HEADER_KIND;
      if ( (0 == (connection->daemon->options & MHD_SUPPRESS_DATE_NO_CLOCK)) && 
	   (NULL == MHD_get_response_header (connection->response,
					     MHD_HTTP_HEADER_DATE)) )
        get_date_string (date);
      else
        date[0] = '\0';
      size += strlen (date);
    }
  else
    {
      size = 2;
      kind = MHD_FOOTER_KIND;
      off = 0;
    }
  must_add_close = ( (connection->state == MHD_CONNECTION_FOOTERS_RECEIVED) &&
		     (connection->read_closed == MHD_YES) &&
		     (0 == strcasecmp (connection->version,
				       MHD_HTTP_VERSION_1_1)) &&
		     (NULL == MHD_get_response_header (connection->response,
						       MHD_HTTP_HEADER_CONNECTION)) );
  if (must_add_close)
    size += strlen ("Connection: close\r\n");
  for (pos = connection->response->first_header; NULL != pos; pos = pos->next)
    if (pos->kind == kind)
      size += strlen (pos->header) + strlen (pos->value) + 4; /* colon, space, linefeeds */
  /* produce data */
  data = MHD_pool_allocate (connection->pool, size + 1, MHD_YES);
  if (NULL == data)
    {
#if HAVE_MESSAGES
      MHD_DLOG (connection->daemon, "Not enough memory for write!\n");
#endif
      return MHD_NO;
    }
  if (MHD_CONNECTION_FOOTERS_RECEIVED == connection->state)
    {
      memcpy (data, code, off);
    }
  if (must_add_close)
    {
      /* we must add the 'close' header because circumstances forced us to
	 stop reading from the socket; however, we are not adding the header
	 to the response as the response may be used in a different context
	 as well */
      memcpy (&data[off], "Connection: close\r\n",
	      strlen ("Connection: close\r\n"));
      off += strlen ("Connection: close\r\n");
    }
  for (pos = connection->response->first_header; NULL != pos; pos = pos->next)
    if (pos->kind == kind)
      off += SPRINTF (&data[off], 
		      "%s: %s\r\n",
		      pos->header, 
		      pos->value);
  if (connection->state == MHD_CONNECTION_FOOTERS_RECEIVED)
    {
      strcpy (&data[off], date);
      off += strlen (date);
    }
  memcpy (&data[off], "\r\n", 2);
  off += 2;

  if (off != size)
    mhd_panic (mhd_panic_cls, __FILE__, __LINE__, NULL);
  connection->write_buffer = data;
  connection->write_buffer_append_offset = size;
  connection->write_buffer_send_offset = 0;
  connection->write_buffer_size = size + 1;
  return MHD_YES;
}


/**
 * We encountered an error processing the request.
 * Handle it properly by stopping to read data
 * and sending the indicated response code and message.
 *
 * @param connection the connection
 * @param status_code the response code to send (400, 413 or 414)
 * @param message the error message to send
 */
static void
transmit_error_response (struct MHD_Connection *connection,
                         unsigned int status_code, 
			 const char *message)
{
  struct MHD_Response *response;

  if (NULL == connection->version)
    {
      /* we were unable to process the full header line, so we don't
	 really know what version the client speaks; assume 1.0 */
      connection->version = MHD_HTTP_VERSION_1_0;
    }
  connection->state = MHD_CONNECTION_FOOTERS_RECEIVED;
  connection->read_closed = MHD_YES;
#if HAVE_MESSAGES
  MHD_DLOG (connection->daemon,
            "Error %u (`%s') processing request, closing connection.\n",
            status_code, message);
#endif
  EXTRA_CHECK (connection->response == NULL);
  response = MHD_create_response_from_buffer (strlen (message),
					      (void *) message, 
					      MHD_RESPMEM_PERSISTENT);
  MHD_queue_response (connection, status_code, response);
  EXTRA_CHECK (connection->response != NULL);
  MHD_destroy_response (response);
  if (MHD_NO == build_header_response (connection))
    {
      /* oops - close! */
      CONNECTION_CLOSE_ERROR (connection,
			      "Closing connection (failed to create response header)\n");
    }
  else
    {
      connection->state = MHD_CONNECTION_HEADERS_SENDING;
    }
}


/**
 * Add "fd" to the "fd_set".  If "fd" is
 * greater than "*max", set "*max" to fd.
 *
 * @param fd file descriptor to add to the set
 * @param set set to modify
 * @param max_fd maximum value to potentially update
 */
static void
add_to_fd_set (int fd, 
	       fd_set *set, 
	       int *max_fd)
{
  FD_SET (fd, set);
  if ( (NULL != max_fd) &&
       (fd > *max_fd) )
    *max_fd = fd;
}


/**
 * Obtain the select sets for this connection.  The given
 * sets (and the maximum) are updated and must have 
 * already been initialized.
 *
 * @param connection connetion to get select sets for
 * @param read_fd_set read set to initialize
 * @param write_fd_set write set to initialize
 * @param except_fd_set except set to initialize (never changed)
 * @param max_fd where to store largest FD put into any set
 * @return MHD_YES on success
 */
int
MHD_connection_get_fdset (struct MHD_Connection *connection,
                          fd_set *read_fd_set,
                          fd_set *write_fd_set,
                          fd_set *except_fd_set, 
			  int *max_fd)
{
  int ret;
  struct MHD_Pollfd p;

  /* we use the 'poll fd' as a convenient way to re-use code 
     when determining the select sets */
  memset (&p, 0, sizeof(struct MHD_Pollfd));
  ret = MHD_connection_get_pollfd (connection, &p);
  if ( (MHD_YES == ret) && (p.fd >= 0) ) {
    if (0 != (p.events & MHD_POLL_ACTION_IN)) 
      add_to_fd_set(p.fd, read_fd_set, max_fd);    
    if (0 != (p.events & MHD_POLL_ACTION_OUT)) 
      add_to_fd_set(p.fd, write_fd_set, max_fd);    
  }
  return ret;
}


/**
 * Obtain the pollfd for this connection
 *
 * @param connection connetion to get poll set for 
 * @param p where to store the polling information
 * @return MHD_YES on success. If return MHD_YES and p->fd < 0, this 
 *                 connection is not waiting for any read or write events
 */
int
MHD_connection_get_pollfd (struct MHD_Connection *connection, 
			   struct MHD_Pollfd *p)
{
  int fd;

  if (NULL == connection->pool)
    connection->pool = MHD_pool_create (connection->daemon->pool_size);
  if (NULL == connection->pool)
    {
      CONNECTION_CLOSE_ERROR (connection,
			      "Failed to create memory pool!\n");
      return MHD_YES;
    }
  fd = connection->socket_fd;
  p->fd = fd;
  if (-1 == fd)
    return MHD_YES;
  while (1)
    {
#if DEBUG_STATES
      MHD_DLOG (connection->daemon, "%s: state: %s\n",
                __FUNCTION__, MHD_state_to_string (connection->state));
#endif
      switch (connection->state)
        {
#if HTTPS_SUPPORT     
	case MHD_TLS_CONNECTION_INIT:
	  if (0 == gnutls_record_get_direction (connection->tls_session))
            p->events |= MHD_POLL_ACTION_IN;
	  else
	    p->events |= MHD_POLL_ACTION_OUT;
	  break;
#endif
        case MHD_CONNECTION_INIT:
        case MHD_CONNECTION_URL_RECEIVED:
        case MHD_CONNECTION_HEADER_PART_RECEIVED:
          /* while reading headers, we always grow the
             read buffer if needed, no size-check required */
          if ((connection->read_closed) &&
              (0 == connection->read_buffer_offset))
            {
	      CONNECTION_CLOSE_ERROR (connection, 
				      "Connection buffer to small for request\n");
              continue;
            }
          if ((connection->read_buffer_offset == connection->read_buffer_size)
              && (MHD_NO == try_grow_read_buffer (connection)))
            {
              transmit_error_response (connection,
                                       (connection->url != NULL)
                                       ? MHD_HTTP_REQUEST_ENTITY_TOO_LARGE
                                       : MHD_HTTP_REQUEST_URI_TOO_LONG,
                                       REQUEST_TOO_BIG);
              continue;
            }
          if (MHD_NO == connection->read_closed)
            p->events |= MHD_POLL_ACTION_IN;
          break;
        case MHD_CONNECTION_HEADERS_RECEIVED:
          /* we should never get here */
          EXTRA_CHECK (0);
          break;
        case MHD_CONNECTION_HEADERS_PROCESSED:
          EXTRA_CHECK (0);
          break;
        case MHD_CONNECTION_CONTINUE_SENDING:
          p->events |= MHD_POLL_ACTION_OUT;
          break;
        case MHD_CONNECTION_CONTINUE_SENT:
          if (connection->read_buffer_offset == connection->read_buffer_size)
            {
              if ((MHD_YES != try_grow_read_buffer (connection)) &&
                  (0 != (connection->daemon->options &
                         (MHD_USE_SELECT_INTERNALLY |
                          MHD_USE_THREAD_PER_CONNECTION))))
                {
                  /* failed to grow the read buffer, and the
                     client which is supposed to handle the
                     received data in a *blocking* fashion
                     (in this mode) did not handle the data as
                     it was supposed to!
                     => we would either have to do busy-waiting
                     (on the client, which would likely fail),
                     or if we do nothing, we would just timeout
                     on the connection (if a timeout is even
                     set!).
                     Solution: we kill the connection with an error */
                  transmit_error_response (connection,
                                           MHD_HTTP_INTERNAL_SERVER_ERROR,
                                           INTERNAL_ERROR);
                  continue;
                }
            }
          if ((connection->read_buffer_offset < connection->read_buffer_size)
              && (MHD_NO == connection->read_closed))
            p->events |= MHD_POLL_ACTION_IN;
          break;
        case MHD_CONNECTION_BODY_RECEIVED:
        case MHD_CONNECTION_FOOTER_PART_RECEIVED:
          /* while reading footers, we always grow the
             read buffer if needed, no size-check required */
          if (MHD_YES == connection->read_closed)
            {
	      CONNECTION_CLOSE_ERROR (connection, 
				      NULL);
              continue;
            }
          p->events |= MHD_POLL_ACTION_IN;
          /* transition to FOOTERS_RECEIVED
             happens in read handler */
          break;
        case MHD_CONNECTION_FOOTERS_RECEIVED:
          /* no socket action, wait for client
             to provide response */
          break;
        case MHD_CONNECTION_HEADERS_SENDING:
          /* headers in buffer, keep writing */
          p->events |= MHD_POLL_ACTION_OUT;
          break;
        case MHD_CONNECTION_HEADERS_SENT:
          EXTRA_CHECK (0);
          break;
        case MHD_CONNECTION_NORMAL_BODY_READY:
          p->events |= MHD_POLL_ACTION_OUT;
          break;
        case MHD_CONNECTION_NORMAL_BODY_UNREADY:
          /* not ready, no socket action */
          break;
        case MHD_CONNECTION_CHUNKED_BODY_READY:
          p->events |= MHD_POLL_ACTION_OUT;
          break;
        case MHD_CONNECTION_CHUNKED_BODY_UNREADY:
          /* not ready, no socket action */
          break;
        case MHD_CONNECTION_BODY_SENT:
          EXTRA_CHECK (0);
          break;
        case MHD_CONNECTION_FOOTERS_SENDING:
          p->events |= MHD_POLL_ACTION_OUT;
          break;
        case MHD_CONNECTION_FOOTERS_SENT:
          EXTRA_CHECK (0);
          break;
        case MHD_CONNECTION_CLOSED:
          return MHD_YES;       /* do nothing, not even reading */
        default:
          EXTRA_CHECK (0);
        }
      break;
    }
  return MHD_YES;
}


/**
 * Parse a single line of the HTTP header.  Advance
 * read_buffer (!) appropriately.  If the current line does not
 * fit, consider growing the buffer.  If the line is
 * far too long, close the connection.  If no line is
 * found (incomplete, buffer too small, line too long),
 * return NULL.  Otherwise return a pointer to the line.
 */
static char *
get_next_header_line (struct MHD_Connection *connection)
{
  char *rbuf;
  size_t pos;

  if (0 == connection->read_buffer_offset)
    return NULL;
  pos = 0;
  rbuf = connection->read_buffer;
  while ((pos < connection->read_buffer_offset - 1) &&
         ('\r' != rbuf[pos]) && ('\n' != rbuf[pos]))
    pos++;
  if (pos == connection->read_buffer_offset - 1)
    {
      /* not found, consider growing... */
      if (connection->read_buffer_offset == connection->read_buffer_size)
        {
          rbuf = MHD_pool_reallocate (connection->pool,
                                      connection->read_buffer,
                                      connection->read_buffer_size,
                                      connection->read_buffer_size * 2 +
                                      MHD_BUF_INC_SIZE);
          if (NULL == rbuf)
            {
              transmit_error_response (connection,
                                       (NULL != connection->url)
                                       ? MHD_HTTP_REQUEST_ENTITY_TOO_LARGE
                                       : MHD_HTTP_REQUEST_URI_TOO_LONG,
                                       REQUEST_TOO_BIG);
            }
          else
            {
              connection->read_buffer_size =
                connection->read_buffer_size * 2 + MHD_BUF_INC_SIZE;
              connection->read_buffer = rbuf;
            }
        }
      return NULL;
    }
  /* found, check if we have proper CRLF */
  if (('\r' == rbuf[pos]) && ('\n' == rbuf[pos + 1]))
    rbuf[pos++] = '\0';         /* skip both r and n */
  rbuf[pos++] = '\0';
  connection->read_buffer += pos;
  connection->read_buffer_size -= pos;
  connection->read_buffer_offset -= pos;
  return rbuf;
}


/**
 * Add an entry to the HTTP headers of a connection.  If this fails,
 * transmit an error response (request too big).
 *
 * @param connection the connection for which a
 *  value should be set
 * @param kind kind of the value
 * @param key key for the value
 * @param value the value itself
 * @return MHD_NO on failure (out of memory), MHD_YES for success
 */
static int
connection_add_header (struct MHD_Connection *connection,
                       char *key, char *value, enum MHD_ValueKind kind)
{
  if (MHD_NO == MHD_set_connection_value (connection,
					  kind,
					  key, value))
    {
#if HAVE_MESSAGES
      MHD_DLOG (connection->daemon,
                "Not enough memory to allocate header record!\n");
#endif
      transmit_error_response (connection, MHD_HTTP_REQUEST_ENTITY_TOO_LARGE,
                               REQUEST_TOO_BIG);
      return MHD_NO;
    }
  return MHD_YES;
}


/**
 * Parse and unescape the arguments given by the client as part
 * of the HTTP request URI.
 *
 * @param kind header kind to use for adding to the connection
 * @param connection connection to add headers to
 * @param args argument URI string (after "?" in URI)
 * @return MHD_NO on failure (out of memory), MHD_YES for success
 */
static int
parse_arguments (enum MHD_ValueKind kind,
                 struct MHD_Connection *connection, 
		 char *args)
{
  char *equals;
  char *amper;

  while (NULL != args)
    {
      equals = strchr (args, '=');
      amper = strchr (args, '&');
      if (NULL == amper)
	{
	  /* last argument */
	  if (NULL == equals)
	    {
	      /* got 'foo', add key 'foo' with NULL for value */
	      connection->daemon->unescape_callback (connection->daemon->unescape_callback_cls,
						     connection,
						     args);
	      return connection_add_header (connection,
					    args,
					    NULL,
					    kind);	      
	    }
	  /* got 'foo=bar' */
	  equals[0] = '\0';
	  equals++;
	  connection->daemon->unescape_callback (connection->daemon->unescape_callback_cls,
						 connection,
						 args);
	  connection->daemon->unescape_callback (connection->daemon->unescape_callback_cls,
						 connection,
						 equals);
	  return connection_add_header (connection, args, equals, kind);
	}
      /* amper is non-NULL here */
      amper[0] = '\0';
      amper++;
      if ( (NULL == equals) ||
	   (equals >= amper) )
	{
	  /* got 'foo&bar' or 'foo&bar=val', add key 'foo' with NULL for value */
	  connection->daemon->unescape_callback (connection->daemon->unescape_callback_cls,
						 connection,
						 args);
	  if (MHD_NO ==
	      connection_add_header (connection,
				     args,
				     NULL,
				     kind))
	    return MHD_NO;
	  /* continue with 'bar' */
	  args = amper;
	  continue;

	}
      /* equals and amper are non-NULL here, and equals < amper,
	 so we got regular 'foo=value&bar...'-kind of argument */
      equals[0] = '\0';
      equals++;
      connection->daemon->unescape_callback (connection->daemon->unescape_callback_cls,
					     connection,
					     args);
      connection->daemon->unescape_callback (connection->daemon->unescape_callback_cls,
					     connection,
					     equals);
      if (MHD_NO == connection_add_header (connection, args, equals, kind))
        return MHD_NO;
      args = amper;
    }
  return MHD_YES;
}


/**
 * Parse the cookie header (see RFC 2109).
 *
 * @return MHD_YES for success, MHD_NO for failure (malformed, out of memory)
 */
static int
parse_cookie_header (struct MHD_Connection *connection)
{
  const char *hdr;
  char *cpy;
  char *pos;
  char *sce;
  char *semicolon;
  char *equals;
  char *ekill;
  char old;
  int quotes;

  hdr = MHD_lookup_connection_value (connection,
				     MHD_HEADER_KIND,
				     MHD_HTTP_HEADER_COOKIE);
  if (hdr == NULL)
    return MHD_YES;
  cpy = MHD_pool_allocate (connection->pool, strlen (hdr) + 1, MHD_YES);
  if (cpy == NULL)
    {
#if HAVE_MESSAGES
      MHD_DLOG (connection->daemon, "Not enough memory to parse cookies!\n");
#endif
      transmit_error_response (connection, MHD_HTTP_REQUEST_ENTITY_TOO_LARGE,
                               REQUEST_TOO_BIG);
      return MHD_NO;
    }
  memcpy (cpy, hdr, strlen (hdr) + 1);
  pos = cpy;
  while (pos != NULL)
    {
      while (*pos == ' ')
        pos++;                  /* skip spaces */

      sce = pos;
      while (((*sce) != '\0') &&
             ((*sce) != ',') && ((*sce) != ';') && ((*sce) != '='))
        sce++;
      /* remove tailing whitespace (if any) from key */
      ekill = sce - 1;
      while ((*ekill == ' ') && (ekill >= pos))
        *(ekill--) = '\0';
      old = *sce;
      *sce = '\0';
      if (old != '=')
        {
          /* value part omitted, use empty string... */
          if (MHD_NO ==
              connection_add_header (connection, pos, "", MHD_COOKIE_KIND))
            return MHD_NO;
          if (old == '\0')
            break;
          pos = sce + 1;
          continue;
        }
      equals = sce + 1;
      quotes = 0;
      semicolon = equals;
      while ((semicolon[0] != '\0') &&
             ((quotes != 0) ||
              ((semicolon[0] != ';') && (semicolon[0] != ','))))
        {
          if (semicolon[0] == '"')
            quotes = (quotes + 1) & 1;
          semicolon++;
        }
      if (semicolon[0] == '\0')
        semicolon = NULL;
      if (semicolon != NULL)
        {
          semicolon[0] = '\0';
          semicolon++;
        }
      /* remove quotes */
      if ((equals[0] == '"') && (equals[strlen (equals) - 1] == '"'))
        {
          equals[strlen (equals) - 1] = '\0';
          equals++;
        }
      if (MHD_NO == connection_add_header (connection,
                                           pos, equals, MHD_COOKIE_KIND))
        return MHD_NO;
      pos = semicolon;
    }
  return MHD_YES;
}


/**
 * Parse the first line of the HTTP HEADER.
 *
 * @param connection the connection (updated)
 * @param line the first line
 * @return MHD_YES if the line is ok, MHD_NO if it is malformed
 */
static int
parse_initial_message_line (struct MHD_Connection *connection, char *line)
{
  char *uri;
  char *httpVersion;
  char *args;

  if (NULL == (uri = strchr (line, ' ')))
    return MHD_NO;              /* serious error */
  uri[0] = '\0';
  connection->method = line;
  uri++;
  while (uri[0] == ' ')
    uri++;
  httpVersion = strchr (uri, ' ');
  if (httpVersion != NULL)
    {
      httpVersion[0] = '\0';
      httpVersion++;
    }
  if (connection->daemon->uri_log_callback != NULL)
    connection->client_context
      =
      connection->daemon->uri_log_callback (connection->daemon->
                                            uri_log_callback_cls, uri);
  args = strchr (uri, '?');
  if (NULL != args)
    {
      args[0] = '\0';
      args++;
      parse_arguments (MHD_GET_ARGUMENT_KIND, connection, args);
    }
  connection->daemon->unescape_callback (connection->daemon->unescape_callback_cls,
					 connection,
					 uri);
  connection->url = uri;
  if (NULL == httpVersion)
    connection->version = "";
  else
    connection->version = httpVersion;
  return MHD_YES;
}


/**
 * Call the handler of the application for this
 * connection.  Handles chunking of the upload
 * as well as normal uploads.
 */
static void
call_connection_handler (struct MHD_Connection *connection)
{
  size_t processed;

  if (NULL != connection->response)
    return;                     /* already queued a response */  
  processed = 0;
  connection->client_aware = MHD_YES;
  if (MHD_NO ==
      connection->daemon->default_handler (connection->daemon->
					   default_handler_cls,
					   connection, connection->url,
					   connection->method,
					   connection->version,
					   NULL, &processed,
					   &connection->client_context))
    {
      /* serious internal error, close connection */
      CONNECTION_CLOSE_ERROR (connection, 
			      "Internal application error, closing connection.\n");
      return;
    }
}



/**
 * Call the handler of the application for this
 * connection.  Handles chunking of the upload
 * as well as normal uploads.
 */
static void
process_request_body (struct MHD_Connection *connection)
{
  size_t processed;
  size_t available;
  size_t used;
  size_t i;
  int instant_retry;
  int malformed;
  char *buffer_head;
  char *end;

  if (NULL != connection->response)
    return;                     /* already queued a response */

  buffer_head = connection->read_buffer;
  available = connection->read_buffer_offset;
  do
    {
      instant_retry = MHD_NO;
      if ((connection->have_chunked_upload == MHD_YES) &&
          (connection->remaining_upload_size == MHD_SIZE_UNKNOWN))
        {
          if ((connection->current_chunk_offset ==
               connection->current_chunk_size)
              && (connection->current_chunk_offset != 0) && (available >= 2))
            {
              /* skip new line at the *end* of a chunk */
              i = 0;
              if ((buffer_head[i] == '\r') || (buffer_head[i] == '\n'))
                i++;            /* skip 1st part of line feed */
              if ((buffer_head[i] == '\r') || (buffer_head[i] == '\n'))
                i++;            /* skip 2nd part of line feed */
              if (i == 0)
                {
                  /* malformed encoding */
                  CONNECTION_CLOSE_ERROR (connection,
					  "Received malformed HTTP request (bad chunked encoding), closing connection.\n");
                  return;
                }
              available -= i;
              buffer_head += i;
              connection->current_chunk_offset = 0;
              connection->current_chunk_size = 0;
            }
          if (connection->current_chunk_offset <
              connection->current_chunk_size)
            {
              /* we are in the middle of a chunk, give
                 as much as possible to the client (without
                 crossing chunk boundaries) */
              processed =
                connection->current_chunk_size -
                connection->current_chunk_offset;
              if (processed > available)
                processed = available;
              if (available > processed)
                instant_retry = MHD_YES;
            }
          else
            {
              /* we need to read chunk boundaries */
              i = 0;
              while (i < available)
                {
                  if ((buffer_head[i] == '\r') || (buffer_head[i] == '\n'))
                    break;
                  i++;
                  if (i >= 6)
                    break;
                }
              /* take '\n' into account; if '\n'
                 is the unavailable character, we
                 will need to wait until we have it
                 before going further */
              if ((i + 1 >= available) &&
                  !((i == 1) && (available == 2) && (buffer_head[0] == '0')))
                break;          /* need more data... */
              malformed = (i >= 6);
              if (!malformed)
                {
                  buffer_head[i] = '\0';
		  connection->current_chunk_size = strtoul (buffer_head, &end, 16);
                  malformed = ('\0' != *end);
                }
              if (malformed)
                {
                  /* malformed encoding */
                  CONNECTION_CLOSE_ERROR (connection,
					  "Received malformed HTTP request (bad chunked encoding), closing connection.\n");
                  return;
                }
              i++;
              if ((i < available) &&
                  ((buffer_head[i] == '\r') || (buffer_head[i] == '\n')))
                i++;            /* skip 2nd part of line feed */

              buffer_head += i;
              available -= i;
              connection->current_chunk_offset = 0;

              if (available > 0)
                instant_retry = MHD_YES;
              if (connection->current_chunk_size == 0)
                {
                  connection->remaining_upload_size = 0;
                  break;
                }
              continue;
            }
        }
      else
        {
          /* no chunked encoding, give all to the client */
          if ( (0 != connection->remaining_upload_size) && 
	       (MHD_SIZE_UNKNOWN != connection->remaining_upload_size) &&
	       (connection->remaining_upload_size < available) )
	    {
              processed = connection->remaining_upload_size;
	    }
          else
	    {
              /**
               * 1. no chunked encoding, give all to the client
               * 2. client may send large chunked data, but only a smaller part is available at one time.
               */
              processed = available;
	    }
        }
      used = processed;
      connection->client_aware = MHD_YES;
      if (MHD_NO ==
          connection->daemon->default_handler (connection->daemon->
                                               default_handler_cls,
                                               connection, connection->url,
                                               connection->method,
                                               connection->version,
                                               buffer_head, &processed,
                                               &connection->client_context))
        {
          /* serious internal error, close connection */
	  CONNECTION_CLOSE_ERROR (connection,
				  "Internal application error, closing connection.\n");
          return;
        }
      if (processed > used)
        mhd_panic (mhd_panic_cls, __FILE__, __LINE__
#if HAVE_MESSAGES
		   , "API violation"
#else
		   , NULL
#endif
		   );
      if (processed != 0)
        instant_retry = MHD_NO; /* client did not process everything */
      used -= processed;
      if (connection->have_chunked_upload == MHD_YES)
        connection->current_chunk_offset += used;
      /* dh left "processed" bytes in buffer for next time... */
      buffer_head += used;
      available -= used;
      if (connection->remaining_upload_size != MHD_SIZE_UNKNOWN)
        connection->remaining_upload_size -= used;
    }
  while (MHD_YES == instant_retry);
  if (available > 0)
    memmove (connection->read_buffer, buffer_head, available);
  connection->read_buffer_offset = available;
}

/**
 * Try reading data from the socket into the
 * read buffer of the connection.
 *
 * @return MHD_YES if something changed,
 *         MHD_NO if we were interrupted or if
 *                no space was available
 */
static int
do_read (struct MHD_Connection *connection)
{
  int bytes_read;

  if (connection->read_buffer_size == connection->read_buffer_offset)
    return MHD_NO;

  bytes_read = connection->recv_cls (connection,
                                     &connection->read_buffer
                                     [connection->read_buffer_offset],
                                     connection->read_buffer_size -
                                     connection->read_buffer_offset);
  if (bytes_read < 0)
    {
      if ((EINTR == errno) || (EAGAIN == errno))
        return MHD_NO;
#if HAVE_MESSAGES
#if HTTPS_SUPPORT
      if (0 != (connection->daemon->options & MHD_USE_SSL))
	MHD_DLOG (connection->daemon,
		  "Failed to receive data: %s\n",
		  gnutls_strerror (bytes_read));
      else
#endif      
	MHD_DLOG (connection->daemon,
		  "Failed to receive data: %s\n", STRERROR (errno));
#endif
      CONNECTION_CLOSE_ERROR (connection, NULL);
      return MHD_YES;
    }
  if (0 == bytes_read)
    {
      /* other side closed connection */
      connection->read_closed = MHD_YES;
      SHUTDOWN (connection->socket_fd, SHUT_RD);
      return MHD_YES;
    }
  connection->read_buffer_offset += bytes_read;
  return MHD_YES;
}

/**
 * Try writing data to the socket from the
 * write buffer of the connection.
 *
 * @return MHD_YES if something changed,
 *         MHD_NO if we were interrupted
 */
static int
do_write (struct MHD_Connection *connection)
{
  int ret;

  ret = connection->send_cls (connection,
                              &connection->write_buffer
                              [connection->write_buffer_send_offset],
                              connection->write_buffer_append_offset
                              - connection->write_buffer_send_offset);

  if (ret < 0)
    {
      if ((EINTR == errno) || (EAGAIN == errno))
        return MHD_NO;
#if HAVE_MESSAGES
#if HTTPS_SUPPORT
      if (0 != (connection->daemon->options & MHD_USE_SSL))
	MHD_DLOG (connection->daemon,
		  "Failed to send data: %s\n",
		  gnutls_strerror (ret));
      else
#endif      
	MHD_DLOG (connection->daemon,
		  "Failed to send data: %s\n", STRERROR (errno));
#endif
      CONNECTION_CLOSE_ERROR (connection, NULL);
      return MHD_YES;
    }
#if DEBUG_SEND_DATA
  FPRINTF (stderr,
           "Sent response: `%.*s'\n",
           ret,
           &connection->write_buffer[connection->write_buffer_send_offset]);
#endif
  connection->write_buffer_send_offset += ret;
  return MHD_YES;
}


/**
 * Check if we are done sending the write-buffer.
 * If so, transition into "next_state".
 * @return MHY_NO if we are not done, MHD_YES if we are
 */
static int
check_write_done (struct MHD_Connection *connection,
                  enum MHD_CONNECTION_STATE next_state)
{
  if (connection->write_buffer_append_offset !=
      connection->write_buffer_send_offset)
    return MHD_NO;
  connection->write_buffer_append_offset = 0;
  connection->write_buffer_send_offset = 0;
  connection->state = next_state;
  MHD_pool_reallocate (connection->pool, connection->write_buffer,
                       connection->write_buffer_size, 0);
  connection->write_buffer = NULL;
  connection->write_buffer_size = 0;
  return MHD_YES;
}


/**
 * We have received (possibly the beginning of) a line in the
 * header (or footer).  Validate (check for ":") and prepare
 * to process.
 */
static int
process_header_line (struct MHD_Connection *connection, char *line)
{
  char *colon;

  /* line should be normal header line, find colon */
  colon = strchr (line, ':');
  if (colon == NULL)
    {
      /* error in header line, die hard */
      CONNECTION_CLOSE_ERROR (connection, 
			      "Received malformed line (no colon), closing connection.\n");
      return MHD_NO;
    }
  /* zero-terminate header */
  colon[0] = '\0';
  colon++;                      /* advance to value */
  while ((colon[0] != '\0') && ((colon[0] == ' ') || (colon[0] == '\t')))
    colon++;
  /* we do the actual adding of the connection
     header at the beginning of the while
     loop since we need to be able to inspect
     the *next* header line (in case it starts
     with a space...) */
  connection->last = line;
  connection->colon = colon;
  return MHD_YES;
}


/**
 * Process a header value that spans multiple lines.
 * The previous line(s) are in connection->last.
 *
 * @param line the current input line
 * @param kind if the line is complete, add a header
 *        of the given kind
 * @return MHD_YES if the line was processed successfully
 */
static int
process_broken_line (struct MHD_Connection *connection,
                     char *line, enum MHD_ValueKind kind)
{
  char *last;
  char *tmp;
  size_t last_len;
  size_t tmp_len;

  last = connection->last;
  if ((line[0] == ' ') || (line[0] == '\t'))
    {
      /* value was continued on the next line, see
         http://www.jmarshall.com/easy/http/ */
      last_len = strlen (last);
      /* skip whitespace at start of 2nd line */
      tmp = line;
      while ((tmp[0] == ' ') || (tmp[0] == '\t'))
        tmp++;                  
      tmp_len = strlen (tmp);
      last = MHD_pool_reallocate (connection->pool,
                                  last,
                                  last_len + 1,
                                  last_len + tmp_len + 1);
      if (last == NULL)
        {
          transmit_error_response (connection,
                                   MHD_HTTP_REQUEST_ENTITY_TOO_LARGE,
                                   REQUEST_TOO_BIG);
          return MHD_NO;
        }
      memcpy (&last[last_len], tmp, tmp_len + 1);
      connection->last = last;
      return MHD_YES;           /* possibly more than 2 lines... */
    }
  EXTRA_CHECK ((last != NULL) && (connection->colon != NULL));
  if ((MHD_NO == connection_add_header (connection,
                                        last, connection->colon, kind)))
    {
      transmit_error_response (connection, MHD_HTTP_REQUEST_ENTITY_TOO_LARGE,
                               REQUEST_TOO_BIG);
      return MHD_NO;
    }
  /* we still have the current line to deal with... */
  if (strlen (line) != 0)
    {
      if (MHD_NO == process_header_line (connection, line))
        {
          transmit_error_response (connection,
                                   MHD_HTTP_BAD_REQUEST, REQUEST_MALFORMED);
          return MHD_NO;
        }
    }
  return MHD_YES;
}


/**
 * Parse the various headers; figure out the size
 * of the upload and make sure the headers follow
 * the protocol.  Advance to the appropriate state.
 */
static void
parse_connection_headers (struct MHD_Connection *connection)
{
  const char *clen;
  MHD_UNSIGNED_LONG_LONG cval;
  struct MHD_Response *response;
  const char *enc;
  char *end;

  parse_cookie_header (connection);
  if ((0 != (MHD_USE_PEDANTIC_CHECKS & connection->daemon->options))
      && (NULL != connection->version)
      && (0 == strcasecmp (MHD_HTTP_VERSION_1_1, connection->version))
      && (NULL ==
          MHD_lookup_connection_value (connection, MHD_HEADER_KIND,
                                       MHD_HTTP_HEADER_HOST)))
    {
      /* die, http 1.1 request without host and we are pedantic */
      connection->state = MHD_CONNECTION_FOOTERS_RECEIVED;
      connection->read_closed = MHD_YES;
#if HAVE_MESSAGES
      MHD_DLOG (connection->daemon,
                "Received `%s' request without `%s' header.\n",
                MHD_HTTP_VERSION_1_1, MHD_HTTP_HEADER_HOST);
#endif
      EXTRA_CHECK (connection->response == NULL);
      response =
        MHD_create_response_from_buffer (strlen (REQUEST_LACKS_HOST),
					 REQUEST_LACKS_HOST,
					 MHD_RESPMEM_PERSISTENT);
      MHD_queue_response (connection, MHD_HTTP_BAD_REQUEST, response);
      MHD_destroy_response (response);
      return;
    }

  clen = MHD_lookup_connection_value (connection,
                                      MHD_HEADER_KIND,
                                      MHD_HTTP_HEADER_CONTENT_LENGTH);
  if (clen != NULL)
    {
      cval = strtoul (clen, &end, 10);
      if ( ('\0' != *end) ||
	 ( (LONG_MAX == cval) && (errno == ERANGE) ) )
        {
#if HAVE_MESSAGES
          MHD_DLOG (connection->daemon,
                    "Failed to parse `%s' header `%s', closing connection.\n",
                    MHD_HTTP_HEADER_CONTENT_LENGTH, clen);
#endif
	  CONNECTION_CLOSE_ERROR (connection, NULL);
          return;
        }
      connection->remaining_upload_size = cval;
    }
  else
    {
      enc = MHD_lookup_connection_value (connection,
					 MHD_HEADER_KIND,
					 MHD_HTTP_HEADER_TRANSFER_ENCODING);
      if (NULL == enc)
        {
          /* this request (better) not have a body */
          connection->remaining_upload_size = 0;
        }
      else
        {
          connection->remaining_upload_size = MHD_SIZE_UNKNOWN;
          if (0 == strcasecmp (enc, "chunked"))
	    connection->have_chunked_upload = MHD_YES;
        }
    }
}


/**
 * This function handles a particular connection when it has been
 * determined that there is data to be read off a socket. All
 * implementations (multithreaded, external select, internal select)
 * call this function to handle reads.
 *
 * @param connection connection to handle
 * @return always MHD_YES (we should continue to process the
 *         connection)
 */
int
MHD_connection_handle_read (struct MHD_Connection *connection)
{
  connection->last_activity = MHD_monotonic_time();
  if (connection->state == MHD_CONNECTION_CLOSED)
    return MHD_YES;
  /* make sure "read" has a reasonable number of bytes
     in buffer to use per system call (if possible) */
  if (connection->read_buffer_offset + MHD_BUF_INC_SIZE >
      connection->read_buffer_size)
    try_grow_read_buffer (connection);
  if (MHD_NO == do_read (connection))
    return MHD_YES;
  while (1)
    {
#if DEBUG_STATES
      MHD_DLOG (connection->daemon, "%s: state: %s\n",
                __FUNCTION__, MHD_state_to_string (connection->state));
#endif
      switch (connection->state)
        {
        case MHD_CONNECTION_INIT:
        case MHD_CONNECTION_URL_RECEIVED:
        case MHD_CONNECTION_HEADER_PART_RECEIVED:
        case MHD_CONNECTION_HEADERS_RECEIVED:
        case MHD_CONNECTION_HEADERS_PROCESSED:
        case MHD_CONNECTION_CONTINUE_SENDING:
        case MHD_CONNECTION_CONTINUE_SENT:
        case MHD_CONNECTION_BODY_RECEIVED:
        case MHD_CONNECTION_FOOTER_PART_RECEIVED:
          /* nothing to do but default action */
          if (MHD_YES == connection->read_closed)
            {
	      MHD_connection_close (connection,
				    MHD_REQUEST_TERMINATED_READ_ERROR);
              continue;
            }
          break;
        case MHD_CONNECTION_CLOSED:
          return MHD_YES;
        default:
          /* shrink read buffer to how much is actually used */
          MHD_pool_reallocate (connection->pool,
                               connection->read_buffer,
                               connection->read_buffer_size + 1,
                               connection->read_buffer_offset);
          break;
        }
      break;
    }
  return MHD_YES;
}


/**
 * This function was created to handle writes to sockets when it has
 * been determined that the socket can be written to. All
 * implementations (multithreaded, external select, internal select)
 * call this function
 *
 * @param connection connection to handle
 * @return always MHD_YES (we should continue to process the
 *         connection)
 */
int
MHD_connection_handle_write (struct MHD_Connection *connection)
{
  struct MHD_Response *response;
  int ret;
  connection->last_activity = MHD_monotonic_time();
  while (1)
    {
#if DEBUG_STATES
      MHD_DLOG (connection->daemon, "%s: state: %s\n",
                __FUNCTION__, MHD_state_to_string (connection->state));
#endif
      switch (connection->state)
        {
        case MHD_CONNECTION_INIT:
        case MHD_CONNECTION_URL_RECEIVED:
        case MHD_CONNECTION_HEADER_PART_RECEIVED:
        case MHD_CONNECTION_HEADERS_RECEIVED:
          EXTRA_CHECK (0);
          break;
        case MHD_CONNECTION_HEADERS_PROCESSED:
          break;
        case MHD_CONNECTION_CONTINUE_SENDING:
          ret = connection->send_cls (connection,
                                      &HTTP_100_CONTINUE
                                      [connection->continue_message_write_offset],
                                      strlen (HTTP_100_CONTINUE) -
                                      connection->continue_message_write_offset);
          if (ret < 0)
            {
              if ((errno == EINTR) || (errno == EAGAIN))
                break;
#if HAVE_MESSAGES
              MHD_DLOG (connection->daemon,
                        "Failed to send data: %s\n", STRERROR (errno));
#endif
	      CONNECTION_CLOSE_ERROR (connection, NULL);
              return MHD_YES;
            }
#if DEBUG_SEND_DATA
          FPRINTF (stderr,
                   "Sent 100 continue response: `%.*s'\n",
                   ret,
                   &HTTP_100_CONTINUE
                   [connection->continue_message_write_offset]);
#endif
          connection->continue_message_write_offset += ret;
          break;
        case MHD_CONNECTION_CONTINUE_SENT:
        case MHD_CONNECTION_BODY_RECEIVED:
        case MHD_CONNECTION_FOOTER_PART_RECEIVED:
        case MHD_CONNECTION_FOOTERS_RECEIVED:
          EXTRA_CHECK (0);
          break;
        case MHD_CONNECTION_HEADERS_SENDING:
          do_write (connection);
	  if (connection->state != MHD_CONNECTION_HEADERS_SENDING)
 	     break;
          check_write_done (connection, MHD_CONNECTION_HEADERS_SENT);
          break;
        case MHD_CONNECTION_HEADERS_SENT:
          EXTRA_CHECK (0);
          break;
        case MHD_CONNECTION_NORMAL_BODY_READY:
          response = connection->response;
          if (response->crc != NULL)
            pthread_mutex_lock (&response->mutex);
          if (MHD_YES != try_ready_normal_body (connection))
            {
              if (response->crc != NULL)
                pthread_mutex_unlock (&response->mutex);
              break;
            }
	  ret = connection->send_cls (connection,
				      &response->data
				      [connection->response_write_position
				       - response->data_start],
				      response->data_size -
				      (connection->response_write_position
				       - response->data_start));
#if DEBUG_SEND_DATA
          if (ret > 0)
            FPRINTF (stderr,
                     "Sent DATA response: `%.*s'\n",
                     ret,
                     &response->data[connection->response_write_position -
                                     response->data_start]);
#endif
          if (response->crc != NULL)
            pthread_mutex_unlock (&response->mutex);
          if (ret < 0)
            {
              if ((errno == EINTR) || (errno == EAGAIN))
                return MHD_YES;
#if HAVE_MESSAGES
              MHD_DLOG (connection->daemon,
                        "Failed to send data: %s\n", STRERROR (errno));
#endif
	      CONNECTION_CLOSE_ERROR (connection, NULL);
              return MHD_YES;
            }
          connection->response_write_position += ret;
          if (connection->response_write_position ==
              connection->response->total_size)
            connection->state = MHD_CONNECTION_FOOTERS_SENT;    /* have no footers... */
          break;
        case MHD_CONNECTION_NORMAL_BODY_UNREADY:
          EXTRA_CHECK (0);
          break;
        case MHD_CONNECTION_CHUNKED_BODY_READY:
          do_write (connection);
	  if (connection->state !=  MHD_CONNECTION_CHUNKED_BODY_READY)
	     break;
          check_write_done (connection,
                            (connection->response->total_size ==
                             connection->response_write_position) ?
                            MHD_CONNECTION_BODY_SENT :
                            MHD_CONNECTION_CHUNKED_BODY_UNREADY);
          break;
        case MHD_CONNECTION_CHUNKED_BODY_UNREADY:
        case MHD_CONNECTION_BODY_SENT:
          EXTRA_CHECK (0);
          break;
        case MHD_CONNECTION_FOOTERS_SENDING:
          do_write (connection);
	  if (connection->state != MHD_CONNECTION_FOOTERS_SENDING)
	    break;
          check_write_done (connection, MHD_CONNECTION_FOOTERS_SENT);
          break;
        case MHD_CONNECTION_FOOTERS_SENT:
          EXTRA_CHECK (0);
          break;
        case MHD_CONNECTION_CLOSED:
          return MHD_YES;
        case MHD_TLS_CONNECTION_INIT:
          EXTRA_CHECK (0);
          break;
        default:
          EXTRA_CHECK (0);
	  CONNECTION_CLOSE_ERROR (connection, "Internal error\n");
          return MHD_YES;
        }
      break;
    }
  return MHD_YES;
}


/**
 * This function was created to handle per-connection processing that
 * has to happen even if the socket cannot be read or written to.  All
 * implementations (multithreaded, external select, internal select)
 * call this function.
 *
 * @param connection connection to handle
 * @return MHD_YES if we should continue to process the
 *         connection (not dead yet), MHD_NO if it died
 */
int
MHD_connection_handle_idle (struct MHD_Connection *connection)
{
  struct MHD_Daemon *daemon;
  unsigned int timeout;
  const char *end;
  int rend;
  char *line;

  while (1)
    {
#if DEBUG_STATES
      MHD_DLOG (connection->daemon, "%s: state: %s\n",
                __FUNCTION__, MHD_state_to_string (connection->state));
#endif
      switch (connection->state)
        {
        case MHD_CONNECTION_INIT:
          line = get_next_header_line (connection);
          if (line == NULL)
            {
              if (connection->state != MHD_CONNECTION_INIT)
                continue;
              if (connection->read_closed)
                {
		  CONNECTION_CLOSE_ERROR (connection, 
					  NULL);
                  continue;
                }
              break;
            }
          if (MHD_NO == parse_initial_message_line (connection, line))
            CONNECTION_CLOSE_ERROR (connection, NULL);
          else
            connection->state = MHD_CONNECTION_URL_RECEIVED;
          continue;
        case MHD_CONNECTION_URL_RECEIVED:
          line = get_next_header_line (connection);
          if (line == NULL)
            {
              if (connection->state != MHD_CONNECTION_URL_RECEIVED)
                continue;
              if (connection->read_closed)
                {
		  CONNECTION_CLOSE_ERROR (connection, 
					  NULL);
                  continue;
                }
              break;
            }
          if (strlen (line) == 0)
            {
              connection->state = MHD_CONNECTION_HEADERS_RECEIVED;
              continue;
            }
          if (MHD_NO == process_header_line (connection, line))
            {
              transmit_error_response (connection,
                                       MHD_HTTP_BAD_REQUEST,
                                       REQUEST_MALFORMED);
              break;
            }
          connection->state = MHD_CONNECTION_HEADER_PART_RECEIVED;
          continue;
        case MHD_CONNECTION_HEADER_PART_RECEIVED:
          line = get_next_header_line (connection);
          if (line == NULL)
            {
              if (connection->state != MHD_CONNECTION_HEADER_PART_RECEIVED)
                continue;
              if (connection->read_closed)
                {
		  CONNECTION_CLOSE_ERROR (connection, 
					  NULL);
                  continue;
                }
              break;
            }
          if (MHD_NO ==
              process_broken_line (connection, line, MHD_HEADER_KIND))
            continue;
          if (strlen (line) == 0)
            {
              connection->state = MHD_CONNECTION_HEADERS_RECEIVED;
              continue;
            }
          continue;
        case MHD_CONNECTION_HEADERS_RECEIVED:
          parse_connection_headers (connection);
          if (connection->state == MHD_CONNECTION_CLOSED)
            continue;
          connection->state = MHD_CONNECTION_HEADERS_PROCESSED;
          continue;
        case MHD_CONNECTION_HEADERS_PROCESSED:
          call_connection_handler (connection); /* first call */
          if (connection->state == MHD_CONNECTION_CLOSED)
            continue;
          if (need_100_continue (connection))
            {
              connection->state = MHD_CONNECTION_CONTINUE_SENDING;
              break;
            }
          if (connection->response != NULL)
            {
              /* we refused (no upload allowed!) */
              connection->remaining_upload_size = 0;
              /* force close, in case client still tries to upload... */
              connection->read_closed = MHD_YES;
            }
          connection->state = (connection->remaining_upload_size == 0)
            ? MHD_CONNECTION_FOOTERS_RECEIVED : MHD_CONNECTION_CONTINUE_SENT;
          continue;
        case MHD_CONNECTION_CONTINUE_SENDING:
          if (connection->continue_message_write_offset ==
              strlen (HTTP_100_CONTINUE))
            {
              connection->state = MHD_CONNECTION_CONTINUE_SENT;
              continue;
            }
          break;
        case MHD_CONNECTION_CONTINUE_SENT:
          if (connection->read_buffer_offset != 0)
            {
              process_request_body (connection);     /* loop call */
              if (connection->state == MHD_CONNECTION_CLOSED)
                continue;
            }
          if ((connection->remaining_upload_size == 0) ||
              ((connection->remaining_upload_size == MHD_SIZE_UNKNOWN) &&
               (connection->read_buffer_offset == 0) &&
               (MHD_YES == connection->read_closed)))
            {
              if ((MHD_YES == connection->have_chunked_upload) &&
                  (MHD_NO == connection->read_closed))
                connection->state = MHD_CONNECTION_BODY_RECEIVED;
              else
                connection->state = MHD_CONNECTION_FOOTERS_RECEIVED;
              continue;
            }
          break;
        case MHD_CONNECTION_BODY_RECEIVED:
          line = get_next_header_line (connection);
          if (line == NULL)
            {
              if (connection->state != MHD_CONNECTION_BODY_RECEIVED)
                continue;
              if (connection->read_closed)
                {
		  CONNECTION_CLOSE_ERROR (connection, 
					  NULL);
                  continue;
                }
              break;
            }
          if (strlen (line) == 0)
            {
              connection->state = MHD_CONNECTION_FOOTERS_RECEIVED;
              continue;
            }
          if (MHD_NO == process_header_line (connection, line))
            {
              transmit_error_response (connection,
                                       MHD_HTTP_BAD_REQUEST,
                                       REQUEST_MALFORMED);
              break;
            }
          connection->state = MHD_CONNECTION_FOOTER_PART_RECEIVED;
          continue;
        case MHD_CONNECTION_FOOTER_PART_RECEIVED:
          line = get_next_header_line (connection);
          if (line == NULL)
            {
              if (connection->state != MHD_CONNECTION_FOOTER_PART_RECEIVED)
                continue;
              if (connection->read_closed)
                {
		  CONNECTION_CLOSE_ERROR (connection, 
					  NULL);
                  continue;
                }
              break;
            }
          if (MHD_NO ==
              process_broken_line (connection, line, MHD_FOOTER_KIND))
            continue;
          if (strlen (line) == 0)
            {
              connection->state = MHD_CONNECTION_FOOTERS_RECEIVED;
              continue;
            }
          continue;
        case MHD_CONNECTION_FOOTERS_RECEIVED:
          call_connection_handler (connection); /* "final" call */
          if (connection->state == MHD_CONNECTION_CLOSED)
            continue;
          if (connection->response == NULL)
            break;              /* try again next time */
          if (MHD_NO == build_header_response (connection))
            {
              /* oops - close! */
	      CONNECTION_CLOSE_ERROR (connection, 
				      "Closing connection (failed to create response header)\n");
              continue;
            }
          connection->state = MHD_CONNECTION_HEADERS_SENDING;

#if HAVE_DECL_TCP_CORK
          /* starting header send, set TCP cork */
          {
            const int val = 1;
            setsockopt (connection->socket_fd, IPPROTO_TCP, TCP_CORK, &val,
                        sizeof (val));
          }
#endif
          break;
        case MHD_CONNECTION_HEADERS_SENDING:
          /* no default action */
          break;
        case MHD_CONNECTION_HEADERS_SENT:
          if (connection->have_chunked_upload)
            connection->state = MHD_CONNECTION_CHUNKED_BODY_UNREADY;
          else
            connection->state = MHD_CONNECTION_NORMAL_BODY_UNREADY;
          continue;
        case MHD_CONNECTION_NORMAL_BODY_READY:
          /* nothing to do here */
          break;
        case MHD_CONNECTION_NORMAL_BODY_UNREADY:
          if (connection->response->crc != NULL)
            pthread_mutex_lock (&connection->response->mutex);
          if (MHD_YES == try_ready_normal_body (connection))
            {
              if (connection->response->crc != NULL)
                pthread_mutex_unlock (&connection->response->mutex);
              connection->state = MHD_CONNECTION_NORMAL_BODY_READY;
              break;
            }
          if (connection->response->crc != NULL)
            pthread_mutex_unlock (&connection->response->mutex);
          /* not ready, no socket action */
          break;
        case MHD_CONNECTION_CHUNKED_BODY_READY:
          /* nothing to do here */
          break;
        case MHD_CONNECTION_CHUNKED_BODY_UNREADY:
          if (connection->response->crc != NULL)
            pthread_mutex_lock (&connection->response->mutex);
          if (MHD_YES == try_ready_chunked_body (connection))
            {
              if (connection->response->crc != NULL)
                pthread_mutex_unlock (&connection->response->mutex);
              connection->state = MHD_CONNECTION_CHUNKED_BODY_READY;
              continue;
            }
          if (connection->response->crc != NULL)
            pthread_mutex_unlock (&connection->response->mutex);
          break;
        case MHD_CONNECTION_BODY_SENT:
          build_header_response (connection);
          if (connection->write_buffer_send_offset ==
              connection->write_buffer_append_offset)
            connection->state = MHD_CONNECTION_FOOTERS_SENT;
          else
            connection->state = MHD_CONNECTION_FOOTERS_SENDING;
          continue;
        case MHD_CONNECTION_FOOTERS_SENDING:
          /* no default action */
          break;
        case MHD_CONNECTION_FOOTERS_SENT:
#if HAVE_DECL_TCP_CORK
          /* done sending, uncork */
          {
            const int val = 0;
            setsockopt (connection->socket_fd, IPPROTO_TCP, TCP_CORK, &val,
                        sizeof (val));
          }
#endif
          end =
            MHD_get_response_header (connection->response, 
				     MHD_HTTP_HEADER_CONNECTION);
	  rend = ( (end != NULL) && (0 == strcasecmp (end, "close")) );
          MHD_destroy_response (connection->response);
          connection->response = NULL;
          if (connection->daemon->notify_completed != NULL)
	    connection->daemon->notify_completed (connection->daemon->
						  notify_completed_cls,
						  connection,
						  &connection->client_context,
						  MHD_REQUEST_TERMINATED_COMPLETED_OK);	    
	  connection->client_aware = MHD_NO;
          end =
            MHD_lookup_connection_value (connection, MHD_HEADER_KIND,
                                         MHD_HTTP_HEADER_CONNECTION);
          connection->client_context = NULL;
          connection->continue_message_write_offset = 0;
          connection->responseCode = 0;
          connection->headers_received = NULL;
	  connection->headers_received_tail = NULL;
          connection->response_write_position = 0;
          connection->have_chunked_upload = MHD_NO;
          connection->method = NULL;
          connection->url = NULL;
          connection->write_buffer = NULL;
          connection->write_buffer_size = 0;
          connection->write_buffer_send_offset = 0;
          connection->write_buffer_append_offset = 0;
          if ( (rend) || ((end != NULL) && (0 == strcasecmp (end, "close"))) )
            {
              connection->read_closed = MHD_YES;
              connection->read_buffer_offset = 0;
            }
          if (((MHD_YES == connection->read_closed) &&
               (0 == connection->read_buffer_offset)) ||
              (connection->version == NULL) ||
              (0 != strcasecmp (MHD_HTTP_VERSION_1_1, connection->version)))
            {
              /* http 1.0, version-less requests cannot be pipelined */
              MHD_connection_close (connection, MHD_REQUEST_TERMINATED_COMPLETED_OK);
              MHD_pool_destroy (connection->pool);
              connection->pool = NULL;
              connection->read_buffer = NULL;
              connection->read_buffer_size = 0;
              connection->read_buffer_offset = 0;
            }
          else
            {
              connection->version = NULL;
              connection->state = MHD_CONNECTION_INIT;
              connection->read_buffer
                = MHD_pool_reset (connection->pool,
                                  connection->read_buffer,
                                  connection->read_buffer_size);
            }
          continue;
        case MHD_CONNECTION_CLOSED:
	  if (connection->response != NULL)
	    {
	      MHD_destroy_response (connection->response);
	      connection->response = NULL;
	    }
	  daemon = connection->daemon;
	  if (0 != pthread_mutex_lock(&daemon->cleanup_connection_mutex))
	    {
	      MHD_PANIC ("Failed to acquire cleanup mutex\n");
	    }
	  DLL_remove (daemon->connections_head,
		      daemon->connections_tail,
		      connection);
	  DLL_insert (daemon->cleanup_head,
		      daemon->cleanup_tail,
		      connection);
	  if (0 != pthread_mutex_unlock(&daemon->cleanup_connection_mutex))
	    {
	      MHD_PANIC ("Failed to release cleanup mutex\n");
	    }
	  return MHD_NO;
        default:
          EXTRA_CHECK (0);
          break;
        }
      break;
    }
  timeout = connection->connection_timeout;
  if ( (timeout != 0) &&
       (timeout <= (MHD_monotonic_time() - connection->last_activity)) )
    {
      MHD_connection_close (connection, MHD_REQUEST_TERMINATED_TIMEOUT_REACHED);
      return MHD_YES;
    }
  return MHD_YES;
}


/**
 * Set callbacks for this connection to those for HTTP.
 *
 * @param connection connection to initialize
 */
void
MHD_set_http_callbacks_ (struct MHD_Connection *connection)
{
  connection->read_handler = &MHD_connection_handle_read;
  connection->write_handler = &MHD_connection_handle_write;
  connection->idle_handler = &MHD_connection_handle_idle;
}


/**
 * Obtain information about the given connection.
 *
 * @param connection what connection to get information about
 * @param infoType what information is desired?
 * @param ... depends on infoType
 * @return NULL if this information is not available
 *         (or if the infoType is unknown)
 */
const union MHD_ConnectionInfo *
MHD_get_connection_info (struct MHD_Connection *connection,
                         enum MHD_ConnectionInfoType infoType, ...)
{
  switch (infoType)
    {
#if HTTPS_SUPPORT
    case MHD_CONNECTION_INFO_CIPHER_ALGO:
      if (connection->tls_session == NULL)
	return NULL;
      connection->cipher = gnutls_cipher_get (connection->tls_session);
      return (const union MHD_ConnectionInfo *) &connection->cipher;
    case MHD_CONNECTION_INFO_PROTOCOL:
      if (connection->tls_session == NULL)
	return NULL;
      connection->protocol = gnutls_protocol_get_version (connection->tls_session);
      return (const union MHD_ConnectionInfo *) &connection->protocol;
    case MHD_CONNECTION_INFO_GNUTLS_SESSION:
      if (connection->tls_session == NULL)
	return NULL;
      return (const union MHD_ConnectionInfo *) &connection->tls_session;
#endif
    case MHD_CONNECTION_INFO_CLIENT_ADDRESS:
      return (const union MHD_ConnectionInfo *) &connection->addr;
    case MHD_CONNECTION_INFO_DAEMON:
      return (const union MHD_ConnectionInfo *) &connection->daemon;
    default:
      return NULL;
    };
}


/**
 * Set a custom option for the given connection, overriding defaults.
 *
 * @param connection connection to modify
 * @param option option to set
 * @param ... arguments to the option, depending on the option type
 * @return MHD_YES on success, MHD_NO if setting the option failed
 */
int 
MHD_set_connection_option (struct MHD_Connection *connection,
			   enum MHD_CONNECTION_OPTION option,
			   ...)
{
  va_list ap;

  switch (option)
    {
    case MHD_CONNECTION_OPTION_TIMEOUT:
      va_start (ap, option);
      connection->connection_timeout = va_arg (ap, unsigned int);
      va_end (ap);
      return MHD_YES;
    default:
      return MHD_NO;
    }
}


/* end of connection.c */
