/*
  This file is part of libmicrohttpd
  (C) 2007 Christian Grothoff

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
 * @file tls_thread_mode_test.c
 * @brief  Testcase for libmicrohttpd HTTPS GET operations
 * @author Sagie Amir
 * @author Christian Grothoff
 *
 * TODO: add test for external select!
 */

#include "platform.h"
#include "microhttpd.h"
#include <sys/stat.h>
#include <limits.h>
#include <curl/curl.h>
#include "tls_test_common.h"

extern const char srv_key_pem[];
extern const char srv_self_signed_cert_pem[];

int curl_check_version (const char *req_version, ...);

/**
 * used when spawning multiple threads executing curl server requests
 *
 */
static void *
https_transfer_thread_adapter (void *args)
{
  static int nonnull;
  struct https_test_data *cargs = args;
  int ret;

  /* time spread incomming requests */
  usleep ((useconds_t) 10.0 * ((double) rand ()) / ((double) RAND_MAX));
  ret = test_https_transfer (cargs->cls,
                             cargs->cipher_suite, cargs->proto_version);
  if (ret == 0)
    return NULL;
  return &nonnull;
}

/**
 * Test non-parallel requests.
 *
 * @return: 0 upon all client requests returning '0', -1 otherwise.
 *
 * TODO : make client_count a parameter - numver of curl client threads to spawn
 */
static int
test_single_client (void *cls, const char *cipher_suite,
                    int curl_proto_version)
{
  void *client_thread_ret;
  struct https_test_data client_args =
    { NULL, cipher_suite, curl_proto_version };

  client_thread_ret = https_transfer_thread_adapter (&client_args);
  if (client_thread_ret != NULL)
    return -1;
  return 0;
}


/**
 * Test parallel request handling.
 *
 * @return: 0 upon all client requests returning '0', -1 otherwise.
 *
 * TODO : make client_count a parameter - numver of curl client threads to spawn
 */
static int
test_parallel_clients (void *cls, const char *cipher_suite,
                       int curl_proto_version)
{
  int i;
  int client_count = 3;
  void *client_thread_ret;
  pthread_t client_arr[client_count];
  struct https_test_data client_args =
    { NULL, cipher_suite, curl_proto_version };

  for (i = 0; i < client_count; ++i)
    {
      if (pthread_create (&client_arr[i], NULL,
                          &https_transfer_thread_adapter, &client_args) != 0)
        {
          fprintf (stderr, "Error: failed to spawn test client threads.\n");

          return -1;
        }
    }

  /* check all client requests fulfilled correctly */
  for (i = 0; i < client_count; ++i)
    {
      if ((pthread_join (client_arr[i], &client_thread_ret) != 0) ||
          (client_thread_ret != NULL))
        return -1;
    }

  return 0;
}


int
main (int argc, char *const *argv)
{
  unsigned int errorCount = 0;
  const char *ssl_version;

  /* initialize random seed used by curl clients */
  unsigned int iseed = (unsigned int) time (NULL);
  srand (iseed);
  ssl_version = curl_version_info (CURLVERSION_NOW)->ssl_version;
  if (NULL == ssl_version)
  {
    fprintf (stderr, "Curl does not support SSL.  Cannot run the test.\n");
    return 0;
  }
  if (NULL != strcasestr (ssl_version, "openssl"))
  {
    fprintf (stderr, "Refusing to run test with OpenSSL.  Please install libcurl-gnutls\n");
    return 0;
  }
  if (0 != curl_global_init (CURL_GLOBAL_ALL))
    {
      fprintf (stderr, "Error: %s\n", strerror (errno));
      return -1;
    }

  char *aes256_sha = "AES256-SHA";
  if (curl_uses_nss_ssl() == 0)
    {
      aes256_sha = "rsa_aes_256_sha";
    }

  errorCount +=
    test_wrap ("multi threaded daemon, single client", &test_single_client,
               NULL,
               MHD_USE_SSL | MHD_USE_DEBUG | MHD_USE_THREAD_PER_CONNECTION,
               aes256_sha, CURL_SSLVERSION_TLSv1, MHD_OPTION_HTTPS_MEM_KEY,
               srv_key_pem, MHD_OPTION_HTTPS_MEM_CERT,
               srv_self_signed_cert_pem, MHD_OPTION_END);

  errorCount +=
    test_wrap ("multi threaded daemon, parallel client",
               &test_parallel_clients, NULL,
               MHD_USE_SSL | MHD_USE_DEBUG | MHD_USE_THREAD_PER_CONNECTION,
               aes256_sha, CURL_SSLVERSION_TLSv1, MHD_OPTION_HTTPS_MEM_KEY,
               srv_key_pem, MHD_OPTION_HTTPS_MEM_CERT,
               srv_self_signed_cert_pem, MHD_OPTION_END);

  if (errorCount != 0)
    fprintf (stderr, "Failed test: %s.\n", argv[0]);

  curl_global_cleanup ();
  return errorCount != 0;
}
