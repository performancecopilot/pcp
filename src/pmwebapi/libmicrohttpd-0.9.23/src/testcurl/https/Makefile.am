SUBDIRS = .

if USE_COVERAGE
  AM_CFLAGS = --coverage
endif

if USE_PRIVATE_PLIBC_H
 PLIBC_INCLUDE = -I$(top_srcdir)/src/include/plibc
endif

AM_CPPFLAGS = \
  $(PLIBC_INCLUDE) \
  -I$(top_srcdir)/src/include \
  -I$(top_srcdir)/src/daemon \
  $(LIBCURL_CPPFLAGS)

check_PROGRAMS = \
  tls_daemon_options_test \
  tls_authentication_test \
  mhds_multi_daemon_test \
  mhds_get_test \
  mhds_get_test_select \
  mhds_session_info_test \
  tls_thread_mode_test \
  tls_multi_thread_mode_test \
  tls_session_time_out_test

EXTRA_DIST = cert.pem key.pem tls_test_keys.h tls_test_common.h

# tls_authentication_test currently fails for unknown reasons
TESTS = \
  tls_daemon_options_test \
  mhds_multi_daemon_test \
  mhds_get_test \
  mhds_get_test_select \
  mhds_session_info_test \
  tls_thread_mode_test \
  tls_multi_thread_mode_test \
  tls_session_time_out_test \
  tls_authentication_test

# cURL dependent tests
tls_session_time_out_test_SOURCES = \
  tls_session_time_out_test.c \
  tls_test_common.c
tls_session_time_out_test_LDADD  = \
  $(top_builddir)/src/testcurl/libcurl_version_check.a \
  $(top_builddir)/src/daemon/libmicrohttpd.la \
  @LIBCURL@ -lgnutls @LIBGCRYPT_LIBS@

tls_daemon_options_test_SOURCES = \
  tls_daemon_options_test.c \
  tls_test_common.c
tls_daemon_options_test_LDADD = \
  $(top_builddir)/src/testcurl/libcurl_version_check.a \
  $(top_builddir)/src/daemon/libmicrohttpd.la \
  @LIBCURL@ -lgnutls @LIBGCRYPT_LIBS@

tls_thread_mode_test_SOURCES = \
  tls_thread_mode_test.c \
  tls_test_common.c
tls_thread_mode_test_LDADD = \
  $(top_builddir)/src/testcurl/libcurl_version_check.a \
  $(top_builddir)/src/daemon/libmicrohttpd.la \
  @LIBCURL@ -lgnutls @LIBGCRYPT_LIBS@

tls_multi_thread_mode_test_SOURCES = \
  tls_multi_thread_mode_test.c \
  tls_test_common.c
tls_multi_thread_mode_test_LDADD = \
  $(top_builddir)/src/testcurl/libcurl_version_check.a \
  $(top_builddir)/src/daemon/libmicrohttpd.la \
  @LIBCURL@ -lgnutls @LIBGCRYPT_LIBS@

tls_authentication_test_SOURCES = \
  tls_authentication_test.c \
  tls_test_common.c
tls_authentication_test_LDADD  = \
  $(top_builddir)/src/testcurl/libcurl_version_check.a \
  $(top_builddir)/src/daemon/libmicrohttpd.la \
  @LIBCURL@ -lgnutls @LIBGCRYPT_LIBS@

mhds_session_info_test_SOURCES = \
  mhds_session_info_test.c \
  tls_test_common.c
mhds_session_info_test_LDADD  = \
  $(top_builddir)/src/testcurl/libcurl_version_check.a \
  $(top_builddir)/src/daemon/libmicrohttpd.la \
  @LIBCURL@ -lgnutls @LIBGCRYPT_LIBS@

mhds_multi_daemon_test_SOURCES = \
  mhds_multi_daemon_test.c \
  tls_test_common.c
mhds_multi_daemon_test_LDADD  = \
  $(top_builddir)/src/testcurl/libcurl_version_check.a \
  $(top_builddir)/src/daemon/libmicrohttpd.la \
  @LIBCURL@ -lgnutls @LIBGCRYPT_LIBS@

mhds_get_test_SOURCES = \
  mhds_get_test.c \
  tls_test_common.c
mhds_get_test_LDADD  = \
  $(top_builddir)/src/testcurl/libcurl_version_check.a \
  $(top_builddir)/src/daemon/libmicrohttpd.la \
  @LIBCURL@ -lgnutls @LIBGCRYPT_LIBS@


mhds_get_test_select_SOURCES = \
  mhds_get_test_select.c \
  tls_test_common.c
mhds_get_test_select_LDADD  = \
  $(top_builddir)/src/testcurl/libcurl_version_check.a \
  $(top_builddir)/src/daemon/libmicrohttpd.la \
  @LIBCURL@ -lgnutls @LIBGCRYPT_LIBS@

