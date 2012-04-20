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
 * @file test_callback.c
 * @brief Testcase for MHD not calling the callback too often
 * @author Jan Seeger 
 * @author Christian Grothoff
 */


#include "MHD_config.h"
#include "platform.h"
#include <curl/curl.h>
#include <microhttpd.h>

struct callback_closure {
  unsigned int called;
};


static ssize_t 
called_twice(void *cls, uint64_t pos, char *buf, size_t max) 
{
  struct callback_closure *cls2 = cls;
  
  if (cls2->called == 0) 
    {
      memset(buf, 0, max);
      strcat(buf, "test");
      cls2->called = 1;
      return strlen(buf);
    }
  if (cls2->called == 1) 
    {
      cls2->called = 2;
      return MHD_CONTENT_READER_END_OF_STREAM;
    }
  fprintf(stderr, 
	  "Handler called after returning END_OF_STREAM!\n");
  return MHD_CONTENT_READER_END_WITH_ERROR;
}


static int
callback(void *cls, struct MHD_Connection *connection, const char *url,
	 const char *method, const char *version, const char *upload_data,
	 size_t *upload_data_size, void **con_cls) {
  struct callback_closure *cbc = calloc(1, sizeof(struct callback_closure));
  struct MHD_Response *r;

  r = MHD_create_response_from_callback (MHD_SIZE_UNKNOWN, 1024, 
					 &called_twice, cbc, 
					 &free);
  MHD_queue_response(connection, 200, r);
  MHD_destroy_response(r);
  return MHD_YES;
}


static size_t
discard_buffer (void *ptr, size_t size, size_t nmemb, void *ctx)
{
  return size * nmemb;
}


int main(int argc, char **argv) 
{
  struct MHD_Daemon *d;
  fd_set rs;
  fd_set ws;
  fd_set es;
  int max;
  CURL *c;
  CURLM *multi;
  CURLMcode mret;
  struct CURLMsg *msg;
  int running;
  struct timeval tv;
  int extra;

  d = MHD_start_daemon(0, 
		       8000,
		       NULL,
		       NULL,
		       callback,
		       NULL,
		       MHD_OPTION_END);
  c = curl_easy_init ();
  curl_easy_setopt (c, CURLOPT_URL, "http://127.0.0.1:8000/");
  curl_easy_setopt (c, CURLOPT_WRITEFUNCTION, &discard_buffer);
  curl_easy_setopt (c, CURLOPT_FAILONERROR, 1);
  curl_easy_setopt (c, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
  curl_easy_setopt (c, CURLOPT_TIMEOUT, 150L);
  curl_easy_setopt (c, CURLOPT_CONNECTTIMEOUT, 150L);
  curl_easy_setopt (c, CURLOPT_NOSIGNAL, 1);
  multi = curl_multi_init ();
  if (multi == NULL)
    {
      curl_easy_cleanup (c);
      MHD_stop_daemon (d);
      return 1;
    }
  mret = curl_multi_add_handle (multi, c);
  if (mret != CURLM_OK)
    {
      curl_multi_cleanup (multi);
      curl_easy_cleanup (c);
      MHD_stop_daemon (d);
      return 2;
    }
  extra = 10;
  while ( (c != NULL) || (--extra > 0) )
    {
      max = -1;
      FD_ZERO(&ws);
      FD_ZERO(&rs);
      FD_ZERO(&es);
      curl_multi_perform (multi, &running);
      if (NULL != multi)
	{
	  mret = curl_multi_fdset (multi, &rs, &ws, &es, &max);
	  if (mret != CURLM_OK)
	    {
	      curl_multi_remove_handle (multi, c);
	      curl_multi_cleanup (multi);
	      curl_easy_cleanup (c);
	      MHD_stop_daemon (d);
	      return 3;
	    }   
	}
      if (MHD_YES !=
	  MHD_get_fdset(d, &rs, &ws, &es, &max))
	{
          curl_multi_remove_handle (multi, c);
          curl_multi_cleanup (multi);
          curl_easy_cleanup (c);
          MHD_stop_daemon (d);
	  return 4;
	}
      tv.tv_sec = 0;
      tv.tv_usec = 1000;
      select(max + 1, &rs, &ws, &es, &tv);
      if (NULL != multi)
	{
	  curl_multi_perform (multi, &running);
	  if (running == 0)
	    {
	      msg = curl_multi_info_read (multi, &running);
	      if (msg == NULL)
		break;
	      if (msg->msg == CURLMSG_DONE)
		{
		  if (msg->data.result != CURLE_OK)
		    printf ("%s failed at %s:%d: `%s'\n",
			    "curl_multi_perform",
			    __FILE__,
			    __LINE__, curl_easy_strerror (msg->data.result));
		  curl_multi_remove_handle (multi, c);
		  curl_multi_cleanup (multi);
		  curl_easy_cleanup (c);
		  c = NULL;
		  multi = NULL;
		}
	    }
	}
      MHD_run(d);
    }
  MHD_stop_daemon(d);
  return 0;
}
