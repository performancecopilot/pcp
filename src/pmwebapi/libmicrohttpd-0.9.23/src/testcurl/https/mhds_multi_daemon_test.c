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
 * @file mhds_multi_daemon_test.c
 * @brief  Testcase for libmicrohttpd multiple HTTPS daemon scenario
 * @author Sagie Amir
 */

#include "platform.h"
#include "microhttpd.h"
#include <curl/curl.h>
#include <limits.h>
#include <sys/stat.h>

#include "tls_test_common.h"

extern int curl_check_version (const char *req_version, ...);
extern const char srv_key_pem[];
extern const char srv_self_signed_cert_pem[];

/*
 * assert initiating two separate daemons and having one shut down
 * doesn't affect the other
 */
int
test_concurent_daemon_pair (void * cls, char *cipher_suite,
                            int proto_version)
{

  int ret;
  struct MHD_Daemon *d1;
  struct MHD_Daemon *d2;
  d1 = MHD_start_daemon (MHD_USE_THREAD_PER_CONNECTION | MHD_USE_SSL |
                         MHD_USE_DEBUG, DEAMON_TEST_PORT,
                         NULL, NULL, &http_ahc, NULL,
                         MHD_OPTION_HTTPS_MEM_KEY, srv_key_pem,
                         MHD_OPTION_HTTPS_MEM_CERT, srv_self_signed_cert_pem,
                         MHD_OPTION_END);

  if (d1 == NULL)
    {
      fprintf (stderr, MHD_E_SERVER_INIT);
      return -1;
    }

  d2 = MHD_start_daemon (MHD_USE_THREAD_PER_CONNECTION | MHD_USE_SSL |
                         MHD_USE_DEBUG, DEAMON_TEST_PORT + 1,
                         NULL, NULL, &http_ahc, NULL,
                         MHD_OPTION_HTTPS_MEM_KEY, srv_key_pem,
                         MHD_OPTION_HTTPS_MEM_CERT, srv_self_signed_cert_pem,
                         MHD_OPTION_END);

  if (d2 == NULL)
    {
      MHD_stop_daemon (d1);
      fprintf (stderr, MHD_E_SERVER_INIT);
      return -1;
    }

  ret =
    test_daemon_get (NULL, cipher_suite, proto_version, DEAMON_TEST_PORT, 0);
  ret +=
    test_daemon_get (NULL, cipher_suite, proto_version,
                     DEAMON_TEST_PORT + 1, 0);

  MHD_stop_daemon (d2);
  ret +=
    test_daemon_get (NULL, cipher_suite, proto_version, DEAMON_TEST_PORT, 0);
  MHD_stop_daemon (d1);
  return ret;
}

int
main (int argc, char *const *argv)
{
  unsigned int errorCount = 0;
  FILE *cert;

  if (0 != curl_global_init (CURL_GLOBAL_ALL))
    {
      fprintf (stderr, "Error (code: %u). l:%d f:%s\n", errorCount, __LINE__,
               __FUNCTION__);
      return -1;
    }
  if ((cert = setup_ca_cert ()) == NULL)
    {
      fprintf (stderr, MHD_E_TEST_FILE_CREAT);
      return -1;
    }

  char *aes256_sha = "AES256-SHA";
  if (curl_uses_nss_ssl() == 0)
    {
      aes256_sha = "rsa_aes_256_sha";
    }
  
  errorCount +=
    test_concurent_daemon_pair (NULL, aes256_sha, CURL_SSLVERSION_SSLv3);

  print_test_result (errorCount, "concurent_daemon_pair");

  curl_global_cleanup ();
  fclose (cert);
  remove (ca_cert_file_name);
  return errorCount != 0;
}
