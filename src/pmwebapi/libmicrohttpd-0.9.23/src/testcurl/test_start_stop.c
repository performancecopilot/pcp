/*
     This file is part of libmicrohttpd
     (C) 2011 Christian Grothoff

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
 * @file test_start_stop.c
 * @brief  test for #1901 (start+stop)
 * @author Christian Grothoff
 */
#include "MHD_config.h"
#include "platform.h"
#include <curl/curl.h>
#include <microhttpd.h>


static int
ahc_echo (void *cls,
          struct MHD_Connection *connection,
          const char *url,
          const char *method,
          const char *version,
          const char *upload_data, size_t *upload_data_size,
          void **unused)
{
  return MHD_NO;
}


static int
testInternalGet (int poll_flag)
{
  struct MHD_Daemon *d;

  d = MHD_start_daemon (MHD_USE_SELECT_INTERNALLY | MHD_USE_DEBUG | poll_flag,
                        11080, NULL, NULL, &ahc_echo, "GET", MHD_OPTION_END);
  if (d == NULL)
    return 1;
  MHD_stop_daemon (d);
  return 0;
}

static int
testMultithreadedGet (int poll_flag)
{
  struct MHD_Daemon *d;

  d = MHD_start_daemon (MHD_USE_THREAD_PER_CONNECTION | MHD_USE_DEBUG  | poll_flag,
                        1081, NULL, NULL, &ahc_echo, "GET", MHD_OPTION_END);
  if (d == NULL)
    return 2;
  MHD_stop_daemon (d);
  return 0;
}

static int
testMultithreadedPoolGet (int poll_flag)
{
  struct MHD_Daemon *d;

  d = MHD_start_daemon (MHD_USE_SELECT_INTERNALLY | MHD_USE_DEBUG | poll_flag,
                        1081, NULL, NULL, &ahc_echo, "GET",
                        MHD_OPTION_THREAD_POOL_SIZE, 4, MHD_OPTION_END);
  if (d == NULL)
    return 4;
  MHD_stop_daemon (d);
  return 0;
}

static int
testExternalGet ()
{
  struct MHD_Daemon *d;

  d = MHD_start_daemon (MHD_USE_DEBUG,
                        1082, NULL, NULL, &ahc_echo, "GET", MHD_OPTION_END);
  if (d == NULL)
    return 8;
  MHD_stop_daemon (d);
  return 0;
}


int
main (int argc, char *const *argv)
{
  unsigned int errorCount = 0;

  errorCount += testInternalGet (0);
  errorCount += testMultithreadedGet (0);
  errorCount += testMultithreadedPoolGet (0);
  errorCount += testExternalGet ();
#ifndef WINDOWS
  errorCount += testInternalGet (MHD_USE_POLL);
  errorCount += testMultithreadedGet (MHD_USE_POLL);
  errorCount += testMultithreadedPoolGet (MHD_USE_POLL);
#endif
  if (errorCount != 0)
    fprintf (stderr, "Error (code: %u)\n", errorCount);
  return errorCount != 0;       /* 0 == pass */
}
