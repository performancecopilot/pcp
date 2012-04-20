/*
     This file is part of libmicrohttpd
     (C) 2007, 2009, 2011 Christian Grothoff

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
 * @file perf_get_concurrent.c
 * @brief benchmark concurrent GET operations
 *        Note that we run libcurl on the machine at the
 *        same time, so the execution time may be influenced
 *        by the concurrent activity; it is quite possible
 *        that more time is spend with libcurl than with MHD,
 *        so the performance scores calculated with this code
 *        should NOT be used to compare with other HTTP servers
 *        (since MHD is actually better); only the relative
 *        scores between MHD versions are meaningful.  
 * @author Christian Grothoff
 */

#include "MHD_config.h"
#include "platform.h"
#include <curl/curl.h>
#include <microhttpd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "gauger.h"

/**
 * How many rounds of operations do we do for each
 * test (total number of requests will be ROUNDS * PAR).
 */
#define ROUNDS 500

/**
 * How many requests do we do in parallel?
 */
#define PAR 4

/**
 * Do we use HTTP 1.1?
 */
static int oneone;

/**
 * Response to return (re-used).
 */
static struct MHD_Response *response;

/**
 * Time this round was started.
 */
static unsigned long long start_time;


/**
 * Get the current timestamp 
 *
 * @return current time in ms
 */
static unsigned long long 
now ()
{
  struct timeval tv;

  GETTIMEOFDAY (&tv, NULL);
  return (((unsigned long long) tv.tv_sec * 1000LL) +
	  ((unsigned long long) tv.tv_usec / 1000LL));
}


/**
 * Start the timer.
 */
static void 
start_timer()
{
  start_time = now ();
}


/**
 * Stop the timer and report performance
 *
 * @param desc description of the threading mode we used
 */
static void 
stop (const char *desc)
{
  double rps = ((double) (PAR * ROUNDS * 1000)) / ((double) (now() - start_time));

  fprintf (stderr,
	   "Parallel GETs using %s: %f %s\n",
	   desc,
	   rps,
	   "requests/s");
  GAUGER (desc,
	  "Parallel GETs",
	  rps,
	  "requests/s");
}


static size_t
copyBuffer (void *ptr, 
	    size_t size, size_t nmemb, 
	    void *ctx)
{
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
  static int ptr;
  const char *me = cls;
  int ret;

  if (0 != strcmp (me, method))
    return MHD_NO;              /* unexpected method */
  if (&ptr != *unused)
    {
      *unused = &ptr;
      return MHD_YES;
    }
  *unused = NULL;
  ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
  if (ret == MHD_NO)
    abort ();
  return ret;
}


static pid_t
do_gets (int port)
{
  pid_t ret;
  CURL *c;
  CURLcode errornum;
  unsigned int i;
  unsigned int j;
  pid_t par[PAR];
  char url[64];

  sprintf(url, "http://127.0.0.1:%d/hello_world", port);
  
  ret = fork ();
  if (ret == -1) abort ();
  if (ret != 0)
    return ret;
  for (j=0;j<PAR;j++)
    {
      par[j] = fork ();
      if (par[j] == 0)
	{
	  for (i=0;i<ROUNDS;i++)
	    {
	      c = curl_easy_init ();
	      curl_easy_setopt (c, CURLOPT_URL, url);
	      curl_easy_setopt (c, CURLOPT_WRITEFUNCTION, &copyBuffer);
	      curl_easy_setopt (c, CURLOPT_WRITEDATA, NULL);
	      curl_easy_setopt (c, CURLOPT_FAILONERROR, 1);
	      curl_easy_setopt (c, CURLOPT_TIMEOUT, 150L);
	      if (oneone)
		curl_easy_setopt (c, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
	      else
		curl_easy_setopt (c, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
	      curl_easy_setopt (c, CURLOPT_CONNECTTIMEOUT, 150L);
	      /* NOTE: use of CONNECTTIMEOUT without also
		 setting NOSIGNAL results in really weird
		 crashes on my system! */
	      curl_easy_setopt (c, CURLOPT_NOSIGNAL, 1);
	      if (CURLE_OK != (errornum = curl_easy_perform (c)))
		{
		  fprintf (stderr,
			   "curl_easy_perform failed: `%s'\n",
			   curl_easy_strerror (errornum));
		  curl_easy_cleanup (c);
		  _exit (1);
		}
	      curl_easy_cleanup (c);
	    }
	  _exit (0);
	}
    }
  for (j=0;j<PAR;j++)
    waitpid (par[j], NULL, 0);
  _exit (0);
}


static void 
join_gets (pid_t pid)
{
  int status;
  
  status = 1;
  waitpid (pid, &status, 0);
  if (0 != status)
    abort ();
}


static int
testInternalGet (int port, int poll_flag)
{
  struct MHD_Daemon *d;

  d = MHD_start_daemon (MHD_USE_SELECT_INTERNALLY | MHD_USE_DEBUG  | poll_flag,
                        port, NULL, NULL, &ahc_echo, "GET", MHD_OPTION_END);
  if (d == NULL)
    return 1;
  start_timer ();
  join_gets (do_gets (port));
  stop (poll_flag ? "internal poll" : "internal select");
  MHD_stop_daemon (d);
  return 0;
}


static int
testMultithreadedGet (int port, int poll_flag)
{
  struct MHD_Daemon *d;

  d = MHD_start_daemon (MHD_USE_THREAD_PER_CONNECTION | MHD_USE_DEBUG  | poll_flag,
                        port, NULL, NULL, &ahc_echo, "GET", MHD_OPTION_END);
  if (d == NULL)
    return 16;
  start_timer ();
  join_gets (do_gets (port));
  stop (poll_flag ? "thread with poll" : "thread with select");
  MHD_stop_daemon (d);
  return 0;
}

static int
testMultithreadedPoolGet (int port, int poll_flag)
{
  struct MHD_Daemon *d;

  d = MHD_start_daemon (MHD_USE_SELECT_INTERNALLY | MHD_USE_DEBUG | poll_flag,
                        port, NULL, NULL, &ahc_echo, "GET",
                        MHD_OPTION_THREAD_POOL_SIZE, 4, MHD_OPTION_END);
  if (d == NULL)
    return 16;
  start_timer ();
  join_gets (do_gets (port));
  stop (poll_flag ? "thread pool with poll" : "thread pool with select");
  MHD_stop_daemon (d);
  return 0;
}

static int
testExternalGet (int port)
{
  struct MHD_Daemon *d;
  pid_t pid;
  fd_set rs;
  fd_set ws;
  fd_set es;
  int max;
  struct timeval tv;
  MHD_UNSIGNED_LONG_LONG tt;
  int tret;

  d = MHD_start_daemon (MHD_USE_DEBUG,
                        port, NULL, NULL, &ahc_echo, "GET", MHD_OPTION_END);
  if (d == NULL)
    return 256;
  start_timer ();
  pid = do_gets (port);
  while (0 == waitpid (pid, NULL, WNOHANG))
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
      tret = MHD_get_timeout (d, &tt);
      if (MHD_YES != tret) tt = 1;
      tv.tv_sec = tt / 1000;
      tv.tv_usec = 1000 * (tt % 1000);
      select (max + 1, &rs, &ws, &es, &tv);
      MHD_run (d);
    }
  stop ("external select");
  MHD_stop_daemon (d);
  return 0;
}


int
main (int argc, char *const *argv)
{
  unsigned int errorCount = 0;
  int port = 1081;

  oneone = NULL != strstr (argv[0], "11");
  if (0 != curl_global_init (CURL_GLOBAL_WIN32))
    return 2;
  response = MHD_create_response_from_buffer (strlen ("/hello_world"),
					      "/hello_world",
					      MHD_RESPMEM_MUST_COPY);
  errorCount += testInternalGet (port++, 0);
  errorCount += testMultithreadedGet (port++, 0);
  errorCount += testMultithreadedPoolGet (port++, 0);
  errorCount += testExternalGet (port++);
#ifndef WINDOWS
  errorCount += testInternalGet (port++, MHD_USE_POLL);
  errorCount += testMultithreadedGet (port++, MHD_USE_POLL);
  errorCount += testMultithreadedPoolGet (port++, MHD_USE_POLL);
#endif
  MHD_destroy_response (response);
  if (errorCount != 0)
    fprintf (stderr, "Error (code: %u)\n", errorCount);
  curl_global_cleanup ();
  return errorCount != 0;       /* 0 == pass */
}
