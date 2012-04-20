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
 * @file tls_authentication_test.c
 * @brief  Testcase for libmicrohttpd HTTPS GET operations
 * @author Sagie Amir
 */

#include "platform.h"
#include "microhttpd.h"
#include <curl/curl.h>
#include <limits.h>
#include <sys/stat.h>

#include "tls_test_common.h"

extern int curl_check_version (const char *req_version, ...);
extern const char test_file_data[];

extern const char ca_key_pem[];
extern const char ca_cert_pem[];
extern const char srv_signed_cert_pem[];
extern const char srv_signed_key_pem[];



/* perform a HTTP GET request via SSL/TLS */
static int
test_secure_get (void * cls, char *cipher_suite, int proto_version)
{
  int ret;
  struct MHD_Daemon *d;

  d = MHD_start_daemon (MHD_USE_THREAD_PER_CONNECTION | MHD_USE_SSL |
                        MHD_USE_DEBUG, DEAMON_TEST_PORT,
                        NULL, NULL, &http_ahc, NULL,
                        MHD_OPTION_HTTPS_MEM_KEY, srv_signed_key_pem,
                        MHD_OPTION_HTTPS_MEM_CERT, srv_signed_cert_pem,
                        MHD_OPTION_END);

  if (d == NULL)
    {
      fprintf (stderr, MHD_E_SERVER_INIT);
      return -1;
    }

  ret = test_daemon_get (NULL, cipher_suite, proto_version, DEAMON_TEST_PORT, 0);

  MHD_stop_daemon (d);
  return ret;
}


int
main (int argc, char *const *argv)
{
  unsigned int errorCount = 0;

  if (setup_ca_cert () == NULL)
    {
      fprintf (stderr, MHD_E_TEST_FILE_CREAT);
      return -1;
    }

  if (0 != curl_global_init (CURL_GLOBAL_ALL))
    {
      fprintf (stderr, "Error (code: %u)\n", errorCount);
      return -1;
    }

  char *aes256_sha = "AES256-SHA";
  if (curl_uses_nss_ssl() == 0)
    {
      aes256_sha = "rsa_aes_256_sha";
    }

  errorCount +=
    test_secure_get (NULL, aes256_sha, CURL_SSLVERSION_TLSv1);

  print_test_result (errorCount, argv[0]);

  curl_global_cleanup ();
  remove (ca_cert_file_name);
  return errorCount != 0;
}
