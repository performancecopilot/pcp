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
 * @file digestauth.c
 * @brief Implements HTTP digest authentication
 * @author Amr Ali
 * @author Matthieu Speder
 */
#include "platform.h"
#include <limits.h>
#include "internal.h"
#include "md5.h"

#define HASH_MD5_HEX_LEN (2 * MD5_DIGEST_SIZE)

/**
 * Beginning string for any valid Digest authentication header.
 */
#define _BASE		"Digest "

/**
 * Maximum length of a username for digest authentication.
 */
#define MAX_USERNAME_LENGTH 128

/**
 * Maximum length of a realm for digest authentication.
 */
#define MAX_REALM_LENGTH 256

/**
 * Maximum length of the response in digest authentication.
 */
#define MAX_AUTH_RESPONSE_LENGTH 128


/**
 * convert bin to hex 
 *
 * @param bin binary data
 * @param len number of bytes in bin
 * @param hex pointer to len*2+1 bytes
 */
static void
cvthex (const unsigned char *bin,
	size_t len,
	char *hex)
{
  size_t i;
  unsigned int j;
  
  for (i = 0; i < len; ++i) 
    {
      j = (bin[i] >> 4) & 0x0f;      
      hex[i * 2] = j <= 9 ? (j + '0') : (j + 'a' - 10);    
      j = bin[i] & 0x0f;    
      hex[i * 2 + 1] = j <= 9 ? (j + '0') : (j + 'a' - 10);
    }
  hex[len * 2] = '\0';
}


/**
 * calculate H(A1) as per RFC2617 spec and store the
 * result in 'sessionkey'.
 *
 * @param alg The hash algorithm used, can be "md5" or "md5-sess"
 * @param username A `char *' pointer to the username value
 * @param realm A `char *' pointer to the realm value
 * @param password A `char *' pointer to the password value
 * @param nonce A `char *' pointer to the nonce value
 * @param cnonce A `char *' pointer to the cnonce value
 * @param sessionkey pointer to buffer of HASH_MD5_HEX_LEN+1 bytes
 */
static void
digest_calc_ha1 (const char *alg,
		 const char *username,
		 const char *realm,
		 const char *password,
		 const char *nonce,
		 const char *cnonce,
		 char *sessionkey)
{
  struct MD5Context md5;
  unsigned char ha1[MD5_DIGEST_SIZE];
  
  MD5Init (&md5);
  MD5Update (&md5, username, strlen (username));
  MD5Update (&md5, ":", 1);
  MD5Update (&md5, realm, strlen (realm));
  MD5Update (&md5, ":", 1);
  MD5Update (&md5, password, strlen (password));
  MD5Final (ha1, &md5);
  if (0 == strcasecmp (alg, "md5-sess")) 
    {
      MD5Init (&md5);
      MD5Update (&md5, ha1, sizeof (ha1));
      MD5Update (&md5, ":", 1);
      MD5Update (&md5, nonce, strlen (nonce));
      MD5Update (&md5, ":", 1);
      MD5Update (&md5, cnonce, strlen (cnonce));
      MD5Final (ha1, &md5);
    }
  cvthex (ha1, sizeof (ha1), sessionkey);
}


/**
 * Calculate request-digest/response-digest as per RFC2617 spec 
 * 
 * @param ha1 H(A1)
 * @param nonce nonce from server
 * @param noncecount 8 hex digits
 * @param cnonce client nonce
 * @param qop qop-value: "", "auth" or "auth-int"
 * @param method method from request
 * @param uri requested URL
 * @param hentity H(entity body) if qop="auth-int"
 * @param response request-digest or response-digest
 */
static void
digest_calc_response (const char *ha1,
		      const char *nonce,
		      const char *noncecount,
		      const char *cnonce,
		      const char *qop,
		      const char *method,
		      const char *uri,
		      const char *hentity,
		      char *response)
{
  struct MD5Context md5;
  unsigned char ha2[MD5_DIGEST_SIZE];
  unsigned char resphash[MD5_DIGEST_SIZE];
  char ha2hex[HASH_MD5_HEX_LEN + 1];
  
  MD5Init (&md5);
  MD5Update (&md5, method, strlen(method));
  MD5Update (&md5, ":", 1);
  MD5Update (&md5, uri, strlen(uri)); 
#if 0
  if (0 == strcasecmp(qop, "auth-int"))
    {
      /* This is dead code since the rest of this module does
	 not support auth-int. */
      MD5Update (&md5, ":", 1);
      if (NULL != hentity)
	MD5Update (&md5, hentity, strlen(hentity));
    }
#endif  
  MD5Final (ha2, &md5);
  cvthex (ha2, MD5_DIGEST_SIZE, ha2hex);
  MD5Init (&md5);  
  /* calculate response */  
  MD5Update (&md5, ha1, HASH_MD5_HEX_LEN);
  MD5Update (&md5, ":", 1);
  MD5Update (&md5, nonce, strlen(nonce));
  MD5Update (&md5, ":", 1);  
  if ('\0' != *qop)
    {
      MD5Update (&md5, noncecount, strlen(noncecount));
      MD5Update (&md5, ":", 1);
      MD5Update (&md5, cnonce, strlen(cnonce));
      MD5Update (&md5, ":", 1);
      MD5Update (&md5, qop, strlen(qop));
      MD5Update (&md5, ":", 1);
    }  
  MD5Update (&md5, ha2hex, HASH_MD5_HEX_LEN);
  MD5Final (resphash, &md5);
  cvthex (resphash, sizeof (resphash), response);
}


/**
 * Lookup subvalue off of the HTTP Authorization header.
 *
 * A description of the input format for 'data' is at
 * http://en.wikipedia.org/wiki/Digest_access_authentication
 *
 *
 * @param dest where to store the result (possibly truncated if
 *             the buffer is not big enough).
 * @param size size of dest
 * @param data pointer to the Authorization header
 * @param key key to look up in data
 * @return size of the located value, 0 if otherwise
 */
static int
lookup_sub_value (char *dest,
		  size_t size,
		  const char *data,
		  const char *key)
{
  size_t keylen;
  size_t len;
  const char *ptr;
  const char *eq;
  const char *q1;
  const char *q2;
  const char *qn;

  if (0 == size)
    return 0;
  keylen = strlen (key);
  ptr = data;
  while ('\0' != *ptr)
    {
      if (NULL == (eq = strchr (ptr, '=')))
	return 0;
      q1 = eq + 1;
      while (' ' == *q1)
	q1++;      
      if ('\"' != *q1)
	{
	  q2 = strchr (q1, ',');
	  qn = q2;
	}
      else
	{
	  q1++;
	  q2 = strchr (q1, '\"');
	  if (NULL == q2)
	    return 0; /* end quote not found */
	  qn = q2 + 1;
	}      
      if ( (0 == strncasecmp (ptr,
			      key,
			      keylen)) &&
	   (eq == &ptr[keylen]) )
	{
	  if (NULL == q2)
	    {
	      len = strlen (q1) + 1;
	      if (size > len)
		size = len;
	      size--;
	      strncpy (dest,
		       q1,
		       size);
	      dest[size] = '\0';
	      return size;
	    }
	  else
	    {
	      if (size > (q2 - q1) + 1)
		size = (q2 - q1) + 1;
	      size--;
	      memcpy (dest, 
		      q1,
		      size);
	      dest[size] = '\0';
	      return size;
	    }
	}
      if (NULL == qn)
	return 0;
      ptr = strchr (qn, ',');
      if (NULL == ptr)
	return 0;
      ptr++;
      while (' ' == *ptr)
	ptr++;
    }
  return 0;
}


/**
 * Check nonce-nc map array with either new nonce counter
 * or a whole new nonce.
 *
 * @param connection The MHD connection structure
 * @param nonce A pointer that referenced a zero-terminated array of nonce
 * @param nc The nonce counter, zero to add the nonce to the array
 * @return MHD_YES if successful, MHD_NO if invalid (or we have no NC array)
 */
static int
check_nonce_nc (struct MHD_Connection *connection,
		const char *nonce,
		unsigned long int nc)
{
  uint32_t off;
  uint32_t mod;
  const char *np;

  mod = connection->daemon->nonce_nc_size;
  if (0 == mod)
    return MHD_NO; /* no array! */
  /* super-fast xor-based "hash" function for HT lookup in nonce array */
  off = 0;
  np = nonce;
  while ('\0' != *np)
    {
      off = (off << 8) | (*np ^ (off >> 24));
      np++;
    }
  off = off % mod;
  /*
   * Look for the nonce, if it does exist and its corresponding
   * nonce counter is less than the current nonce counter by 1,
   * then only increase the nonce counter by one.
   */
  
  pthread_mutex_lock (&connection->daemon->nnc_lock);
  if (0 == nc)
    {
      strcpy(connection->daemon->nnc[off].nonce, 
	     nonce);
      connection->daemon->nnc[off].nc = 0;  
      pthread_mutex_unlock (&connection->daemon->nnc_lock);
      return MHD_YES;
    }
  if ( (nc <= connection->daemon->nnc[off].nc) ||
       (0 != strcmp(connection->daemon->nnc[off].nonce, nonce)) )
    {
      pthread_mutex_unlock (&connection->daemon->nnc_lock);
#if HAVE_MESSAGES
      MHD_DLOG (connection->daemon, 
		"Stale nonce received.  If this happens a lot, you should probably increase the size of the nonce array.\n");
#endif
      return MHD_NO;
    }
  connection->daemon->nnc[off].nc = nc;
  pthread_mutex_unlock (&connection->daemon->nnc_lock);
  return MHD_YES;
}


/**
 * Get the username from the authorization header sent by the client
 *
 * @param connection The MHD connection structure
 * @return NULL if no username could be found, a pointer
 * 			to the username if found
 */
char *
MHD_digest_auth_get_username(struct MHD_Connection *connection)
{
  size_t len;
  char user[MAX_USERNAME_LENGTH];
  const char *header;
  
  if (NULL == (header = MHD_lookup_connection_value (connection,
						     MHD_HEADER_KIND, 
						     MHD_HTTP_HEADER_AUTHORIZATION)))
    return NULL;
  if (0 != strncmp (header, _BASE, strlen (_BASE)))
    return NULL;
  header += strlen (_BASE);
  if (0 == (len = lookup_sub_value (user,
				    sizeof (user),
				    header, 
				    "username")))
    return NULL;
  return strdup (user);
}


/**
 * Calculate the server nonce so that it mitigates replay attacks
 * The current format of the nonce is ...
 * H(timestamp ":" method ":" random ":" uri ":" realm) + Hex(timestamp)
 *
 * @param nonce_time The amount of time in seconds for a nonce to be invalid
 * @param method HTTP method
 * @param rnd A pointer to a character array for the random seed
 * @param rnd_size The size of the random seed array
 * @param uri HTTP URI (in MHD, without the arguments ("?k=v")
 * @param realm A string of characters that describes the realm of auth.
 * @param nonce A pointer to a character array for the nonce to put in
 */
static void
calculate_nonce (uint32_t nonce_time,
		 const char *method,
		 const char *rnd,
		 unsigned int rnd_size,
		 const char *uri,
		 const char *realm,
		 char *nonce)
{
  struct MD5Context md5;
  unsigned char timestamp[4];
  unsigned char tmpnonce[MD5_DIGEST_SIZE];
  char timestamphex[sizeof(timestamp) * 2 + 1];

  MD5Init (&md5);
  timestamp[0] = (nonce_time & 0xff000000) >> 0x18;
  timestamp[1] = (nonce_time & 0x00ff0000) >> 0x10;
  timestamp[2] = (nonce_time & 0x0000ff00) >> 0x08;
  timestamp[3] = (nonce_time & 0x000000ff);    
  MD5Update (&md5, timestamp, 4);
  MD5Update (&md5, ":", 1);
  MD5Update (&md5, method, strlen(method));
  MD5Update (&md5, ":", 1);
  if (rnd_size > 0)
    MD5Update (&md5, rnd, rnd_size);
  MD5Update (&md5, ":", 1);
  MD5Update (&md5, uri, strlen(uri));
  MD5Update (&md5, ":", 1);
  MD5Update (&md5, realm, strlen(realm));
  MD5Final (tmpnonce, &md5);  
  cvthex (tmpnonce, sizeof (tmpnonce), nonce);  
  cvthex (timestamp, 4, timestamphex);
  strncat (nonce, timestamphex, 8);
}


/**
 * Test if the given key-value pair is in the headers for the
 * given connection.
 *
 * @param connection the connection
 * @param key the key
 * @param value the value, can be NULL
 * @return MHD_YES if the key-value pair is in the headers, 
 *         MHD_NO if not
 */
static int
test_header (struct MHD_Connection *connection,
	     const char *key,
	     const char *value)
{
  struct MHD_HTTP_Header *pos;

  for (pos = connection->headers_received; NULL != pos; pos = pos->next)
    {
      if (MHD_GET_ARGUMENT_KIND != pos->kind)
	continue;
      if (0 != strcmp (key, pos->header))
	continue;
      if ( (NULL == value) && 
	   (NULL == pos->value) )
	return MHD_YES;
      if ( (NULL == value) || 
	   (NULL == pos->value) ||
	   (0 != strcmp (value, pos->value)) )
	continue;
      return MHD_YES;      
    }
  return MHD_NO;
}


/**
 * Check that the arguments given by the client as part
 * of the authentication header match the arguments we
 * got as part of the HTTP request URI.
 *
 * @param connection connections with headers to compare against
 * @param args argument URI string (after "?" in URI)
 * @return MHD_YES if the arguments match,
 *         MHD_NO if not
 */
static int
check_argument_match (struct MHD_Connection *connection,
		      const char *args)
{
  struct MHD_HTTP_Header *pos;
  size_t slen = strlen (args) + 1;
  char argb[slen];
  char *argp;
  char *equals;
  char *amper;
  unsigned int num_headers;

  num_headers = 0;
  memcpy (argb, args, slen);
  argp = argb;
  while ( (NULL != argp) &&
	  ('\0' != argp[0]) )
    {
      equals = strchr (argp, '=');
      if (NULL == equals) 
	{	  
	  /* add with 'value' NULL */
	  connection->daemon->unescape_callback (connection->daemon->unescape_callback_cls,
						 connection,
						 argp);
	  if (MHD_YES != test_header (connection, argp, NULL))
	    return MHD_NO;
	  num_headers++;
	  break;
	}
      equals[0] = '\0';
      equals++;
      amper = strchr (equals, '&');
      if (NULL != amper)
	{
	  amper[0] = '\0';
	  amper++;
	}
      connection->daemon->unescape_callback (connection->daemon->unescape_callback_cls,
					     connection,
					     argp);
      connection->daemon->unescape_callback (connection->daemon->unescape_callback_cls,
					     connection,
					     equals);
      if (! test_header (connection, argp, equals))
	return MHD_NO;
      num_headers++;
      argp = amper;
    }
  
  /* also check that the number of headers matches */
  for (pos = connection->headers_received; NULL != pos; pos = pos->next)
    {
      if (MHD_GET_ARGUMENT_KIND != pos->kind)
	continue;
      num_headers--;
    }
  if (0 != num_headers)  
    return MHD_NO;
  return MHD_YES;
}


/**
 * Authenticates the authorization header sent by the client
 *
 * @param connection The MHD connection structure
 * @param realm The realm presented to the client
 * @param username The username needs to be authenticated
 * @param password The password used in the authentication
 * @param nonce_timeout The amount of time for a nonce to be
 * 			invalid in seconds
 * @return MHD_YES if authenticated, MHD_NO if not,
 * 			MHD_INVALID_NONCE if nonce is invalid
 */
int
MHD_digest_auth_check (struct MHD_Connection *connection,
		       const char *realm,
		       const char *username,
		       const char *password,
		       unsigned int nonce_timeout)
{
  size_t len;
  const char *header;
  char *end;
  char nonce[MAX_NONCE_LENGTH];
  char cnonce[MAX_NONCE_LENGTH];
  char qop[15]; /* auth,auth-int */
  char nc[20];
  char response[MAX_AUTH_RESPONSE_LENGTH];
  const char *hentity = NULL; /* "auth-int" is not supported */
  char ha1[HASH_MD5_HEX_LEN + 1];
  char respexp[HASH_MD5_HEX_LEN + 1];
  char noncehashexp[HASH_MD5_HEX_LEN + 9];
  uint32_t nonce_time;
  uint32_t t;
  size_t left; /* number of characters left in 'header' for 'uri' */
  unsigned long int nci;

  header = MHD_lookup_connection_value (connection,
					MHD_HEADER_KIND,
					MHD_HTTP_HEADER_AUTHORIZATION);  
  if (NULL == header) 
    return MHD_NO;
  if (0 != strncmp(header, _BASE, strlen(_BASE))) 
    return MHD_NO;
  header += strlen (_BASE);
  left = strlen (header);

  {
    char un[MAX_USERNAME_LENGTH];

    len = lookup_sub_value (un,
			    sizeof (un),
			    header, "username");
    if ( (0 == len) ||
	 (0 != strcmp(username, un)) ) 
      return MHD_NO;
    left -= strlen ("username") + len;
  }

  {
    char r[MAX_REALM_LENGTH];

    len = lookup_sub_value(r, 
			   sizeof (r),
			   header, "realm");  
    if ( (0 == len) || 
	 (0 != strcmp(realm, r)) )
      return MHD_NO;
    left -= strlen ("realm") + len;
  }

  if (0 == (len = lookup_sub_value (nonce, 
				    sizeof (nonce),
				    header, "nonce")))
    return MHD_NO;
  left -= strlen ("nonce") + len;

  {
    char uri[left];  
  
    if (0 == lookup_sub_value(uri,
			      sizeof (uri),
			      header, "uri")) 
      return MHD_NO;
      
    /* 8 = 4 hexadecimal numbers for the timestamp */  
    nonce_time = strtoul(nonce + len - 8, (char **)NULL, 16);  
    t = (uint32_t) MHD_monotonic_time();    
    /*
     * First level vetting for the nonce validity if the timestamp
     * attached to the nonce exceeds `nonce_timeout' then the nonce is
     * invalid.
     */
    if ( (t > nonce_time + nonce_timeout) ||
	 (nonce_time + nonce_timeout < nonce_time) )
      return MHD_INVALID_NONCE;
    if (0 != strncmp (uri,
		      connection->url,
		      strlen (connection->url)))
    {
#if HAVE_MESSAGES
      MHD_DLOG (connection->daemon, 
		"Authentication failed, URI does not match.\n");
#endif
      return MHD_NO;
    }
    {
      const char *args = strchr (uri, '?');

      if (NULL == args)
	args = "";
      else
	args++;
      if (MHD_YES !=
	  check_argument_match (connection,
				args) ) 
      {
#if HAVE_MESSAGES
	MHD_DLOG (connection->daemon, 
		  "Authentication failed, arguments do not match.\n");
#endif
	return MHD_NO;
      }
    }
    calculate_nonce (nonce_time,
		     connection->method,
		     connection->daemon->digest_auth_random,
		     connection->daemon->digest_auth_rand_size,
		     connection->url,
		     realm,
		     noncehashexp);
    /*
     * Second level vetting for the nonce validity
     * if the timestamp attached to the nonce is valid
     * and possibly fabricated (in case of an attack)
     * the attacker must also know the random seed to be
     * able to generate a "sane" nonce, which if he does
     * not, the nonce fabrication process going to be
     * very hard to achieve.
     */
    
    if (0 != strcmp (nonce, noncehashexp))
      return MHD_INVALID_NONCE;
    if ( (0 == lookup_sub_value (cnonce,
				 sizeof (cnonce), 
				 header, "cnonce")) ||
	 (0 == lookup_sub_value (qop, sizeof (qop), header, "qop")) ||
	 ( (0 != strcmp (qop, "auth")) && 
	   (0 != strcmp (qop, "")) ) ||
	 (0 == lookup_sub_value (nc, sizeof (nc), header, "nc"))  ||
	 (0 == lookup_sub_value (response, sizeof (response), header, "response")) )
    {
#if HAVE_MESSAGES
      MHD_DLOG (connection->daemon, 
		"Authentication failed, invalid format.\n");
#endif
      return MHD_NO;
    }
    nci = strtoul (nc, &end, 16);
    if ( ('\0' != *end) ||
	 ( (LONG_MAX == nci) && 
	   (ERANGE == errno) ) )
    {
#if HAVE_MESSAGES
      MHD_DLOG (connection->daemon, 
		"Authentication failed, invalid format.\n");
#endif
      return MHD_NO; /* invalid nonce format */
    }
    /*
     * Checking if that combination of nonce and nc is sound
     * and not a replay attack attempt. Also adds the nonce
     * to the nonce-nc map if it does not exist there.
     */
    
    if (MHD_YES != check_nonce_nc (connection, nonce, nci))
      return MHD_NO;
    
    digest_calc_ha1("md5",
		    username,
		    realm,
		    password,
		    nonce,
		    cnonce,
		    ha1);
    digest_calc_response (ha1,
			  nonce,
			  nc,
			  cnonce,
			  qop,
			  connection->method,
			  uri,
			  hentity,
			  respexp);  
    return (0 == strcmp(response, respexp)) 
      ? MHD_YES 
      : MHD_NO;
  }
}


/**
 * Queues a response to request authentication from the client
 *
 * @param connection The MHD connection structure
 * @param realm the realm presented to the client
 * @param opaque string to user for opaque value
 * @param signal_stale MHD_YES if the nonce is invalid to add
 * 			'stale=true' to the authentication header
 * @return MHD_YES on success, MHD_NO otherwise
 */
int
MHD_queue_auth_fail_response (struct MHD_Connection *connection,
			      const char *realm,
			      const char *opaque,
			      struct MHD_Response *response,
			      int signal_stale)
{
  int ret;
  size_t hlen;
  char nonce[HASH_MD5_HEX_LEN + 9];

  /* Generating the server nonce */  
  calculate_nonce ((uint32_t) MHD_monotonic_time(),
		   connection->method,
		   connection->daemon->digest_auth_random,
		   connection->daemon->digest_auth_rand_size,
		   connection->url,
		   realm,
		   nonce);
  if (MHD_YES != check_nonce_nc (connection, nonce, 0))
    {
#if HAVE_MESSAGES
      MHD_DLOG (connection->daemon, 
		"Could not register nonce (is the nonce array size zero?).\n");
#endif
      return MHD_NO;  
    }
  /* Building the authentication header */
  hlen = snprintf (NULL,
		   0,
		   "Digest realm=\"%s\",qop=\"auth\",nonce=\"%s\",opaque=\"%s\"%s",
		   realm, 
		   nonce,
		   opaque,
		   signal_stale 
		   ? ",stale=\"true\"" 
		   : "");
  {
    char header[hlen + 1];

    snprintf (header,
	      sizeof(header),
	      "Digest realm=\"%s\",qop=\"auth\",nonce=\"%s\",opaque=\"%s\"%s",
	      realm, 
	      nonce,
	      opaque,
	      signal_stale 
	      ? ",stale=\"true\"" 
	      : "");
    ret = MHD_add_response_header(response,
				  MHD_HTTP_HEADER_WWW_AUTHENTICATE, 
				  header);
  }
  if (MHD_YES == ret) 
    ret = MHD_queue_response(connection, 
			     MHD_HTTP_UNAUTHORIZED, 
			     response);  
  return ret;
}


/* end of digestauth.c */
