/* DO NOT CHANGE THIS LINE */
/*
     This file is part of libmicrohttpd
     (C) 2007, 2009 Christian Grothoff

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
 * @file daemontest_get_response_cleanup.c
 * @brief  Testcase for libmicrohttpd response cleanup
 * @author Christian Grothoff
 */

#include "MHD_config.h"
#include "platform.h"
#include <microhttpd.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#ifndef WINDOWS
#include <sys/socket.h>
#include <unistd.h>
#endif

#define TESTSTR "/* DO NOT CHANGE THIS LINE */"

static int oneone;

static int ok;


static pid_t
fork_curl (const char *url)
{
  pid_t ret;

  ret = fork();
  if (ret != 0)
    return ret;
  execlp ("curl", "curl", "-s", "-N", "-o", "/dev/null", "-GET", url, NULL);
  fprintf (stderr, 
	   "Failed to exec curl: %s\n",
	   strerror (errno));
  _exit (-1);  
}

static void
kill_curl (pid_t pid)
{
  int status;

  //fprintf (stderr, "Killing curl\n");
  kill (pid, SIGTERM);
  waitpid (pid, &status, 0);
}


static ssize_t
push_callback (void *cls, uint64_t pos, char *buf, size_t max)
{
  if (max == 0)
    return 0;
  buf[0] = 'd';
  return 1;
}

static void
push_free_callback (void *cls)
{
  int *ok = cls;

  //fprintf (stderr, "Cleanup callback called!\n");
  *ok = 0;
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
  static int ptr;
  const char *me = cls;
  struct MHD_Response *response;
  int ret;

  //fprintf (stderr, "In CB: %s!\n", method);
  if (0 != strcmp (me, method))
    return MHD_NO;              /* unexpected method */
  if (&ptr != *unused)
    {
      *unused = &ptr;
      return MHD_YES;
    }
  *unused = NULL;
  response = MHD_create_response_from_callback (MHD_SIZE_UNKNOWN, 
						32 * 1024,
						&push_callback,
						&ok,
						&push_free_callback);
  ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
  MHD_destroy_response (response);
  if (ret == MHD_NO)
    abort ();
  return ret;
}


static int
testInternalGet ()
{
  struct MHD_Daemon *d;
  pid_t curl;

  ok = 1;
  d = MHD_start_daemon (MHD_USE_SELECT_INTERNALLY | MHD_USE_DEBUG,
                        11080, NULL, NULL, &ahc_echo, "GET", MHD_OPTION_END);
  if (d == NULL)
    return 1;
  curl = fork_curl ("http://127.0.0.1:11080/");
  sleep (1);
  kill_curl (curl);
  sleep (1);
  // fprintf (stderr, "Stopping daemon!\n");
  MHD_stop_daemon (d);
  if (ok != 0)
    return 2;
  return 0;
}

static int
testMultithreadedGet ()
{
  struct MHD_Daemon *d;
  pid_t curl;

  d = MHD_start_daemon (MHD_USE_THREAD_PER_CONNECTION | MHD_USE_DEBUG,
                        1081, NULL, NULL, &ahc_echo, "GET",
			MHD_OPTION_CONNECTION_TIMEOUT, (unsigned int) 2,
			MHD_OPTION_END);
  if (d == NULL)
    return 16;
  ok = 1;
  //fprintf (stderr, "Forking cURL!\n");
  curl = fork_curl ("http://127.0.0.1:1081/");
  sleep (1);
  kill_curl (curl);
  sleep (1);
  curl = fork_curl ("http://127.0.0.1:1081/");
  sleep (1);
  if (ok != 0)
    {
      kill_curl (curl);
      MHD_stop_daemon (d);
      return 64;
    }
  kill_curl (curl);
  sleep (1);
  //fprintf (stderr, "Stopping daemon!\n");
  MHD_stop_daemon (d);
  if (ok != 0)
    return 32;

  return 0;
}

static int
testMultithreadedPoolGet ()
{
  struct MHD_Daemon *d;
  pid_t curl;

  d = MHD_start_daemon (MHD_USE_SELECT_INTERNALLY | MHD_USE_DEBUG,
                        1081, NULL, NULL, &ahc_echo, "GET",
                        MHD_OPTION_THREAD_POOL_SIZE, 4, MHD_OPTION_END);
  if (d == NULL)
    return 64;
  ok = 1;
  curl = fork_curl ("http://127.0.0.1:1081/");
  sleep (1);
  kill_curl (curl);
  sleep (1);
  //fprintf (stderr, "Stopping daemon!\n");
  MHD_stop_daemon (d);
  if (ok != 0)
    return 128;
  return 0;
}

static int
testExternalGet ()
{
  struct MHD_Daemon *d;
  fd_set rs;
  fd_set ws;
  fd_set es;
  int max;
  time_t start;
  struct timeval tv;
  pid_t curl;

  d = MHD_start_daemon (MHD_USE_DEBUG,
                        1082, NULL, NULL, &ahc_echo, "GET", MHD_OPTION_END);
  if (d == NULL)
    return 256;
  curl = fork_curl ("http://127.0.0.1:1082/");
  
  start = time (NULL);
  while ((time (NULL) - start < 2))
    {
      max = 0;
      FD_ZERO (&rs);
      FD_ZERO (&ws);
      FD_ZERO (&es);
      if (MHD_YES != MHD_get_fdset (d, &rs, &ws, &es, &max))
        {
          MHD_stop_daemon (d);
          return 4096;
        }
      tv.tv_sec = 0;
      tv.tv_usec = 1000;
      select (max + 1, &rs, &ws, &es, &tv);
      MHD_run (d);
    }
  kill_curl (curl);
  start = time (NULL);
  while ((time (NULL) - start < 2))
    {
      max = 0;
      FD_ZERO (&rs);
      FD_ZERO (&ws);
      FD_ZERO (&es);
      if (MHD_YES != MHD_get_fdset (d, &rs, &ws, &es, &max))
        {
          MHD_stop_daemon (d);
          return 4096;
        }
      tv.tv_sec = 0;
      tv.tv_usec = 1000;
      select (max + 1, &rs, &ws, &es, &tv);
      MHD_run (d);
    }
  // fprintf (stderr, "Stopping daemon!\n");
  MHD_stop_daemon (d);
  if (ok != 0)
    return 1024;
  return 0;
}


int
main (int argc, char *const *argv)
{
  unsigned int errorCount = 0;

  oneone = NULL != strstr (argv[0], "11");
  errorCount += testInternalGet ();
  errorCount += testMultithreadedGet ();
  errorCount += testMultithreadedPoolGet ();
  errorCount += testExternalGet ();
  if (errorCount != 0)
    fprintf (stderr, "Error (code: %u)\n", errorCount);
  return errorCount != 0;       /* 0 == pass */
}
