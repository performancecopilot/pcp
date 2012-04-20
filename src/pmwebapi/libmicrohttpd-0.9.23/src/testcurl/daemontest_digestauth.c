/*
     This file is part of libmicrohttpd
     (C) 2010 Christian Grothoff

     libmicrohttpd is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published
     by the Free Software Foundation; either version 2, or (at your
     option) any later version.

     libmicrohttpd is distributed in the hope that it will be useful, but
     WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
     General Public License for more details.

     You should have received a copy of the GNU General Public License
     along with libmicrohttpd; see the file COPYING.  If not, write to the
     Free Software Foundation, Inc., 59 Temple Place - Suite 330,
     Boston, MA 02111-1307, USA.
*/

/**
 * @file daemontest_digestauth.c
 * @brief  Testcase for libmicrohttpd Digest Auth
 * @author Amr Ali
 */

#include "MHD_config.h"
#include "platform.h"
#include <curl/curl.h>
#include <microhttpd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef WINDOWS
#include <sys/socket.h>
#include <unistd.h>
#else
#include <wincrypt.h>
#endif

#define PAGE "<html><head><title>libmicrohttpd demo</title></head><body>Access granted</body></html>"

#define DENIED "<html><head><title>libmicrohttpd demo</title></head><body>Access denied</body></html>"

#define OPAQUE "11733b200778ce33060f31c9af70a870ba96ddd4"

struct CBC
{
  char *buf;
  size_t pos;
  size_t size;
};

static size_t
copyBuffer (void *ptr, size_t size, size_t nmemb, void *ctx)
{
  struct CBC *cbc = ctx;

  if (cbc->pos + size * nmemb > cbc->size)
    return 0;                   /* overflow */
  memcpy (&cbc->buf[cbc->pos], ptr, size * nmemb);
  cbc->pos += size * nmemb;
  return size * nmemb;
}

static int
ahc_echo (void *cls,
          struct MHD_Connection *connection,
          const char *url,
          const char *method,
          const char *version,
          const char *upload_data, size_t *upload_data_size,
          void **unused)
{
  struct MHD_Response *response;
  char *username;
  const char *password = "testpass";
  const char *realm = "test@example.com";
  int ret;

  username = MHD_digest_auth_get_username(connection);
  if ( (username == NULL) ||
       (0 != strcmp (username, "testuser")) )
    {
      response = MHD_create_response_from_buffer(strlen (DENIED), 
						 DENIED,
						 MHD_RESPMEM_PERSISTENT);  
      ret = MHD_queue_auth_fail_response(connection, realm,
					 OPAQUE,
					 response,
					 MHD_NO);    
      MHD_destroy_response(response);  
      return ret;
    }
  ret = MHD_digest_auth_check(connection, realm,
			      username, 
			      password, 
			      300);
  free(username);
  if ( (ret == MHD_INVALID_NONCE) ||
       (ret == MHD_NO) )
    {
      response = MHD_create_response_from_buffer(strlen (DENIED), 
						 DENIED,
						 MHD_RESPMEM_PERSISTENT);  
      if (NULL == response) 
	return MHD_NO;
      ret = MHD_queue_auth_fail_response(connection, realm,
					 OPAQUE,
					 response,
					 (ret == MHD_INVALID_NONCE) ? MHD_YES : MHD_NO);  
      MHD_destroy_response(response);  
      return ret;
    }
  response = MHD_create_response_from_buffer(strlen(PAGE), PAGE,
					     MHD_RESPMEM_PERSISTENT);
  ret = MHD_queue_response(connection, MHD_HTTP_OK, response);  
  MHD_destroy_response(response);
  return ret;
}


static int
testDigestAuth ()
{
  int fd;
  CURL *c;
  CURLcode errornum;
  struct MHD_Daemon *d;
  struct CBC cbc;
  size_t len;
  size_t off = 0;
  char buf[2048];
  char rnd[8];

  cbc.buf = buf;
  cbc.size = 2048;
  cbc.pos = 0;
#ifndef WINDOWS
  fd = open("/dev/urandom", O_RDONLY);
  if (-1 == fd)
    {
	  fprintf(stderr, "Failed to open `%s': %s\n",
	       "/dev/urandom",
		   strerror(errno));
	  return 1;
	}
  while (off < 8)
	{
	  len = read(fd, rnd, 8);
	  if (len == -1)
	    {
		  fprintf(stderr, "Failed to read `%s': %s\n",
		       "/dev/urandom",
			   strerror(errno));
		  (void) close(fd);
		  return 1;
		}
	  off += len;
	}
  (void) close(fd);
#else
  {
    HCRYPTPROV cc;
    BOOL b;
    b = CryptAcquireContext (&cc, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);
    if (b == 0)
    {
      fprintf (stderr, "Failed to acquire crypto provider context: %lu\n",
          GetLastError ());
      return 1;
    }
    b = CryptGenRandom (cc, 8, rnd);
    if (b == 0)
    {
      fprintf (stderr, "Failed to generate 8 random bytes: %lu\n",
          GetLastError ());
    }
    CryptReleaseContext (cc, 0);
    if (b == 0)
      return 1;
  }
#endif
  d = MHD_start_daemon (MHD_USE_SELECT_INTERNALLY | MHD_USE_DEBUG,
                        1337, NULL, NULL, &ahc_echo, PAGE,
			MHD_OPTION_DIGEST_AUTH_RANDOM, sizeof (rnd), rnd,
			MHD_OPTION_NONCE_NC_SIZE, 300,
			MHD_OPTION_END);
  if (d == NULL)
    return 1;
  c = curl_easy_init ();
  curl_easy_setopt (c, CURLOPT_URL, "http://127.0.0.1:1337/");
  curl_easy_setopt (c, CURLOPT_WRITEFUNCTION, &copyBuffer);
  curl_easy_setopt (c, CURLOPT_WRITEDATA, &cbc);
  curl_easy_setopt (c, CURLOPT_HTTPAUTH, CURLAUTH_DIGEST);
  curl_easy_setopt (c, CURLOPT_USERPWD, "testuser:testpass");
  curl_easy_setopt (c, CURLOPT_FAILONERROR, 1);
  curl_easy_setopt (c, CURLOPT_TIMEOUT, 150L);
  curl_easy_setopt (c, CURLOPT_CONNECTTIMEOUT, 150L);
  curl_easy_setopt (c, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
  /* NOTE: use of CONNECTTIMEOUT without also
     setting NOSIGNAL results in really weird
     crashes on my system!*/
  curl_easy_setopt (c, CURLOPT_NOSIGNAL, 1);
  if (CURLE_OK != (errornum = curl_easy_perform (c)))
    {
      fprintf (stderr,
               "curl_easy_perform failed: `%s'\n",
               curl_easy_strerror (errornum));
      curl_easy_cleanup (c);
      MHD_stop_daemon (d);
      return 2;
    }
  curl_easy_cleanup (c);
  MHD_stop_daemon (d);
  if (cbc.pos != strlen (PAGE))
    return 4;
  if (0 != strncmp (PAGE, cbc.buf, strlen (PAGE)))
    return 8;
  return 0;
}

int
main (int argc, char *const *argv)
{
  unsigned int errorCount = 0;

  if (0 != curl_global_init (CURL_GLOBAL_WIN32))
    return 2;
  errorCount += testDigestAuth ();
  if (errorCount != 0)
    fprintf (stderr, "Error (code: %u)\n", errorCount);
  curl_global_cleanup ();
  return errorCount != 0;       /* 0 == pass */
}
