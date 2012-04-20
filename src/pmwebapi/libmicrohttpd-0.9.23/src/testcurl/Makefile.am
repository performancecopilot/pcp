SUBDIRS  = .

if USE_COVERAGE
  AM_CFLAGS = -fprofile-arcs -ftest-coverage
endif

if ENABLE_HTTPS
  SUBDIRS += https
endif

if USE_PRIVATE_PLIBC_H
 PLIBC_INCLUDE = -I$(top_srcdir)/src/include/plibc
endif

AM_CPPFLAGS = \
  $(PLIBC_INCLUDE) \
-I$(top_srcdir) \
-I$(top_srcdir)/src/daemon \
-I$(top_srcdir)/src/include \
$(LIBCURL_CPPFLAGS)

if !HAVE_W32
PERF_GET_CONCURRENT=perf_get_concurrent
if HAVE_CURL_BINARY
CURL_FORK_TEST=daemontest_get_response_cleanup
endif
endif

check_PROGRAMS = \
  test_start_stop \
  daemontest_get \
  daemontest_get_sendfile \
  daemontest_urlparse \
  daemontest_put \
  daemontest_process_headers \
  daemontest_process_arguments \
  daemontest_parse_cookies \
  daemontest_large_put \
  daemontest_get11 \
  daemontest_get_sendfile11 \
  daemontest_put11 \
  daemontest_large_put11 \
  daemontest_long_header \
  daemontest_long_header11 \
  daemontest_get_chunked \
  daemontest_put_chunked \
  daemontest_iplimit11 \
  daemontest_termination \
  daemontest_timeout \
  test_callback \
  $(CURL_FORK_TEST) \
  perf_get $(PERF_GET_CONCURRENT)

if HAVE_POSTPROCESSOR
 check_PROGRAMS += \
  daemontest_post \
  daemontest_postform \
  daemontest_post_loop \
  daemontest_post11 \
  daemontest_postform11 \
  daemontest_post_loop11 
endif

noinst_PROGRAMS = \
  daemon_options_test

if ENABLE_DAUTH
  check_PROGRAMS += \
	daemontest_digestauth daemontest_digestauth_with_arguments
endif

TESTS = $(check_PROGRAMS)

noinst_LIBRARIES = libcurl_version_check.a

libcurl_version_check_a_SOURCES = \
  curl_version_check.c

test_start_stop_SOURCES = \
  test_start_stop.c
test_start_stop_LDADD = \
  $(top_builddir)/src/daemon/libmicrohttpd.la

daemon_options_test_SOURCES = \
  daemon_options_test.c
daemon_options_test_LDADD = \
  $(top_builddir)/src/daemon/libmicrohttpd.la

daemontest_get_SOURCES = \
  daemontest_get.c
daemontest_get_LDADD = \
  $(top_builddir)/src/daemon/libmicrohttpd.la \
  @LIBCURL@ 

test_callback_SOURCES = \
  test_callback.c
test_callback_LDADD = \
  $(top_builddir)/src/daemon/libmicrohttpd.la \
  @LIBCURL@ 

perf_get_SOURCES = \
  perf_get.c \
  gauger.h
perf_get_LDADD = \
  $(top_builddir)/src/daemon/libmicrohttpd.la \
  @LIBCURL@ 

perf_get_concurrent_SOURCES = \
  perf_get_concurrent.c \
  gauger.h
perf_get_concurrent_LDADD = \
  $(top_builddir)/src/daemon/libmicrohttpd.la \
  @LIBCURL@ 

daemontest_digestauth_SOURCES = \
  daemontest_digestauth.c
daemontest_digestauth_LDADD = \
  $(top_builddir)/src/daemon/libmicrohttpd.la \
  @LIBCURL@

daemontest_digestauth_with_arguments_SOURCES = \
  daemontest_digestauth_with_arguments.c
daemontest_digestauth_with_arguments_LDADD = \
  $(top_builddir)/src/daemon/libmicrohttpd.la \
  @LIBCURL@

daemontest_get_sendfile_SOURCES = \
  daemontest_get_sendfile.c
daemontest_get_sendfile_LDADD = \
  $(top_builddir)/src/daemon/libmicrohttpd.la \
  @LIBCURL@ 

daemontest_urlparse_SOURCES = \
  daemontest_urlparse.c
daemontest_urlparse_LDADD = \
  $(top_builddir)/src/daemon/libmicrohttpd.la \
  @LIBCURL@ 

daemontest_get_response_cleanup_SOURCES = \
  daemontest_get_response_cleanup.c
daemontest_get_response_cleanup_LDADD = \
  $(top_builddir)/src/daemon/libmicrohttpd.la 

daemontest_get_chunked_SOURCES = \
  daemontest_get_chunked.c
daemontest_get_chunked_LDADD = \
  $(top_builddir)/src/daemon/libmicrohttpd.la \
  @LIBCURL@ 

daemontest_post_SOURCES = \
  daemontest_post.c
daemontest_post_LDADD = \
  $(top_builddir)/src/daemon/libmicrohttpd.la \
  @LIBCURL@ 

daemontest_process_headers_SOURCES = \
  daemontest_process_headers.c
daemontest_process_headers_LDADD = \
  $(top_builddir)/src/daemon/libmicrohttpd.la \
  @LIBCURL@ 

daemontest_parse_cookies_SOURCES = \
  daemontest_parse_cookies.c
daemontest_parse_cookies_LDADD = \
  $(top_builddir)/src/daemon/libmicrohttpd.la \
  @LIBCURL@ 

daemontest_process_arguments_SOURCES = \
  daemontest_process_arguments.c
daemontest_process_arguments_LDADD = \
  $(top_builddir)/src/daemon/libmicrohttpd.la \
  @LIBCURL@ 

daemontest_postform_SOURCES = \
  daemontest_postform.c
daemontest_postform_LDADD = \
  $(top_builddir)/src/daemon/libmicrohttpd.la \
  @LIBCURL@ 

daemontest_post_loop_SOURCES = \
  daemontest_post_loop.c
daemontest_post_loop_LDADD = \
  $(top_builddir)/src/daemon/libmicrohttpd.la \
  @LIBCURL@ 

daemontest_put_SOURCES = \
  daemontest_put.c
daemontest_put_LDADD = \
  $(top_builddir)/src/daemon/libmicrohttpd.la \
  @LIBCURL@ 

daemontest_put_chunked_SOURCES = \
  daemontest_put_chunked.c
daemontest_put_chunked_LDADD = \
  $(top_builddir)/src/daemon/libmicrohttpd.la \
  @LIBCURL@ 

daemontest_get11_SOURCES = \
  daemontest_get.c
daemontest_get11_LDADD = \
  $(top_builddir)/src/daemon/libmicrohttpd.la \
  @LIBCURL@ 

daemontest_get_sendfile11_SOURCES = \
  daemontest_get_sendfile.c
daemontest_get_sendfile11_LDADD = \
  $(top_builddir)/src/daemon/libmicrohttpd.la \
  @LIBCURL@ 

daemontest_post11_SOURCES = \
  daemontest_post.c
daemontest_post11_LDADD = \
  $(top_builddir)/src/daemon/libmicrohttpd.la \
  @LIBCURL@ 

daemontest_postform11_SOURCES = \
  daemontest_postform.c
daemontest_postform11_LDADD = \
  $(top_builddir)/src/daemon/libmicrohttpd.la \
  @LIBCURL@ 

daemontest_post_loop11_SOURCES = \
  daemontest_post_loop.c
daemontest_post_loop11_LDADD = \
  $(top_builddir)/src/daemon/libmicrohttpd.la \
  @LIBCURL@ 

daemontest_put11_SOURCES = \
  daemontest_put.c
daemontest_put11_LDADD = \
  $(top_builddir)/src/daemon/libmicrohttpd.la \
  @LIBCURL@ 

daemontest_large_put_SOURCES = \
  daemontest_large_put.c
daemontest_large_put_LDADD = \
  $(top_builddir)/src/daemon/libmicrohttpd.la \
  @LIBCURL@ 

daemontest_large_put11_SOURCES = \
  daemontest_large_put.c
daemontest_large_put11_LDADD = \
  $(top_builddir)/src/daemon/libmicrohttpd.la \
  @LIBCURL@ 

daemontest_long_header_SOURCES = \
  daemontest_long_header.c
daemontest_long_header_LDADD = \
  $(top_builddir)/src/daemon/libmicrohttpd.la \
  @LIBCURL@ 

daemontest_long_header11_SOURCES = \
  daemontest_long_header.c
daemontest_long_header11_LDADD = \
  $(top_builddir)/src/daemon/libmicrohttpd.la \
  @LIBCURL@ 

daemontest_iplimit11_SOURCES = \
  daemontest_iplimit.c
daemontest_iplimit11_LDADD = \
  $(top_builddir)/src/daemon/libmicrohttpd.la \
  @LIBCURL@ 

daemontest_termination_SOURCES = \
  daemontest_termination.c
daemontest_termination_LDADD = \
  $(top_builddir)/src/daemon/libmicrohttpd.la \
  @LIBCURL@ 

daemontest_timeout_SOURCES = \
  daemontest_timeout.c
daemontest_timeout_LDADD = \
  $(top_builddir)/src/daemon/libmicrohttpd.la \
  @LIBCURL@ 

