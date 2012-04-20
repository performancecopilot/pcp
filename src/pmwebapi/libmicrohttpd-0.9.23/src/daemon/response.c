/*
     This file is part of libmicrohttpd
     (C) 2007, 2009, 2010 Daniel Pittman and Christian Grothoff

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
 * @file response.c
 * @brief  Methods for managing response objects
 * @author Daniel Pittman
 * @author Christian Grothoff
 */

#include "internal.h"
#include "response.h"


/**
 * Add a header or footer line to the response.
 *
 * @param response response to add a header to
 * @param kind header or footer
 * @param header the header to add
 * @param content value to add
 * @return MHD_NO on error (i.e. invalid header or content format).
 */
static int
add_response_entry (struct MHD_Response *response,
		    enum MHD_ValueKind kind,
		    const char *header, 
		    const char *content)
{
  struct MHD_HTTP_Header *hdr;

  if ( (NULL == response) ||
       (NULL == header) ||
       (NULL == content) ||
       (0 == strlen (header)) ||
       (0 == strlen (content)) ||
       (NULL != strchr (header, '\t')) ||
       (NULL != strchr (header, '\r')) ||
       (NULL != strchr (header, '\n')) ||
       (NULL != strchr (content, '\t')) ||
       (NULL != strchr (content, '\r')) || 
       (NULL != strchr (content, '\n')) )
    return MHD_NO;
  if (NULL == (hdr = malloc (sizeof (struct MHD_HTTP_Header))))
    return MHD_NO;
  if (NULL == (hdr->header = strdup (header)))
    {
      free (hdr);
      return MHD_NO;
    }
  if (NULL == (hdr->value = strdup (content)))
    {
      free (hdr->header);
      free (hdr);
      return MHD_NO;
    }
  hdr->kind = kind;
  hdr->next = response->first_header;
  response->first_header = hdr;
  return MHD_YES;
}


/**
 * Add a header line to the response.
 *
 * @param response response to add a header to
 * @param header the header to add
 * @param content value to add
 * @return MHD_NO on error (i.e. invalid header or content format).
 */
int
MHD_add_response_header (struct MHD_Response *response,
                         const char *header, const char *content)
{
  return add_response_entry (response,
			     MHD_HEADER_KIND,
			     header,
			     content);
}


/**
 * Add a footer line to the response.
 *
 * @param response response to remove a header from
 * @param footer the footer to delete
 * @param content value to delete
 * @return MHD_NO on error (i.e. invalid footer or content format).
 */
int
MHD_add_response_footer (struct MHD_Response *response,
                         const char *footer, const char *content)
{
  return add_response_entry (response,
			     MHD_FOOTER_KIND,
			     footer,
			     content);
}


/**
 * Delete a header line from the response.
 *
 * @param response response to remove a header from
 * @param header the header to delete
 * @param content value to delete
 * @return MHD_NO on error (no such header known)
 */
int
MHD_del_response_header (struct MHD_Response *response,
                         const char *header, const char *content)
{
  struct MHD_HTTP_Header *pos;
  struct MHD_HTTP_Header *prev;

  if ( (NULL == header) || (NULL == content) )
    return MHD_NO;
  prev = NULL;
  pos = response->first_header;
  while (pos != NULL)
    {
      if ((0 == strcmp (header, pos->header)) &&
          (0 == strcmp (content, pos->value)))
        {
          free (pos->header);
          free (pos->value);
          if (NULL == prev)
            response->first_header = pos->next;
          else
            prev->next = pos->next;
          free (pos);
          return MHD_YES;
        }
      prev = pos;
      pos = pos->next;
    }
  return MHD_NO;
}


/**
 * Get all of the headers added to a response.
 *
 * @param iterator callback to call on each header;
 *        maybe NULL (then just count headers)
 * @param iterator_cls extra argument to iterator
 * @return number of entries iterated over
 */
int
MHD_get_response_headers (struct MHD_Response *response,
                          MHD_KeyValueIterator iterator, void *iterator_cls)
{
  struct MHD_HTTP_Header *pos;
  int numHeaders = 0;

  for (pos = response->first_header; NULL != pos; pos = pos->next)
    {
      numHeaders++;
      if ((NULL != iterator) &&
          (MHD_YES != iterator (iterator_cls,
                                pos->kind, pos->header, pos->value)))
        break;
    }
  return numHeaders;
}


/**
 * Get a particular header from the response.
 *
 * @param key which header to get
 * @return NULL if header does not exist
 */
const char *
MHD_get_response_header (struct MHD_Response *response, const char *key)
{
  struct MHD_HTTP_Header *pos;

  if (NULL == key)
    return NULL;
  for (pos = response->first_header; NULL != pos; pos = pos->next)
    if (0 == strcmp (key, pos->header))
      return pos->value;
  return NULL;
}


/**
 * Create a response object.  The response object can be extended with
 * header information and then be used any number of times.
 *
 * @param size size of the data portion of the response, MHD_SIZE_UNKNOWN for unknown
 * @param block_size preferred block size for querying crc (advisory only,
 *                   MHD may still call crc using smaller chunks); this
 *                   is essentially the buffer size used for IO, clients
 *                   should pick a value that is appropriate for IO and
 *                   memory performance requirements
 * @param crc callback to use to obtain response data
 * @param crc_cls extra argument to crc
 * @param crfc callback to call to free crc_cls resources
 * @return NULL on error (i.e. invalid arguments, out of memory)
 */
struct MHD_Response *
MHD_create_response_from_callback (uint64_t size,
                                   size_t block_size,
                                   MHD_ContentReaderCallback crc,
                                   void *crc_cls,
                                   MHD_ContentReaderFreeCallback crfc)
{
  struct MHD_Response *response;

  if ((NULL == crc) || (0 == block_size))
    return NULL;
  if (NULL == (response = malloc (sizeof (struct MHD_Response) + block_size)))
    return NULL;
  memset (response, 0, sizeof (struct MHD_Response));
  response->fd = -1;
  response->data = (void *) &response[1];
  response->data_buffer_size = block_size;
  if (0 != pthread_mutex_init (&response->mutex, NULL))
    {
      free (response);
      return NULL;
    }
  response->crc = crc;
  response->crfc = crfc;
  response->crc_cls = crc_cls;
  response->reference_count = 1;
  response->total_size = size;
  return response;
}


/**
 * Given a file descriptor, read data from the file
 * to generate the response.
 * 
 * @param cls pointer to the response
 * @param pos offset in the file to access
 * @param buf where to write the data
 * @param max number of bytes to write at most
 * @return number of bytes written
 */
static ssize_t
file_reader (void *cls, uint64_t pos, char *buf, size_t max)
{
  struct MHD_Response *response = cls;
  ssize_t n;

  (void) lseek (response->fd, pos + response->fd_off, SEEK_SET);
  n = read (response->fd, buf, max);
  if (0 == n) 
    return MHD_CONTENT_READER_END_OF_STREAM;
  if (n < 0) 
    return MHD_CONTENT_READER_END_WITH_ERROR;
  return n;
}


/**
 * Destroy file reader context.  Closes the file
 * descriptor.
 *
 * @param cls pointer to file descriptor
 */
static void
free_callback (void *cls)
{
  struct MHD_Response *response = cls;

  (void) close (response->fd);
  response->fd = -1;
}


/**
 * Create a response object.  The response object can be extended with
 * header information and then be used any number of times.
 *
 * @param size size of the data portion of the response
 * @param fd file descriptor referring to a file on disk with the data
 * @param off offset to start reading from in the file
 * @return NULL on error (i.e. invalid arguments, out of memory)
 */
struct MHD_Response *MHD_create_response_from_fd_at_offset (size_t size,
							    int fd,
							    off_t offset)
{
  struct MHD_Response *response;

  response = MHD_create_response_from_callback (size,
						4 * 1024,
						&file_reader,
						NULL,
						&free_callback);
  if (NULL == response)
    return NULL;
  response->fd = fd;
  response->fd_off = offset;
  response->crc_cls = response;
  return response;
}


/**
 * Create a response object.  The response object can be extended with
 * header information and then be used any number of times.
 *
 * @param size size of the data portion of the response
 * @param fd file descriptor referring to a file on disk with the data
 * @return NULL on error (i.e. invalid arguments, out of memory)
 */
struct MHD_Response *MHD_create_response_from_fd (size_t size,
						  int fd)
{
  return MHD_create_response_from_fd_at_offset (size, fd, 0);
}


/**
 * Create a response object.  The response object can be extended with
 * header information and then be used any number of times.
 *
 * @param size size of the data portion of the response
 * @param data the data itself
 * @param must_free libmicrohttpd should free data when done
 * @param must_copy libmicrohttpd must make a copy of data
 *        right away, the data maybe released anytime after
 *        this call returns
 * @return NULL on error (i.e. invalid arguments, out of memory)
 * @deprecated use MHD_create_response_from_buffer instead
 */
struct MHD_Response *
MHD_create_response_from_data (size_t size,
                               void *data, int must_free, int must_copy)
{
  struct MHD_Response *response;
  void *tmp;

  if ((NULL == data) && (size > 0))
    return NULL;
  if (NULL == (response = malloc (sizeof (struct MHD_Response))))
    return NULL;
  memset (response, 0, sizeof (struct MHD_Response));
  response->fd = -1;
  if (0 != pthread_mutex_init (&response->mutex, NULL))
    {
      free (response);
      return NULL;
    }
  if ((must_copy) && (size > 0))
    {
      if (NULL == (tmp = malloc (size)))
        {
	  pthread_mutex_destroy (&response->mutex);
          free (response);
          return NULL;
        }
      memcpy (tmp, data, size);
      must_free = MHD_YES;
      data = tmp;
    }
  response->crc = NULL;
  response->crfc = must_free ? &free : NULL;
  response->crc_cls = must_free ? data : NULL;
  response->reference_count = 1;
  response->total_size = size;
  response->data = data;
  response->data_size = size;
  return response;
}


/**
 * Create a response object.  The response object can be extended with
 * header information and then be used any number of times.
 *
 * @param size size of the data portion of the response
 * @param buffer size bytes containing the response's data portion
 * @param mode flags for buffer management
 * @return NULL on error (i.e. invalid arguments, out of memory)
 */
struct MHD_Response *
MHD_create_response_from_buffer (size_t size,
				 void *buffer,
				 enum MHD_ResponseMemoryMode mode)
{
  return MHD_create_response_from_data (size,
					buffer,
					mode == MHD_RESPMEM_MUST_FREE,
					mode == MHD_RESPMEM_MUST_COPY);
}


/**
 * Destroy a response object and associated resources.  Note that
 * libmicrohttpd may keep some of the resources around if the response
 * is still in the queue for some clients, so the memory may not
 * necessarily be freed immediatley.
 */
void
MHD_destroy_response (struct MHD_Response *response)
{
  struct MHD_HTTP_Header *pos;

  if (NULL == response)
    return;
  pthread_mutex_lock (&response->mutex);
  if (0 != --(response->reference_count))
    {
      pthread_mutex_unlock (&response->mutex);
      return;
    }
  pthread_mutex_unlock (&response->mutex);
  pthread_mutex_destroy (&response->mutex);
  if (response->crfc != NULL)
    response->crfc (response->crc_cls);
  while (NULL != response->first_header)
    {
      pos = response->first_header;
      response->first_header = pos->next;
      free (pos->header);
      free (pos->value);
      free (pos);
    }
  free (response);
}


void
MHD_increment_response_rc (struct MHD_Response *response)
{
  pthread_mutex_lock (&response->mutex);
  (response->reference_count)++;
  pthread_mutex_unlock (&response->mutex);
}


/* end of response.c */
