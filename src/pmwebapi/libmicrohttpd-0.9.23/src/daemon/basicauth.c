/*
     This file is part of libmicrohttpd
     (C) 2010, 2011, 2012 Daniel Pittman and Christian Grothoff

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
 * @file basicauth.c
 * @brief Implements HTTP basic authentication methods
 * @author Amr Ali
 * @author Matthieu Speder
 */
#include "platform.h"
#include <limits.h>
#include "internal.h"
#include "base64.h"

/**
 * Beginning string for any valid Basic authentication header.
 */
#define _BASIC_BASE		"Basic "


/**
 * Get the username and password from the basic authorization header sent by the client
 *
 * @param connection The MHD connection structure
 * @param password a pointer for the password
 * @return NULL if no username could be found, a pointer
 * 			to the username if found
 */
char *
MHD_basic_auth_get_username_password(struct MHD_Connection *connection,
				     char** password) 
{
  const char *header;
  char *decode;
  const char *separator;
  char *user;
  
  if ( (NULL == (header = MHD_lookup_connection_value (connection, 
						       MHD_HEADER_KIND,
						       MHD_HTTP_HEADER_AUTHORIZATION))) ||
       (0 != strncmp (header, _BASIC_BASE, strlen(_BASIC_BASE))) )
    return NULL;
  header += strlen (_BASIC_BASE);
  if (NULL == (decode = BASE64Decode (header)))
    {
#if HAVE_MESSAGES
      MHD_DLOG (connection->daemon,
		"Error decoding basic authentication\n");
#endif
      return NULL;
    }
  /* Find user:password pattern */
  if (NULL == (separator = strchr (decode, ':')))
    {
#if HAVE_MESSAGES
      MHD_DLOG(connection->daemon,
	       "Basic authentication doesn't contain ':' separator\n");
#endif
      free (decode);
      return NULL;
    }
  if (NULL == (user = strdup (decode)))
    {
      free (decode);
      return NULL;
    }
  user[separator - decode] = '\0'; /* cut off at ':' */
  if (NULL != password) 
    {
      *password = strdup (separator + 1);  
      if (NULL == *password)
	{
#if HAVE_MESSAGES
	  MHD_DLOG(connection->daemon,
		   "Failed to allocate memory for password\n");
#endif
	  free (decode);
	  free (user);
	  return NULL;
	}
    }
  free (decode);
  return user;
}


/**
 * Queues a response to request basic authentication from the client
 *
 * @param connection The MHD connection structure
 * @param realm the realm presented to the client
 * @return MHD_YES on success, MHD_NO otherwise
 */
int 
MHD_queue_basic_auth_fail_response (struct MHD_Connection *connection,
				    const char *realm, 
				    struct MHD_Response *response) 
{
  int ret;
  size_t hlen = strlen(realm) + strlen("Basic realm=\"\"") + 1;
  char header[hlen];

  snprintf (header, 
	    sizeof (header), 
	    "Basic realm=\"%s\"", 
	    realm);
  ret = MHD_add_response_header (response,
				 MHD_HTTP_HEADER_WWW_AUTHENTICATE,
				 header);
  if (MHD_YES == ret)
    ret = MHD_queue_response (connection, 
			      MHD_HTTP_UNAUTHORIZED, 
			      response);
  return ret;
}

/* end of basicauth.c */
